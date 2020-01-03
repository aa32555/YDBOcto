/****************************************************************
 *								*
 * Copyright (c) 2019-2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "octo.h"
#include "octo_types.h"
#include "logical_plan.h"

// Below is a helper function that should be invoked only from this file hence the prototype is defined in the .c file
// and not in the .h file like is the usual case.
LogicalPlan *lp_optimize_where_multi_equal_ands_helper(LogicalPlan *plan, LogicalPlan *where, int *key_unique_id_array);

/**
 * Verifies that the equals statement in equals can be "fixed", which involves checking
 * that:
 *  1. At least one of the values is a column
 *  2. At least one of the columns is a key
 *  3. If one is a key, the other a column, that they're not from the same table
 *  4. No columns are from compound statements (which don't have xref)
 *
 * This does not generate cross references if needed
 */
int lp_verify_valid_for_key_fix(LogicalPlan *plan, LogicalPlan *equals) {
	LogicalPlan *left, *right;
	int i1, i2;
	SqlTableAlias *table_alias, *table_alias2;

	assert(NULL != equals);
	assert(LP_BOOLEAN_EQUALS == equals->type);
	left = equals->v.lp_default.operand[0];
	if (!(left->type == LP_VALUE || left->type == LP_COLUMN_ALIAS)) {
		return FALSE;
	}
	right = equals->v.lp_default.operand[1];
	if (!(right->type == LP_VALUE || right->type == LP_COLUMN_ALIAS)) {
		return FALSE;
	}
	if (right->type == LP_COLUMN_ALIAS && left->type == LP_COLUMN_ALIAS) {
		// Both are column references; find which occurs first
		i1 = lp_get_key_index(plan, left);
		i2 = lp_get_key_index(plan, right);
		// If the key is in the same table as the temporary value, we can't do anything
		if (i1 == -1 || i2 == -1) {
			UNPACK_SQL_STATEMENT(table_alias, left->v.lp_column_alias.column_alias->table_alias, table_alias);
			UNPACK_SQL_STATEMENT(table_alias2, right->v.lp_column_alias.column_alias->table_alias, table_alias);
			if (table_alias->unique_id == table_alias2->unique_id) {
				return FALSE;
			}
		}
		// If the temporary value is a key from a compound statement, we can't do anything
		if (i1 == -2 || i2 == -2) {
			return FALSE;
		}
	}
	return TRUE;
}

void lp_optimize_where_multi_equal_ands(LogicalPlan *plan, LogicalPlan *where) {
	int		*key_unique_id_array;	// keys_unique_id_ordering[unique_id] = index in the ordered list
	int		max_unique_id;		// 1 more than maximum possible table_id/unique_id
	int		i;
	LogicalPlan	*cur, *lp_key;
	SqlKey		*key;

	max_unique_id = *plan->counter - 1;
	key_unique_id_array = octo_cmalloc(memory_chunks, sizeof(int) * max_unique_id);	// guarantees 0-initialized memory */
	// Look at the sorting of the keys in the plan ordering.
	// When selecting which to fix, always pick the key with the higher unique_id.
	// This should ensure that we will always have the right sequence.
	cur = lp_get_keys(plan);
	i = 1;	// start at non-zero value since 0 is a special value indicating key is not known to the current query/sub-query
		// (e.g. comes in from parent query). Treat 0 as the highest possible unique_id in the function
		// "lp_optimize_where_multi_equal_ands_helper".
	while (cur != NULL) {
		GET_LP(lp_key, cur, 0, LP_KEY);
		key = lp_key->v.lp_key.key;
		// This will end up filling the table with "lowest" id, but since it'll be sequential,
		// that will still be just about right
		key_unique_id_array[key->unique_id] = i;
		cur = cur->v.lp_default.operand[1];
		i++;
	}
	/* The assert of (NULL == where->v.lp_default.operand[1]) below is relied upon
	 * in "lp_optimize_where_multi_equal_ands_helper()".
	 */
	assert((LP_WHERE == where->type) && (NULL == where->v.lp_default.operand[1]));
	lp_optimize_where_multi_equal_ands_helper(plan, where, key_unique_id_array);
}

LogicalPlan *lp_optimize_where_multi_equal_ands_helper(LogicalPlan *plan, LogicalPlan *where, int *key_unique_id_array) {
	LogicalPlan		*cur, *left, *right, *t, *keys;
	LogicalPlan		*first_key, *before_first_key, *last_key, *before_last_key, *xref_keys;
	LogicalPlan		*generated_xref_keys;
	SqlColumnList		*column_list;
	SqlColumnListAlias	*column_list_alias;
	SqlColumnAlias		*column_alias;
	SqlTable		*table;
	SqlTableAlias		*table_alias;
	SqlKey			*key;
	int			left_id, right_id;

	if (where->type == LP_WHERE) {
		cur = where->v.lp_default.operand[0];
	} else {
		cur = where;
	}
	if (NULL == cur) {
		return where;
	}
	if (LP_BOOLEAN_OR == cur->type)	/* OR is currently not optimized (only AND and EQUALS are) */ {
		return where;
	}
	left = cur->v.lp_default.operand[0];
	right = cur->v.lp_default.operand[1];
	if (LP_BOOLEAN_AND == cur->type) {
		LogicalPlan	*new_left, *new_right;

		new_left = lp_optimize_where_multi_equal_ands_helper(plan, left, key_unique_id_array);
		cur->v.lp_default.operand[0] = new_left;
		new_right = lp_optimize_where_multi_equal_ands_helper(plan, right, key_unique_id_array);
		cur->v.lp_default.operand[1] = new_right;
		if (NULL == new_left) {
			if (LP_WHERE == where->type) {
				where->v.lp_default.operand[0] = new_right;
			}
			return new_right;
		}
		if (NULL == new_right) {
			if (LP_WHERE == where->type) {
				where->v.lp_default.operand[0] = new_left;
			}
			return new_left;
		}
		return where;
	}
	if (LP_BOOLEAN_EQUALS != cur->type) {
		return where; // The only things we currently optimize are AND and EQUALS
	}
	// Go through and fix keys as best as we can
	if (FALSE == lp_verify_valid_for_key_fix(plan, cur)) {
		return where;
	}
	if ((LP_COLUMN_ALIAS == right->type) && (LP_COLUMN_ALIAS == left->type)) {
		UNPACK_SQL_STATEMENT(table_alias, right->v.lp_column_alias.column_alias->table_alias, table_alias);
		right_id = table_alias->unique_id;
		UNPACK_SQL_STATEMENT(table_alias, left->v.lp_column_alias.column_alias->table_alias, table_alias);
		left_id = table_alias->unique_id;
		// If both column references correspond to the same table, then we cannot fix either columns/keys.
		if (left_id == right_id) {
			return where;
		}
		assert(0 <= left_id);
		assert(0 <= right_id);
		assert(left_id < *plan->counter);
		assert(right_id < *plan->counter);
		// If both column references correspond to tables from parent query then we cannot fix either.
		if ((0 == key_unique_id_array[left_id]) && (0 == key_unique_id_array[right_id])) {
			return where;
		}
		// If key_unique_id_array[right_id] == 0, that means the right column comes in from a parent query.
		// In that case we want to fix the left column to the right column so no swapping needed. Hence the
		// "0 != " check below (i.e. treats 0 as the highest possible unique_id; see comment in the function
		// "lp_optimize_where_multi_equal_ands()" for more details on this).
		if ((0 != key_unique_id_array[right_id]) && (key_unique_id_array[left_id] < key_unique_id_array[right_id])) {
			t = left;
			left = right;
			right = t;
		}
	} else {
		// At least one of these is a constant; just fix it
		if (LP_COLUMN_ALIAS == right->type) {
			// The left is a value, right a column, swap'em
			UNPACK_SQL_STATEMENT(table_alias, right->v.lp_column_alias.column_alias->table_alias, table_alias);
			right_id = table_alias->unique_id;
			if (0 == key_unique_id_array[right_id]) {
				// The right column corresponds to a unique_id coming in from the parent query.
				// In that case we cannot fix it in the sub-query. Return.
				return where;
			}
			t = left;
			left = right;
			right = t;
		} else if (LP_COLUMN_ALIAS == left->type) {
			// The right is a value, left a column. Check if left comes in from parent query.
			// In that case, we cannot fix it. Else it is already in the right order so no swap needed
			// like was the case in the previous "if" block.
			UNPACK_SQL_STATEMENT(table_alias, left->v.lp_column_alias.column_alias->table_alias, table_alias);
			left_id = table_alias->unique_id;
			if (0 == key_unique_id_array[left_id]) {
				// The left column corresponds to a unique_id coming in from the parent query.
				// In that case we cannot fix it in the sub-query. Return.
				return where;
			}
		} else {
			// Both left and right are values.
			assert(LP_VALUE == left->type);
			assert(LP_VALUE == right->type);
			// This something like 5=5; dumb and senseless, but the M
			//  compiler will optimize it away. Nothing we can do here.
			return where;
		}
	}
	key = lp_get_key(plan, left);
	// If the left isn't a key, generate a cross reference
	if (NULL == key) {
		// Get the table alias and column for left
		column_alias = left->v.lp_column_alias.column_alias;
		UNPACK_SQL_STATEMENT(table_alias, column_alias->table_alias, table_alias);
		/// TODO: how do we handle triggers on generated tables?
		if (table_STATEMENT != table_alias->table->type) {
			return where;
		}
		UNPACK_SQL_STATEMENT(table, table_alias->table, table);
		if (column_STATEMENT != column_alias->column->type) {
			UNPACK_SQL_STATEMENT(column_list_alias, column_alias->column, column_list_alias);
			UNPACK_SQL_STATEMENT(column_list, column_list_alias->column_list, column_list);
			UNPACK_SQL_STATEMENT(column_alias, column_list->value, column_alias);
		}
		generated_xref_keys = lp_generate_xref_keys(plan, table, column_alias, table_alias);
		if (NULL == generated_xref_keys) {
			return where;
		}
		// Remove all keys for the table alias
		before_first_key = lp_get_criteria(plan);
		first_key = keys = lp_get_keys(plan);
		do {
			GET_LP(t, first_key, 0, LP_KEY);
			if (t->v.lp_key.key->unique_id == table_alias->unique_id) {
				break;
			}
			if (NULL == first_key->v.lp_default.operand[1]) {
				break;
			}
			before_first_key = first_key;
			GET_LP(first_key, first_key, 1, LP_KEYS);
		} while (TRUE);
		// Find the last key
		last_key = first_key;
		before_last_key = first_key;
		do {
			GET_LP(t, last_key, 0, LP_KEY);
			if (t->v.lp_key.key->unique_id != table_alias->unique_id) {
				break;
			}
			before_last_key = last_key;
			if (NULL == last_key->v.lp_default.operand[1]) {
				break;
			}
			GET_LP(last_key, last_key, 1, LP_KEYS);
		} while (TRUE);
		// Generate the new key structure
		// First, insert a new key corresponding to the column in question
		xref_keys = lp_make_key(column_alias);
		key = xref_keys->v.lp_key.key;
		key->is_cross_reference_key = TRUE;
		key->cross_reference_column_alias = column_alias;
		if (LP_CRITERIA == before_first_key->type) {
			MALLOC_LP_2ARGS(before_first_key->v.lp_default.operand[0], LP_KEYS);
			before_first_key = before_first_key->v.lp_default.operand[0];
		} else {
			MALLOC_LP_2ARGS(before_first_key->v.lp_default.operand[1], LP_KEYS);
			before_first_key = before_first_key->v.lp_default.operand[1];
		}
		before_first_key->v.lp_default.operand[0] = xref_keys;
		xref_keys = generated_xref_keys;
		assert(xref_keys != NULL);
		before_first_key->v.lp_default.operand[1] = xref_keys;
		/* Note: It is possible the primary key for the table comprises of multiple columns in which case
		 * we need to scan down the xref_keys list (through v.operand[1]) until we hit the end i.e. we cannot
		 * assume there is only one column comprising the primary key. Hence the for loop below.
		 */
		for ( ; NULL != xref_keys->v.lp_default.operand[1]; xref_keys = xref_keys->v.lp_default.operand[1])
			;
		if (NULL != before_last_key->v.lp_default.operand[1]) {
			xref_keys->v.lp_default.operand[1] = before_last_key->v.lp_default.operand[1];
		}
	}
	if (lp_opt_fix_key_to_const(plan, key, right)) {
		/* Now that a key has been fixed to a constant (or another key), eliminate this LP_BOOLEAN_EQUALS from the
		 * WHERE expression as it will otherwise result in a redundant IF check in the generated M code. There is
		 * a subtlety involved with keys from parent queries which is handled below.
		 */
		if (((LP_COLUMN_ALIAS == right->type) && (LP_COLUMN_ALIAS == left->type))
				&& ((0 == key_unique_id_array[left_id]) || (0 == key_unique_id_array[right_id]))) {
			/* Both are columns and one of them belongs to a parent query. In that case, do not eliminate this
			 * altogether from the WHERE clause as that is relied upon in "generate_physical_plan()". Keep it in
			 * an alternate list (where->v.lp_default.operand[1]), use it in "generate_physical_plan()" (in order
			 * to ensure deferred plans get identified properly) and then switch back to where->v.lp_default.operand[0]
			 * for generating the plan M code which has this redundant check of a fixed key eliminated from the IF
			 * condition. This way the generated M code is optimized (in terms of not having a redundant equality
			 * check) even for plans with references to parent queries. It is okay to use
			 * where->v.lp_default.operand[1] for the alternate list because it is guaranteed to be NULL
			 * (asserted in caller function "lp_optimize_where_multi_equal_ands()").
			 */
			LogicalPlan	*where2, *opr1, *lp;

			where2 = lp_get_select_where(plan);
			opr1 = where2->v.lp_default.operand[1];
			if (NULL == opr1) {
				lp = cur;
			} else {
				MALLOC_LP_2ARGS(lp, LP_BOOLEAN_AND);
				lp->v.lp_default.operand[0] = opr1;
				lp->v.lp_default.operand[1] = cur;
			}
			where2->v.lp_default.operand[1] = lp;
		}
		if (LP_WHERE == where->type) {
			where->v.lp_default.operand[0] = NULL;
		}
		return NULL;
	} else
		return where;
}

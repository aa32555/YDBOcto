/****************************************************************
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
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

/**
 * plan should be a LP_TABLE_JOIN
 */
LogicalPlan *join_tables(LogicalPlan *root, LogicalPlan *plan) {
	LogicalPlan	*next = NULL, *table_plan;
	LogicalPlan	*keys = NULL, *select, *criteria, *cur_lp_key;
	LogicalPlan	*sub;
	SqlTable	*table;
	SqlTableAlias	*table_alias;
	SqlColumn	*key_columns[MAX_KEY_COUNT];
	int		max_key, cur_key, unique_id;

	if(plan->type != LP_TABLE_JOIN || plan->v.operand[0] == NULL)
		return plan;
	sub = plan->v.operand[0];
	if(plan->v.operand[0]->type == LP_INSERT) {
		// This plan needs to be inserted as a physical plan
		// Leave it alone here, and let the physical planner grab it
		plan->counter = root->counter;
		plan->v.operand[0] = sub = optimize_logical_plan(plan->v.operand[0]);
		if (NULL == sub)
			return NULL;
		assert(plan->counter == root->counter);
	}
	if (LP_SET_OPERATION == sub->type) {
		LogicalPlan	*set_plans;

		GET_LP(set_plans, sub, 1, LP_PLANS);
		set_plans->v.operand[0] = optimize_logical_plan(set_plans->v.operand[0]);
		if (NULL == set_plans->v.operand[0])
			return NULL;
		set_plans->v.operand[1] = optimize_logical_plan(set_plans->v.operand[1]);
		if (NULL == set_plans->v.operand[1])
			return NULL;
		cur_lp_key = lp_get_output_key(sub);
		lp_insert_key(root, cur_lp_key);
		return plan;
	}
	if(plan->v.operand[1] != NULL) {
		GET_LP(next, plan, 1, LP_TABLE_JOIN);
	}
	if(plan->v.operand[0]->type == LP_INSERT) {
		// Append that plans output keys to my list of keys
		GET_LP(cur_lp_key, sub, 1, LP_OUTPUT);
		GET_LP(cur_lp_key, cur_lp_key, 0, LP_KEY);
		lp_insert_key(root, cur_lp_key);
		if(next)
			if (NULL == join_tables(root, next))
				return NULL;
		return plan;
	}
	GET_LP(table_plan, plan, 0, LP_TABLE);
	select = lp_get_select(root);
	GET_LP(criteria, select, 1, LP_CRITERIA);
	GET_LP(keys, criteria, 0, LP_KEYS);
	// Drill down to the bottom of the keys
	while(keys->v.operand[1] != NULL) {
		GET_LP(keys, keys, 1, LP_KEYS);
	}
	// If we drilled down somewhat, make sure we start on a fresh "key"
	if(keys->v.operand[0] != NULL) {
		MALLOC_LP_2ARGS(keys->v.operand[1], LP_KEYS);
		keys = keys->v.operand[1];
	}
	if(table_plan->type == LP_TABLE) {
		table_alias = table_plan->v.table_alias;
		UNPACK_SQL_STATEMENT(table, table_alias->table, table);
		unique_id = table_alias->unique_id;
		memset(key_columns, 0, MAX_KEY_COUNT * sizeof(SqlColumn*));
		max_key = get_key_columns(table, key_columns);
		for(cur_key = 0; cur_key <= max_key; cur_key++) {
			MALLOC_LP(cur_lp_key, keys->v.operand[0], LP_KEY);
			OCTO_CMALLOC_STRUCT(cur_lp_key->v.key, SqlKey);
			memset(cur_lp_key->v.key, 0, sizeof(SqlKey));
			cur_lp_key->v.key->column = key_columns[cur_key];
			cur_lp_key->v.key->key_num = cur_key;
			cur_lp_key->v.key->unique_id = unique_id;
			cur_lp_key->v.key->table = table;
			cur_lp_key->v.key->type = LP_KEY_ADVANCE;
			cur_lp_key->v.key->owner = table_plan;
			if(cur_key != max_key) {
				MALLOC_LP_2ARGS(keys->v.operand[1], LP_KEYS);
				keys = keys->v.operand[1];
			}
		}
	} else if(table_plan->type == LP_INSERT) {
		// Else, we read from the output of the previous statement statement as a key
		GET_LP(cur_lp_key, table_plan, 1, LP_KEY);
		keys->v.operand[0] = lp_copy_plan(cur_lp_key);
	} else {
		assert(FALSE);
	}
	if(next)
		if (NULL == join_tables(root, next))
			return NULL;
	return plan;
}


LogicalPlan *optimize_logical_plan(LogicalPlan *plan) {
	LogicalPlan	*select, *table_join, *where;
	boolean_t	disable_dnf_expansion;
	LogicalPlan	*new_plan;
	int		num_outer_joins;
	if (NULL == plan)
		return NULL;

	if(plan->type == LP_SET_OPERATION) {
		plan->v.operand[1]->v.operand[0] = optimize_logical_plan(plan->v.operand[1]->v.operand[0]);
		if (NULL == plan->v.operand[1]->v.operand[0])
			return NULL;
		plan->v.operand[1]->v.operand[1] = optimize_logical_plan(plan->v.operand[1]->v.operand[1]);
		if (NULL == plan->v.operand[1]->v.operand[1])
			return NULL;
		return plan;
	}

	// First, "join" all the tables; we should do a search here to find the
	//  optimal join order
	select = lp_get_select(plan);
	GET_LP(table_join, select, 0, LP_TABLE_JOIN);
	// We can end coming here with a already populated set of keys if the plan was split for optimization
	// If so, don't add the keys again
	LogicalPlan *keys;
	keys = lp_get_keys(plan);
	if(keys->v.operand[0] == NULL) {
		if (NULL == join_tables(plan, table_join))
			return NULL;
	}

	// If there are no "OR" or "AND" statements, fix key values
	where = lp_get_select_where(plan);
	new_plan = plan;
	num_outer_joins = where->extra_detail;
	disable_dnf_expansion = (1 < num_outer_joins);
	if (!disable_dnf_expansion)
	{
		where->v.operand[0] = lp_make_normal_disjunctive_form(where->v.operand[0]);
		// Expand the plan, if needed
		LogicalPlan	*cur = where->v.operand[0];
		while(cur != NULL && cur->type == LP_BOOLEAN_OR) {
			SqlOptionalKeyword	*keywords, *new_keyword;

			keywords = lp_get_select_keywords(plan)->v.keywords;
			new_keyword = get_keyword_from_keywords(keywords, OPTIONAL_BOOLEAN_EXPANSION);
			if(new_keyword == NULL) {
				OCTO_CMALLOC_STRUCT(new_keyword, SqlOptionalKeyword);
				dqinit(new_keyword);
				new_keyword->keyword = OPTIONAL_BOOLEAN_EXPANSION;
				dqappend(keywords, new_keyword);
			}
			LogicalPlan	*p = lp_copy_plan(plan);
			LogicalPlan	*child_where = lp_get_select_where(p);

			child_where->v.operand[0] = cur->v.operand[0];
			new_plan = lp_join_plans(p, new_plan, LP_SET_DNF);
			cur = cur->v.operand[1];
		}
		where->v.operand[0] = cur;
		if (new_plan != plan)
			return optimize_logical_plan(new_plan);
	}
	// Perform optimizations where we are able
	do {
		assert(LP_TABLE_JOIN == table_join->type);
		if (NULL != table_join->join_on_condition)
			lp_optimize_where_multi_equal_ands(plan, table_join->join_on_condition);
		table_join = table_join->v.operand[1];
	} while (NULL != table_join);
	/* In case an OUTER JOIN exists, we cannot easily optimize the WHERE condition.
	 * Care has to be taken while invoking key fixing optimizations. Will be done at a later time.
	 * Skip it for now.
	 */
	if (!num_outer_joins)
		lp_optimize_where_multi_equal_ands(plan, where);
	return new_plan;
}

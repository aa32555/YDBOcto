/****************************************************************
 *								*
 * Copyright (c) 2021-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include <assert.h>
#include "octo.h"
#include "octo_types.h"

// STATE and STATUS are pointers to the intended state and status variables
#define SET_HASH_AND_INNER_EXPRESSION(GB_FIELD, STATE, STMT, STATUS, IS_INNER_QUERY_EXPRESSION) \
	{                                                                                       \
		hash_canonical_query((STATE), (STMT), (STATUS), TRUE);                          \
		(GB_FIELD)->hash = *(STATE);                                                    \
		(GB_FIELD)->is_inner_expression = *(IS_INNER_QUERY_EXPRESSION);                 \
	}

#define SET_RET_WITH_GROUP_BY_FIELD(SQL_ELEM, STMT, RET)        \
	{                                                       \
		UNPACK_SQL_STATEMENT(SQL_ELEM, STMT, SQL_ELEM); \
		(RET) = &(SQL_ELEM->group_by_fields);           \
	}

/* Return value
 * 	In case of call_loc == CONST
 * 		return value can be NULL in case where we end up reaching a column_alias along with *is_constant field set to TRUE
 *or FALSE based on column_alias being part of current query or outer query. Apart from the above case it will return the
 *participating expression's `group_by_field` structure. In call_loc cases return value is `group_by_field` of the found expression.
 *Can return NULL if the expression doesn't have anything returnable like constants.
 */
group_by_fields_t *get_group_by_structure(ExpressionSwitchCallLoc call_loc, SqlStatement *stmt, SqlTableAlias *table_alias,
					  boolean_t *is_constant) {
	group_by_fields_t *ret = NULL;
	UNUSED(ret);
	switch (stmt->type) {
	case coalesce_STATEMENT:;
		SqlCoalesceCall *coalesce;
		SET_RET_WITH_GROUP_BY_FIELD(coalesce, stmt, ret);
		break;
	case null_if_STATEMENT:;
		SqlNullIf *null_if;
		SET_RET_WITH_GROUP_BY_FIELD(null_if, stmt, ret);
		break;
	case greatest_STATEMENT:;
		SqlGreatest *greatest;
		SET_RET_WITH_GROUP_BY_FIELD(greatest, stmt, ret);
		break;
	case least_STATEMENT:;
		SqlLeast *least;
		SET_RET_WITH_GROUP_BY_FIELD(least, stmt, ret);
		break;
	case cas_STATEMENT:;
		SqlCaseStatement *cas;
		SET_RET_WITH_GROUP_BY_FIELD(cas, stmt, ret);
		break;
	case cas_branch_STATEMENT:;
		SqlCaseBranchStatement *cas_branch;
		SET_RET_WITH_GROUP_BY_FIELD(cas_branch, stmt, ret);
		break;
	case binary_STATEMENT:;
		SqlBinaryOperation *binary;
		SET_RET_WITH_GROUP_BY_FIELD(binary, stmt, ret);
		break;
	case unary_STATEMENT:;
		SqlUnaryOperation *unary;
		SET_RET_WITH_GROUP_BY_FIELD(unary, stmt, ret);
		break;
	case value_STATEMENT:;
		SqlValue *value;
		UNPACK_SQL_STATEMENT(value, stmt, value);
		if (CALCULATED_VALUE == value->type) {
			SqlStatement *calculated_stmt = value->v.calculated;
			if (function_call_STATEMENT == calculated_stmt->type) {
				SqlFunctionCall *function_call;
				SET_RET_WITH_GROUP_BY_FIELD(function_call, calculated_stmt, ret);
			} else if (coalesce_STATEMENT == calculated_stmt->type) {
				SqlCoalesceCall *coalesce;
				SET_RET_WITH_GROUP_BY_FIELD(coalesce, calculated_stmt, ret);
			} else if (greatest_STATEMENT == calculated_stmt->type) {
				SqlGreatest *greatest;
				SET_RET_WITH_GROUP_BY_FIELD(greatest, calculated_stmt, ret);
			} else if (least_STATEMENT == calculated_stmt->type) {
				SqlLeast *least;
				SET_RET_WITH_GROUP_BY_FIELD(least, calculated_stmt, ret);
			} else if (null_if_STATEMENT == calculated_stmt->type) {
				SqlNullIf *null_if;
				SET_RET_WITH_GROUP_BY_FIELD(null_if, calculated_stmt, ret);
			}
			break;
		} else if (COERCE_TYPE == value->type) {
			if ((COL_LIST_ALIAS == call_loc) || (GROUP_HASH == call_loc)) {
				SET_RET_WITH_GROUP_BY_FIELD(value, stmt, ret);
			} else if (CONST == call_loc) {
				ret = get_group_by_structure(CONST, value->v.coerce_target, table_alias, is_constant);
				break;
			}
		}
		if (CONST == call_loc) {
			ret = NULL;
			*is_constant = TRUE; // is constant
		}
		break;
	case column_alias_STATEMENT:
		if (CONST == call_loc) {
			/* Possible that coerce type or column_list case can get us here
			 * Example: `select id::INTEGER from names group by 1;`
			 */
			ret = NULL;
			if (stmt->v.column_alias->table_alias_stmt->v.table_alias->parent_table_alias != table_alias) {
				*is_constant = TRUE; // is constant
			} else {
				*is_constant = FALSE; // not a constant
			}
		} else {
			// Nothing to do here as this case is taken care by regular qualify_statement code path
			assert((COL_LIST_ALIAS == call_loc) || (GROUP_HASH == call_loc));
		}
		break;
	case column_list_STATEMENT:;
		assert(CONST == call_loc);
		// We do not have COL_LIST_ALIAS call here because COL_LIST_ALIAS is only done for GROUP BY nodes and
		// such call will be done with already a deferenced col_list with its value being directly passed here.
		SqlColumnList *column_list;
		UNPACK_SQL_STATEMENT(column_list, stmt, column_list);
		if (!column_list->value) {
			// Empty column list hence a constant
			ret = NULL; // TRUE
			*is_constant = TRUE;
		} else {
			SqlColumnList *	   cl_start = column_list;
			group_by_fields_t *gb_fields;
			// If any of the node is not a constant then the expression is
			// considered not to be a constant.
			do {
				gb_fields = get_group_by_structure(CONST, column_list->value, table_alias, is_constant);
				if (NULL == gb_fields) {
					ret = gb_fields;
					if (FALSE == *is_constant) {
						break;
					}
				} else if (gb_fields->is_constant) {
					ret = gb_fields; // TRUE
							 // Traverse next node till FALSE or no more nodes
				} else {
					ret = gb_fields; // FALSE
					break;
				}
				column_list = column_list->next;
			} while (column_list != cl_start);
		}
		break;
	case table_alias_STATEMENT:;
	case set_operation_STATEMENT:;
		if (CONST == call_loc) {
			// subqueries are not supported in GroupBy at this time so issue an error
			// assert(FALSE);
			// can't assert as `select (select id from names limit 1) from names group by 1;`
			// as queries like above can get us to this code path
			ret = NULL; // TRUE
			*is_constant = TRUE;
		}
		break;
	case array_STATEMENT:;
		SqlArray *array;
		UNPACK_SQL_STATEMENT(array, stmt, array);
		UNUSED(array);
		assert(table_alias_STATEMENT == array->argument->type);
		if (CONST == call_loc) {
			ret = NULL; // TRUE
			*is_constant = TRUE;
		}
		break;
	default:
		assert(FALSE);
		break;
	}
	return ret;
}

/*
 * Return values
 * 1. When call_loc == CONST
 *	returned value is TRUE or FALSE informing caller that the expression is a constant or not.
 * 2. When call_loc == GROUP_HASH
 * 	`*state` and `*is_inner_expression` is set with expression's `hash` and is_inner_expression value.
 * 3. When call_loc == COL_LIST_ALIAS
 *	no return values but passed `hash` and `is_inner_expression` value is set to appropriate expression `group_by_fields`
 *structure
 */
boolean_t expression_switch_statement(ExpressionSwitchCallLoc call_loc, SqlStatement *stmt, hash128_state_t *state, int *status,
				      boolean_t *is_inner_expression, SqlTableAlias *table_alias) {
	boolean_t ret = FALSE;
	if (NULL == stmt) {
		assert(CONST == call_loc);
		return TRUE;
	}
	if (GROUP_HASH == call_loc) {
		assert(state);
		assert(is_inner_expression);
	}
	boolean_t	   get_group_by_result = FALSE;
	group_by_fields_t *gb_fields = get_group_by_structure(call_loc, stmt, table_alias, &get_group_by_result);
	switch (call_loc) {
	case COL_LIST_ALIAS:
		if (NULL != gb_fields) {
			SqlStatement *type_specific_stmt = stmt;
			if (value_STATEMENT == stmt->type) {
				SqlValue *value;
				UNPACK_SQL_STATEMENT(value, stmt, value);
				if (CALCULATED_VALUE == value->type)
					type_specific_stmt = value->v.calculated;
			}
			SET_HASH_AND_INNER_EXPRESSION(gb_fields, state, type_specific_stmt, status, is_inner_expression)
		}
		break;
	case GROUP_HASH:
		if (NULL != gb_fields) {
			*state = gb_fields->hash;
			*is_inner_expression = gb_fields->is_inner_expression;
		}
		break;
	case CONST:
		if (NULL == gb_fields) {
			ret = get_group_by_result;
		} else {
			ret = gb_fields->is_constant;
		}
		break;
	default:
		assert(FALSE);
	}
	return ret;
}

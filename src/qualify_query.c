/****************************************************************
 *								*
 * Copyright (c) 2019-2021 YottaDB LLC and/or its subsidiaries.	*
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

/* Returns:
 *	0 if query is successfully qualified.
 *	1 if query had errors during qualification.
 * Notes:
 * Insert statements are NOT queries or query expressions, but are handled under this function for now.
 */
int qualify_query(SqlStatement *query_expr_or_insert_stmt, SqlJoin *parent_join, SqlTableAlias *parent_table_alias,
		  QualifyStatementParms *ret) {

	int result = 0;

	if (insert_STATEMENT == query_expr_or_insert_stmt->type) {
		SqlStatement *	    insert_stmt = query_expr_or_insert_stmt;
		SqlInsertStatement *insert;
		UNPACK_SQL_STATEMENT(insert, insert_stmt, insert);
		assert(NULL == parent_join);
		assert(NULL == parent_table_alias);
		assert(NULL == ret->ret_cla);
		result |= qualify_query(insert->src_table_alias_stmt, NULL, NULL, ret);
		/* There is nothing to qualify in "insert->dst_table_alias" and "insert->columns" */
		return result;
	}

	SqlQueryExpression *query_expression;
	UNPACK_SQL_STATEMENT(query_expression, query_expr_or_insert_stmt, query_expression);

	SqlStatement *simple_query_expression = query_expression->simple_query_expression;
	result |= qualify_simple_query(simple_query_expression, parent_join, parent_table_alias, ret);

	if (set_operation_STATEMENT == simple_query_expression->type) {
		SqlSetOperation *set_opr;
		UNPACK_SQL_STATEMENT(set_opr, simple_query_expression, set_operation);

		SqlStatement *const left_operand = set_opr->operand[0];
		SqlStatement *	    leftmost_operand = left_operand;
		while (set_operation_STATEMENT == leftmost_operand->type) {
			SqlSetOperation *left_set;
			UNPACK_SQL_STATEMENT(left_set, leftmost_operand, set_operation);
			leftmost_operand = left_set->operand[0];
		}
		// `leftmost_operand` now points to the leftmost operand in the set operation

		SqlTableAlias *leftmost_table;
		UNPACK_SQL_STATEMENT(leftmost_table, leftmost_operand, table_alias);

		// TODO order-by qualification - column names and numbers only

		return result;
	}

	// TODO wipe everything but needed preparation and call to qualify_statement() for order-by clause
	SqlTableAlias *table_alias;
	UNPACK_SQL_STATEMENT(table_alias, simple_query_expression, table_alias);

	SqlStatementType table_type = table_alias->table->type;
	if (table_value_STATEMENT == table_type) {
		// TODO order-by qualification - full qualification
		/* For a table constructed using the VALUES clause, go through each value specified and look for
		 * any sub-queries. If so, qualify those. Literal values can be skipped.
		 */
		SqlTableValue *table_value;
		UNPACK_SQL_STATEMENT(table_value, table_alias->table, table_value);
		result
		    |= qualify_statement(simple_query_expression->order_by_expression, start_join, table_alias_stmt, 0, &lcl_ret);
		UNPACK_SQL_STATEMENT(row_value, table_value->row_value_stmt, row_value);
		start_row_value = row_value;
		do {
			result |= qualify_statement(row_value->value_list, parent_join, table_alias_stmt, 0, NULL);
			row_value = row_value->next;
		} while (row_value != start_row_value);
		return result;
		break;
	}

	SqlSelectStatement *select;
	UNPACK_SQL_STATEMENT(select, table_alias->table, select);

	SqlJoin *start_join;
	UNPACK_SQL_STATEMENT(start_join, select->table_list, join);

	/* Ensure strict column name qualification checks (i.e. all column name references have to be a valid column
	 * name in a valid existing table) by using NULL as the last parameter in `qualify_statement()` call below.
	 */
	table_alias->do_group_by_checks = FALSE; /* need to set this before invoking "qualify_statement()" */
	SqlColumnListAlias *ret_cla = NULL;
	table_alias->aggregate_depth = 0;
	QualifyStatementParms lcl_ret;
	assert(0 == table_alias->aggregate_depth);
	// Qualify ORDER BY clause next
	/* Now that all column names used in the query have been qualified, allow columns specified in
	 * ORDER BY to be qualified against any column names specified till now without any strict checking.
	 * Hence the use of a non-NULL value ("&ret_cla") for "lcl_ret->ret_cla".
	 */
	lcl_ret.ret_cla = &ret_cla;
	lcl_ret.max_unique_id = ((NULL != ret) ? ret->max_unique_id : NULL);
	// TODO determine if parent_join needs to be spliced in/out of the `start_join` circular LL before/after this statement
	result |= qualify_statement(simple_query_expression->order_by_expression, start_join, table_alias_stmt, 0, &lcl_ret);

	return result;
}

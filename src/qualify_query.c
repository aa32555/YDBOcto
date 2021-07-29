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
 */
int qualify_query(SqlStatement *table_alias_stmt, SqlJoin *parent_join, SqlTableAlias *parent_table_alias,
		  QualifyStatementParms *ret) {
	SqlColumnListAlias *ret_cla;
	SqlSelectStatement *select;
	SqlTableAlias *	    table_alias;
	int		    result;
	SqlStatement *	    optional_order_by;
	SqlStatement *	    simple_query_expression;

	result = 0;
	UNPACK_SQL_STATEMENT(simple_query_expression, table_alias_stmt->simple_query_expression, simple_query_expression);
	result |= qualify_simple_query(simple_query_expression, parent_join, parent_table_alias, ret);
	if (table_alias_STATEMENT != simple_query_expression->type) {
		return; // TODO: xemacs tabs don't work as in vim, need to figure out how to get matching indentation
	}

	UNPACK_SQL_STATEMENT(table_alias, table_alias_stmt->simple_query_expression, table_alias);
	UNPACK_SQL_STATEMENT(select, table_alias->table, select);
	UNPACK_SQL_STATEMENT(join, select->table_list, join);

	ret_cla = NULL;
	table_alias->aggregate_depth
	    = 0; // TODO: fairly sure wiping this out at this level is wrong, but I'm not sure how to handle depth tracking here
	UNPACK_SQL_STATEMENT(optional_order_by, table_alias_stmt->optional_order_by, optional_order_by);
	if (NULL != optional_order_by) {
		QualifyStatementParms lcl_ret;
		// Qualify ORDER BY clause next
		/* Now that all column names used in the query have been qualified, allow columns specified in
		 * ORDER BY to be qualified against any column names specified till now without any strict checking.
		 * Hence the use of a non-NULL value ("&ret_cla") for "lcl_ret->ret_cla".
		 */
		lcl_ret.ret_cla = &ret_cla;
		lcl_ret.max_unique_id = ((NULL != ret) ? ret->max_unique_id : NULL);
		// TODO: is there an easy way to grab the proper value of start_join for the argument below?
		result |= qualify_statement(select->order_by_expression, start_join, table_alias_stmt, 0, &lcl_ret);
	}
	/* TODO: Not 100% sure moving all of the join cleanup to qualify_simple_query() is possible
	 */
	return result;
}

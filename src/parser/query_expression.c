/****************************************************************
 *								*
 * Copyright (c) 2021 YottaDB LLC and/or its subsidiaries.	*
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

/* Function invoked by the rule named "query_expression" in src/parser.y
 */
SqlStatement *query_expression(SqlStatement *simple_query_expression, SqlStatement *optional_order_by) {
	SqlStatement *ret;
	SQL_STATEMENT(ret, query_expression_STATEMENT);
	MALLOC_STATEMENT(ret, query_expression, SimpleQueryExpression);

	ret->order_by_expression = optional_order_by;
	return ret;
}

/****************************************************************
 *								*
 * Copyright (c) 2019-2022 YottaDB LLC and/or its subsidiaries.	*
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

// Function invoked by the rule named "grouping_column_reference" in src/parser/select.y
SqlStatement *grouping_column_reference(SqlStatement *derived_column_expression, SqlStatement *collate_clause) {
	SqlColumnListAlias *alias;
	SqlStatement *	    ret;
	SqlValue *	    value;
	boolean_t	    invalid_syntax;

	assert(NULL == collate_clause); /* below code needs to be fixed to handle collate_clause if/when it becomes non-NULL */
	UNUSED(collate_clause);
	if (value_STATEMENT == derived_column_expression->type) {
		UNPACK_SQL_STATEMENT(value, derived_column_expression, value);
		invalid_syntax
		    = ((INTEGER_LITERAL != value->type) && (COLUMN_REFERENCE != value->type) && (TABLE_ASTERISK != value->type)
		       && (CALCULATED_VALUE != value->type) && (BOOLEAN_VALUE != value->type) && (COERCE_TYPE != value->type));
		if (CALCULATED_VALUE == value->type) {
			SqlStatement *calculated_value = value->v.calculated;
			invalid_syntax
			    = ((function_call_STATEMENT != calculated_value->type) && (coalesce_STATEMENT != calculated_value->type)
			       && (null_if_STATEMENT != calculated_value->type) && (greatest_STATEMENT != calculated_value->type)
			       && (least_STATEMENT != calculated_value->type));
		} else if ((COERCE_TYPE == value->type) && (table_alias_STATEMENT == value->v.coerce_target->type)) {
			invalid_syntax = TRUE;
		}
	} else if (binary_STATEMENT == derived_column_expression->type || (unary_STATEMENT == derived_column_expression->type)) {
		invalid_syntax = FALSE;
	} else if (cas_STATEMENT == derived_column_expression->type) {
		invalid_syntax = FALSE;
	} else {
		invalid_syntax = TRUE;
	}
	if (invalid_syntax)
		return NULL;

	SQL_COLUMN_LIST_ALIAS_STATEMENT(ret);
	UNPACK_SQL_STATEMENT(alias, ret, column_list_alias);
	alias->column_list = create_sql_column_list(derived_column_expression, NULL, &derived_column_expression->loc);
	return ret;
}

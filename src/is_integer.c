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
#include "octo_types.h"
#include "octo.h"
/*
 * Checks if the passed cur_cla is having an -/+ numberic literal and returns the literal
 * Returns info on error_encountered, negative/postive numeric literal and the actual integer through method return value and
 * is_negaitve_numeric_lietal, is_postive_numberic_literal, retval.
 */
boolean_t is_integer(SqlColumnListAlias *cur_cla, boolean_t *ret_is_negative_numeric_literal,
		     boolean_t *ret_is_positive_numeric_literal, long int *retval, char **ret_str) {
	/* Case (2) : If "*ret_cla" is NULL, check if this is a case of ORDER BY column-number.
	 * If so, point "cur_cla" to corresponding cla from the SELECT column list.
	 */
	SqlColumnList *col_list;
	boolean_t      error_encountered = FALSE;
	boolean_t      is_positive_numeric_literal, is_negative_numeric_literal;
	char *	       str = NULL;

	UNPACK_SQL_STATEMENT(col_list, cur_cla->column_list, column_list);
	/* Check for positive numeric literal */
	is_positive_numeric_literal
	    = ((value_STATEMENT == col_list->value->type)
	       && ((INTEGER_LITERAL == col_list->value->v.value->type) || (NUMERIC_LITERAL == col_list->value->v.value->type)));
	/* Check for negative numeric literal next */
	is_negative_numeric_literal = FALSE;
	if (!is_positive_numeric_literal) {
		if (unary_STATEMENT == col_list->value->type) {
			SqlUnaryOperation *unary;

			UNPACK_SQL_STATEMENT(unary, col_list->value, unary);
			is_negative_numeric_literal = ((NEGATIVE == unary->operation) && (value_STATEMENT == unary->operand->type)
						       && ((INTEGER_LITERAL == unary->operand->v.value->type)
							   || (NUMERIC_LITERAL == unary->operand->v.value->type)));
			if (is_negative_numeric_literal) {
				str = unary->operand->v.value->v.string_literal;
			}
		}
	} else {
		str = col_list->value->v.value->v.string_literal;
	}
	if (is_positive_numeric_literal || is_negative_numeric_literal) {
		/* Check if numeric literal is an integer. We are guaranteed by the
		 * lexer that this numeric literal only contains digits [0-9] and
		 * optionally a '.'. Check if the '.' is present. If so issue error
		 * as decimal column numbers are disallowed in ORDER BY.
		 */
		char *ptr, *ptr_top;

		for (ptr = str, ptr_top = str + strlen(str); ptr < ptr_top; ptr++) {
			if ('.' == *ptr) {
				ERROR(ERR_ORDER_BY_POSITION_NOT_INTEGER, is_negative_numeric_literal ? "-" : "", str);
				yyerror(NULL, NULL, &cur_cla->column_list, NULL, NULL, NULL);
				error_encountered = TRUE;
				break;
			}
		}
		if (!error_encountered) {
			/* Now that we have confirmed the string only consists of the digits [0-9],
			 * check if it is a valid number that can be represented in an integer.
			 * If not issue error.
			 */
			*retval = strtol(str, NULL, 10);
			if ((LONG_MIN == *retval) || (LONG_MAX == *retval)) {
				ERROR(ERR_ORDER_BY_POSITION_NOT_INTEGER, is_negative_numeric_literal ? "-" : "", str);
				yyerror(NULL, NULL, &cur_cla->column_list, NULL, NULL, NULL);
				error_encountered = 1;
			}
		}
	}
	*ret_str = str;
	*ret_is_negative_numeric_literal = is_negative_numeric_literal;
	*ret_is_positive_numeric_literal = is_positive_numeric_literal;
	return error_encountered;
}

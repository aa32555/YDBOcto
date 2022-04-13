/****************************************************************
 *								*
 * Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	*
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
#include "parser.h"
#include "helpers.h"

boolean_t is_same_sqlvalue(SqlValue *val1, SqlValue *val2) {
	if ((NULL != val1) && (NULL != val2)) {
		// fprintf(stderr, "DBG: val1->type: %d\t val2->type: %d\n", val1->type, val2->type);
		// fprintf(stderr, "DBG: val1-v.string_literal: %s\tval2->v.string_literal: %s\n", val1->v.string_literal,
		// val2->v.string_literal);
		if ((val1->type == val2->type) && !(strcmp(val1->v.string_literal, val2->v.string_literal))) {
			return TRUE;
		}
	} else if ((NULL == val1) && (NULL == val2)) {
		return TRUE;
	}
	return FALSE;
}

/* Recurse through all CASE statement branches starting from branch pointed to by cas_branch
 * and attempt to retrieve the result of the CASE statement. If the result is guaranteed to be
 * a single value, return that value as a string. Otherwise, signal that there is no common value by returning NULL.
 *
 * Depending on the results of this function, the caller will either:
 *   1. Optimize away the given CASE statement since it always results in the same value, or
 *   2. Retain the CASE statement as is to allow full evaluation of various possible result scenarios.
 */
SqlValue *get_case_branch_result(SqlStatement *stmt) {
	SqlValue *		cur_branch_value, *prev_branch_value;
	SqlValue *		value, *optional_else;
	SqlCaseStatement *	cas_stmt;
	SqlCaseBranchStatement *cas_branch, *cur_branch;

	switch (stmt->type) {
	case cas_STATEMENT:
		UNPACK_SQL_STATEMENT(cas_stmt, stmt, cas);
		cur_branch_value = get_case_branch_result(cas_stmt->branches);
		/* We can only do the CASE statement optimization when all branches AND
		 * any ELSE clause all have the same value. So, we must check the result of
		 * the branches for this CASE statement (i.e. `cas_stmt`) against its optional
		 * ELSE, if there is one.
		 *
		 * If the ELSE is omitted, the CASE statement will implicitly return SQL NULL in the
		 * ELSE case. So, check whether the branches all resulted in a NUL_VALUE SqlValue statement.
		 * In that case, we can do the optimization since all the results are them same, i.e. NULL.
		 *
		 * If an ELSE is specified, we must check it against the result of the previous examination
		 * of the CASE branches, i.e. `cur_branch_value`. If the ELSE and the result of the branches
		 * differ, then we cannot optimize this CASE statement and so return NULL to signal this
		 * to the caller.
		 */
		if (NULL == cas_stmt->optional_else) {
			cur_branch_value = NULL;
		} else if (value_STATEMENT == cas_stmt->optional_else->type) {
			UNPACK_SQL_STATEMENT(optional_else, cas_stmt->optional_else, value);
			if (!(is_same_sqlvalue(cur_branch_value, optional_else))) {
				cur_branch_value = NULL;
			}
		}
		break;
	case cas_branch_STATEMENT:
		UNPACK_SQL_STATEMENT(cas_branch, stmt, cas_branch);
		cur_branch = cas_branch;
		// Store the initial branch value for later comparison against the next branch value.
		cur_branch_value = prev_branch_value = get_case_branch_result(cas_branch->value);
		do {
			if ((NULL == prev_branch_value) || (NULL == cur_branch_value)
			    || !(is_same_sqlvalue(prev_branch_value, cur_branch_value))) {
				/* If the values of the current and previous branches do not match, then we cannot optimize
				 * away the CASE statement, but must instead include all of the branches for evaluation at query
				 * runtime. So, signal that here and terminate the loop.
				 */
				cur_branch_value = NULL;
				break;
			}
			// Store the current branch value for later comparison against the next branch value.
			prev_branch_value = get_case_branch_result(cur_branch->value);
			cur_branch = cur_branch->next;
			cur_branch_value = get_case_branch_result(cur_branch->value);
		} while (cur_branch != cas_branch);
		break;
	case value_STATEMENT:
		UNPACK_SQL_STATEMENT(value, stmt, value);
		cur_branch_value = value;
		break;
	default:
		/* This expression is not another CASE statement or branch, nor a simple column or value.
		 * So, we cannot determine what the final value will be prior to query execution. Consequently,
		 * signal to the caller to skip the optimization in this case by returning NULL.
		 */
		return NULL;
		break;
	}

	return cur_branch_value;
}

/* When all of the CASE branches - including the optional ELSE clause - result in the same value, the CASE statement itself
 * can be removed as an optimization. This is because no matter what each CASE branch evaluates to, the result will be the
 * same. So, just replace the CASE statement with a SqlValue statement containing the shared value in that case.
 *
 * To check whether this optimization can be performed, call get_case_branch_result() to determine whether every CASE branch results
 * in the same value. If so, perform the optimization and remove the redundant CASE branches. Otherwise, omit the optimization and
 * populate the return SqlStatement from the arguments received from the parser without modification.
 */
SqlStatement *optimize_case_statement(SqlStatement *value_expression, SqlStatement *simple_when_clause,
				      SqlStatement *optional_else_clause) {
	SqlStatement *		ret;
	SqlValue *		branch_result;
	boolean_t		optimize_result;
	SqlCaseStatement *	cas;
	SqlCaseBranchStatement *cas_branch;

	branch_result = get_case_branch_result(simple_when_clause);
	optimize_result = TRUE;
	if (NULL != branch_result) {
		if (NULL == optional_else_clause) {
			if (NUL_VALUE != branch_result->type) {
				optimize_result = FALSE;
			}
		} else if ((value_STATEMENT == optional_else_clause->type)
			   && !(is_same_sqlvalue(branch_result, optional_else_clause->v.value))) {
			optimize_result = FALSE;
		}
	} else {
		optimize_result = FALSE;
	}

	if (optimize_result) {
		/* There are multiple CASE branches and all of them result in the same value, so don't bother to include them in the
		 * parse tree. Rather, just add that value (SqlValue) to the parse tree for this rule.
		 */
		UNPACK_SQL_STATEMENT(cas_branch, simple_when_clause, cas_branch);
		ret = cas_branch->value;
	} else {
		SQL_STATEMENT(ret, cas_STATEMENT);
		MALLOC_STATEMENT(ret, cas, SqlCaseStatement);
		UNPACK_SQL_STATEMENT(cas, ret, cas);
		cas->value = value_expression;
		cas->branches = simple_when_clause;
		cas->optional_else = optional_else_clause;
	}

	return ret;
}

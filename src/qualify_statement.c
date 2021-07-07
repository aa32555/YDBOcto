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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "octo.h"
#include "octo_types.h"

#define IS_EXPRESSION_QUALIFICATION(TABLE_ALIAS)                             \
	((SET_GROUP_BY_NUM_TO_EXPRESSION == TABLE_ALIAS->do_group_by_checks) \
	 && ((0 == TABLE_ALIAS->aggregate_depth) || (AGGREGATE_DEPTH_HAVING_CLAUSE == TABLE_ALIAS->aggregate_depth)))

#define ISSUE_GROUP_BY_OR_AGGREGATE_FUNCTION_ERROR(COLUMN_ALIAS_STMT)               \
	{                                                                           \
		SqlStatement *column_name;                                          \
		SqlValue *    value;                                                \
                                                                                    \
		column_name = find_column_alias_name(COLUMN_ALIAS_STMT);            \
		assert(NULL != column_name);                                        \
		UNPACK_SQL_STATEMENT(value, column_name, value);                    \
		ERROR(ERR_GROUP_BY_OR_AGGREGATE_FUNCTION, value->v.string_literal); \
		yyerror(NULL, NULL, &COLUMN_ALIAS_STMT, NULL, NULL, NULL);          \
	}

#define RETURN_IF_GROUP_BY_EXPRESSION_FOUND(SQL_ELEM, DO_GROUP_BY_CHECKS_VALUE)                                          \
	{                                                                                                                \
		if ((DO_GROUP_BY_CHECKS_VALUE == DO_GROUP_BY_CHECKS) && SQL_ELEM->group_by_fields.group_by_column_num) { \
			break;                                                                                           \
		}                                                                                                        \
	}

#define SET_EXPRESSION(TABLE_ALIAS, FUNCTION_EXPRESSION_SET)                                                    \
	{                                                                                                       \
		if (AGGREGATE_DEPTH_GROUP_BY_CLAUSE == TABLE_ALIAS->aggregate_depth) {                          \
			if (QualifyQuery_NONE == TABLE_ALIAS->qualify_query_stage) {                            \
				TABLE_ALIAS->qualify_query_stage = QualifyQuery_GROUP_BY_EXPRESSION;            \
				FUNCTION_EXPRESSION_SET                                                         \
				    = TRUE; /*This helps use to determine if current block set the expression*/ \
			}                                                                                       \
		}                                                                                               \
	}

#define UNSET_EXPRESSION(TABLE_ALIAS, FUNCTION_EXPRESSION_SET)                          \
	{                                                                               \
		if (TRUE == FUNCTION_EXPRESSION_SET) {                                  \
			/* The case block in the current execution had set this value*/ \
			/* Reset it for further calls */                                \
			TABLE_ALIAS->qualify_query_stage = QualifyQuery_NONE;           \
		}                                                                       \
	}
#define SET_INNER_EXPRESSION(SQL_ELEM, TABLE_ALIAS, PTR_IS_INNER_QUERY_EXPRESSION)                        \
	{                                                                                                 \
		if ((PTR_IS_INNER_QUERY_EXPRESSION)                                                       \
		    && ((AGGREGATE_DEPTH_GROUP_BY_CLAUSE == TABLE_ALIAS->aggregate_depth)                 \
			|| (0 == TABLE_ALIAS->aggregate_depth))) {                                        \
			SQL_ELEM->group_by_fields.is_inner_expression = *(PTR_IS_INNER_QUERY_EXPRESSION); \
		}                                                                                         \
	}

#define SET_EXPRESSION_GROUP_BY_NUM(SQL_ELEM, ARGUMENT, STMT, TABLE_ALIAS)                                                   \
	{                                                                                                                    \
		if (IS_EXPRESSION_QUALIFICATION(TABLE_ALIAS)) {                                                              \
			SQL_ELEM->group_by_fields.is_constant                                                                \
			    = expression_switch_statement(CONST, ARGUMENT, NULL, NULL, NULL, TABLE_ALIAS);                   \
			/* Set GROUP BY position if its not a constant */                                                    \
			if (!SQL_ELEM->group_by_fields.is_constant) {                                                        \
				boolean_t is_group_by_node_inner_expression = FALSE;                                         \
				int	  column_number                                                                      \
				    = get_group_by_column_number(TABLE_ALIAS, STMT, &is_group_by_node_inner_expression);     \
				if (-1 != column_number) {                                                                   \
					/* Matching GroupBy node is found note down the node number and if it belongs to the \
					 * current query */                                                                  \
					SQL_ELEM->group_by_fields.group_by_column_num = column_number;                       \
					SQL_ELEM->group_by_fields.is_inner_expression = is_group_by_node_inner_expression;   \
				}                                                                                            \
			}                                                                                                    \
		}                                                                                                            \
	}

/* Note: The code in "qualify_check_constraint.c" is modeled on the below so it is possible changes here might need to be
 *       made there too. And vice versa (i.e. changes to "qualify_statement.c" might need to be made here too).
 *       An automated tool "tools/ci/check_code_base_assertions.csh" alerts us (through the pre-commit script and/or
 *       pipeline jobs) if these two get out of sync.
 */

/* Returns:
 *	0 if query is successfully qualified.
 *	1 if query had errors during qualification.
 */
int qualify_statement(SqlStatement *stmt, SqlJoin *tables, SqlStatement *table_alias_stmt, int depth, QualifyStatementParms *ret,
		      boolean_t *is_inner_query_expression) {
	SqlBinaryOperation *	binary;
	SqlCaseBranchStatement *cas_branch, *cur_branch;
	SqlCaseStatement *	cas;
	SqlColumnAlias *	new_column_alias;
	SqlColumnList *		start_cl, *cur_cl;
	SqlColumnListAlias *	start_cla, *cur_cla;
	SqlFunctionCall *	fc;
	SqlCoalesceCall *	coalesce_call;
	SqlGreatest *		greatest_call;
	SqlLeast *		least_call;
	SqlNullIf *		null_if;
	SqlUnaryOperation *	unary;
	SqlArray *		array;
	SqlValue *		value;
	int			result;
	SqlTableAlias *		column_table_alias, *parent_table_alias, *table_alias;
	SqlColumnListAlias **	ret_cla;
	int			save_max_unique_id;
	int			i;
	boolean_t		function_expression_set = FALSE;

	result = 0;
	UNUSED(function_expression_set);
	if (NULL == stmt)
		return result;
	if ((NULL != ret) && (NULL != ret->max_unique_id)) {
		/* Determine max_unique_id under current subtree and store it in "stmt->hash_canonical_query_cycle".
		 * Hence the temporary reset. At the end, before returning, we will update "ret->max_unique_id" to be MAX.
		 */
		save_max_unique_id = *ret->max_unique_id;
		assert(0 <= save_max_unique_id);
		*ret->max_unique_id = 0;
	} else {
		save_max_unique_id = 0;
	}
	switch (stmt->type) {
	case column_alias_STATEMENT:
		UNPACK_SQL_STATEMENT(new_column_alias, stmt, column_alias);
		UNPACK_SQL_STATEMENT(column_table_alias, new_column_alias->table_alias_stmt, table_alias);
		parent_table_alias = column_table_alias->parent_table_alias;
		/* Assert that if we are doing GROUP BY related checks ("do_group_by_checks" is set to DO_GROUP_BY_CHECKS), then the
		 * "aggregate_function_or_group_by_or_having_specified" field is also TRUE.
		 */
		assert(!(DO_GROUP_BY_CHECKS == parent_table_alias->do_group_by_checks)
		       || parent_table_alias->aggregate_function_or_group_by_or_having_specified);
		/*
		 * This block is used in two cases
		 * 1. GroupBy is used and we need to validate column_alias usages
		 * 3. In case GroupBy is not present but HAVING clause is present. We still want to generate an error on
		 * column_alias usages as they are not grouped. To allow such condition check this case is used to perform the
		 * necessary validation.
		 */
		// 1. Column Reference check
		if ((DO_GROUP_BY_CHECKS == parent_table_alias->do_group_by_checks)
		    && ((0 == parent_table_alias->aggregate_depth)
			|| (AGGREGATE_DEPTH_HAVING_CLAUSE == parent_table_alias->aggregate_depth))
		    && !new_column_alias->group_by_column_number) {
			/* 1) We are doing GROUP BY related validation of column references in the query
			 * 2) The current column reference is not inside an aggregate function AND
			 * 3) The current column reference is not in the GROUP BY clause.
			 * Issue an error.
			 */
			ISSUE_GROUP_BY_OR_AGGREGATE_FUNCTION_ERROR(stmt);
			result = 1;
		}
		/* The column alias is part of an expression. If the column alias belongs to outer query then we know that the
		 * expression which holds it is also part of outer query. If the expression is present in GROUP BY then by storing
		 * this information we allow physical plan generation to decide whether to refer to grouped data at current query
		 * level or fetch the value from outer query. Without this information we will have to either trace through the
		 * expression again to find out whether the column and thus the expression belongs to the present query or not. This
		 * is avoided by storing `inner_query_expression` value at the expression level whose GROUP BY column number will be
		 * matched.
		 * Also, GROUP BY condition is required to make sure column_alias seen in column number replacement in GROUP BY
		 * clause gets checked to see if its part of this query or not.
		 */
		UNPACK_SQL_STATEMENT(table_alias, table_alias_stmt, table_alias);
		if (IS_EXPRESSION_QUALIFICATION(table_alias) || (AGGREGATE_DEPTH_GROUP_BY_CLAUSE == table_alias->aggregate_depth)) {
			// Indicate to the caller whether the current column reference belongs to this query or outer query
			if (parent_table_alias == table_alias) {
				*is_inner_query_expression = TRUE;
			} else {
				*is_inner_query_expression = FALSE;
			}
		}
		break;
	case value_STATEMENT:
		UNPACK_SQL_STATEMENT(value, stmt, value);
		switch (value->type) {
		case TABLE_ASTERISK:
			/* Any usage of table.* in a query leads to this case. It needs to be qualified similar to COLUMN_REFERENCE
			 * because later stages will assume this type is placed in a COLUMN_ALIAS and has the necessary
			 * table_alias_STATEMENT.
			 */
		case COLUMN_REFERENCE:
			UNPACK_SQL_STATEMENT(table_alias, table_alias_stmt, table_alias);
			if (table_alias->do_group_by_checks) {
				/* This is the second pass. Any appropriate error has already been issued so skip this. */
				break;
			}
			ret_cla = ((NULL == ret) ? NULL : ret->ret_cla);
			new_column_alias = qualify_column_name(value, tables, table_alias_stmt, depth + 1, ret_cla);
			result = ((NULL == new_column_alias) && ((NULL == ret_cla) || (NULL == *ret_cla)));
			if (result) {
				yyerror(NULL, NULL, &stmt, NULL, NULL, NULL);
			} else {
				if ((NULL == ret_cla) || (NULL == *ret_cla)) {
					// Convert this statement to a qualified one by changing the "type".
					// Note: "qualify_column_name.c" relies on the below for qualifying column names
					stmt->type = column_alias_STATEMENT;
					stmt->v.column_alias = new_column_alias;
					UNPACK_SQL_STATEMENT(column_table_alias, new_column_alias->table_alias_stmt, table_alias);
					if ((NULL != ret) && (NULL != ret->max_unique_id)) {
						int *max_unique_id;

						max_unique_id = ret->max_unique_id;
						if (*max_unique_id <= column_table_alias->unique_id) {
							*max_unique_id = column_table_alias->unique_id + 1;
						}
					}
					parent_table_alias = column_table_alias->parent_table_alias;
					if ((NULL != parent_table_alias) && (parent_table_alias->aggregate_depth)) {
						if ((0 < table_alias->aggregate_depth)
						    && (QualifyQuery_WHERE == parent_table_alias->qualify_query_stage)) {
							/* `aggregate_function_STATEMENT` case itself throws error when current
							 * table_alias is executing a WHERE clause. Because of that we are sure we
							 * wont reach this block of code when `table_alias == parent_table_alias`.
							 */
							assert(table_alias != parent_table_alias);
							/* Aggregate functions in a sub query is referencing outer query
							 * column. Also, the sub query is inside WHERE clause of the outer
							 * query. This usage is not allowed. Issue an ERROR.
							 */
							ERROR(ERR_AGGREGATE_FUNCTION_WHERE, "");
							result = 1;
							yyerror(NULL, NULL, &stmt, NULL, NULL, NULL);
						}
						int aggregate_depth = parent_table_alias->aggregate_depth;
						if (0 < aggregate_depth) {
							assert(AGGREGATE_FUNCTION_SPECIFIED
							       & parent_table_alias
								     ->aggregate_function_or_group_by_or_having_specified);
						} else if (AGGREGATE_DEPTH_GROUP_BY_CLAUSE == aggregate_depth) {
							if (QualifyQuery_GROUP_BY_EXPRESSION != table_alias->qualify_query_stage) {
								/* Update `group_by_column_count` and `group_by_column_number` */
								new_column_alias->group_by_column_number
								    = ++parent_table_alias->group_by_column_count;
							} else {
								/* This column alias is part of an expression in GroupBy.
								 * Do not update group_by_column_number here as we want to
								 * set GROUP BY column number to the root expression which holds
								 * this column
								 */
							}
						} else if (AGGREGATE_DEPTH_HAVING_CLAUSE == aggregate_depth) {
							/* Indicate to the caller whether the current column reference belongs to
							 * this query or outer query.
							 */
							if (parent_table_alias == table_alias) {
								*is_inner_query_expression = TRUE;
							} else {
								*is_inner_query_expression = FALSE;
							}
						} else {
							/* We are inside a WHERE or FROM/JOIN clause where  aggregate
							 * function use is disallowed so no need to record anything related
							 * to GROUP BY.
							 */
							assert((AGGREGATE_DEPTH_WHERE_CLAUSE == aggregate_depth)
							       || (AGGREGATE_DEPTH_FROM_CLAUSE == aggregate_depth));
						}
					} else if (parent_table_alias != table_alias) {
						// This helps to determine if the column_alias is referring to an outer query
						assert(is_inner_query_expression);
						*is_inner_query_expression = FALSE;
					}
				} else {
					/* Return pointer type is SqlColumnListAlias (not SqlColumnAlias).
					 * In this case, the needed adjustment of data type will be done in an ancestor
					 * caller of "qualify_statement" in the "case column_list_alias_STATEMENT:" code
					 * block (look for "QUALIFY_COLUMN_REFERENCE" in a comment in that code block).
					 * So just return.
					 */
				}
			}
			break;
		case CALCULATED_VALUE:
			result |= qualify_statement(value->v.calculated, tables, table_alias_stmt, depth + 1, ret,
						    is_inner_query_expression);
			break;
		case FUNCTION_NAME:
			/* Cannot validate the function using a "find_function()" call here (like we did "find_table()" for
			 * the table name in the parser). This is because we need type information of the actual function
			 * parameters to determine which function definition matches the current usage and that has to wait
			 * until "populate_data_type()". See detailed comment under "case function_call_STATEMENT:" there.
			 */
			break;
		case COERCE_TYPE:
			UNPACK_SQL_STATEMENT(table_alias, table_alias_stmt, table_alias);
			RETURN_IF_GROUP_BY_EXPRESSION_FOUND(value, table_alias->do_group_by_checks);
			SET_EXPRESSION(table_alias, function_expression_set);
			result |= qualify_statement(value->v.coerce_target, tables, table_alias_stmt, depth + 1, ret,
						    is_inner_query_expression);
			if (result) {
				yyerror(NULL, NULL, &stmt, NULL, NULL, NULL);
				break;
			}
			SET_INNER_EXPRESSION(value, table_alias, is_inner_query_expression);
			SET_EXPRESSION_GROUP_BY_NUM(value, value->v.coerce_target, stmt, table_alias);
			UNSET_EXPRESSION(table_alias, function_expression_set);
			break;
		case BOOLEAN_VALUE:
		case NUMERIC_LITERAL:
		case INTEGER_LITERAL:
		case STRING_LITERAL:
		case NUL_VALUE:
		case PARAMETER_VALUE:
			break;
		case FUNCTION_HASH:
		case DELIM_VALUE:
		case IS_NULL_LITERAL:
		case INVALID_SqlValueType:
		case UNKNOWN_SqlValueType:
			assert(FALSE);
			break;
			/* Do not add "default" case as we want to enumerate each explicit case here instead of having a
			 * general purpose bucket where all types not listed above fall into as that could hide subtle bugs.
			 */
		}
		break;
	case binary_STATEMENT:;
		UNPACK_SQL_STATEMENT(table_alias, table_alias_stmt, table_alias);
		SET_EXPRESSION(table_alias, function_expression_set);
		UNPACK_SQL_STATEMENT(binary, stmt, binary);
		RETURN_IF_GROUP_BY_EXPRESSION_FOUND(binary, table_alias->do_group_by_checks);
		boolean_t constant_expression[2] = {FALSE, FALSE};
		boolean_t tmp_is_inner_expression[2] = {TRUE, TRUE};
		for (i = 0; i < 2; i++) {
			result |= qualify_statement(binary->operands[i], tables, table_alias_stmt, depth + 1, ret,
						    &tmp_is_inner_expression[i]);
			if (IS_EXPRESSION_QUALIFICATION(table_alias)) {
				// We are performing a group by check and we need to know if the operand is a constant or not
				// to determine if we want to find this expression in GROUP BY or not
				constant_expression[i]
				    = expression_switch_statement(CONST, binary->operands[i], NULL, NULL, NULL, table_alias);
			}
		}
		if (is_inner_query_expression) {
			if ((TRUE == tmp_is_inner_expression[0]) && (TRUE == tmp_is_inner_expression[1])) {
				*is_inner_query_expression = TRUE;
			} else {
				*is_inner_query_expression = FALSE;
			}
			if ((AGGREGATE_DEPTH_GROUP_BY_CLAUSE == table_alias->aggregate_depth)
			    || ((0 == table_alias->aggregate_depth)
				|| (AGGREGATE_DEPTH_HAVING_CLAUSE == table_alias->aggregate_depth))) {
				binary->group_by_fields.is_inner_expression = *is_inner_query_expression;
			}
		}
		if (IS_EXPRESSION_QUALIFICATION(table_alias)) {
			if ((TRUE == constant_expression[0]) && (TRUE == constant_expression[1])) {
				binary->group_by_fields.is_constant = TRUE;
			} else {
				binary->group_by_fields.is_constant = FALSE;
			}
			if (!binary->group_by_fields.is_constant) {
				// This expression is not a constant so try to get its GROUP BY location
				boolean_t is_group_by_node_inner_expression = FALSE;
				int	  column_number
				    = get_group_by_column_number(table_alias, stmt, &is_group_by_node_inner_expression);
				if (-1 != column_number) {
					/* Matching GROUP BY node is found note down its location and whether it belongs to the
					 * current query.
					 */
					binary->group_by_fields.group_by_column_num = column_number;
					binary->group_by_fields.is_inner_expression = is_group_by_node_inner_expression;
				}
			}
		}
		UNSET_EXPRESSION(table_alias, function_expression_set);
		break;
	case unary_STATEMENT:;
		UNPACK_SQL_STATEMENT(table_alias, table_alias_stmt, table_alias);
		SET_EXPRESSION(table_alias, function_expression_set);
		UNPACK_SQL_STATEMENT(unary, stmt, unary);
		RETURN_IF_GROUP_BY_EXPRESSION_FOUND(unary, table_alias->do_group_by_checks);
		result |= qualify_statement(unary->operand, tables, table_alias_stmt, depth + 1, ret, is_inner_query_expression);
		SET_INNER_EXPRESSION(unary, table_alias, is_inner_query_expression);
		SET_EXPRESSION_GROUP_BY_NUM(unary, unary->operand, stmt, table_alias);
		UNSET_EXPRESSION(table_alias, function_expression_set);
		break;
	case array_STATEMENT:
		UNPACK_SQL_STATEMENT(array, stmt, array);
		result |= qualify_statement(array->argument, tables, table_alias_stmt, depth + 1, ret, is_inner_query_expression);
		break;
	case function_call_STATEMENT:;
		UNPACK_SQL_STATEMENT(table_alias, table_alias_stmt, table_alias);
		SET_EXPRESSION(table_alias, function_expression_set);
		UNPACK_SQL_STATEMENT(fc, stmt, function_call);
		RETURN_IF_GROUP_BY_EXPRESSION_FOUND(fc, table_alias->do_group_by_checks);
		result |= qualify_statement(fc->function_name, tables, table_alias_stmt, depth + 1, ret, NULL);
		result |= qualify_statement(fc->parameters, tables, table_alias_stmt, depth + 1, ret, is_inner_query_expression);
		SET_INNER_EXPRESSION(fc, table_alias, is_inner_query_expression);
		SET_EXPRESSION_GROUP_BY_NUM(fc, fc->parameters, stmt, table_alias);
		UNSET_EXPRESSION(table_alias, function_expression_set);
		break;
	case coalesce_STATEMENT:;
		UNPACK_SQL_STATEMENT(table_alias, table_alias_stmt, table_alias);
		SET_EXPRESSION(table_alias, function_expression_set);
		UNPACK_SQL_STATEMENT(coalesce_call, stmt, coalesce);
		RETURN_IF_GROUP_BY_EXPRESSION_FOUND(coalesce_call, table_alias->do_group_by_checks);
		result |= qualify_statement(coalesce_call->arguments, tables, table_alias_stmt, depth + 1, ret,
					    is_inner_query_expression);
		SET_INNER_EXPRESSION(coalesce_call, table_alias, is_inner_query_expression);
		SET_EXPRESSION_GROUP_BY_NUM(coalesce_call, coalesce_call->arguments, stmt, table_alias);
		UNSET_EXPRESSION(table_alias, function_expression_set);
		break;
	case greatest_STATEMENT:
		UNPACK_SQL_STATEMENT(table_alias, table_alias_stmt, table_alias);
		SET_EXPRESSION(table_alias, function_expression_set);
		UNPACK_SQL_STATEMENT(greatest_call, stmt, greatest);
		RETURN_IF_GROUP_BY_EXPRESSION_FOUND(greatest_call, table_alias->do_group_by_checks);
		result |= qualify_statement(greatest_call->arguments, tables, table_alias_stmt, depth + 1, ret,
					    is_inner_query_expression);
		SET_INNER_EXPRESSION(greatest_call, table_alias, is_inner_query_expression);
		SET_EXPRESSION_GROUP_BY_NUM(greatest_call, greatest_call->arguments, stmt, table_alias);
		UNSET_EXPRESSION(table_alias, function_expression_set);
		break;
	case least_STATEMENT:
		UNPACK_SQL_STATEMENT(table_alias, table_alias_stmt, table_alias);
		SET_EXPRESSION(table_alias, function_expression_set);
		UNPACK_SQL_STATEMENT(least_call, stmt, least);
		RETURN_IF_GROUP_BY_EXPRESSION_FOUND(least_call, table_alias->do_group_by_checks);
		result |= qualify_statement(least_call->arguments, tables, table_alias_stmt, depth + 1, ret,
					    is_inner_query_expression);
		SET_INNER_EXPRESSION(least_call, table_alias, is_inner_query_expression);
		SET_EXPRESSION_GROUP_BY_NUM(least_call, least_call->arguments, stmt, table_alias);
		UNSET_EXPRESSION(table_alias, function_expression_set);
		break;
	case null_if_STATEMENT:;
		UNPACK_SQL_STATEMENT(table_alias, table_alias_stmt, table_alias);
		SET_EXPRESSION(table_alias, function_expression_set);
		boolean_t is_null_if_inner_query_expression[2] = {TRUE, TRUE};
		boolean_t null_if_constant_expression[2] = {FALSE, FALSE};
		UNPACK_SQL_STATEMENT(null_if, stmt, null_if);
		RETURN_IF_GROUP_BY_EXPRESSION_FOUND(null_if, table_alias->do_group_by_checks);
		result |= qualify_statement(null_if->left, tables, table_alias_stmt, depth + 1, ret,
					    &is_null_if_inner_query_expression[0]);
		result |= qualify_statement(null_if->right, tables, table_alias_stmt, depth + 1, ret,
					    &is_null_if_inner_query_expression[1]);
		if (is_inner_query_expression) {
			if ((TRUE == is_null_if_inner_query_expression[0]) && (TRUE == is_null_if_inner_query_expression[1])) {
				*is_inner_query_expression = TRUE;
			} else {
				*is_inner_query_expression = FALSE;
			}
			if (IS_EXPRESSION_QUALIFICATION(table_alias)
			    || (AGGREGATE_DEPTH_GROUP_BY_CLAUSE == table_alias->aggregate_depth)) {
				null_if->group_by_fields.is_inner_expression = *is_inner_query_expression;
			}
		}
		if (IS_EXPRESSION_QUALIFICATION(table_alias)) {
			null_if_constant_expression[0]
			    = expression_switch_statement(CONST, null_if->left, NULL, NULL, NULL, table_alias);
			null_if_constant_expression[1]
			    = expression_switch_statement(CONST, null_if->right, NULL, NULL, NULL, table_alias);
			if ((TRUE == null_if_constant_expression[0]) && (TRUE == null_if_constant_expression[1])) {
				null_if->group_by_fields.is_constant = TRUE;
			} else {
				null_if->group_by_fields.is_constant = FALSE;
			}
			if (!null_if->group_by_fields.is_constant) {
				// This expression is not a constant so try to get its GROUP BY location
				boolean_t is_group_by_node_inner_expression = FALSE;
				int	  column_number
				    = get_group_by_column_number(table_alias, stmt, &is_group_by_node_inner_expression);
				if (-1 != column_number) {
					/* Matching GROUP BY node is found note down its location and whether it belongs to the
					 * current query.
					 */
					null_if->group_by_fields.group_by_column_num = column_number;
					null_if->group_by_fields.is_inner_expression = is_group_by_node_inner_expression;
				}
			}
		}
		UNSET_EXPRESSION(table_alias, function_expression_set);
		break;
	case aggregate_function_STATEMENT:;
		SqlAggregateFunction *af;
		boolean_t	      depth_adjusted;

		UNPACK_SQL_STATEMENT(af, stmt, aggregate_function);
		UNPACK_SQL_STATEMENT(table_alias, table_alias_stmt, table_alias);
		depth_adjusted = FALSE;
		if (table_alias->aggregate_depth) {
			if (AGGREGATE_DEPTH_GROUP_BY_CLAUSE == table_alias->aggregate_depth) {
				/* We can reach this block if an aggregate function from select list is referenced
				 * using column number. Ex: select count(n1.*) from names group by 1;
				 * We only get to know of this usage during group by check second pass so we need this
				 * check. GROUP BY specified using an aggregate function. Issue error as this usage is not
				 * valid.
				 */
				ERROR(ERR_GROUP_BY_INVALID_USAGE, "");
				yyerror(NULL, NULL, &af->parameter, NULL, NULL, NULL);
				result = 1;
				break;
			}
			if (table_alias->do_group_by_checks) {
				/* This is the second pass. Any appropriate error other than the one in IF block
				 * has already been issued so skip this.
				 */
				break;
			}
			if (0 < table_alias->aggregate_depth) {
				/* Nesting of aggregate functions at the same query level is not allowed */
				ERROR(ERR_AGGREGATE_FUNCTION_NESTED, "");
				result = 1;
			} else {
				/* Note: AGGREGATE_DEPTH_GROUP_BY_CLAUSE is also negative but we should never get here in that
				 *       case as the parser for GROUP BY would have issued an error if ever GROUP BY is
				 *       used without just a plain column name.
				 */
				if (AGGREGATE_DEPTH_GROUP_BY_CLAUSE == table_alias->aggregate_depth) {
					/* GROUP BY specified using an aggregate function. Issue error as this usage is not valid.
					 */
					ERROR(ERR_GROUP_BY_INVALID_USAGE, "");
				} else if (AGGREGATE_DEPTH_WHERE_CLAUSE == table_alias->aggregate_depth) {
					/* Aggregate functions are not allowed inside a WHERE clause */
					ERROR(ERR_AGGREGATE_FUNCTION_WHERE, "");
					result = 1;
				} else if (AGGREGATE_DEPTH_FROM_CLAUSE == table_alias->aggregate_depth) {
					/* Aggregate functions are not allowed inside a FROM clause.
					 * Since the only way to use aggregate functions inside a FROM clause is through
					 * JOIN conditions, issue a JOIN related error (not a FROM related error).
					 */
					ERROR(ERR_AGGREGATE_FUNCTION_JOIN, "");
					result = 1;
				} else if (AGGREGATE_DEPTH_HAVING_CLAUSE == table_alias->aggregate_depth) {
					/* Aggregate functions are allowed inside a HAVING clause.
					 * Temporarily reset depth to 0 before going inside the aggregate function call.
					 */
					table_alias->aggregate_depth = 0;
					depth_adjusted = TRUE;
				} else {
					assert(FALSE);
				}
			}
			if (result) {
				yyerror(&af->parameter->loc, NULL, NULL, NULL, NULL, NULL);
			}
		}
		if (!result) {
			assert(!table_alias->do_group_by_checks || table_alias->aggregate_function_or_group_by_or_having_specified);
			table_alias->aggregate_function_or_group_by_or_having_specified |= AGGREGATE_FUNCTION_SPECIFIED;
			table_alias->aggregate_depth++;
			result |= qualify_statement(af->parameter, tables, table_alias_stmt, depth + 1, ret,
						    is_inner_query_expression);
			if (0 == result) {
				UNPACK_SQL_STATEMENT(cur_cl, af->parameter, column_list);
				assert((AGGREGATE_COUNT_ASTERISK == af->type) || (NULL != cur_cl->value));
				assert((AGGREGATE_COUNT_ASTERISK != af->type) || (NULL == cur_cl->value));
				if ((NULL != cur_cl->value) && (column_alias_STATEMENT == cur_cl->value->type)) {
					UNPACK_SQL_STATEMENT(new_column_alias, cur_cl->value, column_alias);
					if (is_stmt_table_asterisk(new_column_alias->column)) {
						process_aggregate_function_table_asterisk(af);
					}
				}
			}
			table_alias->aggregate_depth--;
		}
		if (depth_adjusted) {
			/* Undo temporary reset of depth now that we have finished qualifying the aggregate function call */
			table_alias->aggregate_depth = AGGREGATE_DEPTH_HAVING_CLAUSE;
		}
		break;
	case cas_STATEMENT:
		UNPACK_SQL_STATEMENT(table_alias, table_alias_stmt, table_alias);
		SET_EXPRESSION(table_alias, function_expression_set);
		boolean_t cas_if_inner_query_expression[3] = {TRUE, TRUE, TRUE};
		boolean_t cas_constant_expression[3] = {FALSE, FALSE, FALSE};
		UNPACK_SQL_STATEMENT(cas, stmt, cas);
		RETURN_IF_GROUP_BY_EXPRESSION_FOUND(cas, table_alias->do_group_by_checks);
		result
		    |= qualify_statement(cas->value, tables, table_alias_stmt, depth + 1, ret, &cas_if_inner_query_expression[0]);
		result |= qualify_statement(cas->branches, tables, table_alias_stmt, depth + 1, ret,
					    &cas_if_inner_query_expression[1]);
		result |= qualify_statement(cas->optional_else, tables, table_alias_stmt, depth + 1, ret,
					    &cas_if_inner_query_expression[2]);
		if (is_inner_query_expression) {
			if ((TRUE == cas_if_inner_query_expression[0]) && (TRUE == cas_if_inner_query_expression[1])
			    && (TRUE == cas_if_inner_query_expression[2])) {
				*is_inner_query_expression = TRUE;
			} else {
				*is_inner_query_expression = FALSE;
			}
			if ((AGGREGATE_DEPTH_GROUP_BY_CLAUSE == table_alias->aggregate_depth)
			    || ((0 == table_alias->aggregate_depth)
				|| (AGGREGATE_DEPTH_HAVING_CLAUSE == table_alias->aggregate_depth))) {
				cas->group_by_fields.is_inner_expression = *is_inner_query_expression;
			}
		}
		if (IS_EXPRESSION_QUALIFICATION(table_alias)) {
			cas_constant_expression[0] = expression_switch_statement(CONST, cas->value, NULL, NULL, NULL, table_alias);
			cas_constant_expression[1]
			    = expression_switch_statement(CONST, cas->branches, NULL, NULL, NULL, table_alias);
			cas_constant_expression[2]
			    = expression_switch_statement(CONST, cas->optional_else, NULL, NULL, NULL, table_alias);
			if ((TRUE == cas_constant_expression[0]) && (TRUE == cas_constant_expression[1])
			    && (TRUE == cas_constant_expression[2])) {
				cas->group_by_fields.is_constant = TRUE;
			} else {
				cas->group_by_fields.is_constant = FALSE;
			}
			if (!cas->group_by_fields.is_constant) {
				// This expression is not a constant so try to get its GROUP BY location
				boolean_t is_group_by_node_inner_expression = FALSE;
				int	  column_number
				    = get_group_by_column_number(table_alias, stmt, &is_group_by_node_inner_expression);
				if (-1 != column_number) {
					/* Matching GROUP BY node is found note down its location and whether it belongs to the
					 * current query.
					 */
					cas->group_by_fields.group_by_column_num = column_number;
					cas->group_by_fields.is_inner_expression = is_group_by_node_inner_expression;
				}
			}
		}
		UNSET_EXPRESSION(table_alias, function_expression_set);
		break;
	case cas_branch_STATEMENT:;
		/* cas_branch need to check its various branches and set `is_inner_expression` and `is_const`
		 * This will allow caller to determine similar parameters of the cas_STATEMENT holding cas_branch_STATEMENT
		 * `*is_inner_query_expression` also need to be set
		 */
		UNPACK_SQL_STATEMENT(table_alias, table_alias_stmt, table_alias);
		SET_EXPRESSION(table_alias, function_expression_set)
		boolean_t cas_branch_if_inner_query_expression[2];
		if (is_inner_query_expression) {
			*is_inner_query_expression = TRUE;
		}
		UNPACK_SQL_STATEMENT(cas_branch, stmt, cas_branch);
		RETURN_IF_GROUP_BY_EXPRESSION_FOUND(cas_branch, table_alias->do_group_by_checks);
		cur_branch = cas_branch;
		cas_branch->group_by_fields.is_constant = TRUE;
		do {
			cas_branch_if_inner_query_expression[0] = TRUE;
			cas_branch_if_inner_query_expression[1] = TRUE;
			result |= qualify_statement(cur_branch->condition, tables, table_alias_stmt, depth + 1, ret,
						    &cas_branch_if_inner_query_expression[0]);
			result |= qualify_statement(cur_branch->value, tables, table_alias_stmt, depth + 1, ret,
						    &cas_branch_if_inner_query_expression[1]);
			if (is_inner_query_expression) {
				// if `*is_inner_query_expression` is not already set to FALSE perform the rest of the computation
				if (*is_inner_query_expression) {
					// Check if the current branch nodes belong to the current query or not
					if ((TRUE == cas_branch_if_inner_query_expression[0])
					    && (TRUE == cas_branch_if_inner_query_expression[1])) {
						*is_inner_query_expression = TRUE;
					} else {
						/* One of the branches has nodes which are not belonging to current query
						 * set `is_inner_query_expression` to FALSE.
						 * This if statement is never entered after this execution.
						 */
						*is_inner_query_expression = FALSE;
					}
				}
			}
			if (IS_EXPRESSION_QUALIFICATION(table_alias)) {
				boolean_t cas_branch_constant_expression[2] = {FALSE, FALSE};
				cas_branch_constant_expression[0]
				    = expression_switch_statement(CONST, cur_branch->condition, NULL, NULL, NULL, table_alias);
				cas_branch_constant_expression[1]
				    = expression_switch_statement(CONST, cur_branch->value, NULL, NULL, NULL, table_alias);
				if ((TRUE == cas_branch_constant_expression[0]) && (TRUE == cas_branch_constant_expression[1])) {
					cas_branch->group_by_fields.is_constant = TRUE;
				} else {
					cas_branch->group_by_fields.is_constant = FALSE;
				}
			}
			cur_branch = cur_branch->next;
		} while (cur_branch != cas_branch);
		if (is_inner_query_expression) {
			if ((AGGREGATE_DEPTH_GROUP_BY_CLAUSE == table_alias->aggregate_depth)
			    || ((0 == table_alias->aggregate_depth)
				|| (AGGREGATE_DEPTH_HAVING_CLAUSE == table_alias->aggregate_depth))) {
				cas_branch->group_by_fields.is_inner_expression = *is_inner_query_expression;
			}
		}
		UNSET_EXPRESSION(table_alias, function_expression_set);
		break;
	case column_list_STATEMENT:;
		UNPACK_SQL_STATEMENT(table_alias, table_alias_stmt, table_alias);
		boolean_t prev_is_inner_query_expression = TRUE;
		UNPACK_SQL_STATEMENT(start_cl, stmt, column_list);
		cur_cl = start_cl;
		do {
			result |= qualify_statement(cur_cl->value, tables, table_alias_stmt, depth + 1, ret,
						    is_inner_query_expression);
			if (is_inner_query_expression) {
				if ((TRUE == prev_is_inner_query_expression) && (TRUE == *is_inner_query_expression)) {
					/* In first iteration and iterations till first condition is true, the second one will be
					 * the decider of branch Once FALSE is set to prev_is_inner_query_expression this branch is
					 * never executed.
					 */
				} else {
					prev_is_inner_query_expression = FALSE;
				}
			}
			cur_cl = cur_cl->next;
		} while (cur_cl != start_cl);
		if (is_inner_query_expression)
			*is_inner_query_expression = prev_is_inner_query_expression;
		break;
	case table_alias_STATEMENT:
	case set_operation_STATEMENT:
		UNPACK_SQL_STATEMENT(table_alias, table_alias_stmt, table_alias);
		if (AGGREGATE_DEPTH_GROUP_BY_CLAUSE == table_alias->aggregate_depth) {
			ERROR(ERR_GROUP_BY_SUB_QUERY, "");
			yyerror(NULL, NULL, &stmt, NULL, NULL, NULL);
			result = 1;
			break;
		}
		if ((0 == table_alias->aggregate_depth) || (AGGREGATE_DEPTH_HAVING_CLAUSE == table_alias->aggregate_depth)) {
			*is_inner_query_expression = TRUE;
		}
		result |= qualify_query(stmt, tables, table_alias, ret);
		break;
	case column_list_alias_STATEMENT:;
		UNPACK_SQL_STATEMENT(start_cla, stmt, column_list_alias);
		UNPACK_SQL_STATEMENT(table_alias, table_alias_stmt, table_alias);
		cur_cla = start_cla;
		do {
			ret_cla = ((NULL == ret) ? NULL : ret->ret_cla);
			assert(depth || (NULL == ret_cla)
			       || (NULL == *ret_cla)); /* assert that caller has initialized "*ret_cla" */
			result |= qualify_statement(cur_cla->column_list, tables, table_alias_stmt, depth + 1, ret,
						    is_inner_query_expression);
			if (0 == result) {
				UNPACK_SQL_STATEMENT(cur_cl, cur_cla->column_list, column_list);
				if (column_alias_STATEMENT == cur_cl->value->type) {
					UNPACK_SQL_STATEMENT(new_column_alias, cur_cl->value, column_alias);
					if (is_stmt_table_asterisk(new_column_alias->column)) {
						process_table_asterisk_cla(stmt, &cur_cla, &start_cla,
									   table_alias->qualify_query_stage);
					}
				}
			}
			/* Check if its an OrderBy or GroupBy invocation */
			if (((NULL != ret_cla) || (AGGREGATE_DEPTH_GROUP_BY_CLAUSE == table_alias->aggregate_depth)) && (0 == depth)
			    && (!table_alias->do_group_by_checks)) {
				SqlColumnListAlias *qualified_cla;
				int		    column_number;
				char *		    str;
				boolean_t	    order_by_alias;

				/* "ret_cla" is non-NULL implies this call is for an ORDER BY column list.
				 * "depth" == 0 implies "cur_cla" is one of the ORDER BY columns
				 * (i.e. not a cla corresponding to an inner evaluation in the ORDER BY expression).
				 * There are 3 cases to handle.
				 */
				if ((NULL != ret_cla) && (NULL != *ret_cla)) {
					/* Case (1) : If "*ret_cla" is non-NULL, it is a case of ORDER BY using an alias name */

					order_by_alias = TRUE;
					/* "QUALIFY_COLUMN_REFERENCE" : This code block finishes the column name validation
					 * that was not finished in the "qualify_column_name" call done in the
					 * "case COLUMN_REFERENCE:" code block above. We found another existing cla that is
					 * already qualified and matches the current non-qualified cla. So fix the current
					 * cla to mirror the already qualified cla.
					 */
					qualified_cla = *ret_cla;
					*ret_cla = NULL; /* initialize for next call to "qualify_statement" */
				} else {
					/* Case (2) : Check if this is a case of column-number usage.
					 * If so, point "cur_cla" to corresponding cla from the SELECT column list.
					 */
					SqlColumnList *col_list;
					boolean_t      error_encountered = FALSE;
					boolean_t      is_positive_numeric_literal, is_negative_numeric_literal;

					order_by_alias = FALSE;
					UNPACK_SQL_STATEMENT(col_list, cur_cla->column_list, column_list);
					/* Check for positive numeric literal */
					is_positive_numeric_literal = ((value_STATEMENT == col_list->value->type)
								       && ((INTEGER_LITERAL == col_list->value->v.value->type)
									   || (NUMERIC_LITERAL == col_list->value->v.value->type)));
					/* Check for negative numeric literal next */
					is_negative_numeric_literal = FALSE;
					if (!is_positive_numeric_literal) {
						if (unary_STATEMENT == col_list->value->type) {
							SqlUnaryOperation *unary;

							UNPACK_SQL_STATEMENT(unary, col_list->value, unary);
							is_negative_numeric_literal
							    = ((NEGATIVE == unary->operation)
							       && (value_STATEMENT == unary->operand->type)
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
						char *	 ptr, *ptr_top;
						long int retval;

						for (ptr = str, ptr_top = str + strlen(str); ptr < ptr_top; ptr++) {
							if ('.' == *ptr) {
								ERROR(ERR_ORDER_BY_POSITION_NOT_INTEGER,
								      is_negative_numeric_literal ? "-" : "", str);
								yyerror(NULL, NULL, &cur_cla->column_list, NULL, NULL, NULL);
								error_encountered = 1;
								break;
							}
						}
						if (!error_encountered) {
							/* Now that we have confirmed the string only consists of the digits [0-9],
							 * check if it is a valid number that can be represented in an integer.
							 * If not issue error.
							 */
							retval = strtol(str, NULL, 10);
							if ((LONG_MIN == retval) || (LONG_MAX == retval)) {
								ERROR(ERR_ORDER_BY_POSITION_NOT_INTEGER,
								      is_negative_numeric_literal ? "-" : "", str);
								yyerror(NULL, NULL, &cur_cla->column_list, NULL, NULL, NULL);
								error_encountered = 1;
							}
						}
						if (!error_encountered) {
							/* Now that we have confirmed the input string is a valid integer,
							 * check if it is within the range of valid column numbers in the
							 * SELECT column list. If not, issue an error. If we already determined
							 * that this is a negative numeric literal, we can issue an error without
							 * looking at anything else. We wait for the decimal and integer range
							 * checks above to mirror the error messages that Postgres does.
							 */
							if (!is_negative_numeric_literal) {
								column_number = (int)retval;
								qualified_cla = get_column_list_alias_n_from_table_alias(
								    table_alias, column_number);
							} else {
								qualified_cla = NULL;
							}
							if (NULL == qualified_cla) {
								if (AGGREGATE_DEPTH_GROUP_BY_CLAUSE
								    == table_alias->aggregate_depth) {
									ERROR(ERR_GROUP_BY_POSITION_INVALID,
									      is_negative_numeric_literal ? "-" : "", str);
								} else {
									ERROR(ERR_ORDER_BY_POSITION_INVALID,
									      is_negative_numeric_literal ? "-" : "", str);
								}
								yyerror(NULL, NULL, &cur_cla->column_list, NULL, NULL, NULL);
								error_encountered = 1;
							}
						}
						if (error_encountered) {
							result = 1;
							break;
						}
					} else {
						qualified_cla = NULL;
					}
				}
				/* Actual repointing to SELECT column list cla happens here (above code set things up for here) */
				if (NULL != qualified_cla) {
					/* Case (2) : Case of column-number */
					cur_cla->column_list = qualified_cla->column_list;
					assert(NULL == cur_cla->alias);
					/* Note: It is not necessary to copy " qualified_cla->alias" into "cur_cla->alias" */
					/* "cur_cla->keywords" might have ASC or DESC keywords (for ORDER BY)
					 * which the qualified_cla would not have so do not overwrite those.
					 */
					cur_cla->type = qualified_cla->type;
					/* Set unique_id and column_number fields. Assert that they are non-zero as
					 * "hash_canonical_query" skips hashing these fields if they are 0.
					 */
					cur_cla->tbl_and_col_id.unique_id = table_alias->unique_id;
					assert(cur_cla->tbl_and_col_id.unique_id);
					if (order_by_alias) {
						/* Find the column number from the matched cla */
						cur_cla->tbl_and_col_id.column_number
						    = get_column_number_from_column_list_alias(qualified_cla, table_alias);
					} else {
						/* The column number was already specified. So use that as is. */
						cur_cla->tbl_and_col_id.column_number = column_number;
					}
					assert(cur_cla->tbl_and_col_id.column_number);
					if (AGGREGATE_DEPTH_GROUP_BY_CLAUSE == table_alias->aggregate_depth) {
						// call qualify_statement again so that GROUP BY is validated
						result |= qualify_statement(cur_cla->column_list, tables, table_alias_stmt,
									    depth + 1, ret, is_inner_query_expression);

						SqlColumnAlias *new_column_alias = NULL;
						SqlStatement *	tmp_stmt = cur_cla->column_list->v.column_list->value;
						column_table_alias = NULL;
						if (value_STATEMENT == tmp_stmt->type) {
							SqlValue *inner_value;
							UNPACK_SQL_STATEMENT(inner_value, tmp_stmt, value);
							if (COERCE_TYPE == inner_value->type) {
								if (column_alias_STATEMENT == inner_value->v.coerce_target->type) {
									new_column_alias
									    = inner_value->v.coerce_target->v.column_alias;
									UNPACK_SQL_STATEMENT(column_table_alias,
											     new_column_alias->table_alias_stmt,
											     table_alias);
								}
							}
						} else if (tmp_stmt->type == column_alias_STATEMENT) {
							new_column_alias
							    = cur_cla->column_list->v.column_list->value->v.column_alias;
							UNPACK_SQL_STATEMENT(column_table_alias, new_column_alias->table_alias_stmt,
									     table_alias);
						} else if (table_alias_STATEMENT == tmp_stmt->type) {
							UNPACK_SQL_STATEMENT(column_table_alias, tmp_stmt, table_alias);
						}
						if (column_table_alias) {
							parent_table_alias = column_table_alias->parent_table_alias;
							if (parent_table_alias == table_alias) {
								if (0 < parent_table_alias->aggregate_depth) {
									parent_table_alias
									    ->aggregate_function_or_group_by_or_having_specified
									    |= GROUP_BY_SPECIFIED;
								} else if (AGGREGATE_DEPTH_GROUP_BY_CLAUSE
									   == parent_table_alias->aggregate_depth) {
									SqlValue *value;
									if ((NULL != new_column_alias)
									    && ((NULL != new_column_alias->column)
										&& (value_STATEMENT
										    == new_column_alias->column->type))) {
										UNPACK_SQL_STATEMENT(
										    value, new_column_alias->column, value);
									} else {
										value = NULL;
									}
									/* `group_by_column_count` and `group_by_column_number` in
									 * case of TABLE_ASTERISK is updated by
									 * `process_table_asterisk_cla()` invocation so no need to
									 * do it here.
									 */
									if (tmp_stmt->type == table_alias_STATEMENT) {
										++parent_table_alias->group_by_column_count;
									} else if ((NULL == value)
										   || (TABLE_ASTERISK != value->type)) {
										new_column_alias->group_by_column_number
										    = ++parent_table_alias->group_by_column_count;
									}
								}
							}
						} else {
							/* Case of the column_list_alias node not being either a table_alias or
							 * column_alias We are sure that this node belongs to the present query So
							 * go ahead and update the current table_alias
							 */
							/* Ex: select 1+1 from names group by 1;
							 * an expression is replacing 1
							 * An expression can be constant or one with a column
							 * If column is involved then based on whether it belongs to
							 * inner query or not needs to be used to update
							 * `table_alias->group_by_column_count`
							 */
							if (0 < table_alias->aggregate_depth)
								table_alias->aggregate_function_or_group_by_or_having_specified
								    |= GROUP_BY_SPECIFIED;
							SqlStatement *tmp = cur_cla->column_list->v.column_list->value;
							if ((tmp->type == value_STATEMENT)
							    && (tmp->v.value->type != CALCULATED_VALUE))
								table_alias->group_by_column_count++;
							else {

								table_alias->group_by_column_count++;
							}
						}
					}
				} else {
					if (AGGREGATE_DEPTH_GROUP_BY_CLAUSE != table_alias->aggregate_depth) {

						/* Case (3) : Case of ORDER BY column expression */
						SqlSelectStatement *select;
						SqlOptionalKeyword *keywords, *keyword;

						/* Check if SELECT DISTINCT was specified */
						UNPACK_SQL_STATEMENT(select, table_alias->table, select);
						UNPACK_SQL_STATEMENT(keywords, select->optional_words, keyword);
						keyword = get_keyword_from_keywords(keywords, OPTIONAL_DISTINCT);
						if (NULL != keyword) {
							/* SELECT DISTINCT was specified. Check if the ORDER BY column expression
							 * matches some column specification in the SELECT column list. If so that
							 * is good. If not issue an error (see YDBOcto#461 for details).
							 */
							if (!match_column_list_alias_in_select_column_list(cur_cla,
													   select->select_list)) {
								ERROR(ERR_ORDER_BY_SELECT_DISTINCT, "");
								yyerror(NULL, NULL, &cur_cla->column_list, NULL, NULL, NULL);
								result = 1;
								break;
							}
						}
					} else {
						assert(AGGREGATE_DEPTH_GROUP_BY_CLAUSE == table_alias->aggregate_depth);
						UNPACK_SQL_STATEMENT(cur_cl, cur_cla->column_list, column_list);
						/* Here we increment only when the element is not a column or a column which is in
						 * the form of COERCE_TYPE i.e. used in cast operations Reason being
						 * column_alias_STATEMENTs increment table_alias group_by_column_count in the
						 * value_STATEMENT case itself during its qualification from COLUMN_REFERENCE to
						 * SqlColumnAlias.
						 */
						if (column_alias_STATEMENT != cur_cl->value->type) {
							table_alias->group_by_column_count++;
						}
					}
				}
			}
			/* Hash the entire GroupBy node. Here we will be hashing expressions which have been directly used.
			 * and also expression which has been placed in GroupBY because of replacement of column numbers.
			 * The hashing is needed as we compare the entire node hash with substructure nodes in expressions of select
			 * list. Note: we will need to set is_inner_expression here because whenever a column number is replaced
			 * with the actual expression we don't again qualify that expression as it is already qualified in select
			 * list and hence we do not go through the code path which initializes is_inner_expression which would have
			 * been done if the expression was directly used in GroupBy.
			 */
			UNPACK_SQL_STATEMENT(cur_cl, cur_cla->column_list, column_list);
			if ((AGGREGATE_DEPTH_GROUP_BY_CLAUSE == table_alias->aggregate_depth)
			    && (!table_alias->do_group_by_checks)) {
				hash128_state_t state;
				int		status;
				status = HASH_LITERAL_VALUES;
				HASH128_STATE_INIT(state, 0);
				assert(is_inner_query_expression);
				// hash and set the hashing result and is_inner_query_expression value to the sql element fields
				expression_switch_statement(COL_LIST_ALIAS, cur_cl->value, &state, &status,
							    is_inner_query_expression, table_alias);
			}
			if ((QualifyQuery_ORDER_BY == table_alias->qualify_query_stage)
			    && (column_alias_STATEMENT == cur_cl->value->type)) {
				UNPACK_SQL_STATEMENT(new_column_alias, cur_cl->value, column_alias);
				if (is_stmt_table_asterisk(new_column_alias->column)) {
					/* At this point if we are in ORDER BY we need to make sure the expanded TABLE_ASTERISK list
					 * is not processed again.
					 */
					cur_cla = new_column_alias->last_table_asterisk_node;
				}
			}
			cur_cla = cur_cla->next;
		} while (cur_cla != start_cla);
		break;
	case create_table_STATEMENT:
	case select_STATEMENT:
	case table_value_STATEMENT:
	case insert_STATEMENT:
	case delete_from_STATEMENT:
	case update_STATEMENT:
	case join_STATEMENT:
	case create_function_STATEMENT:
	case drop_table_STATEMENT:
	case drop_function_STATEMENT:
	case column_STATEMENT:
	case parameter_type_list_STATEMENT:
	case constraint_STATEMENT:
	case keyword_STATEMENT:
	case begin_STATEMENT:
	case commit_STATEMENT:
	case set_STATEMENT:
	case show_STATEMENT:
	case no_data_STATEMENT:
	case delim_char_list_STATEMENT:
	case index_STATEMENT:
	case data_type_struct_STATEMENT:
	case join_type_STATEMENT:
	case discard_all_STATEMENT:
	case row_value_STATEMENT:
	case history_STATEMENT:
	case display_relation_STATEMENT:
	case invalid_STATEMENT:
		/* Do not add "default:" case as we want to enumerate each explicit case here instead of having a
		 * general purpose bucket where all types not listed above fall into as that could hide subtle bugs.
		 */
		ERROR(ERR_UNKNOWN_KEYWORD_STATE, "");
		assert(FALSE);
		result = 1;
		break;
	}
	if ((NULL != ret) && (NULL != ret->max_unique_id)) {
		/* Caller has requested us to store the maximum unique_id seen under this subtree. So do that. */
		stmt->hash_canonical_query_cycle = (uint64_t)(*ret->max_unique_id);
		*ret->max_unique_id = MAX(*ret->max_unique_id, save_max_unique_id);
	}
	return result;
}

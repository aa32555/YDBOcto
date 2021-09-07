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

	// TODO wipe unneeded vars
	SqlJoin *	    join;
	SqlJoin *	    prev_start, *prev_end;
	SqlJoin *	    start_join, *cur_join;
	SqlSelectStatement *select;
	SqlTableAlias *	    table_alias;
	SqlStatement *	    group_by_expression;
	SqlStatementType    table_type;
	SqlTableValue *	    table_value;
	SqlRowValue *	    row_value, *start_row_value;

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
		} // `leftmost_operand` now points to the leftmost operand in the set operation

		SqlTableAlias *leftmost_table;
		UNPACK_SQL_STATEMENT(leftmost_table, leftmost_operand, table_alias);

		// TODO order-by qualification - column names and numbers only

		return result;
	}

	// TODO wipe everything but needed preparation and call to qualify_statement() for order-by clause
	// What is the value needed for parameter SqlJoin *tables?
	SqlTableAlias *table_alias;
	UNPACK_SQL_STATEMENT(table_alias, simple_query_expression, table_alias);
	assert(table_alias_STATEMENT == simple_query_expression->type);
	if (NULL == table_alias->parent_table_alias) {
		table_alias->parent_table_alias = parent_table_alias;
	} else {
		assert(table_alias->parent_table_alias == parent_table_alias);
	}
	table_type = table_alias->table->type;
	switch (table_type) {
	case create_table_STATEMENT:
		return result;
		break;
	case table_value_STATEMENT:
		/* For a table constructed using the VALUES clause, go through each value specified and look for
		 * any sub-queries. If so, qualify those. Literal values can be skipped.
		 */
		UNPACK_SQL_STATEMENT(table_value, table_alias->table, table_value);
		UNPACK_SQL_STATEMENT(row_value, table_value->row_value_stmt, row_value);
		start_row_value = row_value;
		do {
			result |= qualify_statement(row_value->value_list, parent_join, table_alias_stmt, 0, NULL);
			row_value = row_value->next;
		} while (row_value != start_row_value);
		return result;
		break;
	default:
		assert(select_STATEMENT == table_type);
		break;
	}
	UNPACK_SQL_STATEMENT(select, table_alias->table, select);
	UNPACK_SQL_STATEMENT(join, select->table_list, join);

	/* Ensure strict column name qualification checks (i.e. all column name references have to be a valid column
	 * name in a valid existing table) by using NULL as the last parameter in various `qualify_statement()` calls below.
	 */
	table_alias->do_group_by_checks = FALSE; /* need to set this before invoking "qualify_statement()" */
	start_join = cur_join = join;
	/* Qualify FROM clause first. For this qualification, only use tables from the parent query FROM list.
	 * Do not use any tables from the current query level FROM list for this qualification.
	 */
	do {
		/* Qualify sub-queries involved in the join. Note that it is possible a `table` is involved in the join instead
		 * of a `sub-query` in which case the below `qualify_query` call will return right away.
		 */
		result |= qualify_query(cur_join->value, parent_join, table_alias, ret);
		cur_join = cur_join->next;
	} while (cur_join != start_join);
	/* Now that FROM clause has been qualified, qualify the JOIN conditions etc. in the FROM clause.
	 * Also add in joins (if any) from higher/parent level queries so sub-queries (current level) can use them.
	 * And for this part we can use tables from the FROM list at this level. In some cases we can only use
	 * a partial FROM list (as the loop progresses, the list size increases in some cases). In other cases (NATURAL JOIN)
	 * we can use the full FROM list.
	 */
	prev_start = join;
	prev_end = join->prev;
	if (NULL != parent_join) {
		dqappend(join, parent_join);
	}
	cur_join = join;
	do {
		SqlJoin *next_join;

		/* Make sure any table.column references in the ON condition of the JOIN (cur_join->condition) are qualified
		 * until the current table in the join list (i.e. forward references should not be allowed). Hence the set of
		 * "cur_join->next" below (to "start_join" effectively hiding the remaining tables to the right).
		 * See YDBOcto#291 for example query that demonstrates why this is needed.
		 */
		next_join = cur_join->next; /* save join list before tampering with it */
		/* Note that if "parent_join" is non-NULL, we need to include that even though it comes
		 * after all the tables in the join list at the current level. This is so any references
		 * to columns in parent queries are still considered as valid. Hence the parent_join check below.
		 */
		cur_join->next = ((NULL != parent_join) ? parent_join : start_join); /* stop join list at current join */
		table_alias->aggregate_depth = AGGREGATE_DEPTH_FROM_CLAUSE;
		result |= qualify_statement(cur_join->condition, start_join, table_alias_stmt, 0, NULL);
		cur_join->next = next_join; /* restore join list to original */
		cur_join = next_join;
	} while ((cur_join != start_join) && (cur_join != parent_join));
	// Qualify WHERE clause next
	table_alias->aggregate_depth = AGGREGATE_DEPTH_WHERE_CLAUSE;
	if (NULL != ret) {
		ret->ret_cla = NULL;
		/* Note: Inherit ret.max_unique_id from caller (could be parent/outer query in case this is a sub-query) as is */
	}
	result |= qualify_statement(select->where_expression, start_join, table_alias_stmt, 0, ret);
	// Qualify GROUP BY clause next
	group_by_expression = select->group_by_expression;
	/* Note that while table_alias->aggregate_function_or_group_by_specified will mostly be FALSE at this point, it is
	 * possible for it to be TRUE in some cases (see YDBOcto#457 for example query) if this `qualify_query()` invocation
	 * corresponds to a sub-query in say the HAVING clause of an outer query. In that case, `qualify_query()` for the
	 * sub-query would be invoked twice by the `qualify_query()` of the outer query (see `table_alias->do_group_by_checks`
	 * `for` loop later in this function). If so, we can skip the GROUP BY expression processing for the sub-query the
	 * second time. Hence the `&& !table_alias->aggregate_function_or_group_by_specified)` in the `if` check below.
	 */
	if ((NULL != group_by_expression) && !table_alias->aggregate_function_or_group_by_specified) {
		SqlColumnListAlias *start_cla, *cur_cla;
		SqlTableAlias *	    group_by_table_alias;
		SqlColumnList *	    col_list;
		int		    group_by_column_count;

		table_alias->aggregate_depth = AGGREGATE_DEPTH_GROUP_BY_CLAUSE;
		assert(0 == table_alias->group_by_column_count);
		result |= qualify_statement(group_by_expression, start_join, table_alias_stmt, 0, NULL);
		/* Note: table_alias->group_by_column_count can still be 0 if GROUP BY was done on a parent query column */
		if (table_alias->group_by_column_count) {
			table_alias->aggregate_function_or_group_by_specified = TRUE;
		}
		/* Traverse the GROUP BY list to see what columns belong to this table_alias. Include only those in the
		 * GROUP BY list. Exclude any other columns (e.g. columns belonging to outer query) from the list
		 * as they are constant as far as this sub-query is concerned.
		 */
		UNPACK_SQL_STATEMENT(start_cla, group_by_expression, column_list_alias);
		group_by_column_count = 0;
		cur_cla = start_cla;
		do {
			UNPACK_SQL_STATEMENT(col_list, cur_cla->column_list, column_list);
			if (column_alias_STATEMENT == col_list->value->type) {
				SqlColumnAlias *column_alias;

				UNPACK_SQL_STATEMENT(column_alias, col_list->value, column_alias);
				UNPACK_SQL_STATEMENT(group_by_table_alias, column_alias->table_alias_stmt, table_alias);
				if (group_by_table_alias->parent_table_alias != table_alias) {
					/* Column belongs to an outer query. Discard it from the GROUP BY list. */
					SqlColumnListAlias *prev, *next;

					prev = cur_cla->prev;
					next = cur_cla->next;
					prev->next = next;
					next->prev = prev;
					if ((cur_cla == start_cla) && (next != cur_cla)) {
						start_cla = cur_cla = next;
						continue;
					}
				} else {
					if (0 == group_by_column_count) {
						group_by_expression->v.column_list_alias = cur_cla;
					}
					group_by_column_count++;
				}
			} else {
				/* This is a case of an invalid column name specified in the GROUP BY clause.
				 * An "Unknown column" error would have already been issued about this (i.e. result would be 1).
				 * Assert both these conditions.
				 */
				assert(result);
				assert((value_STATEMENT == col_list->value->type)
				       && (COLUMN_REFERENCE == col_list->value->v.value->type));
			}
			cur_cla = cur_cla->next;
			if (cur_cla == start_cla) {
				/* Note: Cannot move the negation of the above check to the `while(TRUE)` done below
				 * because there is a `continue` code path above which should go through without any checks.
				 */
				break;
			}
		} while (TRUE);
		/* The "|| result" case below is to account for query errors (e.g. "Unknown column" error, see comment above) */
		assert((group_by_column_count == table_alias->group_by_column_count) || result);
		if (!group_by_column_count) {
			select->group_by_expression = NULL;
		}
	}
	SqlColumnListAlias *ret_cla = NULL;
	table_alias->aggregate_depth = 0;
	for (;;) {
		QualifyStatementParms lcl_ret;

		assert(0 == table_alias->aggregate_depth);
		// Qualify HAVING clause
		result |= qualify_statement(select->having_expression, start_join, table_alias_stmt, 0, NULL);
		// Qualify SELECT column list next
		result |= qualify_statement(select->select_list, start_join, table_alias_stmt, 0, NULL);
		// Qualify ORDER BY clause next
		/* Now that all column names used in the query have been qualified, allow columns specified in
		 * ORDER BY to be qualified against any column names specified till now without any strict checking.
		 * Hence the use of a non-NULL value ("&ret_cla") for "lcl_ret->ret_cla".
		 */
		lcl_ret.ret_cla = &ret_cla;
		lcl_ret.max_unique_id = ((NULL != ret) ? ret->max_unique_id : NULL);
		result |= qualify_statement(select->order_by_expression, start_join, table_alias_stmt, 0, &lcl_ret);
		if (!table_alias->aggregate_function_or_group_by_specified) {
			/* GROUP BY or AGGREGATE function was never used in the query. No need to do GROUP BY validation checks. */
			break;
		}
		if (table_alias->do_group_by_checks) {
			/* GROUP BY or AGGREGATE function was used in the query. And GROUP BY validation checks already done
			 * as part of the second iteration in this for loop. Can now break out of the loop.
			 */
			break;
		}
		if (table_alias->aggregate_function_or_group_by_specified) {
			/* GROUP BY or AGGREGATE function was used in the query. Do GROUP BY validation checks by doing
			 * a second iteration in this for loop.
			 */
			table_alias->do_group_by_checks = TRUE;
			continue;
		}
	}

	// TODO clean up (delete most)
	UNPACK_SQL_STATEMENT(start_cla, stmt, column_list_alias);
	UNPACK_SQL_STATEMENT(table_alias, table_alias_stmt, table_alias);
	cur_cla = start_cla;
	do {
		ret_cla = ((NULL == ret) ? NULL : ret->ret_cla);
		assert(depth || (NULL == ret_cla) || (NULL == *ret_cla)); /* assert that caller has initialized "*ret_cla" */
		result |= qualify_statement(cur_cla->column_list, tables, table_alias_stmt, depth + 1, ret);
		if (0 == result) {
			UNPACK_SQL_STATEMENT(cur_cl, cur_cla->column_list, column_list);
			if (column_alias_STATEMENT == cur_cl->value->type) {
				UNPACK_SQL_STATEMENT(new_column_alias, cur_cl->value, column_alias);
				if (is_stmt_table_asterisk(new_column_alias->column)) {
					/* Processing table.asterisk here is necessary to update group_by_column_count
					 * correctly in relation to other columns
					 */
					process_table_asterisk_cla(stmt, &cur_cla, table_alias, &start_cla);
				}
			}
		}
		if ((NULL != ret_cla) && (0 == depth)) {
			SqlColumnListAlias *qualified_cla;
			int		    column_number;
			char *		    str;
			boolean_t	    order_by_alias;

			/* "ret_cla" is non-NULL implies this call is for an ORDER BY column list.
			 * "depth" == 0 implies "cur_cla" is one of the ORDER BY columns
			 * (i.e. not a cla corresponding to an inner evaluation in the ORDER BY expression).
			 * There are 3 cases to handle.
			 */
			if (NULL != *ret_cla) {
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
				/* Case (2) : If "*ret_cla" is NULL, check if this is a case of ORDER BY column-number.
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
						    = ((NEGATIVE == unary->operation) && (value_STATEMENT == unary->operand->type)
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
							qualified_cla
							    = get_column_list_alias_n_from_table_alias(table_alias, column_number);
						} else {
							qualified_cla = NULL;
						}
						if (NULL == qualified_cla) {
							ERROR(ERR_ORDER_BY_POSITION_INVALID, is_negative_numeric_literal ? "-" : "",
							      str);
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
				/* Case (2) : Case of ORDER BY column-number */
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
			} else {
				/* Case (3) : Case of ORDER BY column expression */
				SqlSelectStatement *select;
				SqlOptionalKeyword *keywords, *keyword;

				/* Check if SELECT DISTINCT was specified */
				UNPACK_SQL_STATEMENT(select, table_alias->table, select);
				UNPACK_SQL_STATEMENT(keywords, select->optional_words, keyword);
				keyword = get_keyword_from_keywords(keywords, OPTIONAL_DISTINCT);
				if (NULL != keyword) {
					/* SELECT DISTINCT was specified. Check if the ORDER BY column expression matches
					 * some column specification in the SELECT column list. If so that is good.
					 * If not issue an error (see YDBOcto#461 for details).
					 */
					if (!match_column_list_alias_in_select_column_list(cur_cla, select->select_list)) {
						ERROR(ERR_ORDER_BY_SELECT_DISTINCT, "");
						yyerror(NULL, NULL, &cur_cla->column_list, NULL, NULL, NULL);
						result = 1;
						break;
					}
				}
			}
		}
		cur_cla = cur_cla->next;
	} while (cur_cla != start_cla);
	break;
}

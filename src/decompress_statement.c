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

#include <assert.h>

#include "octo.h"
#include "octo_types.h"

#define R2A(X) (void *)((out) + ((size_t)X))

#define CALL_DECOMPRESS_HELPER(value, out, out_length)                                                   \
	{                                                                                                \
		if (NULL != value) {                                                                     \
			if ((R2A(value) >= (void *)out) && (R2A(value) <= (void *)(out + out_length))) { \
				value = R2A(value);                                                      \
			}                                                                                \
			decompress_statement_helper(value, out, out_length);                             \
		}                                                                                        \
	}

void *decompress_statement_helper(SqlStatement *stmt, char *out, long int out_length);

SqlStatement *decompress_statement(char *buffer, long int out_length) {
	hash_canonical_query_cycle++;
	return (SqlStatement *)decompress_statement_helper((SqlStatement *)buffer, buffer, out_length);
}

/*
 * Returns a pointer to a new memory location for stmt within the out buffer
 *
 * If the out buffer is NULL, doesn't copy the statement, but just counts size
 */
void *decompress_statement_helper(SqlStatement *stmt, char *out, long int out_length) {
	SqlTable *	      table;
	SqlTableAlias *	      table_alias;
	SqlSelectStatement *  select;
	SqlColumn *	      cur_column, *start_column;
	SqlColumnListAlias *  column_list_alias, *cur_column_list_alias;
	SqlColumnList *	      column_list, *cur_column_list;
	SqlColumnAlias *      column_alias;
	SqlValue *	      value;
	SqlJoin *	      join, *cur_join;
	SqlOptionalKeyword *  start_keyword, *cur_keyword;
	SqlFunction *	      function;
	SqlView *	      view;
	SqlParameterTypeList *cur_parameter_type_list, *start_parameter_type_list;

	// fprintf(stderr, "\nD: stmt: %p\tout: %p\tout_length: %d\nout + out_length: %p\n", ((char *)stmt), out, out_length,
	// out + out_length);
	assert(((char *)stmt) < out + out_length);
	if (NULL == stmt) {
		return NULL;
	}
	if (stmt->hash_canonical_query_cycle == hash_canonical_query_cycle) {
		return stmt;
	}
	stmt->hash_canonical_query_cycle = hash_canonical_query_cycle; /* Note down this node as being visited. This avoids
									* multiple visits down this same node in the same
									* outermost call of "compress_statement_helper".
									*/
	// In each case below, after storing the pointer of the statement in
	//  a temporary variable we mark the statement as NULL, then check here
	//  to see if the statement has been marked NULL, and if so, skip it
	// This lets us play a bit fast-and-loose with the structures to
	//  avoid extra copies during runtime, and not have issues with double
	//  frees
	if (NULL == stmt->v.value) {
		return NULL;
	}
	if (data_type_struct_STATEMENT == stmt->type) {
		/* Relevant data is the SqlDataTypeStruct member in the `stmt` union member, which is NOT a pointer.
		 * So, do not do R2A conversion and just return as-is. See similar note in compress_statement.c.
		 */
		return stmt;
	}
	// fprintf(stderr, "stmt->type: %d\n", stmt->type);
	stmt->v.value = R2A(stmt->v.value);
	switch (stmt->type) {
	case create_table_STATEMENT:
		UNPACK_SQL_STATEMENT(table, stmt, create_table);
		CALL_DECOMPRESS_HELPER(table->tableName, out, out_length);
		CALL_DECOMPRESS_HELPER(table->source, out, out_length);
		CALL_DECOMPRESS_HELPER(table->columns, out, out_length);
		CALL_DECOMPRESS_HELPER(table->delim, out, out_length);
		CALL_DECOMPRESS_HELPER(table->nullchar, out, out_length);
		/* table->oid is not a pointer value so no need to call CALL_DECOMPRESS_HELPER on this member */
		break;
	case create_function_STATEMENT:
		UNPACK_SQL_STATEMENT(function, stmt, create_function);
		CALL_DECOMPRESS_HELPER(function->function_name, out, out_length);
		CALL_DECOMPRESS_HELPER(function->parameter_type_list, out, out_length);
		CALL_DECOMPRESS_HELPER(function->return_type, out, out_length);
		CALL_DECOMPRESS_HELPER(function->extrinsic_function, out, out_length);
		CALL_DECOMPRESS_HELPER(function->function_hash, out, out_length);
		break;
	case create_view_STATEMENT:
		UNPACK_SQL_STATEMENT(view, stmt, create_view);
		CALL_DECOMPRESS_HELPER(view->view_name, out, out_length);
		CALL_DECOMPRESS_HELPER(view->table, out, out_length);
		/* view->query and view->oid are not pointer values so no need to call CALL_DECOMPRESS_HELPER on these members */
		break;
	case parameter_type_list_STATEMENT:
		UNPACK_SQL_STATEMENT(cur_parameter_type_list, stmt, parameter_type_list);
		if (NULL == cur_parameter_type_list) {
			// No parameter types were specified, nothing to decompress
			break;
		}
		start_parameter_type_list = cur_parameter_type_list;
		do {
			if (0 == cur_parameter_type_list->next) {
				cur_parameter_type_list->next = start_parameter_type_list;
			} else {
				cur_parameter_type_list->next = R2A(cur_parameter_type_list->next);
			}
			CALL_DECOMPRESS_HELPER(cur_parameter_type_list->data_type_struct, out, out_length);
			cur_parameter_type_list->next->prev = cur_parameter_type_list;
			cur_parameter_type_list = cur_parameter_type_list->next;
		} while (cur_parameter_type_list != start_parameter_type_list);
		break;
	case value_STATEMENT:
		UNPACK_SQL_STATEMENT(value, stmt, value);
		value->v.string_literal = R2A(value->v.string_literal);
		break;
	case column_STATEMENT:
		UNPACK_SQL_STATEMENT(cur_column, stmt, column);
		start_column = cur_column;
		do {
			CALL_DECOMPRESS_HELPER(cur_column->columnName, out, out_length);
			// Don't copy table
			CALL_DECOMPRESS_HELPER(cur_column->keywords, out, out_length);
			CALL_DECOMPRESS_HELPER(cur_column->delim, out, out_length);
			if (0 == cur_column->next) {
				cur_column->next = start_column;
			} else {
				cur_column->next = R2A(cur_column->next);
			}
			cur_column->table = R2A(cur_column->table);
			cur_column->next->prev = cur_column;
			cur_column = cur_column->next;
		} while (cur_column != start_column);
		break;
	case keyword_STATEMENT:
		UNPACK_SQL_STATEMENT(start_keyword, stmt, keyword);
		cur_keyword = start_keyword;
		do {
			CALL_DECOMPRESS_HELPER(cur_keyword->v, out, out_length);
			if (cur_keyword->next == 0) {
				cur_keyword->next = start_keyword;
			} else {
				cur_keyword->next = R2A(cur_keyword->next);
			}
			cur_keyword->next->prev = cur_keyword;
			cur_keyword = cur_keyword->next;
		} while (cur_keyword != start_keyword);
		break;
	case table_alias_STATEMENT:
		UNPACK_SQL_STATEMENT(table_alias, stmt, table_alias);
		CALL_DECOMPRESS_HELPER(table_alias->table, out, out_length);
		CALL_DECOMPRESS_HELPER(table_alias->alias, out, out_length);
		// fprintf(stderr, "\nD: table_alias: %p\ttable_alias->alias: %p\n", table_alias, table_alias->alias);
		CALL_DECOMPRESS_HELPER(table_alias->parent_table_alias, out, out_length);
		CALL_DECOMPRESS_HELPER(table_alias->column_list, out, out_length);
		/* The following fields of a SqlTableAlias are not pointer values and so need no CALL_DECOMPRESS_HELPER call:
		 *	unique_id
		 *	aggregate_depth
		 *	aggregate_function_or_group_by_specified
		 *	do_group_by_checks
		 */
		break;
	case select_STATEMENT:
		UNPACK_SQL_STATEMENT(select, stmt, select);
		CALL_DECOMPRESS_HELPER(select->select_list, out, out_length);
		CALL_DECOMPRESS_HELPER(select->table_list, out, out_length);
		CALL_DECOMPRESS_HELPER(select->where_expression, out, out_length);
		CALL_DECOMPRESS_HELPER(select->group_by_expression, out, out_length);
		CALL_DECOMPRESS_HELPER(select->having_expression, out, out_length);
		CALL_DECOMPRESS_HELPER(select->order_by_expression, out, out_length);
		CALL_DECOMPRESS_HELPER(select->optional_words, out, out_length);
		break;
	case column_list_alias_STATEMENT:
		UNPACK_SQL_STATEMENT(column_list_alias, stmt, column_list_alias);

		cur_column_list_alias = column_list_alias;
		do {
			CALL_DECOMPRESS_HELPER(column_list_alias->column_list, out, out_length);
			CALL_DECOMPRESS_HELPER(column_list_alias->alias, out, out_length);
			CALL_DECOMPRESS_HELPER(column_list_alias->keywords, out, out_length);

			cur_column_list_alias->next = R2A(cur_column_list_alias->next);
			cur_column_list_alias->prev = R2A(cur_column_list_alias->prev);
			cur_column_list_alias = cur_column_list_alias->next;
		} while (cur_column_list_alias != column_list_alias);
		break;
	case column_list_STATEMENT:
		UNPACK_SQL_STATEMENT(column_list, stmt, column_list);
		cur_column_list = column_list;
		do {
			CALL_DECOMPRESS_HELPER(cur_column_list->value, out, out_length);
			cur_column_list->next = R2A(cur_column_list->next);
			cur_column_list->prev = R2A(cur_column_list->prev);
			cur_column_list = cur_column_list->next;
		} while (cur_column_list != column_list);

		break;
	case column_alias_STATEMENT:
		UNPACK_SQL_STATEMENT(column_alias, stmt, column_alias);
		CALL_DECOMPRESS_HELPER(column_alias->column, out, out_length);
		CALL_DECOMPRESS_HELPER(column_alias->table_alias_stmt, out, out_length);
		break;
	case join_STATEMENT:
		UNPACK_SQL_STATEMENT(join, stmt, join);
		cur_join = join;
		do {
			cur_join->value = R2A(cur_join->value);
			assert((char *)cur_join->value <= (out + out_length));
			CALL_DECOMPRESS_HELPER(cur_join->condition, out, out_length);
			cur_join->next = R2A(cur_join->next);
			cur_join->prev = R2A(cur_join->prev);
			cur_join = cur_join->next;
		} while (cur_join != join);
		break;
	default:
		printf("stmt->type: %d\n", stmt->type);
		assert(FALSE);
		FATAL(ERR_UNKNOWN_KEYWORD_STATE, "");
		return NULL;
		break;
	}
	return stmt;
}

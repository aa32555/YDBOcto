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

#define STATIC_A2R(X) (void *)(((char *)X) - out)

#define A2R(X)                                                 \
	((X) = (void *)(((char *)X) - out));                   \
	if ((NULL != out)) {                                   \
		if (!((void *)X <= ((void *)(*out_length)))) { \
		}                                              \
		assert((void *)X <= ((void *)(*out_length)));  \
	}

#define CALL_COMPRESS_HELPER(temp, value, new_value, out, out_length, parent_table)             \
	{                                                                                       \
		(temp) = compress_statement_helper((value), (out), (out_length), parent_table); \
		if (NULL != (out)) {                                                            \
			if ((temp) != (value)) {                                                \
				(new_value) = (temp);                                           \
				if (NULL != new_value) {                                        \
					A2R((new_value));                                       \
				}                                                               \
			} else {                                                                \
				if (NULL != (value)) {                                          \
					(new_value) = value->compressed_offset;                 \
				}                                                               \
			}                                                                       \
		}                                                                               \
	}

#define GET_LIST_LENGTH_AND_UPDATE_OUT_LENGTH(LIST_LEN, CUR, START, OUT_LENGTH, TYPE) \
	{                                                                             \
		/* Get total length of linked list to be allocated */                 \
		LIST_LEN = 0;                                                         \
		CUR = START;                                                          \
		do {                                                                  \
			(LIST_LEN)++;                                                 \
			CUR = (CUR)->next;                                            \
		} while (CUR != START);                                               \
		*OUT_LENGTH += LIST_LEN * sizeof(TYPE);                               \
	}

void *compress_statement_helper(SqlStatement *stmt, char *out, long int *out_length, SqlStatement *parent_table);

void compress_statement(SqlStatement *stmt, char **out, long int *out_length) {
	*out_length = 0;
	hash_canonical_query_cycle++;
	compress_statement_helper(stmt, NULL, out_length, NULL);
	assert(0 != *out_length);
	*out = malloc(*out_length);
	*out_length = 0;
	hash_canonical_query_cycle++;
	compress_statement_helper(stmt, *out, out_length, NULL);
}

/*
 * Returns a pointer to a new memory location for stmt within the out buffer
 *
 * If the out buffer is NULL, doesn't copy the statement, but just counts size
 */
void *compress_statement_helper(SqlStatement *stmt, char *out, long int *out_length, SqlStatement *parent_table) {
	SqlColumn *	      cur_column, *start_column, *new_column;
	SqlColumnAlias *      column_alias, *new_column_alias;
	SqlColumnList *	      column_list, *new_column_list, *cur_column_list, *cur_new_column_list, *column_list_list;
	SqlColumnListAlias *  column_list_alias, *cur_new_column_list_alias, *cur_column_list_alias, *column_list_alias_list;
	SqlOptionalKeyword *  start_keyword, *cur_keyword, *new_keyword;
	SqlStatement *	      new_stmt;
	SqlSelectStatement *  select, *new_select;
	SqlTable *	      table, *new_table;
	SqlTableAlias *	      table_alias, *new_table_alias;
	SqlFunction *	      function, *new_function;
	SqlView *	      view, *new_view;
	SqlJoin *	      join, *cur_join, *cur_new_join, *join_list;
	SqlParameterTypeList *new_parameter_type_list, *cur_parameter_type_list, *start_parameter_type_list;
	SqlValue *	      value, *new_value;
	int		      len, list_len = 0, list_index;
	void *		      r, *ret;

	if ((NULL == stmt) || (NULL == stmt->v.value))
		return NULL;
	if (stmt->hash_canonical_query_cycle == hash_canonical_query_cycle) {
		return stmt;
	}
	stmt->hash_canonical_query_cycle = hash_canonical_query_cycle; /* Note down this node as being visited. This avoids
									* multiple visits down this same node in the same
									* outermost call of "compress_statement_helper".
									*/
	if (NULL != out) {
		new_stmt = ((void *)&out[*out_length]);
		memcpy(new_stmt, stmt, sizeof(SqlStatement));
		// A2R(new_stmt);
		stmt->compressed_offset = STATIC_A2R(new_stmt);
		ret = new_stmt;
	} else {
		ret = NULL;
	}
	*out_length += sizeof(SqlStatement);
	if (NULL != out) {
		if (data_type_struct_STATEMENT == new_stmt->type) {
			/* In this case, the relevant data is the SqlDataTypeStruct member from the union member of `stmt`,
			 * i.e. NOT a pointer. So, do not perform the A2R conversion and just return as is.
			 * See similar note in decompress_statement.c.
			 */
			return ret;
		}
		new_stmt->v.value = ((void *)&out[*out_length]);
		A2R(new_stmt->v.value);
	}
	// fprintf(stderr, "C: stmt->type: %d\n", stmt->type);
	switch (stmt->type) {
	case create_table_STATEMENT:
		UNPACK_SQL_STATEMENT(table, stmt, create_table);
		if (NULL != out) {
			new_table = ((void *)&out[*out_length]);
			memcpy(new_table, table, sizeof(SqlTable));
			parent_table = new_stmt;
		}
		*out_length += sizeof(SqlTable);
		/// TODO: tables should no longer be a double list
		CALL_COMPRESS_HELPER(r, table->tableName, new_table->tableName, out, out_length, parent_table);
		CALL_COMPRESS_HELPER(r, table->source, new_table->source, out, out_length, parent_table);
		CALL_COMPRESS_HELPER(r, table->columns, new_table->columns, out, out_length, parent_table);
		CALL_COMPRESS_HELPER(r, table->delim, new_table->delim, out, out_length, parent_table);
		CALL_COMPRESS_HELPER(r, table->nullchar, new_table->nullchar, out, out_length, parent_table);
		/* table->oid is not a pointer value so no need to call CALL_COMPRESS_HELPER on this member */
		break;
	case create_function_STATEMENT:
		UNPACK_SQL_STATEMENT(function, stmt, create_function);
		if (NULL != out) {
			new_function = ((void *)&out[*out_length]);
			memcpy(new_function, function, sizeof(SqlFunction));
		}
		*out_length += sizeof(SqlFunction);
		CALL_COMPRESS_HELPER(r, function->function_name, new_function->function_name, out, out_length, parent_table);
		CALL_COMPRESS_HELPER(r, function->parameter_type_list, new_function->parameter_type_list, out, out_length,
				     parent_table);
		CALL_COMPRESS_HELPER(r, function->return_type, new_function->return_type, out, out_length, parent_table);
		CALL_COMPRESS_HELPER(r, function->extrinsic_function, new_function->extrinsic_function, out, out_length,
				     parent_table);
		CALL_COMPRESS_HELPER(r, function->function_hash, new_function->function_hash, out, out_length, parent_table);
		break;
	case create_view_STATEMENT:
		UNPACK_SQL_STATEMENT(view, stmt, create_view);
		if (NULL != out) {
			new_view = ((void *)&out[*out_length]);
			memcpy(new_view, view, sizeof(SqlView));
		}
		*out_length += sizeof(SqlView);
		CALL_COMPRESS_HELPER(r, view->view_name, new_view->view_name, out, out_length, parent_table);
		CALL_COMPRESS_HELPER(r, view->table, new_view->table, out, out_length, parent_table);
		/* view->query and view->oid are not pointer values so no need to call CALL_COMPRESS_HELPER on these members */
		break;
	case parameter_type_list_STATEMENT:
		UNPACK_SQL_STATEMENT(cur_parameter_type_list, stmt, parameter_type_list);
		if (NULL == cur_parameter_type_list) {
			// No parameter types were specified, nothing to compress
			break;
		}
		start_parameter_type_list = cur_parameter_type_list;
		do {
			if (NULL != out) {
				new_parameter_type_list = ((void *)&out[*out_length]);
				memcpy(new_parameter_type_list, cur_parameter_type_list, sizeof(SqlParameterTypeList));
				new_parameter_type_list->next = new_parameter_type_list->prev = NULL;
			}
			*out_length += sizeof(SqlParameterTypeList);

			CALL_COMPRESS_HELPER(r, cur_parameter_type_list->data_type_struct,
					     new_parameter_type_list->data_type_struct, out, out_length, parent_table);
			cur_parameter_type_list = cur_parameter_type_list->next;
			if ((NULL != out) && (cur_parameter_type_list != start_parameter_type_list)) {
				new_parameter_type_list->next = ((void *)&out[*out_length]);
				A2R(new_parameter_type_list->next);
			}
		} while (cur_parameter_type_list != start_parameter_type_list);
		break;
	case data_type_struct_STATEMENT:
		/* Hit this case only for the initial length count, i.e. (NULL == out).
		 * See note on "data_type_struct_STATEMENT" above.
		 */
		assert(NULL == out);
		break;
	case value_STATEMENT:
		UNPACK_SQL_STATEMENT(value, stmt, value);
		if (NULL != out) {
			new_value = ((void *)&out[*out_length]);
			memcpy(new_value, value, sizeof(SqlValue));
		}
		*out_length += sizeof(SqlValue);
		len = strlen(value->v.string_literal);
		if (NULL != out) {
			memcpy(&out[*out_length], value->v.string_literal, len);
			new_value->v.string_literal = &out[*out_length];
			A2R(new_value->v.string_literal);
		}
		*out_length += len;
		if (NULL != out) {
			out[*out_length] = '\0';
		}
		*out_length += 1;
		break;
	case column_STATEMENT:
		UNPACK_SQL_STATEMENT(cur_column, stmt, column);
		start_column = cur_column;
		do {
			if (NULL != out) {
				new_column = ((void *)&out[*out_length]);
				memcpy(new_column, cur_column, sizeof(SqlColumn));
				new_column->next = new_column->prev = NULL;
				// new_column->table = parent_table;
				// A2R(new_column->table);
			}
			*out_length += sizeof(SqlColumn);
			CALL_COMPRESS_HELPER(r, cur_column->columnName, new_column->columnName, out, out_length, parent_table);
			CALL_COMPRESS_HELPER(r, cur_column->keywords, new_column->keywords, out, out_length, parent_table);
			CALL_COMPRESS_HELPER(r, cur_column->delim, new_column->delim, out, out_length, parent_table);
			cur_column = cur_column->next;
			if ((NULL != out) && (cur_column != start_column)) {
				new_column->next = ((void *)&out[*out_length]);
				A2R(new_column->next);
			}
		} while (cur_column != start_column);
		break;
	case keyword_STATEMENT:
		UNPACK_SQL_STATEMENT(start_keyword, stmt, keyword);
		cur_keyword = start_keyword;
		do {
			if (NULL != out) {
				new_keyword = ((void *)&out[*out_length]);
				memcpy(new_keyword, cur_keyword, sizeof(SqlOptionalKeyword));
				new_keyword->next = new_keyword->prev = NULL;
			}
			*out_length += sizeof(SqlOptionalKeyword);
			CALL_COMPRESS_HELPER(r, cur_keyword->v, new_keyword->v, out, out_length, parent_table);
			cur_keyword = cur_keyword->next;
			if ((NULL != out) && (cur_keyword != start_keyword)) {
				new_keyword->next = ((void *)&out[*out_length]);
				A2R(new_keyword->next);
			}
		} while (cur_keyword != start_keyword);
		break;
	case table_alias_STATEMENT:
		UNPACK_SQL_STATEMENT(table_alias, stmt, table_alias);
		// fprintf(stderr, "C: table_alias: %p\n", stmt);
		if (NULL != out) {
			new_table_alias = ((void *)&out[*out_length]);
			memcpy(new_table_alias, table_alias, sizeof(SqlTableAlias));
		}
		*out_length += sizeof(SqlTableAlias);
		CALL_COMPRESS_HELPER(r, table_alias->table, new_table_alias->table, out, out_length, parent_table);
		CALL_COMPRESS_HELPER(r, table_alias->alias, new_table_alias->alias, out, out_length, parent_table);
		CALL_COMPRESS_HELPER(r, table_alias->parent_table_alias, new_table_alias->parent_table_alias, out, out_length,
				     parent_table);
		CALL_COMPRESS_HELPER(r, table_alias->column_list, new_table_alias->column_list, out, out_length, parent_table);
		/* The following fields of a SqlTableAlias are not pointer values and so need no CALL_COMPRESS_HELPER call:
		 *	unique_id
		 *	aggregate_depth
		 *	aggregate_function_or_group_by_specified
		 *	do_group_by_checks
		 */
		break;
	case select_STATEMENT:
		UNPACK_SQL_STATEMENT(select, stmt, select);
		if (NULL != out) {
			new_select = ((void *)&out[*out_length]);
			memcpy(new_select, select, sizeof(SqlSelectStatement));
		}
		*out_length += sizeof(SqlSelectStatement);
		CALL_COMPRESS_HELPER(r, select->select_list, new_select->select_list, out, out_length, parent_table);
		CALL_COMPRESS_HELPER(r, select->table_list, new_select->table_list, out, out_length, parent_table);
		CALL_COMPRESS_HELPER(r, select->where_expression, new_select->where_expression, out, out_length, parent_table);
		CALL_COMPRESS_HELPER(r, select->group_by_expression, new_select->group_by_expression, out, out_length,
				     parent_table);
		CALL_COMPRESS_HELPER(r, select->having_expression, new_select->having_expression, out, out_length, parent_table);
		CALL_COMPRESS_HELPER(r, select->order_by_expression, new_select->order_by_expression, out, out_length,
				     parent_table);
		CALL_COMPRESS_HELPER(r, select->optional_words, new_select->optional_words, out, out_length, parent_table);
		break;
	case column_list_alias_STATEMENT:
		UNPACK_SQL_STATEMENT(column_list_alias, stmt, column_list_alias);

		if (NULL != out) {
			column_list_alias_list = ((SqlJoin *)&out[*out_length]);
			cur_new_column_list_alias = column_list_alias_list;
		}
		GET_LIST_LENGTH_AND_UPDATE_OUT_LENGTH(list_len, cur_column_list_alias, column_list_alias, out_length,
						      SqlColumnListAlias);

		list_index = 0;
		cur_column_list_alias = column_list_alias;
		do {
			if (NULL != out) {
				cur_new_column_list_alias = ((void *)&out[*out_length]);
				memcpy(cur_new_column_list_alias, cur_column_list_alias, sizeof(SqlColumnListAlias));
			}
			*out_length += sizeof(SqlColumnListAlias);
			CALL_COMPRESS_HELPER(r, cur_column_list_alias->column_list, cur_new_column_list_alias->column_list, out,
					     out_length, parent_table);
			CALL_COMPRESS_HELPER(r, cur_column_list_alias->alias, cur_new_column_list_alias->alias, out, out_length,
					     parent_table);
			CALL_COMPRESS_HELPER(r, cur_column_list_alias->keywords, cur_new_column_list_alias->keywords, out,
					     out_length, parent_table);
			// CALL_COMPRESS_HELPER(r, column_list_alias->duplicate_of_column,
			// new_column_list_alias->duplicate_of_column, out, out_length); SqlTableIdColumnId	   tbl_and_col_id;
			// SqlColumnAlias *outer_query_column_alias;
			if (NULL != out) {
				/* The previous item of the first item should be the last item in the list, since the list is
				 * doubly-linked. Conversely, the next item of the last item should be the first in the list.
				 */
				if (0 == list_index) {
					cur_new_column_list_alias->prev = ((1 == list_len) ? &column_list_alias_list[0]
											   : &column_list_alias_list[list_len - 1]);
				} else {
					cur_new_column_list_alias->prev = &column_list_alias_list[list_index];
				}
				A2R(cur_new_column_list_alias->prev);
				cur_new_column_list_alias->next
				    = ((list_index + 1 == list_len) ? &column_list_alias_list[0]
								    : &column_list_alias_list[list_index + 1]);
				cur_new_column_list_alias = cur_new_column_list_alias->next;
				A2R(cur_new_column_list_alias->next);
			}
			cur_column_list_alias = cur_column_list_alias->next;
			list_index++;
		} while (cur_column_list_alias != column_list_alias);
		assert(list_index == list_len);
		break;
	case column_list_STATEMENT:
		UNPACK_SQL_STATEMENT(column_list, stmt, column_list);

		if (NULL != out) {
			column_list_list = ((SqlColumnList *)&out[*out_length]);
			cur_new_column_list = new_column_list = column_list_list;
		}
		GET_LIST_LENGTH_AND_UPDATE_OUT_LENGTH(list_len, cur_column_list, column_list, out_length, SqlColumnList);

		cur_column_list = column_list;
		list_index = 0;
		do {
			if (NULL != out) {
				memcpy(cur_new_column_list, cur_column_list, sizeof(SqlColumnList));
			}
			CALL_COMPRESS_HELPER(r, cur_column_list->value, cur_new_column_list->value, out, out_length, parent_table);
			if (NULL != out) {
				/* The previous item of the first item should be the last item in the list, since the list is
				 * doubly-linked. Conversely, the next item of the last item should be the first in the list.
				 */
				if (0 == list_index) {
					cur_new_column_list->prev
					    = ((1 == list_len) ? &column_list_list[0] : &column_list_list[list_index - 1]);
				} else {
					cur_new_column_list->prev = &column_list_list[list_index];
				}
				A2R(cur_new_column_list->prev);
				cur_new_column_list->next
				    = ((list_index + 1 == list_len) ? &column_list_list[0] : &column_list_list[list_index + 1]);
				cur_new_column_list = cur_new_column_list->next;
				A2R(cur_new_column_list->next);
			}
			cur_column_list = cur_column_list->next;
			list_index++;
		} while (cur_column_list != column_list);
		assert(list_index == list_len);

		// compress_sqlcolumnlist_list(r, column_list, new_column_list, out, out_length);
		break;
	case column_alias_STATEMENT:
		UNPACK_SQL_STATEMENT(column_alias, stmt, column_alias);
		if (NULL != out) {
			new_column_alias = ((void *)&out[*out_length]);
			memcpy(new_column_alias, column_alias, sizeof(SqlColumnAlias));
		}
		*out_length += sizeof(SqlColumnAlias);
		CALL_COMPRESS_HELPER(r, column_alias->column, new_column_alias->column, out, out_length, parent_table);
		CALL_COMPRESS_HELPER(r, column_alias->table_alias_stmt, new_column_alias->table_alias_stmt, out, out_length,
				     parent_table);
		break;
	case join_STATEMENT:
		UNPACK_SQL_STATEMENT(join, stmt, join);

		if (NULL != out) {
			join_list = ((SqlJoin *)&out[*out_length]);
			cur_new_join = join_list;
		}
		GET_LIST_LENGTH_AND_UPDATE_OUT_LENGTH(list_len, cur_join, join, out_length, SqlJoin);

		cur_join = join;
		list_index = 0;
		do {
			if (NULL != out) {
				memcpy(cur_new_join, cur_join, sizeof(SqlJoin));
				cur_new_join->value = cur_join->value->compressed_offset;
			}
			fprintf(stderr, "C: cur_join: %p\tcur_join->type: %d\n", cur_join, cur_join->type);
			CALL_COMPRESS_HELPER(r, cur_join->condition, cur_new_join->condition, out, out_length, parent_table);
			// Compress the linked list pointers
			if (NULL != out) {
				/* The previous item of the first item should be the last item in the list, since the list is
				 * doubly-linked. Conversely, the next item of the last item should be the first in the list.
				 */
				if (0 == list_index) {
					cur_new_join->prev = ((1 == list_len) ? &join_list[0] : &join_list[list_len - 1]);
				} else {
					cur_new_join->prev = &join_list[list_index];
				}
				A2R(cur_new_join->prev);
				cur_new_join->next = ((list_index + 1 == list_len) ? &join_list[0] : &join_list[list_index + 1]);
				cur_new_join = cur_new_join->next;
				A2R(cur_new_join->next);
			}
			cur_join = cur_join->next;
			list_index++;
		} while (cur_join != join);
		assert(list_index == list_len);
		break;
	default:
		printf("type: %d\n", stmt->type);
		assert(FALSE);
		FATAL(ERR_UNKNOWN_KEYWORD_STATE, "");
		return NULL;
		break;
	}

	return ret;
}

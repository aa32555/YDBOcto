/****************************************************************
 *								*
 * Copyright (c) 2019-2020 YottaDB LLC and/or its subsidiaries.	*
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

#define CALL_COMPRESS_HELPER(temp, value, new_value, out, out_length)             \
	{                                                                         \
		(temp) = compress_statement_helper((value), (out), (out_length)); \
		if (NULL != (out)) {                                              \
			(new_value) = (temp);                                     \
			if (NULL != new_value) {                                  \
				A2R((new_value));                                 \
			}                                                         \
		}                                                                 \
	}
void *compress_statement_helper(SqlStatement *stmt, char *out, int *out_length);

void compress_statement(SqlStatement *stmt, char **out, int *out_length) {
	*out_length = 0;
	compress_statement_helper(stmt, NULL, out_length);
	assert(0 != *out_length);
	*out = malloc(*out_length);
	*out_length = 0;
	compress_statement_helper(stmt, *out, out_length);
}

void *compress_sqlcolumnlist_list(void *temp, SqlColumnList *stmt, SqlColumnList *new_stmt, char *out, int *out_length) {
	SqlColumnList *cur_stmt, *cur_new_stmt;
	SqlColumnList *column_list;
	int	       list_len = 0, list_index;
	void *	       ret;

	fprintf(stderr, "START: out_length: %d\n", *out_length);
	if (NULL != out) {
		ret = ((void *)&out[*out_length]);
		column_list = ((SqlColumnList *)&out[*out_length]);
		cur_new_stmt = new_stmt = column_list;
	} else {
		ret = NULL;
	}

	// Get total length of linked list to be allocated
	cur_stmt = stmt;
	do {
		list_len++;
		cur_stmt = cur_stmt->next;
	} while (cur_stmt != stmt);
	*out_length += list_len * sizeof(SqlColumnList);

	cur_stmt = stmt;
	list_index = 0;
	do {
		if (NULL != out) {
			memcpy(cur_new_stmt, cur_stmt, sizeof(SqlColumnList));
		}
		CALL_COMPRESS_HELPER(temp, cur_stmt->value, cur_new_stmt->value, out, out_length);
		if (NULL != out) {
			/* The previous item of the first item should be the last item in the list, since the list is doubly-linked.
			 * Conversely, the next item of the last item should be the first in the list.
			 */
			if (0 == list_index) {
				cur_new_stmt->prev = ((1 == list_len) ? &column_list[0] : &column_list[list_index - 1]);
			} else {
				cur_new_stmt->prev = &column_list[list_index];
			}
			A2R(cur_new_stmt->prev, cur_new_stmt->prev);
			cur_new_stmt->next = ((list_index + 1 == list_len) ? &column_list[0] : &column_list[list_index + 1]);
			cur_new_stmt = cur_new_stmt->next;
			A2R(cur_new_stmt->next, cur_new_stmt->next);
		}
		cur_stmt = cur_stmt->next;
		list_index++;
	} while (cur_stmt != stmt);
	assert(list_index == list_len);

	fprintf(stderr, "END: out_length: %d\n", *out_length);
	return ret;
}

/*
 * Returns a pointer to a new memory location for stmt within the out buffer
 *
 * If the out buffer is NULL, doesn't copy the statement, but just counts size
 */
void *compress_statement_helper(SqlStatement *stmt, char *out, int *out_length) {
	SqlColumn *	      cur_column, *start_column, *new_column;
	SqlColumnAlias *      column_alias, *new_column_alias;
	SqlColumnListAlias *  column_list_alias, *new_column_list_alias;
	SqlColumnList *	      column_list, *new_column_list;
	SqlOptionalKeyword *  start_keyword, *cur_keyword, *new_keyword;
	SqlStatement *	      new_stmt;
	SqlSelectStatement *  select, *new_select;
	SqlTable *	      table, *new_table;
	SqlTableAlias *	      table_alias, *new_table_alias;
	SqlFunction *	      function, *new_function;
	SqlView *	      view, *new_view;
	SqlJoin *	      join, *new_join, *cur_join, *cur_new_join, *join_list;
	SqlParameterTypeList *new_parameter_type_list, *cur_parameter_type_list, *start_parameter_type_list;
	SqlValue *	      value, *new_value;
	int		      len, list_len = 0, list_index;
	void *		      r, *ret;

	if ((NULL == stmt) || (NULL == stmt->v.value))
		return NULL;
	if (NULL != out) {
		new_stmt = ((void *)&out[*out_length]);
		memcpy(new_stmt, stmt, sizeof(SqlStatement));
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
	switch (stmt->type) {
	case create_table_STATEMENT:
		UNPACK_SQL_STATEMENT(table, stmt, create_table);
		if (NULL != out) {
			new_table = ((void *)&out[*out_length]);
			memcpy(new_table, table, sizeof(SqlTable));
		}
		*out_length += sizeof(SqlTable);
		/// TODO: tables should no longer be a double list
		CALL_COMPRESS_HELPER(r, table->tableName, new_table->tableName, out, out_length);
		CALL_COMPRESS_HELPER(r, table->source, new_table->source, out, out_length);
		CALL_COMPRESS_HELPER(r, table->columns, new_table->columns, out, out_length);
		CALL_COMPRESS_HELPER(r, table->delim, new_table->delim, out, out_length);
		CALL_COMPRESS_HELPER(r, table->nullchar, new_table->nullchar, out, out_length);
		/* table->oid is not a pointer value so no need to call CALL_COMPRESS_HELPER on this member */
		break;
	case create_function_STATEMENT:
		UNPACK_SQL_STATEMENT(function, stmt, create_function);
		if (NULL != out) {
			new_function = ((void *)&out[*out_length]);
			memcpy(new_function, function, sizeof(SqlFunction));
		}
		*out_length += sizeof(SqlFunction);
		CALL_COMPRESS_HELPER(r, function->function_name, new_function->function_name, out, out_length);
		CALL_COMPRESS_HELPER(r, function->parameter_type_list, new_function->parameter_type_list, out, out_length);
		CALL_COMPRESS_HELPER(r, function->return_type, new_function->return_type, out, out_length);
		CALL_COMPRESS_HELPER(r, function->extrinsic_function, new_function->extrinsic_function, out, out_length);
		CALL_COMPRESS_HELPER(r, function->function_hash, new_function->function_hash, out, out_length);
		break;
	case create_view_STATEMENT:
		UNPACK_SQL_STATEMENT(view, stmt, create_view);
		if (NULL != out) {
			new_view = ((void *)&out[*out_length]);
			memcpy(new_view, view, sizeof(SqlView));
		}
		*out_length += sizeof(SqlView);
		CALL_COMPRESS_HELPER(r, view->view_name, new_view->view_name, out, out_length);
		CALL_COMPRESS_HELPER(r, view->table, new_view->table, out, out_length);
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
					     new_parameter_type_list->data_type_struct, out, out_length);
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
			// fprintf(stderr, "value ret: %p\tstr: %s\n", ret, value->v.string_literal);
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
				new_column->table = (void *)0; /* offset 0 : table is first element in compressed structure
								*            which means a relative offset of 0.
								*/
			}
			*out_length += sizeof(SqlColumn);
			CALL_COMPRESS_HELPER(r, cur_column->columnName, new_column->columnName, out, out_length);
			CALL_COMPRESS_HELPER(r, cur_column->keywords, new_column->keywords, out, out_length);
			CALL_COMPRESS_HELPER(r, cur_column->delim, new_column->delim, out, out_length);
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
			CALL_COMPRESS_HELPER(r, cur_keyword->v, new_keyword->v, out, out_length);
			cur_keyword = cur_keyword->next;
			if ((NULL != out) && (cur_keyword != start_keyword)) {
				new_keyword->next = ((void *)&out[*out_length]);
				A2R(new_keyword->next);
			}
		} while (cur_keyword != start_keyword);
		break;
	case table_alias_STATEMENT:
		UNPACK_SQL_STATEMENT(table_alias, stmt, table_alias);
		if (NULL != out) {
			new_table_alias = ((void *)&out[*out_length]);
			memcpy(new_table_alias, table_alias, sizeof(SqlTableAlias));
		}
		*out_length += sizeof(SqlTableAlias);
		CALL_COMPRESS_HELPER(r, table_alias->table, new_table_alias->table, out, out_length);
		CALL_COMPRESS_HELPER(r, table_alias->alias, new_table_alias->alias, out, out_length);
		CALL_COMPRESS_HELPER(r, table_alias->parent_table_alias, new_table_alias->parent_table_alias, out, out_length);
		if (stmt->hash_canonical_query_cycle != hash_canonical_query_cycle) {
			CALL_COMPRESS_HELPER(r, table_alias->column_list, new_table_alias->column_list, out, out_length);
		}
		stmt->hash_canonical_query_cycle = hash_canonical_query_cycle; /* Note down this node as being visited. This avoids
										* multiple visits down this same node in the same
										* outermost call of "compress_statement_helper".
										*/
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
		CALL_COMPRESS_HELPER(r, select->select_list, new_select->select_list, out, out_length);
		CALL_COMPRESS_HELPER(r, select->table_list, new_select->table_list, out, out_length);
		CALL_COMPRESS_HELPER(r, select->where_expression, new_select->where_expression, out, out_length);
		CALL_COMPRESS_HELPER(r, select->group_by_expression, new_select->group_by_expression, out, out_length);
		CALL_COMPRESS_HELPER(r, select->having_expression, new_select->having_expression, out, out_length);
		CALL_COMPRESS_HELPER(r, select->order_by_expression, new_select->order_by_expression, out, out_length);
		CALL_COMPRESS_HELPER(r, select->optional_words, new_select->optional_words, out, out_length);
		break;
	case column_list_alias_STATEMENT:
		UNPACK_SQL_STATEMENT(column_list_alias, stmt, column_list_alias);
		if (NULL != out) {
			new_column_list_alias = ((void *)&out[*out_length]);
			memcpy(new_column_list_alias, column_list_alias, sizeof(SqlColumnListAlias));
		}
		*out_length += sizeof(SqlColumnListAlias);
		fprintf(stderr, "PRE COLUMN_LIST\n");
		CALL_COMPRESS_HELPER(r, column_list_alias->column_list, new_column_list_alias->column_list, out, out_length);
		fprintf(stderr, "POST COLUMN_LIST\n");
		CALL_COMPRESS_HELPER(r, column_list_alias->alias, new_column_list_alias->alias, out, out_length);
		CALL_COMPRESS_HELPER(r, column_list_alias->keywords, new_column_list_alias->keywords, out, out_length);
		// CALL_COMPRESS_HELPER(r, column_list_alias->duplicate_of_column, new_column_list_alias->duplicate_of_column, out,
		// out_length);

		// more
		break;
	case column_list_STATEMENT:
		UNPACK_SQL_STATEMENT(column_list, stmt, column_list);
		if (NULL != out) {
			new_column_list = ((void *)&out[*out_length]);
			fprintf(stderr, "new_column_list: %p\n", new_column_list);
			memcpy(new_column_list, column_list, sizeof(SqlColumnList));
		}
		// *out_length += sizeof(SqlColumnList);
		// CALL_COMPRESS_HELPER(r, column_list->value, new_column_list->value, out, out_length);
		// Compress linked list
		// compress_sqlcolumnlist_list(r, column_list->next, new_column_list->next, out, out_length);
		compress_sqlcolumnlist_list(r, column_list, new_column_list, out, out_length);
		break;
	case column_alias_STATEMENT:
		UNPACK_SQL_STATEMENT(column_alias, stmt, column_alias);
		if (NULL != out) {
			new_column_alias = ((void *)&out[*out_length]);
			memcpy(new_column_alias, column_alias, sizeof(SqlColumnAlias));
		}
		*out_length += sizeof(SqlColumnAlias);
		CALL_COMPRESS_HELPER(r, column_alias->column, new_column_alias->column, out, out_length);
		CALL_COMPRESS_HELPER(r, column_alias->table_alias_stmt, new_column_alias->table_alias_stmt, out, out_length);
		break;
	case join_STATEMENT:
		UNPACK_SQL_STATEMENT(join, stmt, join);
		if (NULL != out) {
			join_list = ((SqlJoin *)&out[*out_length]);
			cur_new_join = new_join = join_list;
		}

		// Get total length of linked list to be allocated
		list_len = 0;
		cur_join = join;
		do {
			list_len++;
			cur_join = cur_join->next;
		} while (cur_join != join);
		*out_length += list_len * sizeof(SqlJoin);

		cur_join = join;
		list_index = 0;
		do {
			if (NULL != out) {
				memcpy(cur_new_join, cur_join, sizeof(SqlJoin));
			}
			// Compress each field
			CALL_COMPRESS_HELPER(r, join->value, new_join->value, out, out_length);
			CALL_COMPRESS_HELPER(r, join->condition, new_join->condition, out, out_length);
			// Compress the linked list pointers
			if (NULL != out) {
				/* The previous item of the first item should be the last item in the list, since the list is
				 * doubly-linked. Conversely, the next item of the last item should be the first in the list.
				 */
				if (0 == list_index) {
					cur_new_join->prev = ((1 == list_len) ? &join_list[0] : &join_list[list_index - 1]);
				} else {
					cur_new_join->prev = &join_list[list_index];
				}
				A2R(cur_new_join->prev, cur_new_join->prev);
				cur_new_join->next = ((list_index + 1 == list_len) ? &join_list[0] : &join_list[list_index + 1]);
				cur_new_join = cur_new_join->next;
				A2R(cur_new_join->next, cur_new_join->next);
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

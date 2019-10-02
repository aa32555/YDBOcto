/****************************************************************
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
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

#define CALL_COMPRESS_HELPER(temp, value, new_value, out, out_length)		\
{										\
	(temp) = compress_statement_helper((value), (out), (out_length));	\
	if((out) != NULL) {							\
		(new_value) = (temp);						\
		if(new_value != NULL) {						\
			A2R((new_value), temp);					\
		}								\
	}									\
}

void *compress_statement_helper(SqlStatement *stmt, char *out, int *out_length);

void compress_statement(SqlStatement *stmt, char **out, int *out_length) {
	*out_length = 0;
	compress_statement_helper(stmt, NULL, out_length);
	*out = malloc(*out_length);
	*out_length = 0;
	compress_statement_helper(stmt, *out, out_length);
}

/*
 * Returns a pointer to a new memory location for stmt within the out buffer
 *
 * If the out buffer is NULL, doesn't copy the statement, but just counts size
 */
void *compress_statement_helper(SqlStatement *stmt, char *out, int *out_length) {
	SqlColumn		*cur_column, *start_column, *new_column;
	SqlOptionalKeyword	*start_keyword, *cur_keyword, *new_keyword;
	SqlStatement		*new_stmt;
	SqlTable		*table, *new_table;
	SqlValue		*value, *new_value;
	int			len;
	void			*r, *ret;

	if(stmt == NULL)
		return NULL;
	if(stmt->v.value == NULL)
		return NULL;
	if(out != NULL) {
		new_stmt = ((void*)&out[*out_length]);
		memcpy(new_stmt, stmt, sizeof(SqlStatement));
		ret = new_stmt;
	}
	*out_length += sizeof(SqlStatement);
	if(out != NULL) {
		new_stmt->v.value = ((void*)&out[*out_length]);
		A2R(new_stmt->v.value, new_stmt->v.value);
	}
	switch(stmt->type) {
	case table_STATEMENT:
		UNPACK_SQL_STATEMENT(table, stmt, table);
		if(out != NULL) {
			new_table = ((void*)&out[*out_length]);
			memcpy(new_table, table, sizeof(SqlTable));
		}
		*out_length += sizeof(SqlTable);
		/// TODO: tables should no longer be a double list
		CALL_COMPRESS_HELPER(r, table->tableName, new_table->tableName, out, out_length);
		CALL_COMPRESS_HELPER(r, table->source, new_table->source, out, out_length);
		CALL_COMPRESS_HELPER(r, table->columns, new_table->columns, out, out_length);
		CALL_COMPRESS_HELPER(r, table->delim, new_table->delim, out, out_length);
		break;
	case value_STATEMENT:
		UNPACK_SQL_STATEMENT(value, stmt, value);
		if(out != NULL) {
			new_value = ((void*)&out[*out_length]);
			memcpy(new_value, value, sizeof(SqlValue));
		}
		*out_length += sizeof(SqlValue);
		switch(value->type) {
		case CALCULATED_VALUE:
			CALL_COMPRESS_HELPER(r, value->v.calculated, new_value->v.calculated, out, out_length);
			break;
		default:
			len = strlen(value->v.string_literal);
			if(out != NULL) {
				memcpy(&out[*out_length], value->v.string_literal, len);
				new_value->v.string_literal = &out[*out_length];
				A2R(new_value->v.string_literal, new_value->v.string_literal);
			}
			*out_length += len;
			if(out != NULL) {
				out[*out_length] = '\0';
			}
			*out_length += 1;
			break;
		}
		break;
	case column_STATEMENT:
		UNPACK_SQL_STATEMENT(cur_column, stmt, column);
		start_column = cur_column;
		do {
			if(out != NULL) {
				new_column = ((void*)&out[*out_length]);
				memcpy(new_column, cur_column, sizeof(SqlColumn));
				new_column->next = new_column->prev = NULL;
			}
			*out_length += sizeof(SqlColumn);
			CALL_COMPRESS_HELPER(r, cur_column->columnName, new_column->columnName, out, out_length);
			// Don't copy table since we want to point to the parent table, which we can't
			// find here. Those relying on a compressed table should call
			// assign_table_to_columns on the decompressed result
			CALL_COMPRESS_HELPER(r, cur_column->keywords, new_column->keywords, out, out_length);
			cur_column = cur_column->next;
			if(out != NULL && cur_column != start_column) {
				new_column->next = ((void*)&out[*out_length]);
				A2R(new_column->next, new_column->next);
			}
		} while(cur_column != start_column);
		break;
	case keyword_STATEMENT:
		UNPACK_SQL_STATEMENT(start_keyword, stmt, keyword);
		cur_keyword = start_keyword;
		do {
			if(out != NULL) {
				new_keyword = ((void*)&out[*out_length]);
				memcpy(new_keyword, cur_keyword, sizeof(SqlOptionalKeyword));
				new_keyword->next = new_keyword->prev = NULL;
			}
			*out_length += sizeof(SqlOptionalKeyword);
			CALL_COMPRESS_HELPER(r, cur_keyword->v, new_keyword->v, out, out_length);
			cur_keyword = cur_keyword->next;
			if(out != NULL && cur_keyword != start_keyword) {
				new_keyword->next = ((void*)&out[*out_length]);
				A2R(new_keyword->next, new_keyword->next);
			}
		} while(cur_keyword != start_keyword);
		break;
	default:
		assert(FALSE);
		FATAL(ERR_UNKNOWN_KEYWORD_STATE, "");
		return NULL;
		break;
	}
	return ret;
}

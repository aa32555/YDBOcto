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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "octo.h"
#include "octo_types.h"
#include "helpers.h"

SqlTable *find_table(const char *table_name) {
	SqlTable *table;
	SqlStatement *stmt;
	ydb_buffer_t  *table_stmt, *save_buffers, save_value, value_b;
	ydb_buffer_t *table_binary_b;
	ydb_buffer_t z_status, z_status_value;
	char *buff;
	int status;
	int num_parts;

	TRACE(CUSTOM_ERROR, "Searching for table %s",
	      table_name);

	/* We look to see if the table has already been loaded, and if so
	 * we get the pointer to process-local memory from the YDB local variable,
	 * then setup the stmt pointer. Else, we load the binary from the database
	 * and store a pointer to the parsed schema.
	 *
	 * We mostly do this to prevent a copy of the table existing for each
	 * call to this function
	 */
	table_stmt = get("%ydboctoloadedschemas", 1, table_name);
	if(table_stmt != NULL) {
		stmt = *((SqlStatement**)table_stmt->buf_addr);
		UNPACK_SQL_STATEMENT(table, stmt, table);
		return table;
	}

	// See how much space is needed
	num_parts = 0;
	table_binary_b = make_buffers(config->global_names.schema, 3, table_name, "b", "");
	YDB_MALLOC_BUFFER(&table_binary_b[3], MAX_STR_CONST);
	while(TRUE) {
		status = ydb_subscript_next_s(table_binary_b, 3, &table_binary_b[1], &table_binary_b[3]);
		if(status == YDB_ERR_NODEEND) {
			break;
		}
		YDB_ERROR_CHECK(status, &z_status, &z_status_value);
		num_parts++;
	}

	if(num_parts == 0) {
		return NULL;
	}

	// Allocate a buffer to hold the full table
	// NOTE: we don't use cmalloc here since we want the table to live for a long time
	buff = malloc(MAX_STR_CONST * num_parts);
	memset(buff, 0, MAX_STR_CONST * num_parts);
	num_parts = 0;
	table_binary_b[3].len_used = 0;
	YDB_MALLOC_BUFFER(&value_b, MAX_STR_CONST);
	while(TRUE) {
		status = ydb_subscript_next_s(table_binary_b, 3, &table_binary_b[1], &table_binary_b[3]);
		if(status == YDB_ERR_NODEEND) {
			break;
		}
		YDB_ERROR_CHECK(status, &z_status, &z_status_value);
		status = ydb_get_s(table_binary_b, 3, &table_binary_b[1], &value_b);
		YDB_ERROR_CHECK(status, &z_status, &z_status_value);
		memcpy(&buff[MAX_STR_CONST * num_parts], value_b.buf_addr, value_b.len_used);
		num_parts++;
	}

	stmt = (void*)buff;
	decompress_statement((char*)stmt, MAX_STR_CONST);
	assign_table_to_columns(stmt);

	save_buffers = make_buffers("%ydboctoloadedschemas", 1, table_name);
	save_value.buf_addr = (char*)&stmt;
	save_value.len_used = save_value.len_alloc = sizeof(void*);
	status = ydb_set_s(&save_buffers[0], 1, &save_buffers[1], &save_value);
	YDB_ERROR_CHECK(status, &z_status, &z_status_value);
	free(save_buffers);
	UNPACK_SQL_STATEMENT(table, stmt, table);

	return table;
}

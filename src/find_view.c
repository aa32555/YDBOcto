/****************************************************************
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
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
#include <errno.h>

#include "octo.h"
#include "octo_types.h"
#include "helpers.h"

// Note that this view is very similar to find_table.c and find_function.c, so changes there may need to be reflected here also.
SqlStatement *find_view(const char *view_name) {
	SqlView * view;
	SqlStatement *stmt;
	ydb_buffer_t  save_value;
	ydb_buffer_t  value_buffer;
	char	      value_str[MAX_BINARY_DEFINITION_FRAGMENT_SIZE];
	char *	      buff, *cur_buff;
	int	      status;
	int32_t	      length;
	long	      length_long;
	MemoryChunk * old_chunk;
	ydb_buffer_t  loaded_schemas, octo_global, view_subs[4], ret;
	char	      retbuff[sizeof(void *)];
	char	      len_str[INT32_TO_STRING_MAX];
	boolean_t     drop_cache;

	YDB_STRING_TO_BUFFER(config->global_names.octo, &octo_global);

	/* We look to see if the view has already been loaded, and if so we get the pointer to process-local memory
	 * from the YDB local variable, then setup the stmt pointer. Else, we load the binary from the database
	 * and store a pointer to the parsed schema.
	 *
	 * Once a view is loaded from the database, it is stored in process local memory and used from there unless
	 * a later CREATE VIEW or DROP VIEW statement has concurrently executed. In this case, we load the view
	 * again from the database and release the process local memory that was housing the previous view definition.
	 */
	YDB_STRING_TO_BUFFER(config->global_names.loadedschemas, &loaded_schemas);
	YDB_STRING_TO_BUFFER(OCTOLIT_VIEWS, &view_subs[0]);
	YDB_STRING_TO_BUFFER("octo", &view_subs[1]);
	YDB_STRING_TO_BUFFER((char *)view_name, &view_subs[2]);
	ret.buf_addr = &retbuff[0];
	ret.len_alloc = sizeof(retbuff);
	status = ydb_get_s(&loaded_schemas, 3, &view_subs[0], &ret);
	switch (status) {
	case YDB_OK:
		/* We have the view definition already stored in process local memory. Use that as long as the
		 * view definition has not changed in the database.
		 */
		stmt = *((SqlStatement **)ret.buf_addr);
		UNPACK_SQL_STATEMENT(view, stmt, create_view);
		/* Check if view has not changed in the database since we cached it
		 *	^%ydboctoocto(OCTOLIT_VIEWS,SCHEMANAME,VIEWNAME)
		 */
		status = ydb_get_s(&octo_global, 3, &view_subs[0], &ret);
		switch (status) {
		case YDB_OK:
			assert(ret.len_alloc > ret.len_used); // ensure space for null terminator
			ret.buf_addr[ret.len_used] = '\0';
			db_oid = (uint64_t)strtoll(ret.buf_addr, NULL, 10);
			if ((LLONG_MIN == (long long)db_oid) || (LLONG_MAX == (long long)db_oid)) {
				ERROR(ERR_SYSCALL_WITH_ARG, "strtoll()", errno, strerror(errno), ret.buf_addr);
				return NULL;
			}
			drop_cache = (db_oid != view->oid);
			break;
		case YDB_ERR_GVUNDEF:
			// A DROP VIEW ran concurrently since we cached it. Reload cache from database.
			drop_cache = TRUE;
			break;
		default:
			YDB_ERROR_CHECK(status);
			return NULL;
			break;
		}
		if (!drop_cache) {
			return view;
		} else {
			/* We do not expect the loaded view cache to be dropped in the normal case.
			 * This includes most of the bats tests in the YDBOcto repo. Therefore, we assert that this
			 * code path is never reached unless an env var is defined that says this is expected.
			 * This way it serves as a good test case of the fact that once a view is loaded in the process
			 * local cache, it is used almost always except for rare cases when we go reload from the database.
			 */
			assert(NULL != getenv("octo_dbg_drop_cache_expected"));
			status = drop_schema_from_local_cache(&view_subs[1], ViewSchema, &view_subs[2]);
			if (YDB_OK != status) {
				// YDB_ERROR_CHECK would already have been done inside "drop_schema_from_local_cache()"
				return NULL;
			}
		}
		break;
	case YDB_ERR_LVUNDEF:
		// View definition does not exist locally. Fall through to read it from database.
		break;
	default:
		YDB_ERROR_CHECK(status);
		return NULL;
		break;
	}

	// Get the length in bytes of the binary view definition
	YDB_STRING_TO_BUFFER(OCTOLIT_LENGTH, &view_subs[2]);
	view_subs[3].buf_addr = len_str;
	view_subs[3].len_alloc = sizeof(len_str);
	status = ydb_get_s(&octo_global, 3, &view_subs[0], &view_subs[3]);
	if (YDB_ERR_GVUNDEF == status) {
		// Definition for view (previous CREATE VIEW statement) doesn't exist
		return NULL;
	}
	YDB_ERROR_CHECK(status);
	if (YDB_OK != status) {
		return NULL;
	}
	view_subs[3].buf_addr[view_subs[3].len_used] = '\0';
	length_long = strtol(view_subs[3].buf_addr, NULL, 10);
	if ((ERANGE != errno) && (0 <= length_long) && (INT32_MAX >= length_long)) {
		length = (int32_t)length_long;
	} else {
		ERROR(ERR_LIBCALL, "strtol");
		return NULL;
	}
	// Switch subscript from OCTOLIT_LENGTH back to OCTOLIT_BINARY to get the binary view definition
	YDB_STRING_TO_BUFFER(OCTOLIT_BINARY, &view_subs[2]);

	/* Allocate a buffer to hold the full view.
	 * Note that we create a new memory chunk here (different from the memory chunk used to store octo structures
	 * for queries) so that this view structure does not get cleaned when one query finishes and the next query starts.
	 */
	old_chunk = memory_chunks;
	memory_chunks = alloc_chunk(length);
	buff = octo_cmalloc(memory_chunks, length);

	value_buffer.buf_addr = value_str;
	value_buffer.len_alloc = sizeof(value_str);
	view_subs[3].len_used = 0;
	cur_buff = buff;
	while (TRUE) {
		/* See "src/run_query.c" under "case create_view_STATEMENT:" for how these global variable nodes are created */
		status = ydb_subscript_next_s(&octo_global, 4, &view_subs[0], &view_subs[3]);
		if (YDB_ERR_NODEEND == status) {
			status = YDB_OK;
			break;
		}
		assert(length > (cur_buff - buff));
		YDB_ERROR_CHECK(status);
		if (YDB_OK != status)
			break;
		status = ydb_get_s(&octo_global, 4, &view_subs[0], &value_buffer);
		YDB_ERROR_CHECK(status);
		if (YDB_OK != status)
			break;
		memcpy(cur_buff, value_buffer.buf_addr, value_buffer.len_used);
		cur_buff += value_buffer.len_used;
		assert(MAX_BINARY_DEFINITION_FRAGMENT_SIZE >= value_buffer.len_used);
		if (length == (cur_buff - buff)) {
			break;
		}
	}
	if (YDB_OK != status) {
		OCTO_CFREE(memory_chunks);
		memory_chunks = old_chunk;
		return NULL;
	}
	assert(length == (cur_buff - buff));

	stmt = (void *)buff;
	decompress_statement((char *)stmt, length);
	if (NULL == stmt) {
		memory_chunks = old_chunk;
		return NULL;
	}

	// Note the pointer to the loaded parse tree root
	save_value.buf_addr = (char *)&stmt;
	save_value.len_used = save_value.len_alloc = sizeof(void *);
	status = ydb_set_s(&loaded_schemas, 2, &view_subs[0], &save_value);
	YDB_ERROR_CHECK(status);
	if (YDB_OK != status) {
		memory_chunks = old_chunk;
		return NULL;
	}
	// Note down the memory chunk so we can free it later
	YDB_STRING_TO_BUFFER(OCTOLIT_CHUNK, &view_subs[2]);
	save_value.buf_addr = (char *)&memory_chunks;
	save_value.len_used = save_value.len_alloc = sizeof(void *);
	status = ydb_set_s(&loaded_schemas, 3, &view_subs[0], &save_value);
	YDB_ERROR_CHECK(status);
	if (YDB_OK != status) {
		memory_chunks = old_chunk;
		return NULL;
	}
	UNPACK_SQL_STATEMENT(view, stmt, create_view);

	// Restore memory chunks
	memory_chunks = old_chunk;

	return view;
}

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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <libyottadb.h>

#include "octo.h"
#include "octo_types.h"
#include "message_formats.h"
#include "rocto.h"
#include "helpers.h"

int handle_describe(Describe *describe, RoctoSession *session) {
	ydb_buffer_t subs_array[3];
	ydb_buffer_t session_global, sql_expression, *source_name = &subs_array[2], *prepared = &subs_array[1], *source_session_id = &subs_array[0];
	ydb_buffer_t plan_name_b;
	size_t query_length = 0, err_buff_size;
	int32_t done = FALSE, status, routine_len = MAX_ROUTINE_LEN;
	uint32_t found = 0;
	char *err_buff;
	ydb_buffer_t schema_global, null_buffer;
	ydb_buffer_t cursor_global, cursor_exe_global[3];
	PhysicalPlan *pplan;
	SqlStatement *statement;
	RowDescription *description;
	NoData *no_data;
	hash128_state_t state;
	char filename[OCTO_PATH_MAX];
	char routine_name[routine_len];
	ydb_buffer_t *filename_lock = NULL;

	// Fetch the named SQL query from the session ^session(id, "prepared", <name>)
	YDB_STRING_TO_BUFFER(config->global_names.session, &session_global);
	YDB_MALLOC_BUFFER(source_session_id, session->session_id->len_used);
	YDB_COPY_BUFFER_TO_BUFFER(session->session_id, source_session_id, done);
	assert(done == TRUE);
	if(describe->item == 'S') {
		YDB_LITERAL_TO_BUFFER("prepared", prepared);
	} else {
		YDB_LITERAL_TO_BUFFER("bound", prepared);
	}
	source_name->buf_addr = describe->name;
	source_name->len_used = source_name->len_alloc = strlen(describe->name);

	YDB_MALLOC_BUFFER(&sql_expression, MAX_STR_CONST);

	// Check if portal exists
	status = ydb_data_s(&session_global, 3, subs_array, &found);
	YDB_ERROR_CHECK(status);
	if (YDB_OK != status) {
		YDB_FREE_BUFFER(source_session_id);
		YDB_FREE_BUFFER(&sql_expression);
		return 1;
	}
	if(found == 0) {
		/// TODO: return error here
	}
	// Send a ParameterDescription

	status = ydb_get_s(&session_global, 3, subs_array, &sql_expression);
	YDB_FREE_BUFFER(source_session_id);
	YDB_ERROR_CHECK(status);
	if (YDB_OK != status) {
		YDB_FREE_BUFFER(&sql_expression);
		return 1;
	}

	query_length = sql_expression.len_used;
	memcpy(input_buffer_combined, sql_expression.buf_addr, query_length);
	YDB_FREE_BUFFER(&sql_expression);
	/*if(input_buffer_combined[query_length-1] != ';' ) {
		input_buffer_combined[query_length++] = ';';
	}*/
	eof_hit = FALSE;
	input_buffer_combined[query_length] = '\0';
	cur_input_index = 0;
	cur_input_more = &no_more;
	//err_buffer = stderr;
	err_buffer = open_memstream(&err_buff, &err_buff_size);
	do {
		memory_chunks = alloc_chunk(MEMORY_CHUNK_SIZE);
		statement = parse_line(input_buffer_combined);
		if(statement == NULL) {
			fflush(err_buffer);
			ERROR(CUSTOM_ERROR, err_buff);
			fclose(err_buffer);
			free(err_buff);
			err_buffer = open_memstream(&err_buff, &err_buff_size);
			OCTO_CFREE(memory_chunks);
			continue;
		}
		// Else, send back the row description
		// Allocate items to create the cursor_exe_global
		YDB_STRING_TO_BUFFER(config->global_names.schema, &schema_global);
		YDB_LITERAL_TO_BUFFER("", &null_buffer);
		YDB_STRING_TO_BUFFER(config->global_names.cursor, &cursor_global);
		YDB_MALLOC_BUFFER(&cursor_exe_global[0], MAX_STR_CONST);
		YDB_LITERAL_TO_BUFFER("exe", &cursor_exe_global[1]);
		YDB_MALLOC_BUFFER(&cursor_exe_global[2], MAX_STR_CONST);

		status = ydb_incr_s(&schema_global, 0, NULL, NULL, &cursor_exe_global[0]);
		YDB_ERROR_CHECK(status);
		if (YDB_OK != status) {
			YDB_FREE_BUFFER(&cursor_exe_global[0]);
			YDB_FREE_BUFFER(&cursor_exe_global[2]);
			OCTO_CFREE(memory_chunks);
			return 1;
		}
		cursor_exe_global[0].buf_addr[cursor_exe_global[0].len_used] = '\0';
		cursor_exe_global[2].len_used = 1;
		*cursor_exe_global[2].buf_addr = '0';

		YDB_FREE_BUFFER(&cursor_exe_global[0]);
		YDB_FREE_BUFFER(&cursor_exe_global[2]);

		switch(statement->type) {
			case table_alias_STATEMENT:
			case set_operation_STATEMENT:
				INVOKE_HASH_CANONICAL_QUERY(state, statement, status);	/* "state" holds final hash */
				if (0 != status) {
					ERROR(ERR_PLAN_HASH_FAILED, "");
					free(filename_lock);
					OCTO_CFREE(memory_chunks);
					return 1;
				}
				status = generate_routine_name(&state, routine_name, routine_len, OutputPlan);
				if (1 == status) {
					ERROR(ERR_PLAN_HASH_FAILED, "");
					free(filename_lock);
					OCTO_CFREE(memory_chunks);
					return 1;
				}
				GET_FULL_PATH_OF_GENERATED_M_FILE(filename, &routine_name[1]);	// updates "filename" to be
												// full path
				if (access(filename, F_OK) == -1) {	// file doesn't exist
					INFO(CUSTOM_ERROR, "Generating M file [%s] (to execute SQL query)", filename);
					filename_lock = make_buffers("^%ydboctoocto", 2, "files", filename);
					// Wait for 5 seconds in case another process is writing to same filename
					status = ydb_lock_incr_s(5000000000, &filename_lock[0], 2, &filename_lock[1]);
					YDB_ERROR_CHECK(status);
					if (YDB_OK != status) {
						free(filename_lock);
						OCTO_CFREE(memory_chunks);
						return 1;
					}
					if (access(filename, F_OK) == -1) {
						pplan = emit_select_statement(statement, filename);
						if(pplan == NULL) {
							status = ydb_lock_decr_s(&filename_lock[0], 2, &filename_lock[1]);
							YDB_ERROR_CHECK(status);
							if (YDB_OK != status) {
								free(filename_lock);
								OCTO_CFREE(memory_chunks);
								return 1;
							}
							free(filename_lock);
							// Need to "fflush" the stream returned by "open_memstream" before
							// trying to access "err_buff" (per man pages of "open_memstream").
							fflush(err_buffer);
							ERROR(CUSTOM_ERROR, err_buff);
							// Need to free "err_buff" right after closing the stream returned by
							// "open_memstream" (per man pages).
							fclose(err_buffer);
							free(err_buff);
							err_buffer = open_memstream(&err_buff, &err_buff_size);
							continue;
						}
						assert(pplan != NULL);
					}
					status = ydb_lock_decr_s(&filename_lock[0], 2, &filename_lock[1]);
					free(filename_lock);
					YDB_ERROR_CHECK(status);
					if (YDB_OK != status) {
						OCTO_CFREE(memory_chunks);
						return 1;
					}
				} else {
					INFO(CUSTOM_ERROR, "Using already generated M file [%s] (to execute SQL query)", filename);
				}
				plan_name_b.buf_addr = filename;
				plan_name_b.len_alloc = plan_name_b.len_used = strlen(filename);
				description = get_plan_row_description(&plan_name_b);
				send_message(session, (BaseMessage*)(&description->type));
				free(description);
				break;
			case no_data_STATEMENT:
				no_data = make_no_data();
				send_message(session, (BaseMessage*)(&no_data->type));
				free(no_data);
				break;
			default:
				description = make_row_description(NULL, 0);
				send_message(session, (BaseMessage*)(&description->type));
				free(description);
		}
		OCTO_CFREE(memory_chunks);
	} while(!eof_hit);

	return 0;
}

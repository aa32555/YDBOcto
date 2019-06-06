/* Copyright (C) 2018-2019 YottaDB, LLC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <libyottadb.h>

#include "octo.h"
#include "octo_types.h"
#include "message_formats.h"
#include "rocto.h"
#include "helpers.h"

int handle_execute(Execute *execute, RoctoSession *session) {
	// At the moment, we don't have "bound function"
	// This feature should be implemented before 1.0
	// For now, just a search-and-replace of anything starting with a '$'
	// This is not super great because it means one could have a SQLI attack
	CommandComplete *response;
	QueryResponseParms parms;
	NoData *no_data;
	EmptyQueryResponse *empty;
	ydb_buffer_t *src_subs;
	ydb_buffer_t session_global, sql_expression;
	ydb_buffer_t z_status, z_status_value;
	size_t new_length = 0, query_length, err_buff_size;
	int length, status, execute_parm;
	int run_query_result = 0;
	char *err_buff;
	ErrorResponse *err;

	TRACE(ERR_ENTERING_FUNCTION, "handle_execute");

	memset(&parms, 0, sizeof(QueryResponseParms));
	parms.session = session;

	// zstatus buffers
	YDB_LITERAL_TO_BUFFER("$ZSTATUS", &z_status);
	YDB_MALLOC_BUFFER(&z_status_value, MAX_STR_CONST);

	// Fetch the named SQL query from the session ^session(id, "prepared", <name>)
	src_subs = make_buffers(config->global_names.session, 3, session->session_id->buf_addr, "bound", execute->source);
	YDB_MALLOC_BUFFER(&sql_expression, MAX_STR_CONST);
	sql_expression.len_alloc -= 1;

	status = ydb_get_s(&src_subs[0], 3, &src_subs[1], &sql_expression);
	YDB_ERROR_CHECK(status, &z_status, &z_status_value);

	sql_expression.buf_addr[sql_expression.len_used] = '\0';
	query_length = strlen(sql_expression.buf_addr);

	if(query_length + 2 > cur_input_max) {
		err = make_error_response(PSQL_Error_ERROR,
				PSQL_Code_Protocol_Violation,
				"execute query length exceeeded maximum size",
				0);
		send_message(session, (BaseMessage*)(&err->type));
		free_error_response(err);
		return 0;
	}
	memcpy(input_buffer_combined, sql_expression.buf_addr, query_length);
	/*if(input_buffer_combined[query_length-1] != ';' ) {
		input_buffer_combined[query_length++] = ';';
	}*/
	eof_hit = FALSE;
	input_buffer_combined[query_length] = '\0';
	cur_input_index = 0;
	cur_input_more = &no_more;
	//err_buffer = stderr;
	err_buffer = open_memstream(&err_buff, &err_buff_size);

	parms.max_data_to_send = execute->rows_to_return;
	do {
		parms.data_sent = FALSE;
		run_query_result = run_query(input_buffer_combined, &handle_query_response, (void*)&parms);
		if(run_query_result == FALSE && !eof_hit) {
			fflush(err_buffer);
			fclose(err_buffer);
			err = make_error_response(PSQL_Error_ERROR,
					PSQL_Code_Syntax_Error,
					err_buff,
					0);
			send_message(session, (BaseMessage*)(&err->type));
			free_error_response(err);
			free(err_buff);
			err_buffer = open_memstream(&err_buff, &err_buff_size);
		}
		if(!parms.data_sent) {
			empty = make_empty_query_response();
			send_message(session, (BaseMessage*)(&empty->type));
			free(empty);
		}
	} while(!eof_hit);

	//free(sql_expression.buf_addr);
	free(z_status_value.buf_addr);

	// TODO: we need to limit the returns and provide a PortalSuspend if a limit on rows was requested
	// For now, we always return all rows
	/*response = make_command_complete("SELECT 25");
	printf("Response type is %c\n\n", response->type);
	send_message(session, (BaseMessage*)(&response->type));
	free(response);*/

	return 0;
}

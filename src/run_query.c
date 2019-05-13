/* Copyright (C) 2018 YottaDB, LLC
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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
#include <string.h>

#include <libyottadb.h>
#include <gtmxc_types.h>

#include <openssl/evp.h>

#include "mmrhash.h"

#include "octo.h"
#include "octo_types.h"
#include "physical_plan.h"
#include "parser.h"
#include "lexer.h"

int run_query(char *query, void (*callback)(SqlStatement *, PhysicalPlan *, int, void*), void *parms) {
	int c, error = 0, i = 0, status;
	int done;
	char *buffer;
	size_t buffer_size = 0;
	FILE *inputFile;
	FILE *out;
	SqlValue *value;
	SqlTable *table, *temp_table;
	SqlStatement *result = 0;
	SqlSelectStatement *select;
	SqlStatement *tmp_statement;
	SqlShowStatement *show;
	SqlSetStatement *set;
	PhysicalPlan *pplan;
	ydb_buffer_t schema_global, table_name_buffer, table_create_buffer, null_buffer;
	ydb_buffer_t cursor_global, cursor_exe_global[3];
	ydb_buffer_t z_status, z_status_value;
	gtm_char_t      err_msgbuf[MAX_STR_CONST];
	gtm_long_t cursorId;
	EVP_MD_CTX *mdctx = NULL;

	memory_chunks = alloc_chunk(MEMORY_CHUNK_SIZE);

	buffer = octo_cmalloc(memory_chunks, 5);

	inputFile = NULL;
	table_name_buffer.buf_addr = malloc(MAX_STR_CONST);
	table_name_buffer.len_used = 0;
	table_name_buffer.len_alloc = MAX_STR_CONST;
	table_create_buffer.buf_addr = malloc(MAX_STR_CONST);
	table_create_buffer.len_used = 0;
	table_create_buffer.len_alloc = MAX_STR_CONST;

	YDB_STRING_TO_BUFFER(config->global_names.schema, &schema_global);
	YDB_LITERAL_TO_BUFFER("", &null_buffer);
	YDB_STRING_TO_BUFFER(config->global_names.cursor, &cursor_global);
	INIT_YDB_BUFFER(&cursor_exe_global[0], MAX_STR_CONST);
	YDB_LITERAL_TO_BUFFER("exe", &cursor_exe_global[1]);
	INIT_YDB_BUFFER(&cursor_exe_global[2], MAX_STR_CONST);

	status = ydb_incr_s(&schema_global, 0, NULL, NULL, &cursor_exe_global[0]);
	YDB_ERROR_CHECK(status, &z_status, &z_status_value);
	cursor_exe_global[0].buf_addr[cursor_exe_global[0].len_used] = '\0';
	cursor_exe_global[2].len_used = 1;
	*cursor_exe_global[2].buf_addr = '0';

	INFO(CUSTOM_ERROR, "Generating SQL for cursor %s", cursor_exe_global[0].buf_addr);

	INFO(CUSTOM_ERROR, "Parsing SQL command %s", query);
	result = parse_line(query);
	INFO(CUSTOM_ERROR, "Done!");
	if(result == NULL) {
		free(table_name_buffer.buf_addr);
		free(table_create_buffer.buf_addr);
		free(cursor_exe_global[0].buf_addr);
		free(cursor_exe_global[2].buf_addr);
		INFO(CUSTOM_ERROR, "Returning failure from run_query");
		return 0;
	}
	if(config->dry_run) {
		octo_cfree(memory_chunks);
		result = NULL;
		free(table_name_buffer.buf_addr);
		free(table_create_buffer.buf_addr);
		free(cursor_exe_global[0].buf_addr);
		free(cursor_exe_global[2].buf_addr);
		return 1;
	}
	switch(result->type) {
	case select_STATEMENT:
		if(mdctx == NULL && ((mdctx = EVP_MD_CTX_new()) == NULL)) {
			FATAL(ERR_LIBSSL_ERROR);
		}
		if(1 != EVP_DigestInit_ex(mdctx, EVP_md5(), NULL)) {
			FATAL(ERR_LIBSSL_ERROR);
		}
		// hash_canonical_query(mdctx, result);
		pplan = emit_select_statement(&cursor_global, cursor_exe_global, result, NULL);
		assert(pplan != NULL);
		cursorId = atol(cursor_exe_global[0].buf_addr);
		SWITCH_FROM_OCTO_GLOBAL_DIRECTORY();
		status = ydb_ci("select", cursorId);
		YDB_ERROR_CHECK(status, &z_status, &z_status_value);
		SWITCH_TO_OCTO_GLOBAL_DIRECTORY();
		(*callback)(result, pplan, cursorId, parms);
		// Deciding to free the select_STATEMENT must be done by the caller, as they may want to rerun it or send row
		// descriptions
		//octo_cfree(memory_chunks);
		//memory_chunks = NULL;
		break;
	case table_STATEMENT:
		out = open_memstream(&buffer, &buffer_size);
		assert(out);
		emit_create_table(out, result);
		fclose(out);
		INFO(CUSTOM_ERROR, "%s", buffer);
		UNPACK_SQL_STATEMENT(value, result->v.table->tableName, value);
		YDB_COPY_STRING_TO_BUFFER(value->v.reference, &table_name_buffer, done);
		if(!done) {
			FATAL(ERR_TABLE_DEFINITION_TOO_LONG, value->v.reference,
					table_name_buffer.len_alloc,
					strlen(value->v.reference));
		}
		YDB_COPY_STRING_TO_BUFFER(buffer, &table_create_buffer, done);
		if(!done) {
			FATAL(ERR_TABLE_DEFINITION_TOO_LONG, value->v.reference,
					table_create_buffer.len_alloc,
					strlen(buffer));
		}
		status = ydb_set_s(&schema_global, 1,
				   &table_name_buffer,
				   &table_create_buffer);
		YDB_ERROR_CHECK(status, &z_status, &z_status_value);
		free(buffer);
		/// TODO: we should drop preexisting tables from here
		if(definedTables == NULL) {
			definedTables = result->v.table;
			dqinit(definedTables);
		} else {
			dqinsert(definedTables, result->v.table, temp_table);
		}
		octo_cfree(memory_chunks);
		break;
	case drop_STATEMENT:
		YDB_COPY_STRING_TO_BUFFER(result->v.drop->table_name->v.value->v.reference, &table_name_buffer, done);
		status = ydb_delete_s(&schema_global, 1,
				      &table_name_buffer,
				      YDB_DEL_NODE);
		YDB_ERROR_CHECK(status, &z_status, &z_status_value);
		/// TODO: we should drop tables here
		octo_cfree(memory_chunks);
		break;
	case insert_STATEMENT:
		WARNING(ERR_FEATURE_NOT_IMPLEMENTED, "table inserts");
		octo_cfree(memory_chunks);
		break;
	case begin_STATEMENT:
	case commit_STATEMENT:
		WARNING(ERR_FEATURE_NOT_IMPLEMENTED, "transactions");
		octo_cfree(memory_chunks);
		break;
	case set_STATEMENT:
	case show_STATEMENT:
		(*callback)(result, NULL, cursorId, parms);
		octo_cfree(memory_chunks);
		break;
	case no_data_STATEMENT:
		octo_cfree(memory_chunks);
		break;
	default:
		FATAL(ERR_FEATURE_NOT_IMPLEMENTED, query);
	}
	result = NULL;
	free(table_name_buffer.buf_addr);
	free(table_create_buffer.buf_addr);
	free(cursor_exe_global[0].buf_addr);
	free(cursor_exe_global[2].buf_addr);
	return 1;
}

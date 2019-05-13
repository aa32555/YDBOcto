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
#ifndef OCTO_H
#define OCTO_H

#include <stdio.h>

#include <libyottadb.h>

#include "errors.h"
#include "octo_types.h"
#include "config.h"
#include "constants.h"
#include "memory_chunk.h"

#include "mmrhash.h"

#if YDB_RELEASE < 125
/* Macro to copy a string (i.e. "char *" pointer in C) to an already allocated ydb_buffer_t structure.
 * If BUFFERP does not have space allocated to hold STRING, then no copy is done
 *	and COPY_DONE will be set to FALSE.
 * Else the copy is done and COPY_DONE will be set to TRUE.
 * User of this macro needs to include <string.h> (needed for "strlen" prototype).
 * See comment before YDB_STRING_TO_BUFFER macro for why YDB_COPY_LITERAL_TO_BUFFER and YDB_COPY_STRING_TO_BUFFER
 * cannot be merged into one macro (i.e. sizeof is not same as strlen in rare case).
 */
#define YDB_COPY_STRING_TO_BUFFER(STRING, BUFFERP, COPY_DONE)	\
{								\
	int	len;						\
								\
	len = strlen(STRING);					\
	if (len <= (BUFFERP)->len_alloc)			\
	{							\
		memcpy((BUFFERP)->buf_addr, STRING, len);	\
		(BUFFERP)->len_used = len;			\
		COPY_DONE = TRUE;				\
	} else							\
		COPY_DONE = FALSE;				\
}


/* Macro to create/fill-in a ydb_buffer_t structure from a C string (char * pointer).
 * Note that YDB_LITERAL_TO_BUFFER does a "sizeof(LITERAL) - 1" whereas YDB_STRING_TO_BUFFER does a "strlen()".
 * Both produce the same output almost always. There is one exception though and that is if LITERAL has embedded null bytes
 * in it. In that case, sizeof() would include the null bytes too whereas strlen() would not. Hence the need for both versions
 * of the macros.
 */
#define YDB_STRING_TO_BUFFER(STRING, BUFFERP)				\
{									\
	(BUFFERP)->buf_addr = STRING;					\
	(BUFFERP)->len_used = (BUFFERP)->len_alloc = strlen(STRING);	\
}
#endif

/**
 * Switches to the octo global directory
 */
#define SWITCH_TO_OCTO_GLOBAL_DIRECTORY()						\
	do {										\
		int result = 0;								\
		ydb_buffer_t z_status, z_status_value;					\
		result = ydb_get_s(&config->zgbldir, 0, NULL, &config->prev_gbldir);	\
		YDB_ERROR_CHECK(status, &z_status, &z_status_value);			\
		result = ydb_set_s(&config->zgbldir, 0, NULL, &config->octo_gbldir);	\
		YDB_ERROR_CHECK(status, &z_status, &z_status_value);			\
	} while(FALSE);

/**
 * Switches from the octo global directory
 */
#define SWITCH_FROM_OCTO_GLOBAL_DIRECTORY()						\
	do {										\
		int result = 0;								\
		result = ydb_set_s(&config->zgbldir, 0, NULL, &config->prev_gbldir);	\
		assert(result == 0);							\
	} while(FALSE);

int emit_column_specification(char *buffer, int buffer_size, SqlColumn *column);
void emit_create_table(FILE *output, struct SqlStatement *stmt);
// Recursively copies all of stmt, including making copies of strings

/**
 * Examines the table to make sure needed columns are specified, and fills out
 * any that are needed but not present.
 *
 * @returns 0 if success, 1 otherwise
 */
int create_table_defaults(SqlStatement *table_statement, SqlStatement *keywords_statement);

char *m_escape_string(const char *string);
int m_escape_string2(char *buffer, int buffer_len, char *string);
char *m_unescape_string(const char *string);

int readline_get_more();
SqlStatement *parse_line(const char *line);

int populate_data_type(SqlStatement *v, SqlValueType *type);
SqlTable *find_table(const char *table_name);
SqlColumn *find_column(char *column_name, SqlTable *table);
int qualify_column_list_alias(SqlColumnListAlias *alias, SqlJoin *tables);
int qualify_column_list(SqlColumnList *select_columns, SqlJoin *tables);
SqlColumnAlias *qualify_column_name(SqlValue *column_value, SqlJoin *tables);
SqlStatement *match_column_in_table(SqlTableAlias *table, char *column_name, int column_name_len);
int qualify_statement(SqlStatement *stmt, SqlJoin *tables);
int qualify_join_conditions(SqlJoin *join, SqlJoin *tables);
int qualify_function_name(SqlStatement *stmt, SqlJoin *tables);
void print_yyloc(YYLTYPE *llocp);
SqlOptionalKeyword *get_keyword(SqlColumn *column, enum OptionalKeyword keyword);
SqlOptionalKeyword *get_keyword_from_keywords(SqlOptionalKeyword *start_keyword, enum OptionalKeyword keyword);
int get_key_columns(SqlTable *table, SqlColumn **key_columns);
int generate_key_name(char *buffer, int buffer_size, int target_key_num, SqlTable *table, SqlColumn **key_columns);

/* Hashing support functions */
int generate_filename(hash128_state_t *state, const char *directory_path, char *full_path, FileType file_type);
void hash_canonical_query(hash128_state_t *state, SqlStatement *stmt);
void ydb_hash_to_string(ydb_uint16 *hash, char *buffer, const unsigned int buf_len);

void assign_table_to_columns(SqlStatement *table_statement);
SqlColumn *column_list_alias_to_columns(SqlTableAlias *table_alias);
int get_column_piece_number(SqlColumnAlias *alias, SqlTableAlias *table_alias);

/**
 * Returns TRUE if the columns are equal, FALSE otherwise
 */
int columns_equal(SqlColumn *a, SqlColumn *b);
int tables_equal(SqlTable *a, SqlTable *b);
int values_equal(SqlValue *a, SqlValue *b);

int no_more();

/* Globals */
SqlTable *definedTables;
int cur_input_index;
int cur_input_max;
int eof_hit;
FILE *inputFile;
FILE *err_buffer;
char *input_buffer_combined;
int (*cur_input_more)();

int get_input(char *buf, int size);
#endif

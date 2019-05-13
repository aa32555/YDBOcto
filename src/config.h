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

#ifndef CONFIG_H
#define CONFIG_H

#include <libyottadb.h>
#include <libconfig.h>

#include "errors.h"

int octo_init(int argc, char **argv, int scan_tables);
int parse_startup_flags(int argc, char **argv);

struct RoctoConfig {
	int port;
	int ssl_on;
	const char *address;
	const char *ssl_cert_file;
	const char *ssl_key_file;
} typedef RoctoConfig;

typedef struct {
	char *schema, *session, *cursor, *octo, *xref;
	char *raw_schema, *raw_session, *raw_cursor, *raw_octo, *raw_xref;
} GlobalNames;

struct OctoConfig {
	config_t config_file;
	RoctoConfig rocto_config;
	GlobalNames global_names;
	enum ERROR_LEVEL record_error_level;
	int dry_run, plan_id;
	const char *tmp_dir, *global_directory, *config_file_name, *global_prefix;
	ydb_buffer_t zgbldir, octo_gbldir, prev_gbldir;
	int is_tty, page_size;
} typedef OctoConfig;

OctoConfig *config;

#endif

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
#include <ctype.h>

#include "octo.h"
#include "octo_types.h"
#include "helpers.h"

/* Attempt to store a row in pg_catalog.pg_views for this view.
 * Note that this function is similar to store_table_in_pg_class.
 */
int store_view_in_pg_views(SqlView *view) {
	SqlValue *	      name_value, *defn_value;
	ydb_buffer_t	      pg_views[5];
	ydb_buffer_t	      octo_views[5];
	ydb_buffer_t	      row_buffer;
	ydb_buffer_t	      oid_buffer;
	long long	      view_oid;
	int		      status;
	char *		      view_name, *schema_name, *definition;
	char		      row_str[MAX_STR_CONST];
	char		      view_oid_str[INT32_TO_STRING_MAX]; /* OIDs are stored as 4-byte unsigned integers:
								  * https://www.postgresql.org/docs/current/datatype-oid.html
								  */
	// Extract the view name and set the schema_name
	UNPACK_SQL_STATEMENT(name_value, view->view_name, value);
	view_name = name_value->v.string_literal;
	schema_name = "octo"; // Use "octo" as a default schema name until schemas are implemented
	// Extract the view definition, i.e. the text for a SELECT query
	UNPACK_SQL_STATEMENT(defn_value, view->view_name, value);
	definition = defn_value->v.string_literal;

	// Setup pg_views table node buffers
	YDB_STRING_TO_BUFFER(config->global_names.octo, &pg_views[0]);
	YDB_STRING_TO_BUFFER(OCTOLIT_TABLES, &pg_views[1]);
	YDB_STRING_TO_BUFFER(OCTOLIT_PG_CATALOG, &pg_views[2]);
	YDB_STRING_TO_BUFFER("pg_views", &pg_views[3]);
	YDB_STRING_TO_BUFFER(schema_name, &pg_views[4]);
	YDB_STRING_TO_BUFFER(view_name, &pg_views[5]);

	/* These are hard-coded magic values related to the Postgres catalog.
	 * Many of these simply aren't relevant for Octo as they pertain to features
	 * that aren't implemented.
	 * The columns that are populated by this module are those that are clearly necessary for
	 * the specified view's definition and use:
	 *	schemaname (schema view was created in)
	 *	viewname (name of view)
	 *	viewowner (user who created the view; will be "octo" if not created via rocto, i.e. when a username is required)
	 *	definition (text definition of the view, i.e. a SELECT query)
	 * Columns of `pg_catalog.pg_views` table in `tests/fixtures/octo-seed.sql`.
	 * Any changes to that table definition will require changes here too.
	 */
	snprintf(row_str, sizeof(row_str), "%s|%s",
			((config->is_rocto) ? config->rocto_config.username : "octo"),
			definition);
	row_buffer.len_alloc = row_buffer.len_used = strlen(row_str);
	row_buffer.buf_addr = row_str;
	/* Set the view name passed in as having an oid VIEWOID in the pg_catalog.
	 * 	i.e. SET ^%ydboctoocto(OCTOLIT_TABLES,OCTOLIT_PG_CATALOG,"pg_views",SCHEMANAME,VIEWNAME)=...
	 */
	status = ydb_set_s(&pg_views[0], 5, &pg_views[1], &row_buffer);
	YDB_ERROR_CHECK(status);
	if (YDB_OK != status) {
		return 1;
	}

	/* Store a cross reference of the VIEWOID in ^%ydboctoocto(OCTOLIT_VIEWS).
	 *	i.e. SET ^%ydboctoocto(OCTOLIT_VIEWS,view_name,view_hash,OCTOLIT_OID)=VIEWOID
	 * That way a later DROP VIEW or CREATE VIEW `view_name` can clean all ^%ydboctoocto
	 * nodes created during the previous CREATE VIEW `view_name`
	 */
	YDB_STRING_TO_BUFFER(config->global_names.octo, &octo_views[0]);
	YDB_STRING_TO_BUFFER(OCTOLIT_VIEWS, &octo_views[1]);
	YDB_STRING_TO_BUFFER(schema_name, &octo_views[2]);
	YDB_STRING_TO_BUFFER(view_name, &octo_views[3]);
	YDB_STRING_TO_BUFFER(OCTOLIT_OID, &octo_views[4]);
	/* Get a unique OID for the passed in view.
	 * 	i.e. $INCREMENT(^%ydboctoocto(OCTOLIT_VIEWS))
	 * Note that this OID is only used for comparing OIDs and is not stored in the pg_catalog or otherwise referenced globally.
	 * Accordingly, it is safe to increment OIDs independent of the global OID store at ^%ydboctoocto(OCTOLIT_OID).
	 */
	OCTO_SET_BUFFER(oid_buffer, view_oid_str);
	status = ydb_incr_s(&octo_views[0], 1, &octo_views[1], NULL, &oid_buffer);
	YDB_ERROR_CHECK(status);
	if (YDB_OK != status) {
		return 1;
	}
	pg_views[4].buf_addr[pg_views[4].len_used] = '\0';

	status = ydb_set_s(&octo_views[0], 4, &octo_views[1], &oid_buffer);
	YDB_ERROR_CHECK(status);
	if (YDB_OK != status) {
		return 1;
	}

	view_oid = strtoll(oid_buffer.buf_addr, NULL, 10); /* copy over class OID before we start changing it for column OID */
	if ((LLONG_MIN == view_oid) || (LLONG_MAX == view_oid)) {
		ERROR(ERR_SYSCALL_WITH_ARG, "strtoll()", errno, strerror(errno), pg_views[4].buf_addr);
		return 1;
	}
	view->oid = view_oid; /* Initialize OID in SqlView. Caller later invokes "compress_statement()" that stores
				   * this as part of the binary view definition in the database.
				   */
	return 0;
}

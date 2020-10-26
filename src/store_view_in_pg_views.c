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

#define CLEANUP_AND_RETURN(PG_VIEWS, OID_BUFFER) \
	{                                       \
		YDB_FREE_BUFFER(&PG_VIEWS[4]);   \
		return 1;                       \
	}

#define CLEANUP_AND_RETURN_IF_NOT_YDB_OK(STATUS, PG_VIEWS, OID_BUFFER) \
	{                                                             \
		YDB_ERROR_CHECK(STATUS);                              \
		if (YDB_OK != STATUS) {                               \
			CLEANUP_AND_RETURN(PG_VIEWS, OID_BUFFER);      \
		}                                                     \
	}

/* Attempt to store a row in pg_catalog.pg_views for this view.
 * Note that this function is similar to store_table_in_pg_class.
 */
int store_view_in_pg_views(SqlView *view) {
	SqlValue *	      name_value, *defn_value;
	ydb_buffer_t	      pg_views[5];
	ydb_buffer_t	      octo_views[5];
	ydb_buffer_t	      row_buffer;
	int		      status, result;
	int32_t		      arg_type_list_len;
	char *		      view_name, *schema_name;
	char		      row_str[MAX_STR_CONST];
	char		      arg_type_list[ARGUMENT_TYPE_LIST_MAX_LEN];
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
			((config->is_rocto) ? config->rocto_config->username : "octo"),
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
	/* Unlike functions or tables, view deletion doesn't require deletion of plans using the deleted view. This is because views
	 * are essentially named subqueries and deleting the name doesn't make the subquery any less usable. Accordingly, views can
	 * be deleted while plans relying on the underlying subquery named by the view may continue running unmodified. Hence, there
	 * is no OID or cross-reference generation performed here as there is in store_function_in_pg_proc and
	 * store_table_in_pg_class.
	 */
	return 0;
}

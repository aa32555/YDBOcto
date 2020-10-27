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

#include <assert.h>

#include "octo.h"
#include "helpers.h"

/* Deletes all references to a view name from the catalog.
 * Undoes what was done by "store_view_in_pg_views.c".
 * Returns
 *	0 for normal
 *	1 for error
 */
int delete_view_from_pg_views(ydb_buffer_t *view_name_buffer) {
	int	     status;
	ydb_buffer_t pg_views[6];
	ydb_buffer_t octo_views[4];
	char *	     schema_name;

	schema_name = "octo"; // Use "octo" as a default schema name until schemas are implemented

	YDB_STRING_TO_BUFFER(config->global_names.octo, &pg_views[0]);
	YDB_STRING_TO_BUFFER(OCTOLIT_TABLES, &pg_views[1]);
	YDB_STRING_TO_BUFFER(OCTOLIT_PG_CATALOG, &pg_views[2]);
	YDB_STRING_TO_BUFFER("pg_views", &pg_views[3]);
	YDB_STRING_TO_BUFFER(schema_name, &pg_views[4]);
	YDB_STRING_TO_BUFFER(view_name_buffer->buf_addr, &pg_views[5]);

	// Check keys for VIEWNAME (usually stored as
	// ^%ydboctoocto(OCTOLIT_VIEWS,VIEWSCHEMA,VIEWNAME)
	YDB_STRING_TO_BUFFER(config->global_names.octo, &octo_views[0]);
	YDB_STRING_TO_BUFFER(OCTOLIT_VIEWS, &octo_views[1]);
	octo_views[2] = *view_name_buffer;
	YDB_STRING_TO_BUFFER(OCTOLIT_OID, &octo_views[3]);
	status = ydb_get_s(&octo_views[0], 3, &octo_views[1], &pg_views[3]);
	if (YDB_ERR_GVUNDEF != status) {
		YDB_ERROR_CHECK(status);
		if (YDB_OK != status) {
			return 1;
		}
		/* Delete view OID node : i.e. KILL ^%ydboctoocto(OCTOLIT_TABLES,OCTOLIT_PG_CATALOG,"pg_views",VIEWSCHEMA,VIEWNAME)
		 * Since other views may exist for the given schema_name, don't delete that, but only the given view_name for that
		 * schema.
		 */
		status = ydb_delete_s(&pg_views[0], 5, &pg_views[1], YDB_DEL_NODE);
		YDB_ERROR_CHECK(status);
		if (YDB_OK != status) {
			return 1;
		}
	}
	return 0;
}

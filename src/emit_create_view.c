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

#include "octo.h"
#include "octo_types.h"

// Emits DDL specification for the given view
// Args:
//	FILE *output: output file to write DDL to
//	SqlStatement *stmt: a SqlView type SqlStatement
// Returns:
//	0 for success, 1 for error
int emit_create_view(FILE *output, struct SqlStatement *stmt) {
	SqlView *	      view;
	SqlValue *	      view_name;

	if (stmt == NULL)
		return 0;
	view = stmt->v.create_view;
	assert(view->view_name);
	// assert(view->parameter_type_list);
	UNPACK_SQL_STATEMENT(view_name, view->view_name, value);
	fprintf(output, "CREATE VIEW `%s` AS ", view_name->v.string_literal);
	fprintf(output, "%s", view->query);
	fprintf(output, ";");
	return 0;
}

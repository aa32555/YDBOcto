/****************************************************************
 *								*
 * Copyright (c) 2019-2020 YottaDB LLC and/or its subsidiaries.	*
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

#include "octo.h"
#include "octo_types.h"

SqlColumn *find_column(char *column_name, SqlTable *table) {
	SqlColumn *cur_column = NULL, *start_column = NULL;
	SqlValue * value = NULL;

	TRACE(CUSTOM_ERROR, "Searching for column %s in table %s", column_name, table->tableName->v.value->v.reference);

	UNPACK_SQL_STATEMENT(start_column, table->columns, column);
	cur_column = start_column;
	do {
		UNPACK_SQL_STATEMENT(value, cur_column->columnName, value);
		if (strcmp(column_name, value->v.reference) == 0) {
			return cur_column;
		}
		cur_column = cur_column->next;
	} while (cur_column != start_column);
	return NULL;
}

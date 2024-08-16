/****************************************************************
 *								*
 * Copyright (c) 2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "logical_plan.h"
#include "physical_plan.h"

SqlKey *get_iter_key(PhysicalPlan *pplan, int key_index) {
	// Get key pointer from lvn
	// varname
	ydb_buffer_t varname;
	YDB_LITERAL_TO_BUFFER(OCTOLIT_ITERKEYS, &varname);
	// subs pplan_unique_id, key_index
	ydb_buffer_t subs[2];
	subs[0].buf_addr = (char *)&pplan;
	subs[0].len_used = subs[0].len_alloc = sizeof(void *);
	char key_unique_id[INT32_TO_STRING_MAX];
	subs[1].buf_addr = key_unique_id;
	subs[1].len_alloc = sizeof(key_unique_id);
	subs[1].len_used = snprintf(subs[1].buf_addr, subs[1].len_alloc, "%d", key_index);
	ydb_buffer_t ret;
	char	     retbuff[sizeof(void *)];
	OCTO_SET_BUFFER(ret, retbuff);
	// ydb_get_s
	int status = ydb_get_s(&varname, 2, &subs[0], &ret);
	if (YDB_OK == status) {
		SqlKey *key = *((SqlKey **)ret.buf_addr);
		return key;
	} else {
		return NULL;
	}
}

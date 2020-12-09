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

#include "octo.h"

#define CLEANUP_AND_RETURN_IF_NOT_YDB_OK(STATUS) \
	{                                        \
		if (YDB_OK != STATUS) {          \
			return STATUS;           \
		}                                \
	}

/* Store the binary representation of the CREATE VIEW statement in one or more of the following nodes
 *	^%ydboctoschema(OCTOLIT_VIEWS,schema_name,view_name,OCTOLIT_BINARY,0)
 *	^%ydboctoschema(OCTOLIT_VIEWS,schema_name,view_name,OCTOLIT_BINARY,1)
 *	^%ydboctoschema(OCTOLIT_VIEWS,schema_name,view_name,OCTOLIT_BINARY,2)
 *	...
 * Each node can store up to MAX_DEFINITION_FRAGMENT_SIZE bytes.
 * Assumes "view_name_buffers" is an array of 5 "ydb_buffer_t" structures with the following initialized
 *	view_name_buffers[0] = OCTOLIT_VIEWS
 *	view_name_buffers[1] = schema_name
 *	view_name_buffers[2] = view_name
 * This function initializes the following
 *	view_name_buffers[3] = OCTOLIT_BINARY
 *	view_name_buffers[4] = 0, 1, 2, ... as needed
 * Returns
 *	YDB_OK on success.
 *	YDB_ERR_* on failure.
 * Note: The below logic is very similar to "src/store_binary_table_definition.c" and "src/store_binary_table_definition.c"
 */
int store_binary_view_definition(ydb_buffer_t *view_name_buffers, char *binary_view_defn, int binary_view_defn_length) {
	ydb_buffer_t view_binary_buffer, octo_global, *sub_buffer;
	int	     i, cur_length, status;
	char	     fragment_num_buff[INT64_TO_STRING_MAX];

	view_binary_buffer.len_alloc = MAX_DEFINITION_FRAGMENT_SIZE;
	YDB_STRING_TO_BUFFER(config->global_names.octo, &octo_global);
	YDB_STRING_TO_BUFFER(OCTOLIT_BINARY, &view_name_buffers[3]);
	sub_buffer = &view_name_buffers[4];
	sub_buffer->buf_addr = fragment_num_buff;
	sub_buffer->len_alloc = sizeof(fragment_num_buff);
	i = 0;
	cur_length = 0;
	while (cur_length < binary_view_defn_length) {
		sub_buffer->len_used = snprintf(sub_buffer->buf_addr, sub_buffer->len_alloc, "%d", i);
		view_binary_buffer.buf_addr = &binary_view_defn[cur_length];
		if (MAX_DEFINITION_FRAGMENT_SIZE < (binary_view_defn_length - cur_length)) {
			view_binary_buffer.len_used = MAX_DEFINITION_FRAGMENT_SIZE;
		} else {
			view_binary_buffer.len_used = binary_view_defn_length - cur_length;
		}
		status = ydb_set_s(&octo_global, 5, view_name_buffers, &view_binary_buffer);
		CLEANUP_AND_RETURN_IF_NOT_YDB_OK(status);
		cur_length += MAX_DEFINITION_FRAGMENT_SIZE;
		i++;
	}
	YDB_STRING_TO_BUFFER(OCTOLIT_LENGTH, &view_name_buffers[3]);
	// Use sub_buffer as a temporary buffer below
	sub_buffer->len_used = snprintf(sub_buffer->buf_addr, sub_buffer->len_alloc, "%d", binary_view_defn_length);
	status = ydb_set_s(&octo_global, 4, view_name_buffers, sub_buffer);
	CLEANUP_AND_RETURN_IF_NOT_YDB_OK(status);
	return YDB_OK;
}

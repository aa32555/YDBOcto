/****************************************************************
 *								*
 * Copyright (c) 2021-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include <assert.h>
#include <stdlib.h>

#include "octo.h"
#include "octo_types.h"
#include "memory_chunk.h"

/* Find the hash in GroupBy list which matches hash for `stmt_to_match`.
 * Return GroupBy list node/column number which corresponds to the hash.
 * Returns -1 if no matching hash is found.
 */
int get_group_by_column_number(SqlTableAlias *table_alias, SqlStatement *stmt_to_match, boolean_t *is_inner_expression) {
	// Hash the stmt_to_match
	int		stmt_status = HASH_LITERAL_VALUES;
	hash128_state_t hash_to_match;
	HASH128_STATE_INIT(hash_to_match, 0);
	hash_canonical_query(&hash_to_match, stmt_to_match, &stmt_status, TRUE);
	// Convert hash_to_match -> hash_to_match_str
	ydb_uint16   res_hash;
	unsigned int hash_len = MAX_ROUTINE_LEN;
	char *	     hash_to_match_str = (char *)malloc(sizeof(char) * hash_len);
	ydb_mmrhash_128_result(&hash_to_match, 0, &res_hash);
	ydb_hash_to_string(&res_hash, hash_to_match_str, hash_len);
	// Get GroupBy expression list
	SqlSelectStatement *select;
	UNPACK_SQL_STATEMENT(select, table_alias->table, select);
	SqlStatement *group_by_expression = select->group_by_expression;
	if (!group_by_expression)
		return -1;
	// Form the hash string of GroupBy expression node and see if it matches hash string of stmt_to_match
	char *		    group_by_hash_str;
	int		    column_number = 1;
	SqlColumnList *	    cl;
	SqlColumnListAlias *cla;
	SqlColumnListAlias *cur_cla, *start_cla;
	hash128_state_t	    group_by_hash;
	UNPACK_SQL_STATEMENT(cla, group_by_expression, column_list_alias);
	cur_cla = start_cla = cla;
	HASH128_STATE_INIT(group_by_hash, 0);
	do {
		UNPACK_SQL_STATEMENT(cl, cur_cla->column_list, column_list);
		expression_switch_statement(GROUP_HASH, cl->value, &group_by_hash, NULL, is_inner_expression, NULL);
		// group_by_hash -> group_by_hash_str
		group_by_hash_str = (char *)malloc(sizeof(char) * hash_len);
		ydb_mmrhash_128_result(&group_by_hash, 0, &res_hash);
		ydb_hash_to_string(&res_hash, group_by_hash_str, hash_len);
		// match the hash
		if (0 == strncmp(group_by_hash_str, hash_to_match_str, hash_len)) {
			// We have a match free malloc'd memory and return the node number
			free(group_by_hash_str);
			free(hash_to_match_str);
			return column_number;
		}
		// We don't have a match. Keep looking.
		free(group_by_hash_str);
		column_number++;
		cur_cla = cur_cla->next;
	} while (cur_cla != start_cla);
	// Couldn't find a matching GroupBy node indicate to caller by returning -1
	free(hash_to_match_str);
	return -1;
}

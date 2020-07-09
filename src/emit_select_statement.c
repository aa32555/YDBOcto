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

#include <libyottadb.h>

#include "octo.h"
#include "template_strings.h"
#include "logical_plan.h"
#include "physical_plan.h"
#include "helpers.h"

PSQL_TypeSize get_type_size_from_psql_type(PSQL_TypeOid type) {
	switch (type) {
	case PSQL_TypeOid_int4:
		return PSQL_TypeSize_int4;
		break;
	case PSQL_TypeOid_numeric:
		return PSQL_TypeSize_numeric;
		break;
	case PSQL_TypeOid_varchar:
		return PSQL_TypeSize_varchar;
		break;
	case PSQL_TypeOid_unknown:
		return PSQL_TypeSize_unknown;
		break;
	case PSQL_TypeOid_bool:
		return PSQL_TypeSize_bool;
		break;
	default:
		assert(FALSE);
		ERROR(ERR_UNKNOWN_KEYWORD_STATE, "");
		return PSQL_TypeSize_unknown;
		break;
	}
}

/**
 * Emits M code for retrieving values representing this SELECT statement
 *
 * @param cursor_exe_global an array of size 3
 * Returns a table describing the temporary table containing the resulting
 *  values
 */
PhysicalPlan *emit_select_statement(SqlStatement *stmt, char *plan_filename) {
	LPActionType	    set_oper_type;
	LogicalPlan *	    plan, *cur_plan, *column_alias;
	PhysicalPlan *	    pplan;
	SqlValue *	    value;
	char		    output_key[INT32_TO_STRING_MAX], valbuff[INT32_TO_STRING_MAX];
	int32_t		    output_key_id, status;
	int16_t		    num_columns = 0;
	ydb_buffer_t *	    plan_meta, value_buffer;
	SetOperType *	    set_oper;
	PhysicalPlanOptions options;
	PSQL_TypeOid	    column_type;
	PSQL_TypeSize	    type_size;

	TRACE(ERR_ENTERING_FUNCTION, "emit_select_statement");
	memset(output_key, 0, INT32_TO_STRING_MAX);

	assert(stmt && ((table_alias_STATEMENT == stmt->type) || (set_operation_STATEMENT == stmt->type)));
	plan = generate_logical_plan(stmt);
	if (NULL == plan) {
		return NULL;
	}
	lp_emit_plan(plan, "BEFORE optimize_logical_plan()");
	if (lp_verify_structure(plan, NULL) == FALSE) {
		ERROR(ERR_PLAN_NOT_WELL_FORMED, "");
		return NULL;
	}
	plan = optimize_logical_plan(plan);
	if (NULL == plan) {
		ERROR(ERR_FAILED_TO_OPTIMIZE_PLAN, "");
		return NULL;
	}
	lp_emit_plan(plan, "AFTER optimize_logical_plan()");
	memset(&options, 0, sizeof(PhysicalPlanOptions));
	pplan = NULL;
	options.last_plan = &pplan;
	pplan = generate_physical_plan(plan, &options);
	if (NULL == pplan) {
		ERROR(ERR_PLAN_NOT_GENERATED, "physical");
		return NULL;
	}
	assert(NULL != plan_filename);
	status = emit_physical_plan(pplan, plan_filename);
	if (YDB_OK != status)
		return NULL;

	set_oper = pplan->set_oper_list;
	set_oper_type = ((NULL == set_oper) ? LP_INVALID_ACTION : set_oper->set_oper_type);
	assert(((LP_INVALID_ACTION == set_oper_type) && !set_oper_type) || (LP_SET_UNION == set_oper_type)
	       || (LP_SET_UNION_ALL == set_oper_type) || (LP_SET_DNF == set_oper_type) || (LP_SET_EXCEPT == set_oper_type)
	       || (LP_SET_EXCEPT_ALL == set_oper_type) || (LP_SET_INTERSECT == set_oper_type)
	       || (LP_SET_INTERSECT_ALL == set_oper_type));
	output_key_id = (set_oper_type ? set_oper->output_id : pplan->outputKey->unique_id);
	// Prepare metadata buffers
	plan_meta = make_buffers(config->global_names.octo, 5, OCTOLIT_PLAN_METADATA, plan_filename, OCTOLIT_OUTPUT_KEY, "", "");
	YDB_MALLOC_BUFFER(&value_buffer, INT32_TO_STRING_MAX);
	OCTO_INT32_TO_BUFFER(output_key_id, &value_buffer);
	/* Kill any prior data for this given plan (in the rare case it exists).
	 * This will fix any out-of-sync situation between the plan and corresponding db nodes.
	 */
	status = ydb_delete_s(&plan_meta[0], 2, &plan_meta[1], YDB_DEL_TREE);
	/* Ignore any non-zero return value from "ydb_delete_s". Just proceed with "ydb_set_s". */
	UNUSED(status); /* UNUSED macro needed to avoid unused-variable warning from clang-analyzer */
	// Store output key for the given plan
	status = ydb_set_s(&plan_meta[0], 3, &plan_meta[1], &value_buffer);
	YDB_FREE_BUFFER(&value_buffer);
	if (YDB_OK != status) {
		free(plan_meta);
		return NULL;
	}
	YDB_LITERAL_TO_BUFFER(OCTOLIT_OUTPUT_COLUMNS, &plan_meta[3]);
	YDB_MALLOC_BUFFER(&plan_meta[4], INT16_TO_STRING_MAX); // Column ID

	// Note down column data types
	cur_plan = pplan->projection;
	do {
		assert(cur_plan->type == LP_COLUMN_LIST);
		GET_LP(column_alias, cur_plan, 0, LP_WHERE);
		assert(NULL != column_alias->v.lp_default.operand[1]);
		GET_LP(column_alias, column_alias, 1, LP_COLUMN_LIST_ALIAS);
		UNPACK_SQL_STATEMENT(value, column_alias->v.lp_column_list_alias.column_list_alias->alias, value);
		// This assumes the SqlValue will outlive this RowDescription
		num_columns++;
		OCTO_INT16_TO_BUFFER(num_columns, &plan_meta[4]);

		YDB_LITERAL_TO_BUFFER("name", &plan_meta[5]);
		value_buffer.buf_addr = value->v.string_literal;
		value_buffer.len_used = value_buffer.len_alloc = strlen(value_buffer.buf_addr);
		status = ydb_set_s(plan_meta, 5, &plan_meta[1], &value_buffer);
		YDB_ERROR_CHECK(status);
		if (YDB_OK != status)
			break;

		YDB_LITERAL_TO_BUFFER("table_id", &plan_meta[5]);
		value_buffer.buf_addr = "0";
		value_buffer.len_used = value_buffer.len_alloc = strlen(value_buffer.buf_addr);
		status = ydb_set_s(plan_meta, 5, &plan_meta[1], &value_buffer);
		YDB_ERROR_CHECK(status);
		if (YDB_OK != status)
			break;

		YDB_LITERAL_TO_BUFFER("column_id", &plan_meta[5]);
		value_buffer.buf_addr = "0";
		value_buffer.len_used = value_buffer.len_alloc = strlen(value_buffer.buf_addr);
		status = ydb_set_s(plan_meta, 5, &plan_meta[1], &value_buffer);
		YDB_ERROR_CHECK(status);
		if (YDB_OK != status)
			break;

		YDB_LITERAL_TO_BUFFER("data_type", &plan_meta[5]);
		value_buffer.buf_addr = valbuff;
		value_buffer.len_alloc = INT32_TO_STRING_MAX;
		column_type = get_psql_type_from_sqlvaluetype(column_alias->v.lp_column_list_alias.column_list_alias->type);
		OCTO_INT32_TO_BUFFER(column_type, &value_buffer);
		status = ydb_set_s(plan_meta, 5, &plan_meta[1], &value_buffer);
		YDB_ERROR_CHECK(status);
		if (YDB_OK != status)
			break;

		YDB_LITERAL_TO_BUFFER("data_type_size", &plan_meta[5]);
		value_buffer.len_alloc = INT16_TO_STRING_MAX;
		type_size = get_type_size_from_psql_type(column_type);
		// PostgreSQL protocol specifies a 16-bit integer to store each data type size
		// Details linked in rocto/message_formats.h
		OCTO_INT16_TO_BUFFER((int16_t)type_size, &value_buffer);
		status = ydb_set_s(plan_meta, 5, &plan_meta[1], &value_buffer);
		YDB_ERROR_CHECK(status);
		if (YDB_OK != status)
			break;

		YDB_LITERAL_TO_BUFFER("type_modifier", &plan_meta[5]);
		value_buffer.buf_addr = "-1";
		value_buffer.len_used = value_buffer.len_alloc = strlen(value_buffer.buf_addr);
		status = ydb_set_s(plan_meta, 5, &plan_meta[1], &value_buffer);
		YDB_ERROR_CHECK(status);
		if (YDB_OK != status)
			break;

		YDB_LITERAL_TO_BUFFER("format_code", &plan_meta[5]);
		value_buffer.buf_addr = "0";
		value_buffer.len_used = value_buffer.len_alloc = strlen(value_buffer.buf_addr);
		status = ydb_set_s(plan_meta, 5, &plan_meta[1], &value_buffer);
		YDB_ERROR_CHECK(status);
		if (YDB_OK != status)
			break;
		cur_plan = cur_plan->v.lp_default.operand[1];
	} while (NULL != cur_plan);
	if (YDB_OK != status) {
		YDB_FREE_BUFFER(&plan_meta[4]);
		free(plan_meta);
		return NULL;
	}
	// Note down number of output columns for use in RowDescriptions
	value_buffer.buf_addr = valbuff;
	value_buffer.len_alloc = INT16_TO_STRING_MAX;
	OCTO_INT16_TO_BUFFER(num_columns, &value_buffer);
	status = ydb_set_s(plan_meta, 3, &plan_meta[1], &value_buffer);
	YDB_FREE_BUFFER(&plan_meta[4]);
	free(plan_meta);
	YDB_ERROR_CHECK(status);
	if (YDB_OK != status)
		return NULL;
	// Create a table from the last physical table which reads from the output values
	return pplan;
}

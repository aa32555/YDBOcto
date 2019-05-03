/* Copyright (C) 2018-2019 YottaDB, LLC
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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#include "octo.h"
#include "octo_types.h"

#include "logical_plan.h"
#include "physical_plan.h"

#include "template_helpers.h"

void add_sql_value_hash(EVP_MD_CTX *mdctx, SqlStatement *element) {
	// Helper function: adds string values of SqlValue or continues recursion for calculates values.
	if (element && element->type == STRING_LITERAL) {
		if(1 != EVP_DigestUpdate(mdctx, element->v.string_literal, strlen(element->v.string_literal))) {
			FATAL(ERR_LIBSSL_ERROR);
		}
	} else if (element && element->type == COLUMN_REFERENCE) {
		if(1 != EVP_DigestUpdate(mdctx, element->v.reference, strlen(element->v.reference))) {
			FATAL(ERR_LIBSSL_ERROR);
		}
	} else if (element && element->type == CALCULATED_VALUE) {
		hash_canonical_query(mdctx, element->v.calculated);
	}
}

void hash_canonical_query(EVP_MD_CTX *mdctx, SqlStatement *stmt) {
	// Prepares SqlStatement for hashing by adding all statement elements to mdctx using EVP_Digest_Update.
	// Assumes EVP_DigestInit, OPENSSL_malloc, and EVP_DigestFinal are performed by caller.

	// Statements
	SqlBeginStatement *begin;
	SqlCaseStatement *cas;
	SqlCaseBranchStatement *cas_branch;
	SqlCommitStatement *commit;
	SqlDropStatement *drop;
	SqlInsertStatement *insert;
	SqlNoDataStatement *no_data;
	SqlSelectStatement *select;
	SqlSetStatement *set;
	SqlShowStatement *show;

	SqlFunctionCall *function_call;
	SqlJoin *join;
	SqlValue *value;

	// Columns and tables
	SqlColumn *column; // Note singular versus plural
	SqlColumnAlias *column_alias;
	SqlColumnList *column_list;
	SqlColumnListAlias *column_list_alias;
	SqlTable *table;
	SqlTableAlias *table_alias;

	// Operations and keywords
	SqlBinaryOperation *binary;
	SqlSetOperation *set_operation;
	SqlUnaryOperation *unary;
	SqlOptionalKeyword *constraint;
	SqlOptionalKeyword *keyword;

	switch (stmt->type) {
		case begin_STATEMENT:
			break;
		case cas_STATEMENT:
			break;
		case cas_branch_STATEMENT:
			break;
		case commit_STATEMENT:
			break;
		case drop_STATEMENT:
			break;
		case insert_STATEMENT:
			break;
		case no_data_STATEMENT:
			break;
		case select_STATEMENT:
			select = stmt->v.select;
			// SqlColumnListAlias
			if (select->select_list) {
				hash_canonical_query(mdctx, select->select_list);
			}
			// SqlJoin
			if (select->table_list) {
				hash_canonical_query(mdctx, select->table_list);
			}
			// SqlValue (?)
			if (select->where_expression) {
				add_sql_value_hash(mdctx, select->where_expression);
			}
			// SqlValue (?)
			if (select->order_expression) {
				add_sql_value_hash(mdctx, select->order_expression);
			}
			// SqlOptionalKeyword
			if (select->optional_words) {
				hash_canonical_query(mdctx, select->optional_words);
			}
			// SqlSetOperation
			if (select->set_operation) {
				hash_canonical_query(mdctx, select->set_operation);
			}
			break;
		case set_STATEMENT:
			break;
		case show_STATEMENT:
			break;
		case function_call_STATEMENT:
			break;
		case join_STATEMENT:
			break;
		case value_STATEMENT:
			break;
		case column_STATEMENT:
			break;
		case column_alias_STATEMENT:
			break;
		case column_list_STATEMENT:
			break;
		case column_list_alias_STATEMENT:
			break;
		case table_STATEMENT:
			break;
		case table_alias_STATEMENT:
			break;
		case binary_STATEMENT:
			break;
		case set_operation_STATEMENT:
			break;
		case unary_STATEMENT:
			break;
		case constraint_STATEMENT:
			break;
		case keyword_STATEMENT:
			break;
		default:
			break;
	}

}

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

void add_sql_type_hash(EVP_MD_CTX *mdctx, int stmt_type) {
	// Helper function: adds statement type values to hash digest
	if(1 != EVP_DigestUpdate(mdctx, (char*)&stmt_type, sizeof(int))) {
		FATAL(ERR_LIBSSL_ERROR);
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

	// Iteration pointers for doubly-linked list traversal
	SqlCaseBranchStatement *cur_cas_branch;
	SqlColumn *cur_column;
	SqlColumnList *cur_column_list;
	SqlColumnListAlias *cur_column_list_alias;
	SqlJoin *cur_join;
	SqlOptionalKeyword *cur_keyword;
	SqlTable *cur_table;

	if(stmt == NULL)
		return;

	switch (stmt->type) {
		case begin_STATEMENT:
			add_sql_type_hash(mdctx, begin_STATEMENT);
			break;
		case cas_STATEMENT:
			UNPACK_SQL_STATEMENT(cas, stmt, cas);
			add_sql_type_hash(mdctx, cas_STATEMENT);
			// SqlValue
			hash_canonical_query(mdctx, cas->value);
			// SqlCaseBranchStatement
			hash_canonical_query(mdctx, cas->branches);
			// SqlValue
			hash_canonical_query(mdctx, cas->optional_else);
			break;
		case cas_branch_STATEMENT:
			UNPACK_SQL_STATEMENT(cas_branch, stmt, cas_branch);
			cur_cas_branch = cas_branch->next;
			do {
				add_sql_type_hash(mdctx, cas_branch_STATEMENT);
				// SqlValue
				hash_canonical_query(mdctx, cas_branch->condition);
				// SqlValue
				hash_canonical_query(mdctx, cas_branch->value);
				cur_cas_branch = cur_cas_branch->next;
			} while (cur_cas_branch != cas_branch->next);
			break;
		case commit_STATEMENT:
			add_sql_type_hash(mdctx, commit_STATEMENT);
			break;
		case drop_STATEMENT:
			UNPACK_SQL_STATEMENT(drop, stmt, drop);
			add_sql_type_hash(mdctx, drop_STATEMENT);
			// SqlValue
			hash_canonical_query(mdctx, drop->table_name);
			// SqlOptionalKeyword
			hash_canonical_query(mdctx, drop->optional_keyword);
			break;
		case insert_STATEMENT:
			assert(FALSE);
			/*
			UNPACK_SQL_STATEMENT(insert, stmt, insert);
			add_sql_type_hash(mdctx, insert_STATEMENT);
			// SqlTable
			hash_canonical_query(mdctx, insert->destination);
			hash_canonical_query(mdctx, insert->source);
			hash_canonical_query(mdctx, insert->columns);
			*/
			break;
		case no_data_STATEMENT:
			add_sql_type_hash(mdctx, no_data_STATEMENT);
			break;
		case select_STATEMENT:
			UNPACK_SQL_STATEMENT(select, stmt, select);
			add_sql_type_hash(mdctx, select_STATEMENT);
			// SqlColumnListAlias
			hash_canonical_query(mdctx, select->select_list);
			// SqlJoin
			hash_canonical_query(mdctx, select->table_list);
			// SqlValue (?)
			hash_canonical_query(mdctx, select->where_expression);
			// SqlValue (?)
			hash_canonical_query(mdctx, select->order_expression);
			// SqlOptionalKeyword
			hash_canonical_query(mdctx, select->optional_words);
			// SqlSetOperation
			hash_canonical_query(mdctx, select->set_operation);
			break;
		case set_STATEMENT:
			UNPACK_SQL_STATEMENT(set, stmt, set);
			add_sql_type_hash(mdctx, set_STATEMENT);
			hash_canonical_query(mdctx, set->variable);
			hash_canonical_query(mdctx, set->value);
			break;
		case show_STATEMENT:
			UNPACK_SQL_STATEMENT(show, stmt, show);
			add_sql_type_hash(mdctx, show_STATEMENT);
			hash_canonical_query(mdctx, show->variable);
			break;
		case function_call_STATEMENT:
			UNPACK_SQL_STATEMENT(function_call, stmt, function_call);
			add_sql_type_hash(mdctx, function_call_STATEMENT);
			// SqlValue
			hash_canonical_query(mdctx, function_call->function_name);
			// SqlColumnList
			hash_canonical_query(mdctx, function_call->parameters);
			break;
		case join_STATEMENT:
			UNPACK_SQL_STATEMENT(join, stmt, join);
			cur_join = join->next;
			do {
				add_sql_type_hash(mdctx, join_STATEMENT);
				// SqlTableAlias
				hash_canonical_query(mdctx, join->value);
				// SqlValue
				hash_canonical_query(mdctx, join->condition);
				cur_join = cur_join->next;
			} while (cur_join != join->next);
			// SqlJoinType
			add_sql_type_hash(mdctx, join->type);
			break;
		case value_STATEMENT:
			UNPACK_SQL_STATEMENT(value, stmt, value);
			add_sql_type_hash(mdctx, value_STATEMENT);
			// SqlValueType
			add_sql_type_hash(mdctx, value->type);
			// SqlDataType
			add_sql_type_hash(mdctx, value->data_type);
			if (value->type == STRING_LITERAL) {
				if(1 != EVP_DigestUpdate(mdctx, value->v.string_literal, strlen(value->v.string_literal))) {
					FATAL(ERR_LIBSSL_ERROR);
				}
			} else if (value->type == COLUMN_REFERENCE) {
				if(1 != EVP_DigestUpdate(mdctx, value->v.reference, strlen(value->v.reference))) {
					FATAL(ERR_LIBSSL_ERROR);
				}
			} else if (value->type == CALCULATED_VALUE) {
				hash_canonical_query(mdctx, value->v.calculated);
			}
			break;
		case column_STATEMENT:
			UNPACK_SQL_STATEMENT(column, stmt, column);
			cur_column = column;
			do {
				add_sql_type_hash(mdctx, column_STATEMENT);
				hash_canonical_query(mdctx, column->columnName);
				// SqlDataType
				add_sql_type_hash(mdctx, column->type);
				hash_canonical_query(mdctx, column->table);
				hash_canonical_query(mdctx, column->keywords);
				cur_column = cur_column->next;
			} while (cur_column != column);
			break;
		case column_alias_STATEMENT:
			UNPACK_SQL_STATEMENT(column_alias, stmt, column_alias);
			add_sql_type_hash(mdctx, column_alias_STATEMENT);
			// SqlColumn or SqlColumnListAlias
			// hash_canonical_query(mdctx, column_alias->column);
			// SqlTableAlias
			hash_canonical_query(mdctx, column_alias->table_alias);
			break;
		case column_list_STATEMENT:
			UNPACK_SQL_STATEMENT(column_list, stmt, column_list);
			cur_column_list = column_list;
			do {
				add_sql_type_hash(mdctx, column_list_STATEMENT);
				// SqlValue or SqlColumnAlias
				hash_canonical_query(mdctx, column_list->value);
				cur_column_list = cur_column_list->next;
			} while (cur_column_list != column_list);
			break;
		case column_list_alias_STATEMENT:
			UNPACK_SQL_STATEMENT(column_list_alias, stmt, column_list_alias);
			cur_column_list_alias = column_list_alias;
			do {
				add_sql_type_hash(mdctx, column_list_alias_STATEMENT);
				// SqlColumnList
				hash_canonical_query(mdctx, column_list_alias->column_list);
				// SqlValue
				hash_canonical_query(mdctx, column_list_alias->alias);
				// SqlOptionalKeyword
				hash_canonical_query(mdctx, column_list_alias->keywords);
				// SqlValueType
				add_sql_type_hash(mdctx, column_list_alias->type);
				cur_column_list_alias = cur_column_list_alias->next;
			} while (cur_column_list_alias != column_list_alias);
			break;
		case table_STATEMENT:
			UNPACK_SQL_STATEMENT(table, stmt, table);
			add_sql_type_hash(mdctx, table_STATEMENT);
			hash_canonical_query(mdctx, table->tableName);
			hash_canonical_query(mdctx, table->source);
			hash_canonical_query(mdctx, table->delim);
			break;
		case table_alias_STATEMENT:
			UNPACK_SQL_STATEMENT(table_alias, stmt, table_alias);
			add_sql_type_hash(mdctx, table_alias_STATEMENT);
			// SqlTable or SqlSelectStatement
			hash_canonical_query(mdctx, table_alias->table);
			// SqlValue
			hash_canonical_query(mdctx, table_alias->alias);
			// Since unique_id is an int, can use treat it as if it were a type enum
			add_sql_type_hash(mdctx, table_alias->unique_id);
			// SqlColumnListAlias
			// hash_canonical_query(mdctx, table_alias->column_list);
			break;
		case binary_STATEMENT:
			UNPACK_SQL_STATEMENT(binary, stmt, binary);
			add_sql_type_hash(mdctx, binary_STATEMENT);
			// BinaryOperations
			add_sql_type_hash(mdctx, binary->operation);
			// SqlStatement (?)
			hash_canonical_query(mdctx, binary->operands[0]);
			// SqlStatement (?)
			hash_canonical_query(mdctx, binary->operands[1]);
			break;
		case set_operation_STATEMENT:
			UNPACK_SQL_STATEMENT(set_operation, stmt, set_operation);
			add_sql_type_hash(mdctx, set_operation_STATEMENT);
			// SqlSetOperationType
			add_sql_type_hash(mdctx, set_operation->type);
			// SqlStatement (?)
			hash_canonical_query(mdctx, set_operation->operand[0]);
			// SqlStatement (?)
			hash_canonical_query(mdctx, set_operation->operand[1]);
			break;
		case unary_STATEMENT:
			UNPACK_SQL_STATEMENT(unary, stmt, unary);
			add_sql_type_hash(mdctx, unary_STATEMENT);
			// UnaryOperations
			add_sql_type_hash(mdctx, unary->operation);
			// SqlStatement (?)
			hash_canonical_query(mdctx, unary->operand);
			break;
		case constraint_STATEMENT:
			// NOT IMPLEMENTED
			// UNPACK_SQL_STATEMENT(constraint, stmt, constraint);
			// add_sql_type_hash(mdctx, constraint_STATEMENT);
			break;
		case keyword_STATEMENT:
			UNPACK_SQL_STATEMENT(keyword, stmt, keyword);
			cur_keyword = keyword;
			do {
				add_sql_type_hash(mdctx, keyword_STATEMENT);
				// OptionalKeyword
				add_sql_type_hash(mdctx, keyword->keyword);
				// SqlValue or SqlSelectStatement
				hash_canonical_query(mdctx, keyword->v);
				cur_keyword = cur_keyword->next;
			} while (cur_keyword != keyword);
			break;
		default:
			break;
	}

}

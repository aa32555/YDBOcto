/****************************************************************
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef OCTO_TYPES_H
#define OCTO_TYPES_H

#include <stdint.h>	/* needed for uint64_t */

typedef void *yyscan_t;

#include "memory_chunk.h"
#include "double_list.h"

// Allocates ONE structure of type TYPE
#define	OCTO_CMALLOC_STRUCT(RET, TYPE)								\
{												\
	RET = (TYPE *)octo_cmalloc(memory_chunks, sizeof(TYPE));				\
}

#define SQL_STATEMENT(VAR, TYPE)			      					\
{												\
	OCTO_CMALLOC_STRUCT(VAR, SqlStatement);							\
	(VAR)->type = TYPE;									\
}

#define MALLOC_STATEMENT(VAR, NAME, TYPE)							\
{												\
	OCTO_CMALLOC_STRUCT((VAR)->v.NAME, TYPE);						\
}

#define UNPACK_SQL_STATEMENT(result, item, StatementType)					\
{												\
	assert((item)->type == StatementType ## _STATEMENT);					\
	(result) = (item)->v.StatementType;							\
}

#define PACK_SQL_STATEMENT(out, item, StatementType)						\
{												\
	SQL_STATEMENT(out, StatementType ## _STATEMENT);					\
	(out)->v.StatementType = item;								\
}

#define SHALLOW_COPY_SQL_STATEMENT(dst, src, NAME, TYPE) do {					\
	SQL_STATEMENT((dst), src->type);							\
	MALLOC_STATEMENT((dst), NAME, TYPE);							\
	*(dst)->v.NAME = *(src)->v.NAME;							\
} while(0);

#define SAFE_SNPRINTF(written, buff_ptr, buffer, buffer_size, ...) do {					\
	(written) = snprintf((buff_ptr), (buffer_size) - ((buff_ptr) - (buffer)), ## __VA_ARGS__);	\
	if((written) < ((buffer_size) - ((buff_ptr) - (buffer)))) {					\
		buff_ptr += written;									\
	}												\
} while (FALSE);

/* Determines the corresponding (SqlStatement *) structures that points to a (SqlTable *) structure */
#define	SQL_STATEMENT_FROM_TABLE_STATEMENT(RET, TABLE)			\
{									\
	RET = (SqlStatement *)((char *)TABLE - sizeof(SqlStatement));	\
	assert(table_STATEMENT == RET->type);				\
	assert(RET->v.table == TABLE);					\
}

/* Shamelessly stolen from mlkdef.h in YottaDB */
/* convert relative pointer to absolute pointer */
#define R2A(X) (void*)(((unsigned char*) &(X)) + ((size_t)X))

/* store absolute pointer Y in X as a relative pointer */
#define A2R(X, Y) ((X) = (void*)(((unsigned char*)(Y)) - ((unsigned char*) &(X))))


typedef long long unsigned int uint8;

typedef int boolean_t;

typedef enum FileType {
	CrossReference,
	OutputPlan,
	YDBTrigger,
} FileType;

typedef enum SqlStatementType {
	table_STATEMENT,
	select_STATEMENT,
	insert_STATEMENT,
	drop_STATEMENT,
	value_STATEMENT,
	function_call_STATEMENT,
	binary_STATEMENT,
	unary_STATEMENT,
	column_list_STATEMENT,
	column_STATEMENT,
	join_STATEMENT,
	data_type_STATEMENT,
	constraint_STATEMENT,
	constraint_type_STATEMENT,
	keyword_STATEMENT,
	column_list_alias_STATEMENT,
	column_alias_STATEMENT,
	table_alias_STATEMENT,
	join_type_STATEMENT,
	set_operation_STATEMENT,
	begin_STATEMENT,
	commit_STATEMENT,
	cas_STATEMENT,
	cas_branch_STATEMENT,
	set_STATEMENT,
	show_STATEMENT,
	no_data_STATEMENT,
	sort_spec_list_STATEMENT,
	invalid_STATEMENT
} SqlStatementType;

// The order of these must be kept in sync with `LPActionType` in `src/optimization_transforms/lp_action_type.hd`
typedef enum UnaryOperations {
	FORCE_NUM,
	NEGATIVE,
	BOOLEAN_NOT,
	BOOLEAN_EXISTS,
	BOOLEAN_NOT_EXISTS	// Not used but needed to be in sync with LP_BOOLEAN_NOT_EXISTS in `lp_action_type.hd`
} UnaryOperations;

// The order of these must be kept in sync with `LPActionType` in `src/optimization_transforms/lp_action_type.hd`
typedef enum BinaryOperations {
	ADDITION,
	SUBTRACTION,
	DIVISION,
	MULTIPLICATION,
	MODULO,
	CONCAT,
	BOOLEAN_OR,
	BOOLEAN_AND,
	BOOLEAN_IS,
	BOOLEAN_EQUALS,
	BOOLEAN_NOT_EQUALS,
	BOOLEAN_LESS_THAN,
	BOOLEAN_GREATER_THAN,
	BOOLEAN_LESS_THAN_OR_EQUALS,
	BOOLEAN_GREATER_THAN_OR_EQUALS,
	BOOLEAN_REGEX_SENSITIVE,
	BOOLEAN_REGEX_INSENSITIVE,
	BOOLEAN_IN,
	BOOLEAN_NOT_IN,
	BOOLEAN_NULL,
	BOOLEAN_NOT_NULL,
} BinaryOperations;

typedef enum SqlValueType {
	UNKNOWN_SqlValueType,
	NUMERIC_LITERAL,
	INTEGER_LITERAL,
	STRING_LITERAL,
	COLUMN_REFERENCE,
	CALCULATED_VALUE,
	FUNCTION_NAME,
	BOOLEAN_VALUE,
	PARAMETER_VALUE,
	NUL_VALUE,
	COERCE_TYPE,
	INVALID_SqlValueType
} SqlValueType;

typedef enum SqlDataType {
	UNKNOWN_SqlDataType,
	CHARACTER_STRING_TYPE,
	NUMERIC_TYPE,
	INTEGER_TYPE
} SqlDataType;

/* Note: Additions of keywords in the middle of the table can cause SIG-11s because the actual binary value
 *       of these enums (e.g. PRIMARY_KEY) is stored in the ^%ydboctoschema(<tablename>,"b",*) global nodes
 *       and using a newer build of Octo without killing ^%ydboctoschema could load a table definition that
 *       is out of date with respect to the newer build.
 * Note: Any additions/deletions to this list might need to be correspondingly changed in "lp_emit_plan.c".
 */
typedef enum OptionalKeyword {
	NO_KEYWORD,
	PRIMARY_KEY,
	NOT_NULL,
	UNIQUE_CONSTRAINT,
	OPTIONAL_SOURCE,
	OPTIONAL_END,
	OPTIONAL_START,
	OPTIONAL_DELIM,
	OPTIONAL_EXTRACT,
	OPTIONAL_CASCADE,
	OPTIONAL_RESTRICT,
	OPTIONAL_PIECE,
	OPTIONAL_KEY_NUM,
	OPTIONAL_ADVANCE,
	OPTIONAL_LIMIT,
	OPTIONAL_DISTINCT,
	OPTIONAL_XREF_INDEX,		// not sure if this should be here; gets populated through LP
	OPTIONAL_BOOLEAN_EXPANSION,	// indicates that this statement is part of an OR boolean expression expansion to BNF form
	OPTIONAL_ASC,
	OPTIONAL_DESC,
} OptionalKeyword;

typedef enum SqlSetOperationType {
	SET_UNION,
	SET_UNION_ALL,
	SET_EXCEPT,
	SET_EXCEPT_ALL,
	SET_INTERSECT,
	SET_INTERSECT_ALL
} SqlSetOperationType;

typedef enum SqlJoinType {
	NO_JOIN,
	TABLE_SPEC,
	CROSS_JOIN,
	INNER_JOIN,
	RIGHT_JOIN,
	LEFT_JOIN,
	FULL_JOIN,
	NATURAL_JOIN
} SqlJoinType;

typedef enum SqlSortType {
	ASC_TYPE,
	DESC_TYPE
} SqlSortType;

#define YYLTYPE yyltype

typedef struct YYLTYPE YYLTYPE;
struct YYLTYPE
{
	int first_line;
	int first_column;
	int last_line;
	int last_column;
};

struct SqlColumn;
struct SqlColumnAlias;
//struct SqlConstraint;
struct SqlSelectStatement;
struct SqlInsertStatement;
struct SqlDropStatement;
struct SqlUnaryOperation;
struct SqlBinaryOperation;
struct SqlFunctionCall;
struct SqlValue;
struct SqlColumnList;
struct SqlTable;
struct SqlTableAlias;
struct SqlJoin;
struct SqlColumnListAlias;
struct SqlStatement;
struct SqlSetOperation;
struct SqlBeginStatement;
struct SqlCommitStatement;
struct SqlCaseStatement;
struct SqlCaseBranchStatement;
struct SqlSetStatement;
struct SqlShowStatement;
struct SqlNoDataStatement;

/**
 * Represents a SQL column; doubly linked list
 *
 * WARNING: in some cases, SqlColumnList is used instead of the linked list, namely when
 *  we are dealing with a SELECT column list because the column may be a calculated column
 */
typedef struct SqlColumn
{
	struct SqlStatement		*columnName;
	enum SqlDataType		type;
	int				column_number;
	struct SqlStatement		*table;
	struct SqlStatement		*keywords;
	struct SqlColumnListAlias	*pre_qualified_cla; /* initialized/usable only if "type" field is UNKNOWN_SqlDataType */
	dqcreate(SqlColumn);
} SqlColumn;

typedef struct SqlColumnAlias
{
	// SqlColumn or SqlColumnListAlias
	struct SqlStatement *column;
	// SqlTableAlias
	struct SqlStatement *table_alias;
} SqlColumnAlias;

/**
 * Represents a SQL table
 */
typedef struct SqlTable
{
	struct SqlStatement *tableName;
	struct SqlStatement *source;
	struct SqlStatement *columns;
	struct SqlStatement *delim;
} SqlTable;

typedef struct SqlTableAlias
{
	// SqlTable or SqlSelectStatement
	struct SqlStatement *table;
	// SqlValue
	struct SqlStatement *alias;
	int unique_id;
	// SqlColumnListAlias list of available columns
	struct SqlStatement *column_list;
} SqlTableAlias;

/**
 * Represents an optional KEYWORD which has a value associated with it */
typedef struct SqlOptionalKeyword
{
	enum OptionalKeyword keyword;
	// Keyword value (SqlValue) or UNION statement (SqlSelectStatement)
	struct SqlStatement *v;
	dqcreate(SqlOptionalKeyword);
} SqlOptionalKeyword;

/**
 * Effectively provides a list of tables that may or may not be joined
 */
typedef struct SqlJoin
{
	// SqlTableAlias
	//  -> was SqlTable, should be changed everywhere
	struct SqlStatement *value;
	// SqlValue
	struct SqlStatement *condition;
	dqcreate(SqlJoin);
	enum SqlJoinType type;
} SqlJoin;

/**
 * Represents a SQL SELECT statement
 */
typedef struct SqlSelectStatement
{
	// SqlColumnListAlias
	struct SqlStatement	*select_list;
	// SqlJoin
	struct SqlStatement	*table_list;
	// SqlValue (?)
	struct SqlStatement	*where_expression;
	// SqlValue (?)
	struct SqlStatement	*order_expression;
	// SqlOptionalKeyword
	struct SqlStatement	*optional_words;
} SqlSelectStatement;

typedef struct SqlInsertStatement
{
	SqlTable *destination;
	struct SqlStatement *source;
	struct SqlStatement *columns;
} SqlInsertStatement;

typedef struct SqlDropStatement
{
	// SqlValue
	struct SqlStatement *table_name;
	// SqlOptionalKeyword
	struct SqlStatement *optional_keyword;
} SqlDropStatement;

/*
 * Represents an binary operation
 */
typedef struct SqlUnaryOperation
{
	enum UnaryOperations operation; // '+', '-'
	struct SqlStatement *operand;
} SqlUnaryOperation;

/*
 * Represents an arithmetic operation
 */
typedef struct SqlBinaryOperation
{
	enum BinaryOperations operation; // '+', '-', '*', '/'
	struct SqlStatement *operands[2];
} SqlBinaryOperation;

typedef struct SqlFunctionCall {
	// SqlValue
	struct SqlStatement *function_name;
	// SqlColumnList
	struct SqlStatement *parameters;
} SqlFunctionCall;

typedef struct SqlValue {
	enum SqlValueType	type;
	enum SqlValueType	coerced_type;
	enum SqlDataType	data_type;
	union {
		char *string_literal;
		char *reference;
		// SqlBinaryOperation, SqlUnaryOperation, SqlFunctionCall
		struct SqlStatement *calculated;
		// Target to coerce; SqlValue, SqlColumnAlias
		struct SqlStatement *coerce_target;
	} v;
} SqlValue;

/**
 * Used to represent a SELECT column list, not a table column list
 */
typedef struct SqlColumnList {
	// SqlValue or SqlColumnAlias
	struct SqlStatement *value;
	dqcreate(SqlColumnList);
} SqlColumnList;

typedef struct SqlColumnListAlias {
	// SqlColumnList
	struct SqlStatement		*column_list;
	// SqlValue
	struct SqlStatement		*alias;
	// Keywords used for the SORT column
	struct SqlStatement		*keywords;
	SqlValueType			type;
	struct SqlColumnListAlias	*pre_qualified_cla; /* initialized/usable only if "type" field is UNKNOWN_SqlValueType */
	dqcreate(SqlColumnListAlias);
} SqlColumnListAlias;

/*
 * A SQL set operation, such as UNION, EXCEPT, or INTERSECT
 *
 * This is separated from a binary operation because, at this time, I believe it
 *  to be not useable as part of an expression; if this proves to be wrong,
 *  we should consider merging it
 */
typedef struct SqlSetOperation {
	SqlSetOperationType type;
	struct SqlStatement *operand[2];
} SqlSetOperation;

typedef struct SqlBeginStatement {
	// Filler so compiler doesn't complain about empty type;
	// when we add something to this struct, simply replace this filler
	char b;
} SqlBeginStatement;

typedef struct SqlCommitStatement {
	// Filler so compiler doesn't complain about empty type;
	// when we add something to this struct, simply replace this filler
	char b;
} SqlCommitStatement;

typedef struct SqlCaseStatement {
	// SqlValue
	struct SqlStatement *value;
	// SqlCaseBranchStatement
	struct SqlStatement *branches;
	// SqlValue
	struct SqlStatement *optional_else;
} SqlCaseStatement;

typedef struct SqlCaseBranchStatement {
	// SqlValue
	struct SqlStatement *condition;
	// SqlValue
	struct SqlStatement *value;
	dqcreate(SqlCaseBranchStatement);
} SqlCaseBranchStatement;

typedef struct SqlSetStatement {
	struct SqlStatement *variable;
	struct SqlStatement *value;
} SqlSetStatement;

typedef struct SqlShowStatement {
	struct SqlStatement *variable;
} SqlShowStatement;

typedef struct SqlNoDataStatement {
	// Filler so compiler doesn't complain about empty type;
	// when we add something to this struct, simply replace this filler
	char b;
} SqlNoDataStatement;

typedef struct SqlSortSpecList {
	// SqlValue
	struct SqlStatement	*column_value;
	struct SqlStatement	*sort_type;
	dqcreate(SqlSortSpecList);
} SqlSortSpecList;

typedef struct SqlStatement{
	enum SqlStatementType	type;
	struct YYLTYPE loc;
	union {
		struct SqlBeginStatement *begin;
		struct SqlCommitStatement *commit;
		struct SqlSelectStatement *select;
		struct SqlInsertStatement *insert;
		struct SqlDropStatement *drop;
		struct SqlValue *value;
		struct SqlFunctionCall *function_call;
		struct SqlBinaryOperation *binary;
		struct SqlUnaryOperation *unary;
		struct SqlColumnList *column_list;
		struct SqlColumn *column; // Note singular versus plural
		struct SqlJoin *join;
		struct SqlTable *table;
		struct SqlOptionalKeyword *constraint;
		struct SqlOptionalKeyword *keyword;
		struct SqlColumnListAlias *column_list_alias;
		struct SqlColumnAlias *column_alias;
		struct SqlTableAlias *table_alias;
		struct SqlSetOperation *set_operation;
		struct SqlCaseStatement *cas;
		struct SqlCaseBranchStatement *cas_branch;
		struct SqlSetStatement *set;
		struct SqlShowStatement *show;
		struct SqlNoDataStatement *no_data;
		struct SqlSortSpecList *sort_spec_list;
		enum SqlDataType data_type;
		enum SqlJoinType join_type;
	} v;
	uint64_t		hash_canonical_query_cycle;	// used during "hash_canonical_query" to avoid
								// multiple traversals of same node.
} SqlStatement;

/// TODO: this should be located in octo.h, but for some reason it's not being detected there
SqlStatement *natural_join_condition(SqlStatement *left, SqlStatement *right);
#endif

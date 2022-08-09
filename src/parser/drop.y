/****************************************************************
 *								*
 * Copyright (c) 2019-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

drop_table_statement
  : DROP TABLE column_name drop_behavior drop_data_retention {
      INVOKE_DROP_TABLE_STATEMENT($$, $column_name, $drop_behavior, $drop_data_retention, FALSE);
    }
  | DROP TABLE IF EXISTS column_name drop_behavior drop_data_retention {
      INVOKE_DROP_TABLE_STATEMENT($$, $column_name, $drop_behavior, $drop_data_retention, TRUE);
    }
  ;

drop_function_statement
  : DROP FUNCTION identifier optional_function_parameter_type_list {
	INVOKE_DROP_FUNCTION($$, $identifier, $optional_function_parameter_type_list, FALSE);
    }
  | DROP FUNCTION sql_keyword optional_function_parameter_type_list {
	INVOKE_DROP_FUNCTION($$, $sql_keyword, $optional_function_parameter_type_list, FALSE);
    }
  | DROP FUNCTION PARENLESS_FUNCTION optional_function_parameter_type_list {
	INVOKE_DROP_FUNCTION($$, $PARENLESS_FUNCTION, $optional_function_parameter_type_list, FALSE);
    }
  | DROP FUNCTION IF EXISTS identifier optional_function_parameter_type_list {
	INVOKE_DROP_FUNCTION($$, $identifier, $optional_function_parameter_type_list, TRUE);
    }
  | DROP FUNCTION IF EXISTS sql_keyword optional_function_parameter_type_list {
	INVOKE_DROP_FUNCTION($$, $sql_keyword, $optional_function_parameter_type_list, TRUE);
    }
  | DROP FUNCTION IF EXISTS PARENLESS_FUNCTION optional_function_parameter_type_list {
	INVOKE_DROP_FUNCTION($$, $PARENLESS_FUNCTION, $optional_function_parameter_type_list, TRUE);
    }
  ;

optional_function_parameter_type_list
  : /* Empty */ { $$ = NULL; }
  | LEFT_PAREN function_parameter_type_list RIGHT_PAREN {
      $$ = $function_parameter_type_list;
    }
  ;

drop_behavior
  : /* Empty */ { $$ = NULL; }
  | CASCADE {
      INVOKE_DROP_BEHAVIOR($$, OPTIONAL_CASCADE);
    }
  | RESTRICT {
      INVOKE_DROP_BEHAVIOR($$, OPTIONAL_RESTRICT);
    }
  ;

/* Note: C code using this element should cast it back to type `enum OptionalKeyword` */
drop_data_retention
  : /* Empty */ {
      $$ = (SqlStatement *)NO_KEYWORD;
  }
  | KEEPDATA {
      $$ = (SqlStatement *)OPTIONAL_KEEPDATA;
    }
  ;

#################################################################
#								#
# Copyright (c) 2023-2024 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

-- TFQ01 : Test various queries generated by fuzz testing

-------------------------------------------------------------------------------
-- Test https://gitlab.com/YottaDB/DBMS/YDBOcto/-/issues/803#note_1388510181
-------------------------------------------------------------------------------
-- This used to previously SIG-11.
-- Expected output is an ERR_SELECT_STAR_NO_TABLES error.
select (select *) between 1 and 2;

-------------------------------------------------------------------------------
-- Test https://gitlab.com/YottaDB/DBMS/YDBOcto/-/issues/803#note_1388513250
-------------------------------------------------------------------------------
-- This used to previously assert fail as follows.
-- 	src/populate_data_type.c:1165: populate_data_type: Assertion '(select_STATEMENT != table_alias->table->type) || (table_alias->table->v.select->select_list == table_alias->column_list)' failed.
-- Expected output for below query is `1` (aka `t` for TRUE).
select (select 1) between 1 and 2;
select (select 1 is null) between false and true;

-------------------------------------------------------------------------------
-- Test https://gitlab.com/YottaDB/DBMS/YDBOcto/-/issues/803#note_1492162026
-------------------------------------------------------------------------------

-- This used to previously SIG-11.
-- Expected output is a "syntax error".
create table tmp (firstname varchar, fullname1 varchar extract concat(firstname,));

-- This used to previously Assert fail at get_key_columns.c:47
-- Expected output is an ERR_TOO_MANY_TABLE_KEYCOLS error
create table tmp (id integer key num 1000);

-- Test HUGE positive number that in turn is treated as a negative number when atoi() is done.
-- Expected output is an ERR_TOO_MANY_TABLE_KEYCOLS error
create table tmp (id1 integer key num 48901234567890 key num 1);

-- This used to previously SIG-11 in generate_physical_plan.c
-- Expected output is an ERR_TOO_MANY_SELECT_KEYCOLS error
create table c ( id0 INTEGER, id1 INTEGER, id2 INTEGER, id3 INTEGER, id4 INTEGER, id5 INTEGER, id6 INTEGER, id7 INTEGER, name VARCHAR(20), PRIMARY KEY (id0, id1, id2, id3, id4, id5, id6, id7));
select 1 from  c c1, c c2, c c3, c c4, c c5, c c6, c c7, c c8, c c9, c c10, c c11, c c12, c c13, c c14, c c15, c c16, c c17, c c18, c c19, c c20, c c21, c c22, c c23, c c24, c c25, c c26, c c27, c c28, c c29, c c30, c c31, c c32, c c33;
drop table c;

-- This used to previously SIG-11 in parse_literal_to_parameter.c:33
-- Expected output is no error in the "create table" or when inserting 'abc%' but
-- an ERR_CHECK_CONSTRAINT_VIOLATION error when inserting 'abcd'.
create table tmp (lastname varchar, check((lastname like 'abc\%')));
insert into tmp values ('abc%');
insert into tmp values ('abcd');
drop table tmp;

-----------------------------------------------------------------------------------------
-- Test of binary operations evaluating to NULL in IN operand list along with TABLENAME.ASTERISK
-- Test of (4) in https://gitlab.com/YottaDB/DBMS/YDBOcto/-/issues/803#note_1492162026
-- This used to previously SIG-11 in "validate_table_asterisk_binary_operation()"
-- Expected output is no error in any of the below queries.
-- Note: These queries are not run through crosscheck as Postgres issues an operator not unique error.
-----------------------------------------------------------------------------------------
select n1.* in (NULL % NULL , n1.*) from names n1;

-- Test of similar queries like above.
select n1.* in (NULL, n1.*) from names n1;
select n1.* in (NULL - NULL , n1.*) from names n1;
select n1.* in (NULL + NULL , n1.*) from names n1;
select n1.* in (NULL * NULL , n1.*) from names n1;

-------------------------------------------------------------------------------
-- Test CASE inside a subquery used as a binary operand in outer query
-- This used to previously SIG-11 in lp_is_operand_type_string.c.
-- Expected output is no error.
-------------------------------------------------------------------------------
-- Test https://gitlab.com/YottaDB/DBMS/YDBOcto/-/issues/803#note_1495138200
select (case when id > (select 1) then (select 1) else (select 0) end)=1 as idbool from names;

-- Test https://gitlab.com/YottaDB/DBMS/YDBOcto/-/issues/803#note_1495376878
select (case when true then 1 end) = 1;

-- Test "Issue 1" in https://gitlab.com/YottaDB/DBMS/YDBOcto/-/issues/1040 issue description
select 1 group by 92729326636806986;

-- Test https://gitlab.com/YottaDB/DBMS/YDBOcto/-/issues/1040#note_1890253295
select 1 group by 17777000000000010101;
\d;

-- Test "Issue 2" in https://gitlab.com/YottaDB/DBMS/YDBOcto/-/issues/1040 issue description
create table tmp (id integer primary key, dob date EXTRACT "$GET(^datetimedateys(""il"")))") global "^date(keys(""id"")))"   global "^datetimedate";
drop table tmp;
-- Test https://gitlab.com/YottaDB/DBMS/YDBOcto/-/issues/1040#note_1894418515
create table tmp (id integer) global "^date(1)" global "^date2";
drop table tmp;

-- Test https://gitlab.com/YottaDB/DBMS/YDBOcto/-/issues/1040#note_1897959123
select 1 where (select 't');
-- Test https://gitlab.com/YottaDB/DBMS/YDBOcto/-/issues/1040#note_1898075506
delete from names where (select 't');
-- Test https://gitlab.com/YottaDB/DBMS/YDBOcto/-/issues/1040#note_1898221014
update names set firstname = 'abcd' where (select 't');
select 1 from names group by firstname having (select 't');
select 1 from names inner join names n2 on (select 't');


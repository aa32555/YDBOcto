#################################################################
#								#
# Copyright (c) 2024 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

CREATE TABLE testadd (id INTEGER PRIMARY KEY, dob TIMESTAMP EXTRACT date_add(date'01-01-2023',interval'1 year')) global "^test(keys(""id""))";
CREATE TABLE testsub (id INTEGER PRIMARY KEY, dob TIMESTAMP EXTRACT date_sub(date'01-01-2023',interval'1 year')) global "^test(keys(""id""))";
select * from testadd;
select * from testsub;
create table test (id integer primary key, dob date check((date'01-01-2023' + interval'1 year')>dob)) global "^test(keys(""id""))";
create table test1 (id integer primary key, dob date check((date'01-01-2023' + interval'1 year')>dob));
insert into test1 values(1,date'01-01-2024');
insert into test1 values(1,date'01-01-2022');
select * from test1;
create view v1 as select date '01-01-2023'+interval '1 year';
create view v2 as select date'01-01-2023'- interval '1 year';
create view v3 as select date_add(date'01-01-2023',interval '1 year');
create view v4 as select date_sub(date'01-01-2023',interval '1 year');
select * from v1;
select * from v2;
select * from v3;
select * from v4;
select date'01-01-2023' BETWEEN date'01-01-2023'+interval'1'year AND date'01-01-2023'-interval'1'year;
select date'01-01-2023' BETWEEN date'01-01-2023'-interval'1'year AND date'01-01-2023'+interval'1'year;


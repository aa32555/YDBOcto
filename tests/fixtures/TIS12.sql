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

select 1 from values(interval '1');
select 1 union select interval '1';
select * from (select 1 union select interval '1');
select exists (select interval'1');
select interval'1'+'hello';
select interval'1'+1.1;
select interval'1'+TRUE;
select interval'1'+NULL;
select 'hello'+interval'1';
select 1.1+interval'1';
select TRUE+interval'1';
select NULL+interval'1';

select interval'1'-'hello';
select interval'1'-1.1;
select interval'1'-TRUE;
select interval'1'-NULL;
select 'hello'-interval'1';
select 1.1-interval'1';
select TRUE-interval'1';
select NULL-interval'1';

select interval'1'*'hello';
select interval'1'*1.1;
select interval'1'*TRUE;
select interval'1'*NULL;
select 'hello'*interval'1';
select 1.1*interval'1';
select TRUE*interval'1';
select NULL*interval'1';

select interval'1'/'hello';
select interval'1'/1.1;
select interval'1'/TRUE;
select interval'1'/NULL;
select 'hello'/interval'1';
select 1.1/interval'1';
select TRUE/interval'1';
select NULL/interval'1';

select interval'1'%'hello';
select interval'1'%1.1;
select interval'1'%TRUE;
select interval'1'%NULL;
select 'hello'%interval'1';
select 1.1%interval'1';
select TRUE%interval'1';
select NULL%interval'1';

select 'hello'::interval;

select CAST('hello' AS INTERVAL);

select ARRAY(select interval'1');
select ARRAY(values(interval'1'));

select interval'1' LIKE 'hello';
select interval'1' SIMILAR TO 'hello';
select interval'1' ~~ 'hello';
select interval'1' !~~ TO 'hello';
select 'hello' LIKE interval'1';
select 'hello' SIMILAR TO interval'1';
select 'hello' !~~ interval'1';
select 'hello' ~~ interval'1';

select interval '1' AND TRUE;
select TRUE AND interval '1';
select interval '1' AND FALSE;
select FALSE AND interval '1';

select 1 between 2 and interval '1';
select interval '1' between interval '1' and interval'3';
select interval'1' between '1' AND '2';

select interval'1'=ANY(select interval'1');
select '1'=ANY(select interval'1');
select interval'1'=ALL(select interval'1');
select '1'=ALL(select interval'1');
select interval'1'=SOME(select interval'1');
select '1'=SOME(select interval'1');

select case interval'1' when interval'1' then 1 END;
select case when interval'1'=interval'1' then 1 END;
select case 1 when 1 then interval'1' END;
select case 1 when 2 then 1 else interval'1' END;

select NOT NOT interval'1';
select interval '1' IS NULL;
select interval '1' IS NOT NULL;

select interval'1' IN (select 1);
select 1 in (interval'1',interval'2');
select 1 in (select interval'1');

select interval'1' not IN (1,2);
select interval'1' not IN (select 1);
select 1 not in (interval'1',interval'2');
select 1 not in (select interval'1');

select coalesce(interval'1');
select greatest(interval'1');
select least(interval'1');
select null_if(interval'1',interval'2');
select null_if(inteval'1',NULL);
select null_if(NULL, inteval'1');

select abs(interval'1');
select distinct interval '1';

create table test (id integer, dob interval);
create table test (id integer, dob interval day to second);
create function test_func(interval) returns integer as $$testf^test;
create function test_func(integer) returns interval as $$testf^test;
create table test (id integer, dob interval day to second);
create function test_func(interval day to second) returns integer as $$testf^test;
create function test_func(integer) returns interval day to second as $$testf^test;

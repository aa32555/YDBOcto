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

select date'01-01-2023' + interval '1--1';
select date'01-01-2023' + interval '1--1'year;
select date'01-01-2023' + interval '1--1'year to month;
select date'01-01-2023' + interval '1--1'day to second;
select date'01-01-2023' + interval '1--1'day to minute;
select date'01-01-2023' + interval '1--1'day to hour;
select date'01-01-2023' + interval '1--1'day;
select date'01-01-2023' + interval '1--1'month;
select date'01-01-2023' + interval '1--1'hour;
select date'01-01-2023' + interval '1--1'second;
select date'01-01-2023' + interval '1--1'minute;
select date'01-01-2023' + interval '1:-1';
select date'01-01-2023' + interval '1:-1'day to second;
select date'01-01-2023' + interval '1:-1'day to minute;
select date'01-01-2023' + interval '1:-1'day to hour;
select date'01-01-2023' + interval '1:-1'hour;
select date'01-01-2023' + interval '1:-1'second;
select date'01-01-2023' + interval '1:-1'minute;

select date'01-01-2023' + interval '1:1:-1';
select date'01-01-2023' + interval '1:1:-1'day to second;
select date'01-01-2023' + interval '1:1:-1'day to minute;
select date'01-01-2023' + interval '1:1:-1'day to hour;
select date'01-01-2023' + interval '1:1:-1'hour;
select date'01-01-2023' + interval '1:1:-1'second;
select date'01-01-2023' + interval '1:1:-1'minute;

select date'01-01-2023' + interval '1:1:-1.1';
select date'01-01-2023' + interval '1:1:-1.1'year;
select date'01-01-2023' + interval '1:1:-1.1'year to month;
select date'01-01-2023' + interval '1:1:-1.1'day to second;
select date'01-01-2023' + interval '1:1:-1.1'day to minute;
select date'01-01-2023' + interval '1:1:-1.1'day to hour;
select date'01-01-2023' + interval '1:1:-1.1'day;
select date'01-01-2023' + interval '1:1:-1.1'month;
select date'01-01-2023' + interval '1:1:-1.1'hour;
select date'01-01-2023' + interval '1:1:-1.1'second;
select date'01-01-2023' + interval '1:1:-1.1'minute;

select date'01-01-2023' + interval '1 1:-1:1';
select date'01-01-2023' + interval '1 1:-1:1'year;
select date'01-01-2023' + interval '1 1:-1:1'year to month;
select date'01-01-2023' + interval '1 1:-1:1'day to second;
select date'01-01-2023' + interval '1 1:-1:1'day to minute;
select date'01-01-2023' + interval '1 1:-1:1'day to hour;
select date'01-01-2023' + interval '1 1:-1:1'day;
select date'01-01-2023' + interval '1 1:-1:1'month;
select date'01-01-2023' + interval '1 1:-1:1'hour;
select date'01-01-2023' + interval '1 1:-1:1'second;
select date'01-01-2023' + interval '1 1:-1:1'minute;


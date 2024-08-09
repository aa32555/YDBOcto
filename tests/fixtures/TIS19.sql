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

select date'2222-01-01' + interval '-1-1';
select date'2023-01-01' + interval'-1:1:1';
select date'2023-01-01' + interval'-1:1:1.1';
select date'2023-01-01' + interval'-1:1:1.999999';
select date'2023-01-01' + interval'1 day -1:1:1.999999';
select date'2023-01-01' + interval'-1-1 1 day -1:1:1.999999';

select date'2023-01-01' + interval '1:-1'year;
select date'2023-01-01' + interval '1:-1'year to month;
select date'2023-01-01' + interval '1:-1'day;

select date'2023-01-01' + interval '1:-1'month;
select date'2023-01-01' + interval '1:1:-1'year;
select date'2023-01-01' + interval '1:1:-1'year to month;
select date'2023-01-01' + interval '1:1:-1'day;
select date'2023-01-01' + interval '1:1:-1'month;

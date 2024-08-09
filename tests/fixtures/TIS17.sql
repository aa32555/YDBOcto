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

select date'2023-01-01' + interval'1-11';
select date'2023-01-01' + interval'1-12';
select date'2023-01-01' + interval'1-13';
select date'2023-01-01' + interval'1 1:59:00';
select date'2023-01-01' + interval'1 1:60:00';
select date'2023-01-01' + interval'1 1:59:60';
select date'2023-01-01' + interval'1 1:59:61';
select date'2023-01-01' + interval'1 1:59:62';
select date'2023-01-01' + interval'1 999:59:60';
select date'2023-01-01' + interval'1-12 999 999:59:60';
select date'2023-01-01' + interval'1-11 999 999:59:60';
select date'2023-01-01' + interval'69:56.11';

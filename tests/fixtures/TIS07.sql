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

select timestamp with time zone'6873-4-30 2:10:46+09:26' - interval'+699 year 62 month'year to month;
select timestamp with time zone'6169-4-30' - interval'2 month';
select timestamp'6169-4-30' - interval'2 month';
select timestamp'2881-7-30 18:3:8' - interval'+101'month;
select date'3976-12-30' + interval'988-2'hour to second;
select date'7643-3-30' + interval'+371'month;
select date'5652-2-29' + interval'-654'; -- leap year

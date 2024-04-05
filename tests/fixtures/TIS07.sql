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

select timestamp with time zone'4-30-6873 2:10:46+09:26' - interval'+699 year 62 month'year to month;
select timestamp with time zone'4-30-6169' - interval'2 month';
select timestamp'4-30-6169' - interval'2 month';
select timestamp'7-30-2881 18:3:8' - interval'+101'month;
select date'12-30-3976' + interval'988-2'hour to second;
select date'3-30-7643' + interval'+371'month;
select date'2-29-5652' + interval'-654'; -- leap year

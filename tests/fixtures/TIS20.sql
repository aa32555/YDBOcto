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

--select date'01-01-2023'+interval'1.11';
select date'01-01-2023'+interval'1.11 second';
-- select date'01-01-2023'+interval'1.11' second;
select date'01-01-2023'+interval'1:1:1.11';
select date'01-01-2023'+interval'1:1:1.11'hour to second;
select date'01-01-2023'+interval'1:1:1.11'minute to second;

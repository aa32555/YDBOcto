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

select date'2023-01-01' + interval'1:1:1.999999'; -- Postgres output is 2023-01-01 01:01:01.999999
select date'2023-01-01' + interval'1:1:1.9999999'; -- Postgres output is 2023-01-01 01:01:02
select date'2023-01-01' + interval'59:59:59.9999999'; -- Postgres output is 2023-01-03 12:00:00
select date'2023-01-01' + interval'59:59:60.9999999'; -- Postgres output is 2023-01-03 12:00:01


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
-- These are expected to generate error by Postgres but Octo doesn't handle error generation yet to expect 0 as the result for all
select extract(timezone_hour from time'01:01:01');
select extract(timezone_hour from date'01-01-2023');
select extract(timezone_hour from timestamp'01-01-2023');

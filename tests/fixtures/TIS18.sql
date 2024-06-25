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

select date'2023-01-01' + interval '59 minute 60 second'day to hour;
select date'2023-01-01' + interval '60 minute 1 second'day to hour;
select date'2023-01-01'+interval '1 day 24 hour' minute to second;
select date'2023-01-01'+interval '12 months'year;
select date'2023-01-01'+interval '0 month 365 day'year to month; -- day doesn't get normalized like others

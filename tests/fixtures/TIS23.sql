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
select extract(year from interval'1 year 1 month 1 day 1 hour 1 minute 1 second');
select extract(month from interval'1 year 1 month 1 day 1 hour 1 minute 1 second');
select extract(day from interval'1 year 1 month 1 day 1 hour 1 minute 1 second');
select extract(hour from interval'1 year 1 month 1 day 1 hour 1 minute 1 second');
select extract(minute from interval'1 year 1 month 1 day 1 hour 1 minute 1 second');
select extract(second from interval'1 year 1 month 1 day 1 hour 1 minute 1 second');
select extract(second from interval'1 year 1 month 1 day 1 hour 1 minute 1.1 second');
select extract(second from interval'1 year 1 month 1 day 1 hour 1 minute 1.0001 second');
select extract(second from interval'1 year 1 month 1 day 1 hour 1 minute 1.01000 second');

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

-- Following queries ideally should return error but at present it just returns 0
select extract(hour from date'01-01-2023');
select extract(day from time'01:01:01');

-- Following query tests boundary values
select extract(year from interval'999999999999999 year');
select extract(month from interval'999999999999999 month');
select extract(hour from interval'999999999999999 hour');
select extract(minute from interval'999999999999999 minute');
select extract(second from interval'999999999999999 second');
select extract(day from interval'999999999999999 day');
select extract(year from date'01-01-0001' - interval'1000000 year');
select extract(year from date'01-01-0001' - interval'999999 year');
select extract(year from date'01-01-0001' - interval'99999 year');

-- Following has a different result in postgres because it rounds sub-second
select extract(second from interval'1000000000 hour 1000000000 minute 1000000000.999999999 second');

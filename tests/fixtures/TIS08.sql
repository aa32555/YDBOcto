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

-- All are expected to generate ERR_INTERVAL_FIELD_OUT_OF_RANGE
select date'01-01-0001' + interval'999999999999999 year';
select date'01-01-0001' + interval'999999999999999 month';
select date'01-01-0001' + interval'999999999999999 hour';
select date'01-01-0001' + interval'999999999999999 minute';
select date'01-01-0001' + interval'999999999999999 second';
select date'01-01-0001' + interval'999999999999999 day';

-- All are expected to generate ERR_INVALID_DATE_TIME_VALUE
select date'01-01-0001' - interval'1000000 year';
select date'01-01-0001' - interval'999999 year';
select date'01-01-0001' - interval'99999 year';

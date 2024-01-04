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

select date'01-01-2023' + interval'yearmonth';
select date'01-01-2023' + interval'year1';
select date'01-01-2023' + interval'yearsmonth';
select date'01-01-2023' + interval'yeara';
select date'01-01-2023' + interval'1 yeara';
select date'01-01-2023' + interval'yearsa';
select date'01-01-2023' + interval'1 yearsa';
select date'01-01-2023' + interval'yeaa';
select date'01-01-2023' + interval'1 yeaa';
select date'01-01-2023' + interval'1-1 year year 1 year 1 year';
select date'01-01-2023' + interval'mon';
select date'01-01-2023' + interval'1 mon';
select date'01-01-2023' + interval'mona';
select date'01-01-2023' + interval'1 mona';
select date'01-01-2023' + interval'minua';
select date'01-01-2023' + interval'1 minua';
select date'01-01-2023' + interval'min';
select date'01-01-2023' + interval'1 min';
select date'01-01-2023' + interval'1-1 year year 1 year 1 year';
select date'01-01-2023' + interval'1- year year 1 year 1 year';
select date'01-01-2023' + interval'1-1 month month 1 month 1 month';
select date'01-01-2023' + interval'1- month month 1 month 1 month';
select date'01-01-2023' + interval'1-1 1 month';
select date'01-01-2023' + interval'1- 1 month';
select date'01-01-2023' + interval'1-1 1:1 1 month';
select date'01-01-2023' + interval'dan';
select date'01-01-2023' + interval'1 dan';
select date'01-01-2023' + interval'1 day 1 day';
select date'01-01-2023' + interval'1 1:1 1 day';
select date'01-01-2023' + interval'1 1: 1 day';
select date'01-01-2023' + interval'houa';
select date'01-01-2023' + interval'1 houa';
select date'01-01-2023' + interval'1 hour 1 hour';
select date'01-01-2023' + interval'1:1 1 hour';
select date'01-01-2023' + interval'1 se';
select date'01-01-2023' + interval'1:1:1 1 second';
select date'01-01-2023' + interval'1:1:1 1 minute';
select date'01-01-2023' + interval'1--1';
select date'01-01-2023' + interval'1:1 1';
select date'01-01-2023' + interval'1:1+1';
select date'01-01-2023' + interval'1:1-1';
select date'01-01-2023' + interval'year';
select date'01-01-2023' + interval'month';
select date'01-01-2023' + interval'year month';
select date'01-01-2023' + interval'year month second';
select date'01-01-2023' + interval'';
select date'01-01-2023' + interval'1 1-1';
select date'01-01-2023' + interval'1 1:1 1';
select date'01-01-2023' + interval'1+';
select date'01-01-2023' + interval'1+1';
select date'01-01-2023' + interval'1+1day';
select date'01-01-2023' + interval'1*1day';
select date'01-01-2023' + interval'1 1 year';
select date'01-01-2023' + interval'1:+1:1';
select date'01-01-2023' + interval'1:-1:1';
select date'01-01-2023' + interval'1:1:+1';
select date'01-01-2023' + interval'1:1:-1';
select date'01-01-2023' + interval'1:1:-';
select date'01-01-2023' + interval'1 -';
select date'01-01-2023' + interval'1 +';
select date'01-01-2023' + interval'+';
select date'01-01-2023' + interval'-';
select date'01-01-2023' + interval'1:1 1:1';
select date'01-01-2023' + interval'1-1 1-';
select date'01-01-2023' + interval'1-1 1-1';

-- Field specification included 1st level of tests
select date'01-01-2023' + interval'1 month 1' month;
select date'01-01-2023' + interval'1- 1'month;
select date'01-01-2023' + interval'1 1:1 1'day;
select date'01-01-2023' + interval'1 day 1'day;
select date'01-01-2023' + interval'1:1 1'hour;
select date'01-01-2023' + interval'1 hour 1'hour;
select date'01-01-2023' + interval'1:1 1'minute;
select date'01-01-2023' + interval'1 minute 1'minute;
select date'01-01-2023' + interval'1 second 1'second;
select date'01-01-2023' + interval'1-1 1'year to month;
select date'01-01-2023' + interval'1 year 1 month 1'year to month;
select date'01-01-2023' + interval'1 month 1'year to month;
select date'01-01-2023' + interval'1: 1'day to second;
select date'01-01-2023' + interval'1 second 1'day to second;
select date'01-01-2023' + interval'1: 1'day to minute;
select date'01-01-2023' + interval'1 minute 1'day to minute;
select date'01-01-2023' + interval'1: 1'day to hour;
select date'01-01-2023' + interval'1 hour 1'day to hour;
select date'01-01-2023' + interval'1: 1'hour to second;
select date'01-01-2023' + interval'1 second 1'hour to second;
select date'01-01-2023' + interval'1: 1'hour to minute;
select date'01-01-2023' + interval'1 minute 1'hour to minute;
select date'01-01-2023' + interval'1: 1'minute to second;
select date'01-01-2023' + interval'1 second 1'minute to second;

-- Invalid subtraction order of operands
select interval'1 1:' - date'01-01-2023'; -- ERR_INVALID_INTERVAL_SUBTRACTION error expected
select interval'1 1:' - timestamp'01-01-2023 01:01:01'; -- ERR_INVALID_INTERVAL_SUBTRACTION error expected
select interval'1 1:' - timestamp with time zone'01-01-2023 01:01:01-05:00'; -- ERR_INVALID_INTERVAL_SUBTRACTION error expected
select interval'1 1:' - time'01:01:01'; -- ERR_INVALID_INTERVAL_SUBTRACTION error expected
select interval'1 1:' - time with time zone'01:01:01-05:00'; -- ERR_INVALID_INTERVAL_SUBTRACTION error expected


-- Following are invalid in Octo but works in Postgres
-- The delimiter in this case is '*' but more effort needed to find out which all are allowed in this particular case
-- alphabet in place of * is invalid in Postgres so not sure.
select date'01-01-2023' + interval'1year*1month'; -- 1 year 1 month

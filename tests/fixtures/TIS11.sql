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

-- valid
select date'01-01-2023' + interval'1 1:';
select date'01-01-2023' + interval'1- 1 year';
select date'01-01-2023' + interval'1-1 1 year';
select date'01-01-2023' + interval'1 1:1 1 year';
select date'01-01-2023' + interval'1 1:1 1 month';
select date'01-01-2023' + interval'1-1 1 year';
select date'01-01-2023' + interval'1*year'; -- 1 year
select date'01-01-2023' + interval'1!year'; -- 1 year
select date'01-01-2023' + interval'-1:1:1';
select date'01-01-2023' + interval'- 1 1:'; -- -1 day 1 hour

select date'01-01-2023' + interval'1 day 1:1:1'; -- 1 day 1 hour 1minute 1 second
select date'01-01-2023' + interval'1-1'; -- 1 year 1 month
select date'01-01-2023' + interval'1 1:1:1'; -- 1 day 1 hour 1minute 1 second
select date'01-01-2023' + interval'1 year 1 month';
select date'01-01-2023' + interval'1 day';
select date'01-01-2023' + interval'1 year';
select date'01-01-2023' + interval'1 month';
select date'01-01-2023' + interval'1 hour';
select date'01-01-2023' + interval'1 minute';
select date'01-01-2023' + interval'1 second';
select date'01-01-2023' + interval'1 seconds';
select date'01-01-2023' + interval'5 hours 1 minute 1 second';
select date'01-01-2023' + interval'4 years 2 months';

select date'01-01-2023' + interval'1 hour hour';
select date'01-01-2023' + interval'minute minute 1 minute';
select date'01-01-2023' + interval'1 minute minute';

-- field specification 1st level testing
select date'01-01-2023' + interval'1 year 1'month;
select date'01-01-2023' + interval'1:1 1'month;
select date'01-01-2023' + interval'1 1:1 1'month;
select date'01-01-2023' + interval'1-1 1'day;
select date'01-01-2023' + interval'1:1 1'day;
select date'01-01-2023' + interval'1 hour 1'day;
select date'01-01-2023' + interval'1-1 1'hour;
select date'01-01-2023' + interval'1 second 1'hour;
select date'01-01-2023' + interval'1-1 1'minute;
select date'01-01-2023' + interval'1 hour 1'minute;
select date'01-01-2023' + interval'1 second 1'minute;
select date'01-01-2023' + interval'1-1 1'second;
select date'01-01-2023' + interval'1 month 1'second;
select date'01-01-2023' + interval'1 hour 1'second;
select date'01-01-2023' + interval'1:1 1'year to month;
select date'01-01-2023' + interval'1 day 1'year to month;
select date'01-01-2023' + interval'1 month'year to month;
select date'01-01-2023' + interval'1-1 1'day to second;
select date'01-01-2023' + interval'1 hour 1'day to second;
select date'01-01-2023' + interval'1 day 1'day to second;
select date'01-01-2023' + interval'1'day to second;
select date'01-01-2023' + interval'1-1 1'day to minute;
select date'01-01-2023' + interval'1 hour 1'day to minute;
select date'01-01-2023' + interval'1 day 1'day to minute;
select date'01-01-2023' + interval'1'day to minute;
select date'01-01-2023' + interval'1-1 1'day to hour;
select date'01-01-2023' + interval'1 minute 1'day to hour;
select date'01-01-2023' + interval'1 day 1'day to hour;
select date'01-01-2023' + interval'1'day to hour;
select date'01-01-2023' + interval'1-1 1'hour to second;
select date'01-01-2023' + interval'1 minute 1'hour to second;
select date'01-01-2023' + interval'1 hour 1'hour to second;
select date'01-01-2023' + interval'1 day 1'hour to second;
select date'01-01-2023' + interval'1'hour to second;
select date'01-01-2023' + interval'1-1 1'hour to minute;
select date'01-01-2023' + interval'1 hour 1'hour to minute;
select date'01-01-2023' + interval'1 second 1'hour to minute;
select date'01-01-2023' + interval'1 day 1'hour to minute;
select date'01-01-2023' + interval'1'hour to minute;
select date'01-01-2023' + interval'1-1 1'minute to second;
select date'01-01-2023' + interval'1 minute 1'minute to second;
select date'01-01-2023' + interval'1 day 1'minute to second;
select date'01-01-2023' + interval'1'minute to second;
select date'01-01-2023' + interval'1- 1'year;

-- leap year subtraction testing
select date'03-01-2024' - interval'1 day';
select timestamp'03-01-2024 00:00:00' - interval'1 second';
select timestamp with time zone'03-01-2024 00:00:00-05:00' - interval'1 second';
select time'00:00:00' - interval'1 second';

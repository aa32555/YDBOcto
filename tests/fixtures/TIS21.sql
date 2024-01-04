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
-- Year
-- Positive month
select date'01-01-2023' + interval'0 year 12 month'year;
select date'01-01-2023' + interval'0 year 11 month'year;

-- Negatie month
select date'01-01-2023' + interval'+750 hour -1 minute'day to hour;
select date'01-01-2023' + interval'2 year -1 month'year;
select date'01-01-2023' + interval'-2 year -1 month'year;
select date'01-01-2023' + interval'-2-1'year;
select date'01-01-2023' + interval'-2 year -12 month'year;

-- minute
select date'01-01-2023' + interval '1 minute -1 second'minute;
select date'01-01-2023' + interval '1 minute -0.1 second'minute;
select date'01-01-2023' + interval '0 minute -1 second'minute;
select date'01-01-2023' + interval '0 minute -0.9 second'minute;

-- hour
select date'01-01-2023' + interval '1 hour -1 minute 0 second'hour;
select date'01-01-2023' + interval '1 hour 0 minute 0 second 'hour;
select date'01-01-2023' + interval '0 hour 60 minute 0 second 'hour;
select date'01-01-2023' + interval '0 hour 59 minute 0 second 'hour;
select date'01-01-2023' + interval '0 hour -1 minute 0 second 'hour;
select date'01-01-2023' + interval '0 hour -60 minute 0 second 'hour;
select date'01-01-2023' + interval '-1 hour -60 minute 0 second 'hour;
select date'01-01-2023' + interval '-1 hour -59 minute 0 second 'hour;
select date '01-01-2023' + interval '1 hour 0 minute 3600 second'hour;
select date '01-01-2023' + interval '1 hour 0 minute 3599 second'hour;
select date '01-01-2023' + interval '0 hour 0 minute 3599 second'hour;
select date '01-01-2023' + interval '0 hour 0 minute 3600 second'hour;
select date '01-01-2023' + interval '1 hour 0 minute -1 second'hour;
select date '01-01-2023' + interval '0 hour 0 minute -3599 second'hour;
select date '01-01-2023' + interval '0 hour 0 minute -3600 second'hour;

-- day_minute
select date'01-01-2023' + interval '1 day 0 hour -1 minute'day to minute;
select date'01-01-2023' + interval '1 day 0 hour -1 minute 0 second'day to minute;
select date'01-01-2023' + interval '1 day 1 hour -1 minute 0 second'day to minute;

-- Misc
select date'1-27-7378' - interval'12 day +8 hour -6 minute -1 second'day to minute;
select date'1-01-2023' - interval'12 day +8 hour 1 minute -1 second'day to minute;
select date'6-10-3861'+ interval'-500 minute 20 second'hour to minute;
select date'01-01-2023' + interval'+50 hour -768 minute -518.363 second'day to minute;
select date'1-27-7378' - interval'+333 hour -772 minute -859.170 second'day to minute;
select date'01-01-2023' + interval'445 hour -895 minute -140.266 second'day to minute;
select timestamp'6-26-3949 13:27:32' + interval'-250 minute 302 second'day to minute;

select time'7:53:12' + interval'-895 hour +460 minute +976.175 second'hour to minute;
select time'7:53:12' + interval'-1 hour +1 minute +1 second'hour to minute;

-- Interval daylight savings related queries
select timestamp with time zone'06-01-2024 00:00:00' + interval'-83 0:0';
select timestamp with time zone'06-01-2024 00:00:00' + interval'-83'day;
select timestamp with time zone'03-11-2024 00:00:00' + interval'-2'day;

select timestamp with time zone'10-21-5396 22:47:2-03:47' - interval'+226 -987:15:49.438'day to minute;
select timestamp with time zone'11-30-7925 17:28:8-06:48'- interval'+265 day 247 hour 930 minute -187.479 seconds';
select timestamp with time zone'10-8-5729 17:47:47+00:58' - interval'-979:0:11.470';
select timestamp with time zone'10-8-2024 00:00:00' - interval'-700 hours';
select timestamp with time zone'10-8-2024 00:00:00' - interval'-625 hours';
select timestamp with time zone'10-8-2024 00:00:00' - interval'-626 hours';

select timestamp with time zone'10-21-5396 22:47:2-03:47' - interval'+226 -987:15:49.438'day to minute;
select timestamp with time zone '11-08-2024 08:00:00-05' - interval '1 day 744 hours';
select timestamp with time zone '11-08-2024 08:00:00-05' - interval '31 days';
select timestamp with time zone '11-08-2024 08:00:00-05' - interval '744 hours';
select timestamp with time zone '03-08-2024 08:00:00-05' + interval '744 hours';
select timestamp with time zone '03-08-2024 08:00:00-05' + interval '31 days';
select timestamp with time zone '04-08-2024 08:00:00-04' - interval '31 days';
select timestamp with time zone '04-08-2024 08:00:00-04' - interval '744 hours';
select timestamp with time zone '10-10-2024 08:00:00-04' - interval '-744 hours';
select timestamp with time zone '10-10-2024 08:00:00-04' + interval ' 744 hours';
select timestamp with time zone '10-10-2024 08:00:00-04' - interval '-31 days';
select timestamp with time zone '10-10-2024 08:00:00-04' + interval ' 31 days';
select timestamp with time zone'10-8-5729 00:00:00' - interval'-979 hours';
select timestamp with time zone'10-8-2024 00:00:00' - interval'-979 hours';
select timestamp with time zone'10-8-2024 00:00:00' - interval'-700 hours';

select timestamp with time zone '10-10-2024 08:00:00-04' + interval ' 31 days';
select timestamp with time zone '10-10-2024 08:00:00-04' - interval '-31 days';

select timestamp with time zone '10-10-2024 08:00:00-04' + interval ' 744 hours';
select timestamp with time zone '10-10-2024 08:00:00-04' - interval '-744 hours';
select timestamp with time zone'10-8-5729 17:47:47+00:58' - interval'-979:0:11';
select timestamp with time zone'10-8-5729 17:47:47+00:58' - interval'-979:0:11.470';
select timestamp with time zone'11-30-7925 17:28:8-06:48' - interval'+265 day 247 hour 930 minute -187.479 seconds';
select timestamp with time zone'10-21-5396 22:47:2-03:47' - interval'+226 -987:15:49.438'day to minute;
select timestamp with time zone'03-11-2024 00:00:00' + interval'-2'day;
select timestamp with time zone'06-01-2024 00:00:00' + interval'-83'day;
select interval'+92-1'year to month + timestamp with time zone'10-4-3228 14:35:6+11:23';
select timestamp with time zone'11-3-2024 02:00:00' + interval'1 minutes';
select timestamp with time zone'11-7-4134 14:19:42-05:56' - interval'-215';

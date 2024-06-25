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
select date'2023-01-01' + interval'0 year 12 month'year;
select date'2023-01-01' + interval'0 year 11 month'year;

-- Negatie month
select date'2023-01-01' + interval'+750 hour -1 minute'day to hour;
select date'2023-01-01' + interval'2 year -1 month'year;
select date'2023-01-01' + interval'-2 year -1 month'year;
select date'2023-01-01' + interval'-2-1'year;
select date'2023-01-01' + interval'-2 year -12 month'year;

-- minute
select date'2023-01-01' + interval '1 minute -1 second'minute;
select date'2023-01-01' + interval '1 minute -0.1 second'minute;
select date'2023-01-01' + interval '0 minute -1 second'minute;
select date'2023-01-01' + interval '0 minute -0.9 second'minute;

-- hour
select date'2023-01-01' + interval '1 hour -1 minute 0 second'hour;
select date'2023-01-01' + interval '1 hour 0 minute 0 second 'hour;
select date'2023-01-01' + interval '0 hour 60 minute 0 second 'hour;
select date'2023-01-01' + interval '0 hour 59 minute 0 second 'hour;
select date'2023-01-01' + interval '0 hour -1 minute 0 second 'hour;
select date'2023-01-01' + interval '0 hour -60 minute 0 second 'hour;
select date'2023-01-01' + interval '-1 hour -60 minute 0 second 'hour;
select date'2023-01-01' + interval '-1 hour -59 minute 0 second 'hour;
select date '2023-01-01' + interval '1 hour 0 minute 3600 second'hour;
select date '2023-01-01' + interval '1 hour 0 minute 3599 second'hour;
select date '2023-01-01' + interval '0 hour 0 minute 3599 second'hour;
select date '2023-01-01' + interval '0 hour 0 minute 3600 second'hour;
select date '2023-01-01' + interval '1 hour 0 minute -1 second'hour;
select date '2023-01-01' + interval '0 hour 0 minute -3599 second'hour;
select date '2023-01-01' + interval '0 hour 0 minute -3600 second'hour;

-- day_minute
select date'2023-01-01' + interval '1 day 0 hour -1 minute'day to minute;
select date'2023-01-01' + interval '1 day 0 hour -1 minute 0 second'day to minute;
select date'2023-01-01' + interval '1 day 1 hour -1 minute 0 second'day to minute;

-- Misc
select date'7378-1-27' - interval'12 day +8 hour -6 minute -1 second'day to minute;
select date'2023-1-01' - interval'12 day +8 hour 1 minute -1 second'day to minute;
select date'3861-6-10'+ interval'-500 minute 20 second'hour to minute;
select date'2023-01-01' + interval'+50 hour -768 minute -518.363 second'day to minute;
select date'7378-1-27' - interval'+333 hour -772 minute -859.170 second'day to minute;
select date'2023-01-01' + interval'445 hour -895 minute -140.266 second'day to minute;
select timestamp'3949-6-26 13:27:32' + interval'-250 minute 302 second'day to minute;

select time'7:53:12' + interval'-895 hour +460 minute +976.175 second'hour to minute;
select time'7:53:12' + interval'-1 hour +1 minute +1 second'hour to minute;

-- Interval daylight savings related queries
select timestamp with time zone'2024-06-01 00:00:00' + interval'-83 0:0';
select timestamp with time zone'2024-06-01 00:00:00' + interval'-83'day;
select timestamp with time zone'2024-03-11 00:00:00' + interval'-2'day;

select timestamp with time zone'5396-10-21 22:47:2-03:47' - interval'+226 -987:15:49.438'day to minute;
select timestamp with time zone'7925-11-30 17:28:8-06:48'- interval'+265 day 247 hour 930 minute -187.479 seconds';
select timestamp with time zone'5729-10-8 17:47:47+00:58' - interval'-979:0:11.470';
select timestamp with time zone'2024-10-8 00:00:00' - interval'-700 hours';
select timestamp with time zone'2024-10-8 00:00:00' - interval'-625 hours';
select timestamp with time zone'2024-10-8 00:00:00' - interval'-626 hours';

select timestamp with time zone'5396-10-21 22:47:2-03:47' - interval'+226 -987:15:49.438'day to minute;
select timestamp with time zone '2024-11-08 08:00:00-05' - interval '1 day 744 hours';
select timestamp with time zone '2024-11-08 08:00:00-05' - interval '31 days';
select timestamp with time zone '2024-11-08 08:00:00-05' - interval '744 hours';
select timestamp with time zone '2024-03-08 08:00:00-05' + interval '744 hours';
select timestamp with time zone '2024-03-08 08:00:00-05' + interval '31 days';
select timestamp with time zone '2024-04-08 08:00:00-04' - interval '31 days';
select timestamp with time zone '2024-04-08 08:00:00-04' - interval '744 hours';
select timestamp with time zone '2024-10-10 08:00:00-04' - interval '-744 hours';
select timestamp with time zone '2024-10-10 08:00:00-04' + interval ' 744 hours';
select timestamp with time zone '2024-10-10 08:00:00-04' - interval '-31 days';
select timestamp with time zone '2024-10-10 08:00:00-04' + interval ' 31 days';
select timestamp with time zone'5729-10-8 00:00:00' - interval'-979 hours';
select timestamp with time zone'2024-10-8 00:00:00' - interval'-979 hours';
select timestamp with time zone'2024-10-8 00:00:00' - interval'-700 hours';

select timestamp with time zone '2024-10-10 08:00:00-04' + interval ' 31 days';
select timestamp with time zone '2024-10-10 08:00:00-04' - interval '-31 days';

select timestamp with time zone '2024-10-10 08:00:00-04' + interval ' 744 hours';
select timestamp with time zone '2024-10-10 08:00:00-04' - interval '-744 hours';
select timestamp with time zone'5729-10-8 17:47:47+00:58' - interval'-979:0:11';
select timestamp with time zone'5729-10-8 17:47:47+00:58' - interval'-979:0:11.470';
select timestamp with time zone'7925-11-30 17:28:8-06:48' - interval'+265 day 247 hour 930 minute -187.479 seconds';
select timestamp with time zone'5396-10-21 22:47:2-03:47' - interval'+226 -987:15:49.438'day to minute;
select timestamp with time zone'2024-03-11 00:00:00' + interval'-2'day;
select timestamp with time zone'2024-06-01 00:00:00' + interval'-83'day;
select interval'+92-1'year to month + timestamp with time zone'3228-10-4 14:35:6+11:23';
select timestamp with time zone'2024-11-3 02:00:00' + interval'1 minutes';
select timestamp with time zone'4134-11-7 14:19:42-05:56' - interval'-215';

-- before dst to dst
select timestamp with time zone'2024-03-09 03:00:00' + interval'24 hour';
-- in dst to after dst
select timestamp with time zone'2024-11-02 03:00:00' + interval'24 hour';
-- after dst to in dst
select timestamp with time zone'2024-11-03 03:00:00' + interval'-24 hour';
select timestamp with time zone'2024-11-03 03:00:00' + interval'-1 month';
-- in dst to before dst
select timestamp with time zone'2024-03-10 03:00:00' + interval'-24 hour';
-- before dst to after dst
select timestamp with time zone'2024-03-08 03:00:00' + interval'8 months';
-- after dst to before dst
select timestamp with time zone'2024-11-08 03:00:00' + interval'-8 months';

-- before dst to dst in reverse
select timestamp with time zone'2024-03-09 03:00:00' + interval'-8 months';

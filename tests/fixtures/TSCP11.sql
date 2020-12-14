#################################################################
#								#
# Copyright (c) 2021 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

-- TSCP11 : OCTO288 : Support functions now(), lpad(), date_add(), coalesce(), date_format(), isnull(), current_user(), truncate(), day()
-- Note that some of those functions have already been implemented previously and are tested elsewhere.
SELECT lpad('a', 5);
SELECT lpad('aaa', 2);
SELECT cast(now() as varchar(20)); -- truncates to nearest second since Octo and Psql may be run a few milliseconds apart
SELECT cast(localtimestamp as varchar(20));
SELECT current_timestamp::varchar(9);
SELECT localtime::varchar(9);
SELECT current_time::varchar(9);

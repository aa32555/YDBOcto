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

select extract(hour from interval'1000000000 minute 1000000000 second 1000000000 hour');
select extract(hour from interval'1000000000 minute 1000000000.999999999 second 1000000000 hour');
select extract(hour from interval'1000000000 hour 1000000000 minute 1000000000.999999999 second');
select extract(minute from interval'1000000000 hour 1000000000 minute 1000000000.9999999999 second');


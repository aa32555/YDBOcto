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

create view v1 as select date '2023-01-01'+interval '1 year';
create view v2 as select date'2023-01-01'- interval '1 year';
create view v3 as select date_add(date'2023-01-01',interval '1 year');
create view v4 as select date_sub(date'2023-01-01',interval '1 year');
select * from v1;
select * from v2;
select * from v3;
select * from v4;

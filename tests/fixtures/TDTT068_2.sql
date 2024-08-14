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

-- Max literal length allowed is YDB_MAX_STR, following is just an example working of large sub-seconds values. Sub-second values after 6 decimal places are ignored.
select timestamp with time zone'2024-02-21T13:31:48.05098021111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111+07:00';
select timestamp with time zone'2024-02-21T13:31:48.05098021111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111+07:00';
select timestamp with time zone'2024-02-21T13:31:48.0509802111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111+07:00';

create table TDTT068 (id integer primary key, dob timestamp with time zone) global "^TDTT068" readonly;
select * from TDTT068;

create table TDTT068rw(id integer primary key, dob timestamp with time zone);
insert into TDTT068rw values(1, timestamp with time zone'2024-02-21T13:31:48.05098021111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111+07:00');
insert into TDTT068rw values(2, timestamp with time zone'2024-02-21T13:31:48.05098021111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111+07:00');
insert into TDTT068rw values(3, timestamp with time zone'2024-02-21T13:31:48.0509802111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111+07:00');
select * from TDTT068rw;

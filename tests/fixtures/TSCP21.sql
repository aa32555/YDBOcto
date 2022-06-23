#################################################################
#                                                              #
# Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.      #
# All rights reserved.                                         #
#                                                              #
#      This source code contains the intellectual property     #
#      of its copyright holder(s), and is made available       #
#      under a license.  If you do not know the terms of       #
#      the license, please stop and do not read further.       #
#                                                              #
#################################################################

select lastname from names group by lastname having FALSE!=NOT 'test'=lastname;
select lastname from names group by lastname having FALSE!=(NOT 'test'=lastname);
select FALSE != NOT 'test'='test1';
select FALSE != NOT ('test'='test1');
select FALSE != NOT TRUE;
select NOT TRUE = FALSE;
select TRUE = NOT FALSE;
select TRUE = NOT TRUE AND FALSE;
select TRUE = NOT NOT TRUE AND TRUE;

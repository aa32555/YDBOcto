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

-- Following queries doen't work in Postgres and they issue parser error but if 'test'='test1' is used within parenthesis then the query works fine. Octo at present is able to parse the following correctly and will create an expression equivalent to the parentesis one below.
select FALSE != 'test'='test1';
select FALSE = 'test' = 'test1';

select FALSE != ('test'='test1');
select FALSE = ('test' = 'test1');

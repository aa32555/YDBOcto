#################################################################
#								#
# Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
# Derived from https://github.com/shivarajugowda/jdbcSQLTest

SELECT c-d,
       d-e,
       abs(a),
       a,
       (a+b+c+d+e)/5
  FROM t1
 WHERE a>b
    OR c>d
 ORDER BY 1,5,3,2,4
;

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

SELECT c,
       a+b*2+c*3,
       c-d,
       abs(a),
       a+b*2+c*3+d*4+e*5
  FROM t1
 WHERE d>e
    OR (c<=d-2 OR c>=d+2)
    OR (e>c OR e<d)
 ORDER BY 1,3,5,2,4
;

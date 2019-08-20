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

SELECT a+b*2,
       abs(a),
       d,
       (SELECT count(*) FROM t1 AS x WHERE x.b<t1.b),
       b-c
  FROM t1
 WHERE b>c
 /*ORDER BY 5,1,3,2,4*/
;

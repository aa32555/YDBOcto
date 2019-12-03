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

SELECT (a+b+c+d+e)/5,
       e,
       (SELECT count(*) FROM t1 AS x WHERE x.c>t1.c AND x.d<t1.d)
  FROM t1
 WHERE (e>c OR e<d)
    OR EXISTS(SELECT 1 FROM t1 AS x WHERE x.b<t1.b)
 ORDER BY 3,1,2
;

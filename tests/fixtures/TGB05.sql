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

-- TGB05 : OCTO55 : Edge case GROUP BY and/or HAVING queries that work in Octo but do not in Postgres

--> Below query does NOT error out in Octo because aggregate functions used in WHERE clause of the outer-query
-->	is actually inside the HAVING clause of a sub-query.
--> But it errors out in Postgres with "ERROR:  aggregate functions are not allowed in WHERE".
SELECT * FROM names n1 WHERE n1.firstName IN (SELECT n2.firstname FROM names n2 GROUP BY n2.firstname HAVING n2.firstname = MAX(n1.firstname));


#################################################################
#								#
# Copyright (c) 2020-2021 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

-- TPK01 : OCTO279 : Octo returns incorrect results if key column is the empty string

-- All columns are key columns
SELECT * FROM T1;
SELECT * FROM (SELECT * FROM T1);

-- Some columns are not key columns
SELECT * FROM T2;
SELECT * FROM T2 WHERE a = '';
SELECT * FROM T2 WHERE a = NULL;
SELECT * FROM T2 WHERE a is NULL;
SELECT * FROM T2 WHERE a = 'a3';
SELECT * FROM T2 WHERE b = '';
SELECT * FROM T2 WHERE b = NULL;
SELECT * FROM T2 WHERE b is NULL;
SELECT * FROM T2 WHERE b = 'b1';
SELECT * FROM T2 WHERE c = '';
SELECT * FROM T2 WHERE c = NULL;
SELECT * FROM T2 WHERE c is NULL;
SELECT * FROM T2 WHERE c = 'c2';
SELECT * FROM T2 WHERE a != '';
SELECT * FROM T2 WHERE a != NULL;
SELECT * FROM T2 WHERE a is not NULL;
SELECT * FROM T2 WHERE a != 'a3';
SELECT * FROM T2 WHERE b != '';
SELECT * FROM T2 WHERE b != NULL;
SELECT * FROM T2 WHERE b is not NULL;
SELECT * FROM T2 WHERE b != 'b1';
SELECT * FROM T2 WHERE c != '';
SELECT * FROM T2 WHERE c != NULL;
SELECT * FROM T2 WHERE c is not NULL;
SELECT * FROM T2 WHERE c != 'c2';
SELECT * FROM (SELECT * FROM T2);
SELECT * FROM (SELECT * FROM T2 WHERE a = '');
SELECT * FROM (SELECT * FROM T2 WHERE a = NULL);
SELECT * FROM (SELECT * FROM T2 WHERE a is NULL);
SELECT * FROM (SELECT * FROM T2 WHERE a = 'a3');
SELECT * FROM (SELECT * FROM T2 WHERE b = '');
SELECT * FROM (SELECT * FROM T2 WHERE b = NULL);
SELECT * FROM (SELECT * FROM T2 WHERE b is NULL);
SELECT * FROM (SELECT * FROM T2 WHERE b = 'b1');
SELECT * FROM (SELECT * FROM T2 WHERE c = '');
SELECT * FROM (SELECT * FROM T2 WHERE c = NULL);
SELECT * FROM (SELECT * FROM T2 WHERE c is NULL);
SELECT * FROM (SELECT * FROM T2 WHERE c = 'c2');
SELECT * FROM (SELECT * FROM T2 WHERE a != '');
SELECT * FROM (SELECT * FROM T2 WHERE a != NULL);
SELECT * FROM (SELECT * FROM T2 WHERE a is not NULL);
SELECT * FROM (SELECT * FROM T2 WHERE a != 'a3');
SELECT * FROM (SELECT * FROM T2 WHERE b != '');
SELECT * FROM (SELECT * FROM T2 WHERE b != NULL);
SELECT * FROM (SELECT * FROM T2 WHERE b is not NULL);
SELECT * FROM (SELECT * FROM T2 WHERE b != 'b1');
SELECT * FROM (SELECT * FROM T2 WHERE c != '');
SELECT * FROM (SELECT * FROM T2 WHERE c != NULL);
SELECT * FROM (SELECT * FROM T2 WHERE c is not NULL);
SELECT * FROM (SELECT * FROM T2 WHERE c != 'c2');

-- Test that when START is specified, we go PAST the START value and not include it in the FOR loop
-- when STARTINCLUDE is also not specified. We know that is the case in the ORDER_STATUS table in vista-mini.sql
-- so we use that. We do not expect a row with the ORDER_STATUS_ID column (first column) holding the value of 0 to be
-- selected (it would be if the code did not honor the START keyword). VistA relies on this to work correctly with Octo.
SELECT * FROM ORDER_STATUS;

-- Rerun the same query as above but with STARTINCLUDE specified.
SELECT * FROM STARTINCLUDE_ORDER_STATUS;

-- Test queries that use the xref plan return correct results
SELECT * from T2 where a = 'a1';
SELECT * from T2 where a = 'a2';
SELECT * from T2 where a = 'a3';
SELECT * from T2 where a = 'a4';
SELECT * from T2 where a = '';
SELECT * from T2 where a is NULL;

SELECT * from T2 where b = 'b1';
SELECT * from T2 where b = 'b2';
SELECT * from T2 where b = 'b3';
SELECT * from T2 where b = 'b4';
SELECT * from T2 where b = '';
SELECT * from T2 where b is NULL;

SELECT * from T2 where c = 'c1';
SELECT * from T2 where c = 'c2';
SELECT * from T2 where c = 'c3';
SELECT * from T2 where c = 'c4';
SELECT * from T2 where c = '';
SELECT * from T2 where c is NULL;


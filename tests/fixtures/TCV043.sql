#################################################################
#								#
# Copyright (c) 2023-2024 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

-- https://gitlab.com/YottaDB/DBMS/YDBOcto/-/merge_requests/1244#note_1388986513
-- "TCV043 : A drop view when the view has a function and a table works fine"
-- Function and table usage
create view v1 as select abs(1);
create view v2 as select * from v1;
drop view v2;
drop view v1;
drop table octoonerowtable; -- System objects cannot be dropped

-- Function usage
CREATE VIEW v292 as SELECT * from names where id = ABS(ABS(-1)+2);
CREATE VIEW v292d as SELECT * from v292 where id = ABS(ABS(-1)+2);
CREATE VIEW v292e as SELECT * from v292d where id = ABS(ABS(-1)+2);
DROP VIEW IF EXISTS v292e;
DROP VIEW IF EXISTS v292d; -- This should not fail
DROP VIEW IF EXISTS v292; -- This should not fail

CREATE VIEW v293 as SELECT * from names where id = ABS(ABS(ABS(-1)+2)-1);
CREATE VIEW v293d as SELECT * from v293 where id = ABS(ABS(ABS(-1)+2)-1);
CREATE VIEW v293e as SELECT * from v293d where id = ABS(ABS(ABS(-1)+2)-1);
DROP VIEW IF EXISTS v293e;
DROP VIEW IF EXISTS v293d; -- This should not fail
DROP VIEW IF EXISTS v293; -- This should not fail

CREATE FUNCTION SAMEVALUE(VARCHAR) RETURNS VARCHAR AS $$samevalue^functions;
CREATE VIEW v97 as SELECT * FROM names WHERE SAMEVALUE(firstname) > SAMEVALUE(lastname);
CREATE VIEW v97d as SELECT * FROM v97 WHERE SAMEVALUE(firstname) > SAMEVALUE(lastname);
CREATE VIEW v97e as SELECT * FROM v97d WHERE SAMEVALUE(firstname) > SAMEVALUE(lastname);
DROP VIEW IF EXISTS v97e;
DROP VIEW IF EXISTS v97d; -- This should not fail
DROP VIEW IF EXISTS v97; -- This should not fail


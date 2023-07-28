-- ######################################################################
-- #                                                                   #
-- # Copyright (c) 2020-2023 YottaDB LLC and/or its subsidiaries.           #
-- # All rights reserved.                                              #
-- #                                                                   #
-- #   This source code contains the intellectual property             #
-- #   of its copyright holder(s), and is made available               #
-- #   under a license.  If you do not know the terms of               #
-- #   the license, please stop and do not read further.               #
-- #                                                                   #
-- ######################################################################

/* This file was autogenerated with the following commands:
num=3
schema=sqllogic$num
fixtures=$(git rev-parse --show-toplevel)/tests/fixtures
$fixtures/sqllogic/insert.py $num
# Remove CREATE INDEX, INSERT INTO, and DROP TABLE statements, since Octo does not understand them.
sed '/^INSERT INTO\|CREATE INDEX\|DROP TABLE/d' $schema.sql > $fixtures/$schema.sql
*/
CREATE TABLE s3t1(id INTEGER PRIMARY KEY, a INTEGER, b INTEGER, c INTEGER, d INTEGER, e INTEGER)
 GLOBAL "^s3t1";

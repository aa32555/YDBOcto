-- ######################################################################
-- #									#
-- # Copyright (c) 2019-2020 YottaDB LLC and/or its subsidiaries.	#
-- # All rights reserved.						#
-- #									#
-- #	This source code contains the intellectual property		#
-- #	of its copyright holder(s), and is made available		#
-- #	under a license.  If you do not know the terms of		#
-- #	the license, please stop and do not read further.		#
-- #									#
-- ######################################################################

-- Below is to skip the INSERT commands if the table already exists (CREATE TABLE will cause an error and script will exit)
\set ON_ERROR_STOP on

CREATE TABLE stock_availability (product_id INT PRIMARY KEY, available BOOLEAN);

INSERT INTO stock_availability (product_id, available)
VALUES
   (100, TRUE),
   (200, FALSE),
   (300, 't'),
   (400, '1'),
   (500, 'y'),
   (600, 'yes'),
   (700, 'no'),
   (800, '0');


#################################################################
#								#
# Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

-- TCS16 : OCTO463 : CASE does not issue a syntax error in WHEN conditions if all code paths lead to the same result

-- The following case cannot be cross-checked against PostgreSQL, since PostgreSQL will
-- issue the following error for these queries:
-- ERROR:  operator does not exist: text = boolean
-- LINE 6:         WHEN ( 'Orechiette' != pastas.pastaname ) THEN NULL
--				   ^
-- HINT:  No operator matches the given name and argument types. You might need to add explicit type casts.

-- However, Octo and MySQL will accept this query and return 0 rows. So test separately
-- by doing outref verification.

-- NULL case, with case_value, omitted ELSE, NULL branches
SELECT DISTINCT Min(DISTINCT pastas.pastaname), pastas.id
FROM   pastas
WHERE  ( ( pastas.id - 6 ) = 2 )
GROUP  BY pastas.id
HAVING pastas.id = CASE NULL
	WHEN ( 'Orechiette' != pastas.pastaname ) THEN NULL
	WHEN ( 'Orechiette' < pastas.pastaname ) THEN NULL
END;

-- Nested CASE statement, NULL case, ELSEs omitted
SELECT DISTINCT Min(DISTINCT pastas.pastaname), pastas.id
FROM   pastas
WHERE  ( ( pastas.id - 6 ) = 2 )
GROUP  BY pastas.id
HAVING pastas.id = CASE
	WHEN ( 'Orechiette' != pastas.pastaname ) THEN CASE
		WHEN ( 'Orechiette' != pastas.pastaname ) THEN NULL
	END
	WHEN ( 'Orechiette' < pastas.pastaname ) THEN NULL
END;


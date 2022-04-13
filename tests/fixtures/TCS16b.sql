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

-- The following cases cannot be cross-checked against PostgreSQL, since PostgreSQL will
-- issue the following error for these queries:
--   LINE 5: HAVING pastas.id = CASE
--   HINT:  No operator matches the given name and argument types. You might need to add explicit type casts.

-- However, Octo and MySQL will accept these queries and return 0 rows. So test these separately
-- by doing outref verification.
-- TODO: When #288 is resolved and MySQL crosscheck functionality is incorporated, then these
-- queries can be tested against MySQL instead of against an outref.

-- NULL case
SELECT DISTINCT Min(DISTINCT pastas.pastaname), pastas.id
FROM   pastas
WHERE  ( ( pastas.id - 6 ) = 2 )
GROUP  BY pastas.id
HAVING pastas.id = CASE
	WHEN ( 'Orechiette' != pastas.pastaname ) THEN NULL
	WHEN ( 'Orechiette' < pastas.pastaname ) THEN NULL
	ELSE NULL
END;

-- NULL case, omitted ELSE
SELECT DISTINCT Min(DISTINCT pastas.pastaname), pastas.id
FROM   pastas
WHERE  ( ( pastas.id - 6 ) = 2 )
GROUP  BY pastas.id
HAVING pastas.id = CASE
	WHEN ( 'Orechiette' != pastas.pastaname ) THEN NULL
	WHEN ( 'Orechiette' < pastas.pastaname ) THEN NULL
	ELSE NULL
END;

-- Nested CASE statement, NULL case
SELECT DISTINCT Min(DISTINCT pastas.pastaname), pastas.id
FROM   pastas
WHERE  ( ( pastas.id - 6 ) = 2 )
GROUP  BY pastas.id
HAVING pastas.id = CASE
	WHEN ( 'Orechiette' != pastas.pastaname ) THEN CASE
		WHEN ( 'Orechiette' != pastas.pastaname ) THEN NULL ELSE NULL
	END
	WHEN ( 'Orechiette' < pastas.pastaname ) THEN NULL
	ELSE NULL
END;

-- NULL case, with case_value, omitted ELSE, NULL branches
SELECT DISTINCT Min(DISTINCT pastas.pastaname), pastas.id
FROM   pastas
WHERE  ( ( pastas.id - 6 ) = 2 )
GROUP  BY pastas.id
HAVING pastas.id = CASE NULL
	WHEN ( 'Orechiette' != pastas.pastaname ) THEN NULL
	WHEN ( 'Orechiette' < pastas.pastaname ) THEN NULL
END;

-- NULL case, without case_value, omitted ELSE, NULL branches
SELECT DISTINCT Min(DISTINCT pastas.pastaname), pastas.id
FROM   pastas
WHERE  ( ( pastas.id - 6 ) = 2 )
GROUP  BY pastas.id
HAVING pastas.id = CASE
	WHEN ( 'Orechiette' != pastas.pastaname ) THEN NULL
	WHEN ( 'Orechiette' < pastas.pastaname ) THEN NULL
END;

-- NULL case, with case_value, but omitted ELSE, non-NULL branch
SELECT DISTINCT Min(DISTINCT pastas.pastaname), pastas.id
FROM   pastas
WHERE  ( ( pastas.id - 6 ) = 2 )
GROUP  BY pastas.id
HAVING pastas.id = CASE NULL
	WHEN ( 'Orechiette' != pastas.pastaname ) THEN NULL
	WHEN ( 'Orechiette' < pastas.pastaname ) THEN 2
END;

-- NULL case, without case_value, omitted ELSE, non-NULL branch
SELECT DISTINCT Min(DISTINCT pastas.pastaname), pastas.id
FROM   pastas
WHERE  ( ( pastas.id - 6 ) = 2 )
GROUP  BY pastas.id
HAVING pastas.id = CASE
	WHEN ( 'Orechiette' != pastas.pastaname ) THEN NULL
	WHEN ( 'Orechiette' < pastas.pastaname ) THEN 2
END;


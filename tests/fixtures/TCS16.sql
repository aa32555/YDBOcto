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

-- Basic cases
SELECT DISTINCT Min(DISTINCT pastas.pastaname), pastas.id
FROM   pastas
WHERE  ( ( pastas.id - 6 ) = 2 )
GROUP  BY pastas.id
HAVING pastas.id = CASE
	WHEN ( 'Orechiette' != pastas.pastaname ) THEN pastas.id
	WHEN ( 'Orechiette' < pastas.pastaname ) THEN pastas.id
	ELSE pastas.id
END;

SELECT CASE
        WHEN id<3 THEN 2
        ELSE id+1
END from pastas;

SELECT CASE
        WHEN id < 2 THEN NULL
        WHEN id < 4 THEN CASE
                WHEN id < 3 THEN NULL
                ELSE id+1
        END
END from pastas;

-- Nested CASE statement
SELECT DISTINCT Min(DISTINCT pastas.pastaname), pastas.id
FROM   pastas
WHERE  ( ( pastas.id - 6 ) = 2 )
GROUP  BY pastas.id
HAVING pastas.id = CASE
	WHEN ( 'Orechiette' != pastas.pastaname ) THEN CASE
		WHEN ( 'Orechiette' != pastas.pastaname ) THEN pastas.id ELSE pastas.id
	END
	WHEN ( 'Orechiette' < pastas.pastaname ) THEN pastas.id
	ELSE pastas.id
END;

-- CASE statement with value_expression
SELECT DISTINCT Min(DISTINCT pastas.pastaname),
                pastas.id
FROM   pastas
WHERE  ( ( pastas.id - 6 ) = 2 )
GROUP  BY pastas.id
HAVING pastas.id = CASE pastas.pastaname
                     WHEN 'Orechiette' THEN pastas.id
                     ELSE pastas.id
                   END;

-- Nested CASE statement, NULL case
SELECT DISTINCT Min(DISTINCT pastas.pastaname), pastas.id
FROM   pastas
WHERE  ( ( pastas.id - 6 ) = 2 )
GROUP  BY pastas.id
HAVING pastas.id::text = CASE
	WHEN ( 'Orechiette' != pastas.pastaname ) THEN CASE
		WHEN ( 'Orechiette' != pastas.pastaname ) THEN NULL ELSE NULL
	END
	WHEN ( 'Orechiette' < pastas.pastaname ) THEN NULL
	ELSE NULL
END;

-- NULL case, without case_value, omitted ELSE, NULL branches
SELECT DISTINCT Min(DISTINCT pastas.pastaname), pastas.id
FROM   pastas
WHERE  ( ( pastas.id - 6 ) = 2 )
GROUP  BY pastas.id
HAVING pastas.id::text = CASE
	WHEN ( 'Orechiette' != pastas.pastaname ) THEN NULL
	WHEN ( 'Orechiette' < pastas.pastaname ) THEN NULL
END;

-- NULL case, without case_value, omitted ELSE, non-NULL branch
SELECT DISTINCT Min(DISTINCT pastas.pastaname), pastas.pastaname
FROM   pastas
WHERE  ( ( pastas.id - 6 ) = 2 )
GROUP  BY pastas.pastaname
HAVING pastas.pastaname = CASE
	WHEN ( 'Orechiette' != pastas.pastaname ) THEN NULL
	WHEN ( 'Orechiette' < pastas.pastaname ) THEN 2::text
END;

-- NULL case, with case_value, omitted ELSE, NULL branches
SELECT DISTINCT Min(DISTINCT pastas.pastaname), pastas.id
FROM   pastas
WHERE  ( ( pastas.id - 6 ) = 2 )
GROUP  BY pastas.id
HAVING pastas.id::text = CASE NULL::boolean
	WHEN ( 'Orechiette' != pastas.pastaname ) THEN NULL
	WHEN ( 'Orechiette' < pastas.pastaname ) THEN NULL
END;

-- Nested CASE statement, NULL case, ELSEs omitted
SELECT DISTINCT Min(DISTINCT pastas.pastaname), pastas.id
FROM   pastas
WHERE  ( ( pastas.id - 6 ) = 2 )
GROUP  BY pastas.id
HAVING pastas.id::text = CASE
	WHEN ( 'Orechiette' != pastas.pastaname ) THEN CASE
		WHEN ( 'Orechiette' != pastas.pastaname ) THEN NULL
	END
	WHEN ( 'Orechiette' < pastas.pastaname ) THEN NULL
END;


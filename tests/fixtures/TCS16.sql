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

-- Basic case
SELECT DISTINCT Min(DISTINCT pastas.pastaname), pastas.id
FROM   pastas
WHERE  ( ( pastas.id - 6 ) = 2 )
GROUP  BY pastas.id
HAVING pastas.id = CASE
	WHEN ( 'Orechiette' != pastas.pastaname ) THEN pastas.id
	WHEN ( 'Orechiette' < pastas.pastaname ) THEN pastas.id
	ELSE pastas.id
END;

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

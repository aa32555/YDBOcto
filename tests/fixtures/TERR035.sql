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

-- TERR035 : OCTO463 : Test of CASE statement errors

-- Nested CASE statement with ELSE omitted
SELECT DISTINCT Min(DISTINCT pastas.pastaname), pastas.id
FROM   pastas
WHERE  ( ( pastas.id - 6 ) = 2 )
GROUP  BY pastas.id
HAVING pastas.id = CASE
	WHEN ( 'Orechiette' != pastas.pastaname ) THEN CASE
		WHEN ( 'Orechiette' < pastas.pastaname ) THEN pastas.id
	END
	WHEN ( 'Orechiette' < pastas.pastaname ) THEN pastas.id
	ELSE pastas.id
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

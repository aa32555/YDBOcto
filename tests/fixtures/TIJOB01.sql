#################################################################
#								#
# Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

-- TIJOB01 : inner join pastas order by pasta id ascending

select pastas.id, firstName, lastName, pastaName from names4 inner join pastas on pastas.pastaName = names4.favoritePasta order by pastas.id;

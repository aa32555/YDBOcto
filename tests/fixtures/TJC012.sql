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

-- TJC012 : OCTO320 : Pipeline failures from TJC007

SELECT DISTINCT * FROM Customers LEFT JOIN Categories AS alias1 ON ((Customers.CustomerName > alias1.CategoryName)) WHERE ((Customers.Country != 'Canada') AND (Customers.City >= Customers.City));


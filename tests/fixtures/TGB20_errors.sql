#################################################################
#								#
# Copyright (c) 2020-2022 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

SELECT ALL first_name,count(customer_id) FROM customers c group by c.first_name order by EXISTS (SELECT 1 FROM orders where c.last_name != 'James' group by order_id);

select first_name from customers c join (select n2.order_date,n2.customer_id from orders n2 group by c.first_name) as n2 on (c.customer_id = n2.customer_id) group by first_name;

SELECT ALL first_name FROM customers  LEFT JOIN (SELECT alias4.order_date,alias4.customer_id FROM orders alias4 GROUP BY alias4.order_date,alias4.customer_id HAVING alias4.order_date LIKE '05/23/1784') AS alias4 ON (customers.customer_id = alias4.customer_id) WHERE EXISTS (SELECT 1 FROM customers alias6 ORDER BY count(alias4.order_date));

SELECT ALL first_name FROM customers  LEFT JOIN (SELECT alias4.customer_id FROM orders alias4 GROUP BY alias4.order_date,alias4.customer_id HAVING alias4.order_date LIKE '05/23/1784') AS alias4 ON (customers.customer_id = alias4.customer_id) WHERE EXISTS (SELECT 1 FROM customers alias6 ORDER BY alias4.order_date);

SELECT ALL first_name, EXISTS(SELECT 1 from orders n2 group by n2.order_id having c.last_name = 'James') from customers c group by first_name;

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

SELECT ALL first_name FROM customers c GROUP BY first_name HAVING EXISTS (SELECT 1 FROM orders GROUP BY order_id having c.first_name = 'James');
SELECT ALL first_name FROM customers c GROUP BY first_name HAVING EXISTS (SELECT 1 FROM orders n1 GROUP BY order_id having EXISTS(SELECT 1 from orders n2 group by n2.order_id having c.first_name != 'James'));
SELECT ALL first_name, EXISTS(SELECT 1 from orders n2 group by n2.order_id having c.first_name = 'James') from customers c group by first_name;
SELECT ALL first_name FROM customers c order by EXISTS (SELECT 1 FROM orders where c.last_name != 'James' group by order_id);
-- GAN_TODO: check why the ordering is different and wheterh its alright and remove --sort-needed-check from both the queries
SELECT ALL first_name,count(customer_id) FROM customers c group by c.first_name order by EXISTS (SELECT 1 FROM orders where c.first_name != 'James' group by order_id); --sort-needed-check
SELECT ALL first_name,count(customer_id) FROM customers c group by c.first_name order by EXISTS (SELECT 1 FROM orders where c.first_name ='James' group by order_id); --sort-needed-check


SELECT ALL first_name FROM customers c GROUP BY first_name HAVING EXISTS (SELECT 1 FROM orders GROUP BY order_id having count(c.customer_id) > 1);
SELECT ALL first_name FROM customers c GROUP BY first_name HAVING EXISTS (SELECT 1 FROM orders n1 GROUP BY order_id having EXISTS(SELECT 1 from orders n2 group by n2.order_id having count(c.customer_id) > 1));
SELECT ALL first_name FROM customers c GROUP BY first_name HAVING EXISTS (SELECT 1 FROM orders n1 GROUP BY order_id having EXISTS(SELECT 1 from orders n2 group by n2.order_id having count(c.first_name) > 1));
-- SELECT ALL first_name FROM customers c GROUP BY first_name HAVING EXISTS (SELECT count(c.customer_id) FROM orders);

SELECT ALL 1 FROM customers c HAVING EXISTS (SELECT 1 FROM orders n1 GROUP BY order_id having EXISTS(SELECT 1 from orders n2 group by n2.order_id having count(c.first_name) > 1));

select count((select first_name from customers limit 1)) from customers;

SELECT ALL first_name FROM ((select first_name from customers c1) UNION ALL (select first_name from customers c2)) c GROUP BY first_name HAVING EXISTS (SELECT 1 FROM orders GROUP BY order_id having c.first_name = 'James');
SELECT ALL first_name FROM ((select first_name from customers c1) UNION ALL (select first_name from customers c2) UNION ALL (select first_name from customers c3)) c GROUP BY first_name HAVING EXISTS (SELECT 1 FROM orders GROUP BY order_id having c.first_name = 'James');
SELECT ALL first_name FROM ((select * from customers c1) UNION ALL (select * from customers c2) UNION ALL (select * from customers c3)) c GROUP BY first_name HAVING EXISTS (SELECT 1 FROM orders GROUP BY order_id having count(c.customer_id) > 1);

-- Grouped column reference in inner query
SELECT ALL first_name FROM customers c GROUP BY first_name HAVING EXISTS (SELECT 1 FROM orders GROUP BY order_id having count(c.first_name) > 1);
SELECT ALL first_name FROM customers c GROUP BY first_name HAVING EXISTS (SELECT 1 FROM orders GROUP BY order_id having c.first_name='James');

-- WHERE clause usage
SELECT ALL first_name FROM customers  LEFT JOIN (SELECT alias4.order_date,alias4.customer_id FROM orders alias4 GROUP BY alias4.order_date,alias4.customer_id HAVING alias4.order_date LIKE '05/23/1784') AS alias4 ON (customers.customer_id = alias4.customer_id) WHERE EXISTS (SELECT 1 FROM customers alias6 ORDER BY alias4.order_date);

-- VALUES usage
SELECT ALL c.column2 FROM (VALUES (1,'George'),(2,'John'),(3,'Thomas'),(4,'James'),(5,'James')) c GROUP BY c.column2 HAVING EXISTS (SELECT 1 FROM (VALUES (1),(2),(3),(4),(5),(6)) o GROUP BY o.column1 having count(c.column1) > 1);

-- SET operation usage
SELECT ALL c.column2 FROM ((select 1 as column1,'George' as column2) UNION (select 2 as column1,'John' as column2) UNION (select 3 as column1,'Thomas' as column2) UNION (select 4 as column1,'James' as column2) UNION (select 5 as column1,'James' as column2)) c GROUP BY c.column2 HAVING EXISTS (SELECT 1 FROM ((select 1 as column1) UNION (select 2 as column1) UNION (select 3 as column1) UNION (select 4 as column1) UNION (select 5 as column1) UNION (select 6 as column1)) o GROUP BY o.column1 having count(c.column1) > 1);

SELECT ALL first_name, EXISTS(SELECT 1 from orders n2 group by n2.order_id having count(c.last_name) > 1) from customers c group by first_name;
SELECT ALL first_name from customers c group by first_name order by EXISTS(SELECT 1 from orders n2 group by n2.order_id having count(c.last_name) > 1);
SELECT ALL first_name, EXISTS(SELECT 1 from orders n2 group by n2.order_id having c.first_name = 'James') from customers c group by first_name;

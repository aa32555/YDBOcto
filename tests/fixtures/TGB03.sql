#################################################################
#								#
# Copyright (c) 2019-2020 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

-- TGB03 : OCTO55 : GROUP BY and AGGREGATE FUNCTIONS : Various valid scenarios in NORTHWIND schema

--> Below are examples of aggregate function use without GROUP BY
SELECT COUNT(CustomerID) AS NumberOfCustomers FROM Orders;
SELECT COUNT(EmployeeID) AS NumberOfEmployees FROM Orders;
SELECT COUNT(OrderID) AS NumberOfOrders FROM Orders;
SELECT COUNT(DISTINCT CustomerID) AS NumberOfCustomers FROM Orders;
SELECT COUNT(DISTINCT EmployeeID) AS NumberOfEmployees FROM Orders;
SELECT COUNT(DISTINCT OrderID) AS NumberOfOrders FROM Orders;
SELECT c.CustomerID,o.OrderID FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID;
SELECT COUNT(*) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID;
SELECT COUNT(c.CustomerId) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID;
SELECT COUNT(DISTINCT c.CustomerId) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID;
SELECT COUNT(o.OrderID) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID;
SELECT COUNT(DISTINCT o.OrderID) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID;
SELECT SUM(c.CustomerId) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID;
SELECT SUM(DISTINCT c.CustomerId) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID;
SELECT SUM(o.OrderID) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID;
SELECT SUM(DISTINCT o.OrderID) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID;
SELECT MIN(c.CustomerId) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID;
SELECT MIN(DISTINCT c.CustomerId) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID;
SELECT MIN(o.OrderID) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID;
SELECT MIN(DISTINCT o.OrderID) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID;
SELECT MAX(c.CustomerId) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID;
SELECT MAX(DISTINCT c.CustomerId) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID;
SELECT MAX(o.OrderID) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID;
SELECT MAX(DISTINCT o.OrderID) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID;

--> Below are examples of aggregate function use with GROUP BY
SELECT s.ShipperName,COUNT(o.OrderID) AS NumberOfOrders FROM Orders o LEFT JOIN Shippers s ON o.ShipperID = s.ShipperID GROUP BY ShipperName;
SELECT COUNT(CustomerID),Country FROM Customers GROUP BY Country;
SELECT COUNT(OrderID),MAX(CustomerID),EmployeeID FROM Orders WHERE EmployeeID BETWEEN 0 AND 7 GROUP BY EmployeeID;
SELECT COUNT(OrderID),MAX(OrderID),MIN(OrderID),MAX(CustomerID),MIN(CustomerID),EmployeeID FROM Orders WHERE EmployeeID BETWEEN 0 AND 7 GROUP BY EmployeeID;
SELECT c.CustomerID,o.OrderID FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID GROUP BY c.CustomerID,o.OrderID;
SELECT COUNT(c.CustomerID),o.OrderID FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID GROUP BY o.OrderID;
SELECT COUNT(c.CustomerID),COUNT(o.OrderID) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID;
SELECT 1+COUNT(c.CustomerID)+COUNT(o.OrderID) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID;
SELECT e.employeeid,COUNT(o.orderid) FROM Employees e LEFT JOIN Orders o ON e.employeeid = o.employeeid GROUP BY e.employeeid;
SELECT e.employeeid,SUM(o.orderid) FROM Employees e LEFT JOIN Orders o ON e.employeeid = o.employeeid GROUP BY e.employeeid;
SELECT e.employeeid,MAX(o.orderid) FROM Employees e LEFT JOIN Orders o ON e.employeeid = o.employeeid GROUP BY e.employeeid;
SELECT e.employeeid,MIN(o.orderid) FROM Employees e LEFT JOIN Orders o ON e.employeeid = o.employeeid GROUP BY e.employeeid;

--> Test that MIN and MAX work on VARCHAR type
SELECT MIN(Country) FROM Customers;
SELECT MAX(Country) FROM Customers;

--> Test that aggregate function on a column that is also specified in the GROUP BY does not error out.
SELECT COUNT(CustomerID) FROM Customers GROUP BY CustomerID;

--> Below should not error out even though the same column is used inside and outside an aggregate function
-->    because there is a GROUP BY on that column.
SELECT COUNT(CustomerName),CustomerName FROM Customers GROUP BY CustomerName;

--> Below should not error out since non-aggregate function usage of `Country` occurs in WHERE clause
SELECT COUNT(CustomerID) FROM Customers WHERE Country = 'USA';
SELECT COUNT(Country) FROM Customers WHERE Country = 'USA';

--> Test ORDER BY with GROUP BY
SELECT COUNT(CustomerID),Country FROM Customers GROUP BY Country ORDER BY COUNT(CustomerID), Country;
SELECT COUNT(CustomerID),Country FROM Customers GROUP BY Country ORDER BY COUNT(CustomerID) ASC, Country;
SELECT COUNT(CustomerID),Country FROM Customers GROUP BY Country ORDER BY COUNT(CustomerID) DESC, Country;
SELECT COUNT(OrderID),CustomerID FROM Orders GROUP BY CustomerID ORDER BY COUNT(OrderID) desc, CustomerID;

--> Test GROUP BY with JOINs
SELECT c.CustomerID,MAX(o.OrderID),COUNT(o.OrderID) FROM Customers c INNER JOIN Orders o ON c.CustomerID = o.CustomerID GROUP BY c.CustomerID ORDER BY MAX(ABS(10429-o.OrderID)) DESC, c.CustomerID, COUNT(o.OrderID); -- sort-needed-check
SELECT c.CustomerID,MAX(o.OrderID),COUNT(o.OrderID) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID GROUP BY c.CustomerID ORDER BY MAX(ABS(10429-o.OrderID)) DESC, c.CustomerID, COUNT(o.OrderID); -- sort-needed-check
SELECT c.CustomerID,MAX(o.OrderID),COUNT(o.OrderID) FROM Customers c RIGHT JOIN Orders o ON c.CustomerID = o.CustomerID GROUP BY c.CustomerID ORDER BY MAX(ABS(10429-o.OrderID)) DESC, c.CustomerID, COUNT(o.OrderID); -- sort-needed-check
SELECT c.CustomerID,MAX(o.OrderID),COUNT(o.OrderID) FROM Customers c FULL JOIN Orders o ON c.CustomerID = o.CustomerID GROUP BY c.CustomerID ORDER BY MAX(ABS(10429-o.OrderID)) DESC, c.CustomerID, COUNT(o.OrderID); -- sort-needed-check
SELECT COUNT(c.CustomerID),COUNT(o.OrderID),c.CustomerID FROM Customers c INNER JOIN Orders o ON c.CustomerID = o.CustomerID GROUP BY c.CustomerID ORDER BY COUNT(o.OrderID) DESC, c.CustomerID, COUNT(o.OrderID);
SELECT COUNT(c.CustomerID),COUNT(o.OrderID),c.CustomerID FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID GROUP BY c.CustomerID ORDER BY COUNT(o.OrderID) DESC, c.CustomerID, COUNT(o.OrderID);
SELECT COUNT(c.CustomerID),COUNT(o.OrderID),c.CustomerID FROM Customers c RIGHT JOIN Orders o ON c.CustomerID = o.CustomerID GROUP BY c.CustomerID ORDER BY COUNT(o.OrderID) DESC, c.CustomerID, COUNT(o.OrderID);
SELECT COUNT(c.CustomerID),COUNT(o.OrderID),c.CustomerID FROM Customers c FULL JOIN Orders o ON c.CustomerID = o.CustomerID GROUP BY c.CustomerID ORDER BY COUNT(o.OrderID) DESC, c.CustomerID, COUNT(o.OrderID);

--> Test HAVING with GROUP BY
SELECT c.CustomerID,MAX(o.OrderID),COUNT(o.OrderID) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID GROUP BY c.CustomerID HAVING COUNT(o.OrderID) BETWEEN 5 and 10 ORDER BY MAX(ABS(10429-o.OrderID)) DESC, c.CustomerID, COUNT(o.OrderID); -- sort-needed-check
SELECT COUNT(OrderID),CustomerID FROM Orders GROUP BY CustomerID HAVING COUNT(OrderID) BETWEEN 3 AND 6 ORDER BY COUNT(OrderID) desc, CustomerID;
SELECT COUNT(OrderID),MAX(OrderID) as maxo,MIN(OrderID) as mino,MAX(CustomerID) as maxc,MIN(CustomerID) as minc,EmployeeID FROM Orders WHERE EmployeeID BETWEEN 0 AND 7 GROUP BY EmployeeID HAVING MAX(OrderID) > 10400 AND MIN(OrderID) < 10275;
SELECT COUNT(OrderID),MAX(OrderID) as maxo,MIN(OrderID) as mino,MAX(CustomerID) as maxc,MIN(CustomerID) as minc,EmployeeID FROM Orders GROUP BY EmployeeID HAVING MAX(OrderID) > 10400 AND MIN(OrderID) < 10275 AND EmployeeID BETWEEN 0 AND 7;
SELECT c.CustomerID,COUNT(o.OrderID) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID GROUP BY c.CustomerID HAVING COUNT(o.OrderID) BETWEEN 5 and 10 ORDER BY COUNT(o.OrderID) DESC, c.CustomerID;
SELECT c.CustomerID,MAX(o.OrderID) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID GROUP BY c.CustomerID HAVING COUNT(o.OrderID) BETWEEN 5 and 10 ORDER BY COUNT(o.OrderID) DESC, c.CustomerID;
SELECT c.CustomerID,MAX(o.OrderID) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID GROUP BY c.CustomerID HAVING COUNT(o.OrderID) BETWEEN 5 and 10 ORDER BY MAX(o.OrderID) DESC, c.CustomerID;
SELECT c.CustomerID,MAX(o.OrderID) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID GROUP BY c.CustomerID HAVING COUNT(o.OrderID) BETWEEN 5 and 10 ORDER BY 2 DESC, 1;
SELECT c.CustomerID,MAX(o.OrderID),COUNT(o.OrderID) FROM Customers c LEFT JOIN Orders o ON c.CustomerID = o.CustomerID GROUP BY c.CustomerID HAVING COUNT(o.OrderID) BETWEEN 5 and 10 ORDER BY MAX(ABS(10429-o.OrderID)) DESC, c.CustomerID; -- sort-needed-check

--> Test HAVING without GROUP BY
SELECT COUNT(OrderID) FROM Orders HAVING MAX(OrderID) BETWEEN 10250 AND 10500 ORDER BY COUNT(OrderID) desc;
SELECT 1 FROM Customers WHERE Country = 'Abcd' HAVING COUNT(CustomerID) = 1;


#################################################################
#								#
# Copyright (c) 2021 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

-- TPGA000 : Database hierarchy retrieval query

SELECT
db.oid AS did, db.oid, db.datname AS name, db.dattablespace AS spcoid,
spcname, datallowconn, pg_encoding_to_char(encoding) AS encoding,
pg_get_userbyid(datdba) AS datowner, datcollate, datctype, datconnlimit,
has_database_privilege(db.oid, 'CREATE') AS cancreate,
current_setting('default_tablespace') AS default_tablespace,
descr.description AS comments, db.datistemplate AS is_template,
(SELECT array_to_string(ARRAY(
SELECT array_to_string(defaclacl::text[], ', ')
FROM pg_default_acl
WHERE defaclobjtype = 'r' AND defaclnamespace = 0::OID
), ', ')) AS tblacl,
(SELECT array_to_string(ARRAY(
SELECT array_to_string(defaclacl::text[], ', ')
FROM pg_default_acl
WHERE defaclobjtype = 'S' AND defaclnamespace = 0::OID
), ', ')) AS seqacl,
(SELECT array_to_string(ARRAY(
SELECT array_to_string(defaclacl::text[], ', ')
FROM pg_default_acl
WHERE defaclobjtype = 'f' AND defaclnamespace = 0::OID
), ', ')) AS funcacl,
(SELECT array_to_string(ARRAY(
SELECT array_to_string(defaclacl::text[], ', ')
FROM pg_default_acl
WHERE defaclobjtype = 'T' AND defaclnamespace = 0::OID
), ', ')) AS typeacl,
(SELECT array_agg(provider || '=' || label) FROM pg_shseclabel sl1 WHERE sl1.objoid=db.oid) AS seclabels,
array_to_string(datacl::text[], ', ') AS acl
FROM pg_database db
LEFT OUTER JOIN pg_tablespace ta ON db.dattablespace=ta.OID
LEFT OUTER JOIN pg_shdescription descr ON (
db.oid=descr.objoid AND descr.classoid='pg_database'::regclass
)
WHERE db.oid > 0::OID
ORDER BY datname;

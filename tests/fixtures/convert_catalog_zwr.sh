#!/bin/bash
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

# Copy PostgreSQL catalog functions without "pg_catalog" prefix
cp ./postgres-seed.zwr $1/postgres-seed.zwr
grep '\^%ydboctoocto("functions","PG_CATALOG' ./postgres-seed.zwr |
	sed 's/PG_CATALOG\.\(.*"\)/\1/' | sed '/^$/d' >> $1/postgres-seed.zwr			# Remove prefix

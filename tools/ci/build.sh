#!/bin/bash
#################################################################
#								#
# Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

source /opt/yottadb/current/ydb_env_set

start_dir=$(pwd)

# Install the POSIX plugin
cd /root
git clone https://gitlab.com/YottaDB/Util/YDBposix.git
cd YDBposix
mkdir build
cd build
cmake ..
make
make install
# Source the ENV script again to refresh mroutines
source /opt/yottadb/current/ydb_env_unset
source /opt/yottadb/current/ydb_env_set
echo "Done setting up POSIX plugin"
echo "ydb_routines: $ydb_routines"


cd $start_dir
mkdir build
cd build
git clone https://github.com/bats-core/bats-core.git
cd bats-core
./install.sh /usr/local
cd ..
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
make 2> make_warnings.txt
../tools/ci/sort_warnings.sh
echo -n "Checking for unexpected warning(s)... "
diff sorted_warnings.txt ../tools/ci/expected_warnings.ref &> differences.txt
if [ $(wc -l differences.txt | awk '{print $1}') -gt 0 ]; then
  echo "New build warnings detected!"
  cat differences.txt
  exit 1
fi
echo "OK."
source activate
pushd src
$ydb_dist/mupip set -n=true -reg '*'
# Load the data
./octo -f ../../tests/fixtures/names.sql
$ydb_dist/mupip load ../../tests/fixtures/names.zwr
popd
make test


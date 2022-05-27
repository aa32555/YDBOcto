#!/bin/bash
#################################################################
#								#
# Copyright (c) 2019-2022 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

# Set verbose mode so we see each command as it gets executed
set -v
set -x

jobname=$1	# could be "make-rocky", "make-ubuntu", "make-tls-rocky", "make-tls-rocky" or "test-auto-upgrade"
subtaskname=$2 # Could be "force" or "none" in case jobname is "test-auto-upgrade"

# Determine if we are running on Ubuntu or Rocky Linux (Centos 8 successor)
. /etc/os-release

case "$ID" in
	ubuntu)
		is_ubuntu="true"
		is_rocky8="false"
		;;
	rocky)
		is_ubuntu="false"
		is_rocky8="true"
		;;
	*)
		echo "unsupported distribution"
		exit 1
		;;
esac

source /opt/yottadb/current/ydb_env_set
set -u # Enable detection of uninitialized variables. Do *after* ydb_env_set since this script relies on uninitialized variables.
set -o pipefail	# this way $? is set to zero only if ALL commands in a pipeline succeed. Else only last command determines $?
		# For example, this ensures the "ninja | ... | grep ..." command used a little later returns a non-zero exit
		# status even if "ninja" fails (due to a build failure) and not just if "grep" fails.

start_dir=$(pwd)
# Below ensures any errors in this script cause it to exit with a non-zero status right away
set -e

# Compel use of `make` for test-auto-upgrade pipeline job to avoid test failures present with Ninja builds.
# See the note at https://gitlab.com/YottaDB/DBMS/YDBOcto/-/merge_requests/863#note_488254228 for details.
if [[ "test-auto-upgrade" == $jobname && "force" == $subtaskname ]]; then
	USE_MAKE=1
else
	USE_MAKE=0
fi

compile_octo() {
	echo "# Compile Octo"
	# We do not want any failures in "make" or "ninja" to exit the script (need to print the build errors into stdout)
	# So disable the "set -e" setting temporarily for this step.
	set +e
	# Use Ninja by default, but allow overriding it to use Make.
	if [ "$USE_MAKE" = 1 ]; then
		make -j $(grep -c ^processor /proc/cpuinfo) 2> build_warnings.txt
	else
		# Only show warnings in the GitLab UI. Show the full output in `build_warnings.txt`.
		# See https://ninja-build.org/manual.html#_environment_variables for the syntax of NINJA_STATUS.
		NINJA_STATUS="[ninja] [%f/%t] " ninja | tee build_warnings.txt | grep -v '^\[ninja\] '
	fi
	exit_status=$?
	if [[ 0 != $exit_status ]]; then
		echo "# $build_tool failed with exit status [$exit_status]. output follows below"
		cat build_warnings.txt
		exit $exit_status
	fi
	# Re-enable "set -e" now that "ninja" is done.
	set -e
}

if [ "$USE_MAKE" = 1 ]; then
	generator="Unix Makefiles"
	build_tool="make -j $(grep -c ^processor /proc/cpuinfo)"
else
	generator=Ninja
	build_tool=ninja
fi

# For now, cannot run valgrind on bats with Rocky Linux; only Ubuntu
# See this issue opened by @shabiel: https://github.com/bats-core/bats-core/issues/494
# However, we are restructuring the test as part of https://gitlab.com/YottaDB/DBMS/YDBOcto/-/issues/205,
# and therefore, we can enable the test on Rocky Linux again once we do that.
if [ "test-auto-upgrade" != $jobname ] && $is_ubuntu; then
  # Enable valgrind when running tests. This has less than a 30 second slowdown out of a 35 minute build.
  ctestCommand="ctest -T memcheck"
  use_valgrind=1
else
  ctestCommand="ctest"
  use_valgrind=0
fi
echo " -> ctestCommand = $ctestCommand"

echo "# Install the YottaDB POSIX plugin"
pushd $start_dir
./tools/ci/install_posix.sh "cmake"
./tools/ci/install_ydbaim.sh
popd

echo "# Source the ENV script again to YottaDB environment variables after installing POSIX plugin"
set +u # Temporarily disable detection of uninitialized variables since ydb_env_set relies on them.
source /opt/yottadb/current/ydb_env_unset
source /opt/yottadb/current/ydb_env_set
set -u # Re-enable detection of uninitialized variables
echo " -> Done setting up POSIX plugin"
echo " -> ydb_routines: $ydb_routines"

echo "# Download and Install BATS testing framework"
cd $start_dir
mkdir build
cd build
git clone https://github.com/bats-core/bats-core.git
cd bats-core
./install.sh /usr/local
cd ..

# Note: The below set of lines also exist in `tests/test_helpers.bash.in` so any change here might need to be made there too.
# Log env vars, shell vars, locale info, ulimit, memory/disk/cpu limits in files for later analysis of test failures.
# Mask any sensitive env vars out.
env | grep -vE "HUB_USERNAME|HUB_PASSWORD|CI_JOB_TOKEN|CI_REGISTRY_PASSWORD|CI_BUILD_TOKEN|CI_REPOSITORY_URL" > dbg_env.out
set | grep -vE "HUB_USERNAME|HUB_PASSWORD|CI_JOB_TOKEN|CI_REGISTRY_PASSWORD|CI_BUILD_TOKEN|CI_REPOSITORY_URL" > dbg_set.out
locale > dbg_locale.out
locale -a > dbg_localeall.out

# Log ulimit, memory/disk/cpu limits in files for later analysis of test failures.
# Note that the below are the same across all subtests so we don't record this in each subtest output directory so
# the below lines are not duplicated in `tests/test_helpers.bash.in`.
ulimit -a > dbg_ulimit.out	# ulimit settings of memory/file descriptors/stack etc.
free > dbg_free.out		# system RAM and SWAP
df -h > dbg_dfh.out		# system disk space
lscpu > dbg_lscpu.out		# system CPUs

echo "# Download PostgreSQL JDBC driver for testing"
export JDBC_VERSION=42.2.12
wget https://jdbc.postgresql.org/download/postgresql-$JDBC_VERSION.jar

echo "# Check repo for unused outref files"
pushd ../tests
unused_outrefs=$(../tools/ci/find_unused_outrefs.sh)
if [ "$unused_outrefs" != "" ]; then
  echo " -> Unused outrefs found!"
  echo "$unused_outrefs"
  exit 1
fi
popd

# If this is the "test-auto-upgrade" job, skip steps that are covered by other jobs (e.g. "make-ubuntu" etc.)
if [[ "test-auto-upgrade" != $jobname ]]; then
	echo "# Check repo for unused test files"
	pushd ../cmake
	unused_tests=$(../tools/ci/find_unused_tests.sh)
	if [ "$unused_tests" != "" ]; then
	  echo " -> Unused test files found!"
	  echo "$unused_tests"
	  exit 1
	fi
	popd

	# Run clang-format check on the Ubuntu pipeline jobs (i.e. "make-ubuntu" and "make-tls-ubuntu").
	# Do not run it on the Rocky linux pipeline jobs (i.e. "make-rocky" and "make-tls-rocky") as
	# we have seen that Rocky Linux could have different clang-format versions than Ubuntu
	# (at the time of this writing Rocky Linux had clang-format-12 whereas Ubuntu had clang-format-10)
	# and each could format the same C program differently causing pipeline failures. So run the
	# clang-format only on one OS. We pick Ubuntu for now. Hence the "$is_ubuntu" check below.
	if $is_ubuntu; then
		# If we found a recent enough version, run clang-format
		if CLANG_FORMAT="$(../tools/ci/find-llvm-tool.sh clang-format 9)"; then
			echo "# Check code style using clang-format"
			# This modifies the files in place so no need to record the output.
			../tools/ci/clang-format-all.sh $CLANG_FORMAT
		else
			# Otherwise, fail the pipeline.
			echo " -> A recent enough version of clang-format was not found!"
			exit 1
		fi
	fi

	if [ -x "$(command -v shellcheck)" ]; then
		find .. -name build -prune -o -name '*.sh' -print0 | xargs -0 shellcheck -e SC1091,SC2154,SC1090,SC2086,SC2053,SC2046
	else
		echo " -> Shellcheck not found!"
		exit 1
	fi
fi

pushd ..

# Confirm all error message mnemonics and text are included in the documentation
./tools/ci/doc_error_update.sh "check"

# Confirm code base assertions are still valid
./tools/ci/check_code_base_assertions.sh

popd

echo "# Randomly choose to test Debug or Release build"
if [[ $(( RANDOM % 2)) -eq 0 ]]; then
	build_type="Debug"
else
	build_type="RelWithDebInfo"
fi
echo " -> build_type = $build_type"

if [[ "test-auto-upgrade" != $jobname ]]; then
	echo "# Randomly choose whether to use the full test suite or its limited version (prefer full version 3/4 times)"
	if [[ $(( RANDOM % 4)) -eq 0 ]]; then
		full_test="OFF"
	else
		full_test="ON"
	fi
	echo "# Randomly choose whether to test from installed directory OR from build directory (prefer install 3/4 times)"
	if [[ $(( RANDOM % 4)) -eq 0 ]]; then
		disable_install="ON"
	else
		disable_install="OFF"
	fi
else
	# Always run the full test suite in case of "test-auto-upgrade" job.
	# That will give us maximum coverage for auto-upgrade testing.
	full_test="ON"
	# Disable installs for "test-auto-upgrade" as we need to run tests with 2 Octo builds and so need to keep those
	# two builds in separate subdirectories and cannot install both into $ydb_dist.
	disable_install="ON"
fi
echo " -> full_test = $full_test"
echo " -> disable_install = $disable_install"

if [[ ("test-auto-upgrade" == $jobname) && ("force" != $subtaskname) ]]; then
	if [[ $CI_COMMIT_BRANCH == "" ]]; then
		# This is possible if the pipeline runs for example when a new tag is created on a pre-existing commit.
		# (for example when the r1.0.0 tag was created). In this case, treat this job as a success.
		echo "INFO : CI_COMMIT_BRANCH env var is empty"
		echo "INFO : Cannot run the test-auto-upgrade test in this case. Exiting with success."
		exit 0
	fi
	# Record git log --all output in a file just in case it helps later. Not used by this script.
	git log --graph --all --oneline --pretty=format:'%h%d; %ai; %an; %s' > gitlogall.txt
	# Checkout a random prior commit to test if auto-upgrade of plans/xrefs/triggers/binary-table-definitions etc.
	# from that commit to the current/latest commit works fine in Octo.
	git checkout -B $CI_COMMIT_BRANCH HEAD
	# Copy M program that is needed for later before we switch to an older git branch.
	cp ../tools/ci/testAutoUpgrade.m .
	# Do not go prior to the hard stop commit (SHA pasted below) as an AUTO_UPGRADE error is issued otherwise.
	hardstopcommit=e2a016b21a1f7d9f2dc55b0655942ab7b8cdd92e
	#############################################################################################
	# First verify requirements for this test to succeed. If any of those are not met, just return success right away
	# as the auto upgrade test is not possible.
	#############################################################################################
	# Find common ancestor of upstream/master and HEAD. That is where we stop the search for a random commit
	upstream_URL=https://gitlab.com/YottaDB/DBMS/YDBOcto
	if ! git remote | grep -q upstream_repo; then
		git remote add upstream_repo "$upstream_URL"
		git fetch upstream_repo
	fi
	stopcommit=$(git merge-base HEAD upstream_repo/master)
	# Find HEAD commit to verify its not the same as $stopcommit
	HEAD_COMMIT_ID=$(git rev-list HEAD~1..HEAD)
	if [[ "$stopcommit" == "$HEAD_COMMIT_ID" ]]; then
		echo "INFO : HEAD commit and stopcommit is the same. No in between commits to choose from."
		echo "INFO : Back off stopcommit by 1 commit"
		stopcommit=$(git rev-list HEAD~2..HEAD~1)
	fi
	# Find common ancestor of $hardstopcommit and $stopcommit. Verify it is $hardstopcommit. If not, we cannot test.
	startcommit=$(git merge-base $hardstopcommit $stopcommit)
	if [[ "$startcommit" != "$hardstopcommit" ]]; then
		echo "INFO : Ancestor commit of $hardstopcommit and $stopcommit was not the former but instead is [$startcommit]"
		echo "INFO : Cannot run the test-auto-upgrade test in this case. Exiting with success."
		exit 0
	fi
	#############################################################################################
	# Now that we verified that requirements for this test are met, go ahead with the actual test.
	#############################################################################################
	git log --graph --oneline $startcommit~1..$stopcommit > gitlogmaster.txt
	# Note: The awk usage below is needed to only skip commits that branch off an otherwise linear commit history.
	awk '($1 == "*") && ($2 != "|") {print $0;}' gitlogmaster.txt > commit_history.txt
	numcommits=$(wc -l commit_history.txt | awk '{print $1}')
	while true;
	do
		commitnumber=$(shuf -i 1-$numcommits -n 1)
		commitsha=$(head -$commitnumber commit_history.txt | tail -1 | awk '{print $2}')
		if [[ "3d03de63" == "$commitsha" ]]; then
			# This commit has a known issue in `tests/fixtures/TOJ03.m` that can cause the TJC001 subtest to time out
			# (see https://gitlab.com/YottaDB/DBMS/YDBOcto/-/merge_requests/1035/pipelines for failure details).
			# This is fixed in the immediately next commit (e748ab3f) so do not choose this particular commit
			# as otherwise the "test-auto-upgrade" pipeline job (the current job) will also timeout (see
			# https://gitlab.com/YottaDB/DBMS/YDBOcto/-/merge_requests/977#note_923383570 for description of failure).
			echo "# Skipping $commitsha as it has a known issue that can cause job to timeout"
			continue
		fi
		# Now that we are here, the chosen random older commit has no known issue. So break out of the while loop.
		break
	done
	echo $commitsha > commit_picked.txt
	# BEGIN For Developers Troubleshooting Autoupgrade pipelines: Set old commit here
	# commitsha=cc515a49
	# END For Developers Troubleshooting Autoupgrade pipelines
	echo "# Random older commit picked = $commitsha"
	echo "# Checkout the older commit"
	git checkout $commitsha
	# Due to https://gitlab.com/YottaDB/DBMS/YDBOcto/-/issues/712 and # https://gitlab.com/YottaDB/DB/YDB/-/issues/661, ensure
	# that test framework files corresponding to an older commit are updated minimally enough so they will work with a
	# later/newer version of ydb_env_set.
	git checkout 8587b12086666c88ea2c8a19b55a736629269907 -- ../tools/get_ydb_release.sh
	sed -i 's/unset ydb_chset/export ydb_chset=M/' ../tests/test_helpers.bash.in
	# Run only a random fraction of the bats tests as we will be running an auto upgrade test on the same queries
	# once more a little later.
	cp ../cmake/bats-tests.cmake bats-tests.cmake.orig
	# Temporarily switch ydb_routines for running M program (testAutoUpgrade.m)
	saveydbroutines="$ydb_routines"
	export ydb_routines="."	# so testAutoUpgrade.o gets created in current directory
	$ydb_dist/yottadb -run batsTestsChooseRandom^testAutoUpgrade < bats-tests.cmake.orig > bats-tests.cmake.new
	export ydb_routines="$saveydbroutines"	# Switch back to original ydb_routines
	cp bats-tests.cmake.new ../cmake/bats-tests.cmake
else
	# If a "test-auto-upgrade" job and an old commit was chosen, we could see rare failures.
	# Don't want that to pollute the output.
	# Hence the below CTEST_OUTPUT_ON_FAILURE=TRUE setting is done only in other jobs.
	export CTEST_OUTPUT_ON_FAILURE=TRUE
fi

# This function is duplicated in tools/ci/vistatest.sh
cleanup_before_exit() {
	echo "# Cleanup files and directories that don't need to be included in the pipeline artifacts"
	rm -rf CMakeFiles _CPack_Packages bats-test.*/go src/CMakeFiles || true
	rm -f postgresql*.jar ./*.cmake || true
	rm -f src/test_* || true	# these are the unit test case executables (should not be needed otherwise)
	rm -f src/*.dbg
}
trap cleanup_before_exit EXIT

echo "# Configure the build system for Octo"

cmake -G "$generator" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_INSTALL_PREFIX=${ydb_dist}/plugin -DCMAKE_BUILD_TYPE=$build_type -DFULL_TEST_SUITE=$full_test -DDISABLE_INSTALL=$disable_install ..
compile_octo

# If this is the "test-auto-upgrade" job, skip steps that are covered by other jobs (e.g. "make-ubuntu" etc.)
if [[ "test-auto-upgrade" != $jobname ]]; then
	echo "# Check for unexpected warnings and error/exit if unexpected errors are found"
	../tools/ci/sort_warnings.sh build_warnings.txt sorted_build_warnings.txt
	echo " -> Checking for unexpected warning(s) while compiling ... "
	if [[ $build_type == "Debug" ]]; then
		reference=../tools/ci/expected_warnings.ref
	else
		reference=../tools/ci/expected_warnings-release.ref
	fi

	compare() {
		expected="$1"
		actual="$2"
		full_warnings="$3"
		# We do not want any failures in "diff" command below to exit the script (we want to see the actual diff a few steps later).
		# So never count this step as failing even if the output does not match.
		diff "$expected" "$actual" &> differences.txt || true

		if [ $(wc -l differences.txt | awk '{print $1}') -gt 0 ]; then
			set +x  # don't print these diagnostics twice
			echo " -> Expected warnings differ from actual warnings! diff output follows"
			echo " -> note: '<' indicates an expected warning, '>' indicates an actual warning"
			echo " -> help: you can see the full warnings at $CI_JOB_URL/artifacts/raw/build/$3"
			if echo "$full_warnings" | grep -q clang_tidy; then
				echo " -> help: to generate clang-tidy warnings locally, run 'cmake -D CMAKE_EXPORT_COMPILE_COMMANDS=ON .. && ../tools/ci/clang-tidy-all.sh'"
			fi
			cat differences.txt
			set -x
			exit 1
		fi
	}
	compare $reference sorted_build_warnings.txt build_warnings.txt

	echo "# Check for unexpected warning(s) from clang-tidy ..."
	../tools/ci/clang-tidy-all.sh > clang_tidy_warnings.txt 2>/dev/null
	../tools/ci/sort_warnings.sh clang_tidy_warnings.txt sorted_clang_warnings.txt
	# In release mode, `assert`s are compiled out and clang-tidy will emit false positives.
	if [ "$build_type" = Debug ]; then
		compare ../tools/ci/clang_tidy_warnings.ref sorted_clang_warnings.txt clang_tidy_warnings.txt
	elif $is_rocky8; then
		# Rocky Linux has a newer version of clang (clang-12) compared to Ubuntu (clang-10) at the time of this writing.
		# And therefore gives a slightly different warning. Hence the need to maintain a rocky specific warning file.
		# We could have chosen to run the clang test only on Ubuntu but that might run the risk of not catching issues
		# shown by a newer clang-tidy. Hence we decided to maintain almost duplicate reference files
		# tools/ci/clang_tidy_warnings_rocky-release.ref and tools/ci/clang_tidy_warnings-release.ref.
		compare ../tools/ci/clang_tidy_warnings_rocky-release.ref sorted_clang_warnings.txt clang_tidy_warnings.txt
	else
		compare ../tools/ci/clang_tidy_warnings-release.ref sorted_clang_warnings.txt clang_tidy_warnings.txt
	fi

	echo "# prepare binary tarball"
	# Declare the tarball generation logic as a function in case we need to rebuild in release mode for Docker image creation
	create_tarball() {
		# Gather elements of tarball name format: yottadb_octo_<octo_version>_<os_id><os_version>_<platform_arch>_pro.tar.gz
		octo_version="$(src/octo --version | grep "Octo version" | cut -f 3 -d ' ')"
		os_id="$(../tools/get_platform_name.sh)"
		os_version="$(../tools/get_platform_version.sh)"
		platform_arch="$(../tools/get_platform_arch.sh)"
		if [[ -f $ydb_dist/plugin/libgtmtls.so ]]; then
			tls_support="tls_"
		else
			tls_support=""
		fi
		tarball_name="yottadb_octo_${octo_version}_${tls_support}${os_id}${os_version}_${platform_arch}_pro"

		# Transfer requisite files into tarball directory and compress
		echo "# Create plugin directory structure for later reference by [octo]install.sh"
		mkdir -p $tarball_name/plugin/r $tarball_name/plugin/o/utf8 $tarball_name/plugin/octo/bin
		echo "# Copy YDBPosix into build directory for later access by [octo]install.sh"
		cp $ydb_dist/plugin/libydbposix.so $tarball_name/plugin
		cp $ydb_dist/plugin/ydbposix.xc $tarball_name/plugin
		cp $ydb_dist/plugin/o/_ydbposix.so $tarball_name/plugin/o
		cp $ydb_dist/plugin/o/utf8/_ydbposix.so $tarball_name/plugin/o/utf8
		echo "# Copy Octo-specific dependencies for later access by [octo]install.sh"
		cp octoinstall.sh $tarball_name
		cp ../tools/get_ydb_release.sh $tarball_name
		cp ../tools/get_platform_name.sh $tarball_name
		cp ../tools/get_platform_version.sh $tarball_name
		cp ../tools/get_platform_arch.sh $tarball_name
		cp ../src/aux/*.m $tarball_name/plugin/r
		cp src/ydbocto.ci $tarball_name/plugin/octo
		cp src/ydbocto.xc $tarball_name/plugin/octo
		cp src/octo-seed.* $tarball_name/plugin/octo
		cp ../src/aux/octo.conf.default $tarball_name/plugin/octo/octo.conf
		echo "# Copy Octo binaries and libraries for later access by [octo]install.sh"
		cp src/octo src/rocto $tarball_name/plugin/octo/bin
		cp src/_ydbocto.so $tarball_name/plugin/o
		cp src/libcocto.so $tarball_name/plugin/o
		cp src/utf8/_ydbocto.so $tarball_name/plugin/o/utf8
		echo "# Copy .dbg files for debugging RelWithDebInfo builds"
		if [[ -f src/octo.dbg && -f src/rocto.dbg ]]; then
			cp src/*.dbg $tarball_name/plugin/octo
		fi

		echo "# Build binary package"
		tar -czvf $tarball_name.tar.gz $tarball_name
	}
	create_tarball

	echo "# Randomly choose to install from tarball or via make/ninja install"
	if [[ $(( RANDOM % 2)) -eq 0 ]]; then
		echo "# install from tarball"
		cd $tarball_name
		./octoinstall.sh
		cd ..
	else
		echo "# $build_tool install"
		$build_tool install
	fi
fi

# Skip Postgres setup for the forced auto upgrade job as it does not use psql. All the other jobs use it.
if [[ ("test-auto-upgrade" != $jobname) || ("force" != $subtaskname) ]]; then
	set +u # Temporarily disable detection of uninitialized variables for the following -z null check
	if [ -z $USER ]; then
	  echo " -> export USER=root"
	  export USER=root
	fi
	set -u

	echo "# Start PostgreSQL Server"
	if $is_ubuntu; then
	  /etc/init.d/postgresql start
	elif $is_rocky8; then
	  cp ../tools/ci/postgres-rocky/postgresql-setup /usr/bin/postgresql-setup
	  chmod +x /usr/bin/postgresql-setup
	  postgresql-setup initdb
	  mv ../tools/ci/postgres-rocky/postgresql.conf /var/lib/pgsql/data/postgresql.conf
	  chown -v postgres.postgres /var/lib/pgsql/data/postgresql.conf
	  su postgres -c "/usr/bin/postgres -D /var/lib/pgsql/data -p 5432" &
	  sleep 2
	fi

	echo "# Make the current user a superuser"
	su - postgres -c psql <<PSQL
	create user $USER;
	alter user $USER SUPERUSER;
PSQL
fi

echo "# Setup for tests"
pushd src
# The below step is not needed by the bats tests as they do their own database initialization (in "init_test" function)
# but cmocka tests do not have any such facility hence require this setup.
$ydb_dist/mupip set -null_subscripts=always -reg '*'

echo "# Source ydb_env_set after building and installing Octo"
if [[ $disable_install == "OFF" ]]; then
	set +u # Temporarily disable detection of uninitialized variables since ydb_env_set relies on them.
	source /opt/yottadb/current/ydb_env_unset
	source /opt/yottadb/current/ydb_env_set
	set -u # Re-enable detection of uninitialized variables
	echo " -> Done setting up Octo plugin"
	echo " -> ydb_routines: $ydb_routines"
else
	# In case DISABLE_INSTALL = ON, we need to set the correct UTF-8 directory for $ZROUTINES
	# In case DISABLE_INSTALL = OFF, ydb_env_set takes care of that for us.
	if [ "$ydb_chset" = "UTF-8" ]; then
		ydb_routines="$(pwd)/utf8/_ydbocto.so $ydb_routines"
	else
		ydb_routines="$(pwd)/_ydbocto.so $ydb_routines"
	fi
	export ydb_routines
	echo " -> ydb_routines: $ydb_routines"
fi

popd

if [[ ("test-auto-upgrade" != $jobname) || ("force" != $subtaskname) ]]; then
	# Force password authentication for PSQL by revising and reloading the config file. This is needed to prevent authentication
	# failures of the form "FATAL: Ident authentication failed for user ..." when attempting to connect to the PostgreSQL server.
	if $is_ubuntu; then
		# Ubuntu
		psql_conf=$(find /etc/postgresql -name "pg_hba.conf")
	elif $is_rocky8; then
		# Rocky Linux
		psql_conf=$(find /var/lib/pgsql -name "pg_hba.conf")
	fi
	sed -i "s/ident/md5/" $psql_conf
	psql postgres <<PSQL
	SELECT pg_reload_conf();
PSQL

	echo "# Run the tests"
	# We do not want any failures in "ctest" to exit the script (need to do some cleanup so the artifacts
	# are not that huge etc.). So disable the "set -e" setting temporarily for this step.
	set +e
	${ctestCommand} -j $(grep -c ^processor /proc/cpuinfo)
	exit_status=$?
	echo " -> exit_status from ${ctestCommand} = $exit_status"

	# If we ran valgrind, ctest puts the logs in a different file for some reason.
	if [ $use_valgrind = 1 ]; then
		mv Testing/Temporary/LastDynamicAnalysis* Testing/Temporary/LastTest.log
	fi

	# This block and much under it is duplicated in tools/ci/vistatest.sh
	# Re-enable "set -e" now that ctest is done.
	set -e
	# Unset verbose mode as the below for loop and bats-test.* usages can print thousands of lines
	# and/or very long lines that can pollute the pipeline console output
	set +v
	set +x
	# Find out list of passed bats dirs. sortis not necessary but nice to have.
	find . -maxdepth 1 -type d | sed 's#^\./##' | grep '^bats-test' | sort > all_bats_dirs.txt
	# Find out list of failed bats dirs. sort is not necessary but nice to have.
	# Note that "grep" exits with status of 1 if no match is found. But we do not want the script to error (due to "set -e")
	# because of this ("set -o pipefail" will cause final exit status of pipeline to be non-zero if at least one of the
	# commands exits with a non-zero status). Hence the "|| true" usage below.
	grep "Temporary files in" Testing/Temporary/LastTest.log || true | awk '{print $NF}' | sed 's,.*/,,g' | sort > failed_bats_dirs.txt
	# Update "passed_bats_dirs.txt" for use by a later stage (to remove passed directories and reduce pipeline artifact size)
	# No need to do this for "test-auto-upgrade" job as it does not use this file. And it is actually not correct to run
	# this code for that job as we are running an older commit and it could have multiple subtests with name ambiguity
	# which would then cause this script (that belongs to the latest commit) to incorrectly (and prematurely) "exit 1" below.
	if [[ ("test-auto-upgrade" != $jobname) ]]; then
		# Note down list of bats test directory names and corresponding subtest name in one file
		cat ./*/bats_test.out > all_bats_test.out
		ls -lart ./*/bats_test.out > lslart_bats_test.out	# this is to note down time stamp of the bats_test.out files
		grep '^ok' Testing/Temporary/LastTest.log > passed_bats_subtests.txt || true
		grep '^not ok' Testing/Temporary/LastTest.log > failed_bats_subtests.txt || true
		touch summary_bats_dirs.txt passed_bats_dirs.txt
		# Find out list of bats dirs corresponding to passed subtests.
		for tstdir in bats-test.*
		do
			if [[ ! -d $tstdir || ! -e $tstdir/bats_test.out ]]; then
				# $tstdir is not a directory OR it does not contain the file bats_test.out.
				# Cannot determine if this is a failed or passed or timedout bats test.
				echo "SUSPECT : $tstdir" >> summary_bats_dirs.txt
				continue
			fi
			cd $tstdir
			subtest=$(sed 's/.*subtest \[//;s/].*//;' bats_test.out)
			# Need -F below in case there are any special characters in the subtest name (e.g. '*')
			# We do not want to treat those as regex in the grep.
			passed=$(grep -F -c "$subtest" ../passed_bats_subtests.txt) || true
			failed=$(grep -F -c "$subtest" ../failed_bats_subtests.txt) || true
			if [[ $((passed + failed)) -gt 1 ]]; then
				echo " --> Multiple subtests with name [$subtest] found in passed_bats_subtests.txt and/or failed_bats_subtests.txt"
				echo " --> Please first fix ambiguity by giving the subtests unique names. Exiting."
				echo " --> List of subtests found is pasted below."
				grep -F "$subtest" ../passed_bats_subtests.txt ../failed_bats_subtests.txt
				exit 1
			elif [[ $passed -eq 1 ]]; then
				echo "PASSED  : $tstdir : $subtest" >> ../summary_bats_dirs.txt
				echo $tstdir >> ../passed_bats_dirs.txt
			elif [[ $failed -eq 1 ]]; then
				echo "FAILED  : $tstdir : $subtest" >> ../summary_bats_dirs.txt
			else
				# It has to be a timed out test. It is also possible some passed/failed subtests show up here
				# in case "$subtest" matched multiple lines. If so, treat that as a timedout directory for now.
				echo "TIMEDOUT : $tstdir : $subtest" >> ../summary_bats_dirs.txt
			fi
			cd ..
		done
	fi
	# Restore verbose output now that for loop and bats-test.* usages (long/lots-of lines) are done
	set -v
	set -x
fi

if [[ "test-auto-upgrade" != $jobname ]]; then
	if [[ -s passed_bats_dirs.txt ]]; then
		echo '# Remove "bats-test*" directories corresponding to passed subtests (reduces pipeline artifact size)'
		# Unset verbose mode as the below can print a very long line of output
		# and pollute the pipeline console output
		set +v
		set +x
		rm -rf $(cat passed_bats_dirs.txt)
		# Restore verbose output now that for long line of output is done
		set -v
		set -x
	fi
	# Don't print hundreds of lines of logfiles
	set +x
	for file in Testing/Temporary/MemoryChecker.*.log; do
		if [ -s $file ]; then
			echo "ERROR  : Octo leaked memory or accessed uninitialized bytes in build/$file"
			exit_status=1
		fi
	done
	set -x
	if [[ 0 == $exit_status ]]; then
		if [[ $build_type != "RelWithDebInfo" || $disable_install != "OFF" ]]; then
			echo "# Rebuild Octo for packaging as it wasn't a RelWithDebInfo build or was built with installation disabled"
			cmake -G "$generator" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_INSTALL_PREFIX=${ydb_dist}/plugin -DCMAKE_BUILD_TYPE=RelWithDebInfo -DDISABLE_INSTALL=OFF ..
			$build_tool
			create_tarball
		fi

		if $is_ubuntu; then
			echo "# Ubuntu pipelines only: Copy installation script into tarball directory for use in Docker image construction"
			cp ../tools/ci/docker-install.sh $tarball_name
			echo "# Copy dummy data for use in Docker image. No other fixtures are needed as Northwind tests full functionality"
			cp ../tests/fixtures/northwind.* $tarball_name
		fi
	fi
else
	# If this is the "test-auto-upgrade" job, ignore errors in ctest (possible some tests fail because we are running
	# tests using an older Octo commit that had a bug which was fixed afterwards). We will test auto-upgrade on the
	# passing subtests only.
	exit_status=0
	echo "# Cleanup unit test case executables from oldsrc directory"
	cleanup_before_exit
	echo '# Move old commit build of Octo to [oldsrc] directory'
	mv src oldsrc
	echo '# Delete cmake/make artifacts of older Octo build to make way for newer Octo build'
	rm -rf CMakeCache.txt CMakeFiles
	if [[ "force" != $subtaskname ]]; then
		echo '# Reset git repo to latest commit branch'
		git reset --hard $CI_COMMIT_BRANCH
		echo '# Rebuild Octo using the latest commit branch for the auto-upgrade test'
		cmakeflags="-DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_INSTALL_PREFIX=${ydb_dist}/plugin"
	else
		cmakeflags="-DFORCE_BINARY_DEFINITION_AUTO_UPGRADE=ON"	# Force auto upgrade
	fi
	cmakeflags="$cmakeflags -DCMAKE_BUILD_TYPE=$build_type -DFULL_TEST_SUITE=$full_test"
	cmakeflags="$cmakeflags -DDISABLE_INSTALL=$disable_install"
	# Randomly select a power of two to use for altering the size of OCTO_INIT_BUFFER_LEN to test for regressions
	new_buffer_size=$(( 2 ** (RANDOM % 11) ))
	sed -i "s/OCTO_INIT_BUFFER_LEN [0-9]*/OCTO_INIT_BUFFER_LEN $new_buffer_size/" ../src/octo.h

	cmake -G "$generator" $cmakeflags ..
	compile_octo
	echo "# Cleanup unit test case executables from newsrc directory"
	rm -rf src/CMakeFiles
	rm -f src/test_*	# these are the unit test case executables (should not be needed otherwise)
	echo '# Move new commit build of Octo to [newsrc] directory'
	mv src newsrc
	echo '# Delete cmake/make artifacts of newer Octo build'
	rm -rf CMakeCache.txt CMakeFiles
	# Unset verbose mode as the below for loop can print thousands of lines and pollute the pipeline console output
	set +x
	set +v
	if [[ "force" != $subtaskname ]]; then
		echo '# Remove "bats-test*" directories corresponding to failed subtests (if any)'
		rm -rf $(cat failed_bats_dirs.txt)
		echo '# Do auto-upgrade tests on the leftover "bats-test*" directories.'
		gldfile="yottadb.gld"
		export ydb_gbldir=$gldfile
		defaultdat="mumps.dat"
		octodat="octo.dat"
		aimdat="aim.dat"
		touch skip_bats_test.txt gde_change_segment.txt
		ydb_icu_version=$(pkg-config --modversion icu-io)	# needed for UTF-8 chset in for loop below
		export ydb_icu_version

		# Note down if older commit is prior to the YDBOcto#275 commit when NULL and empty string began to be
		# treated the same. This will be used later to skip a few tests.
		pre_octo275_commit="babc2e2e78eb00813cb5d76a8f2bbda66742c1b7"	# 1 commit before the #275 commit
		# Disable the "set -e" setting temporarily as the "git merge-base" can return exit status 0 or 1
		set +e
		git merge-base --is-ancestor $commitsha $pre_octo275_commit
		is_post_octo275_commit=$?
		# Re-enable "set -e" now that "git merge-base" invocation is done.
		set -e

		# Note down if older commit is prior to the YDBOcto#649 commit. This will be used later to skip a few tests.
		pre_octo649_commit="9c64861100d7f6c6653a75f7b06f036465c2f486"	# 1 commit before the #649 commit
		# Disable the "set -e" setting temporarily as the "git merge-base" can return exit status 0 or 1
		set +e
		git merge-base --is-ancestor $commitsha $pre_octo649_commit
		is_post_octo649_commit=$?
		# Re-enable "set -e" now that "git merge-base" invocation is done.
		set -e

		# Note down if older commit is prior to the YDBAIM implementation commit
		# If older commit, then we won't have an aim.dat, and we need to create the
		# segment, region and data file
		pre_ydbaim_commit="809ed8d862be726c80a4ef1f4f43bebac1e9bc7b"    # 1 commit before the AIM commit
		# Disable the "set -e" setting temporarily as the "git merge-base" can return exit status 0 or 1
		set +e
		git merge-base --is-ancestor $commitsha $pre_ydbaim_commit
		is_post_ydbaim_commit=$?
		# Re-enable "set -e" now that "git merge-base" invocation is done.
		set -e

		# Point src to newsrc
		ln -s newsrc src
		for tstdir in bats-test.*
		do
			cd $tstdir
			if [[ ! -e $gldfile || ! -e $defaultdat || ! -e $octodat ]]; then
				# This test directory does not contain a 2-region octo setup. auto-upgrade cannot be tested here. Skip.
				# Okay to have AIM region not listed here
				echo "SKIPPED : $tstdir : Does not contain $gldfile or $defaultdat or $octodat" >> ../bats_test.txt
				cd ..
				rm -rf $tstdir
				continue
			fi
			if ! ls ./*.sql 1> /dev/null 2>&1; then
				# This test directory does not contain any "*.sql" files. Skip auto-upgrade test.
				echo "SKIPPED : $tstdir : Does not contain *.sql files" >> ../bats_test.txt
				cd ..
				rm -rf $tstdir
				continue
			fi
			subtest=$(sed 's/.*subtest \[//;s/].*//;' bats_test.out)
			# ----------------------------------------------------------------------------
			# Exclude specific set of subtests (reasons explained below)
			# ----------------------------------------------------------------------------
			# 1) The TC011 subtest used to have invalid queries which issue errors in later commits
			#    For example, "CREATE TABLE names (id NUMERIC(16,18) PRIMARY KEY ..." used to work fine before
			#    But would issue a "ERR_NUMERIC_SCALE" error due to the YDBOcto#636 fixes.
			#    Therefore, the output of octo against these queries using an older commit and the current commit
			#    could be different (depending on the randomly chosen older commit). And that in turn would
			#    cause the "test-auto-upgrade" pipeline job to signal a false failure. Therefore skip this subtest.
			# 2) The TPC019 subtest runs the query only with "rocto" and expects a ERR_ROCTO_QUERY_TOO_LONG error.
			#    The same error does not happen with "octo" (which means the query will run further along and
			#    terminate abnormally due to not enough "stacksize" limit) which is what this test will use.
			#    Therefore skip this subtest.
			# 3) The TQG03 subtest randomly chooses "nullcharnames" schema and before the YDBOcto#275 commit,
			#    it could use the "NULLCHAR" keyword in the "CREATE TABLE" command which is no longer supported
			#    after the YDBOcto#275 commit. Therefore the auto upgrade would issue a syntax error in that case
			#    as it sees an unsupported NULLCHAR keyword in the "NULLCHARNAMES" table's text definition.
			#    Therefore skip this subtest if the random older commit is prior to the YDBOcto#275 commit.
			# 4) All the TQG* subtests (TQG01, TQG02, TQG03, TQG04, TQG05, TQG06 as of now) could use the empty
			#    string in their queries if the older commit was prior to the YDBOcto#275 commit. Those queries
			#    could give different output in the newer build since the empty string will now be treated as NULL.
			#    Therefore, skip all TQG* subtests if the random older commit is prior to the YDBOcto#275 commit.
			#    Note that (3) is subsumed by (4) so just one check is needed below for both those cases.
			# 5) By a similar reasoning as (4), the TJC004/TJC005/TJC006/TJC007 subtests in the
			#    "test_jdbc_connection" test use the same query generator that the "TQG*" subtests use.
			#    And so need to be skipped for the same conditions as TQG*.
			# 6) By a similar reasoning, the TC027/TC033/TC034 subtests (in the "test_createtable" bats test) used
			#    "NULLCHAR" previously and so have to be skipped.
			# 7) The "TLQ02" subtest runs a large query which require a lot of memory when rerun in the
			#    test-auto-upgrade job (VmSize value in /proc/PID/status goes as high as 3.3GiB) and causes the
			#    process to get "Killed" in the gitlab pipelines (likely the OOM killer kicks in) so skip this.
			#    Note though that as part of YDBOcto#649, this query was reduced 10x and so we need to skip only
			#    if the random prior commit is older than the #649 commit.
			# ----------------------------------------------------------------------------
			if [[ ($subtest =~ "TC011 : ") || ($subtest =~ "TPC019 : ")                    \
					|| (($subtest =~ ^"TQG") && (0 == $is_post_octo275_commit))    \
					|| (($subtest =~ ^"TJC004") && (0 == $is_post_octo275_commit)) \
					|| (($subtest =~ ^"TJC005") && (0 == $is_post_octo275_commit)) \
					|| (($subtest =~ ^"TJC006") && (0 == $is_post_octo275_commit)) \
					|| (($subtest =~ ^"TJC007") && (0 == $is_post_octo275_commit)) \
					|| (($subtest =~ ^"TC027") && (0 == $is_post_octo275_commit))  \
					|| (($subtest =~ ^"TC033") && (0 == $is_post_octo275_commit))  \
					|| (($subtest =~ ^"TC034") && (0 == $is_post_octo275_commit))  \
					|| (($subtest =~ ^"TLQ02") && (0 == $is_post_octo649_commit)) ]]; then
				echo "SKIPPED : $tstdir : [subtest : $subtest]" >> ../bats_test.txt
				cd ..
				rm -rf $tstdir
				continue
			fi
			echo "# Running *.sql files in $tstdir : [subtest : $subtest]" | tee -a ../errors.log
			echo "INCLUDE : $tstdir" >> ../include_bats_test.txt
			# Check if subtest ran in M or UTF-8 mode and switch ydb_chset and ydb_routines accordingly
			# A few older commits had env vars logged in the file "env.out" but later commits changed it
			# to "dbg_env.out" so allow for either of those files since we don't know which older commit
			# got randomly picked.
			if [[ ! -e dbg_env.out ]]; then
				# "dbg_env.out" does not exist. This means it is an older commit that stored env vars in
				# the file "env.out". So rename it to "dbg_env.out" just like more recent commits would do.
				mv env.out dbg_env.out
			fi
			# Now that we are guaranteed env vars are in the file "dbg_env.out" search that file for whether
			# the chset was M or UTF-8.
			is_utf8=$(grep -c "^ydb_chset=UTF-8" dbg_env.out) || true
			if [[ $is_utf8 == 0 ]]; then
				export ydb_chset=M
				utf8_path="."
			else
				export ydb_chset=UTF-8
				utf8_path="utf8"
			fi
			export ydb_routines=". ../newsrc/$utf8_path/_ydbocto.so $ydb_dist/plugin/o/$utf8_path/_ydbposix.so $ydb_dist/plugin/o/$utf8_path/_ydbaim.so $ydb_dist/$utf8_path/libyottadbutil.so"
			# Create the AIM database if the previous commit predates AIM
			if [ 0 -eq $is_post_ydbaim_commit ]; then
				echo "# Previous commit predates AIM; create AIM database for upgrade"
				$ydb_dist/yottadb -run ^GDE > gde_create_aim.txt 2>&1 << AIM
add -segment AIMSEG -file="$aimdat" -access_method=MM -block_size=2048
add -region AIMREG -dyn=AIMSEG -nojournal -key_size=1019 -null_subscripts=always -record_size=2048
add -name %ydbAIM* -region=AIMREG
exit
AIM
				mupip create -region=AIMREG &>> gde_create_aim.txt
			fi
			# Change absolute path names of database files to relative path names for ease of later debugging (if needed)
			$ydb_dist/yottadb -run GDE >> gde_change_segment.txt 2>&1 << FILE
			change -segment DEFAULT -file_name=$defaultdat
			change -segment OCTOSEG -file_name=$octodat
			change -segment AIMSEG -file_name=$aimdat
FILE

			# TEST1 and TEST2 below together test that Octo automatically recreates any
			# binary-definitions/plans/xrefs/triggers as needed thereby testing YDBOcto#90.
			errors_found=0
			for sqlfile in *.sql
			do
				# TEST1
				# We do not want any failures in the "octo" invocation below to exit the script.
				# So disable the "set -e" setting temporarily for this step.
				set +e
				outfile="autoupgrade.$sqlfile.out"
				../newsrc/octo -f $sqlfile > $outfile 2>&1
				ret_status=$?
				# Re-enable "set -e" now that "octo" invocation is done.
				set -e
				if [[ 1 -lt $ret_status ]]; then
					# Invoking newer build of Octo on environment set up by older Octo resulted in a
					# non-zero exit status greater than 1. This most likely means a fatal error like a SIG-11 or
					# Assert failure etc. Octo exits with 0 for success and 1 when an error is encountered,
					# while asserts exit with 250 and SIG-11s with 245. Since we are only testing auto-upgrade
					# behavior here, we can accept an exit code of 1, as this will be issued only for failing
					# queries, which are expected in some test cases as of this writing. For all other error
					# codes, fail and record the error.
					echo " ERROR : [newsrc/octo -f $tstdir/$sqlfile] > autoupgrade.$sqlfile.out : Exit status = $ret_status" | tee -a ../errors.log
					echo " ERROR :   --> It is likely that bumping up FMT_BINARY_DEFINITION would fix such failures" | tee -a ../errors.log
					exit_status=1
					errors_found=1
				fi
				# If this is a test output directory for the "test_query_generator" test, then do additional
				# testing of actual output. We expect the output to be identical between the older commit and
				# the current commit even though the current commit reused binary table/function defnitions and
				# plans/triggers/xrefs generated by the older commit. We can do actual output verification of the
				# "test_query_generator" test because we know this test validates Octo's output against Postgres
				# and we do not expect any errors in the output of Octo using either the older or newer commit.
				#
				# Sometimes TQG* subtests store the octo output in files of the form (if they go through JDBC driver)
				#	TQG06-0_nocomment-000.sql.octo.out
				# Sometimes they store it in files of the form (if they do not go through JDBC driver)
				#	TQG01-0-000.octo.out
				# Handle both cases below.
				octooutfile=""
				if [[ -e $sqlfile.octo.out ]]; then
					octooutfile="$sqlfile.octo.out"
					usedjdbcdriver=1
				else
					# shellcheck disable=SC2001
					basename=$(echo $sqlfile | sed 's/\.sql//g')
					if [[ -e $basename ]]; then
						octooutfile=$basename
					fi
					usedjdbcdriver=0
				fi
				if [[ ($subtest =~ ^"TQG") && (-n $octooutfile) ]]; then
					# a) If the random older commit predates the YDBOcto#649 commit, then the octo output
					#    would not contain the row-header and row summary line at the head and tail of the octo output.
					#    So filter that out from the newer Octo build output.
					# b) If the random older commit postdates the YDBOcto#649 commit, even then the octo output would
					#    not contain the row-header and row summary line if the test ran using the JDBC driver.
					#    So remove the same lines from the octo output of the newer commit.
					pre_octo649_commit="9c64861100d7f6c6653a75f7b06f036465c2f486"
					# Disable the "set -e" setting temporarily as the "git merge-base" can return exit status 0 or 1
					set +e
					git merge-base --is-ancestor $commitsha $pre_octo649_commit
					is_post_octo649_commit=$?
					# Re-enable "set -e" now that "git merge-base" invocation is done.
					set -e
					if [[ (0 == $is_post_octo649_commit) || (0 != $usedjdbcdriver) ]]; then
						mv $outfile $outfile.tmp
						tail -n +2 $outfile.tmp | head -n -1 > $outfile
					fi
					# TEST2
					# $sqlfile.log is Octo's output for the same query using the older commit build
					# It could contain "null" references if it was run through the JDBC driver.
					# Replace that with the empty string for the below diff since we did not use the JDBC
					# driver for the newer Octo build output.
					reffile="autoupgrade.$sqlfile.ref"
					sed 's/^null$//;s/^null|/|/;s/|\<null\>/|/g' $octooutfile > $reffile
					# Check if the output required sorting. We deduce this from presence of *unsorted* files.
					logfile="autoupgrade.$sqlfile.log"
					if compgen -G "$sqlfile.unsorted.**" > /dev/null; then
						mv $reffile $reffile.unsorted
						sort $reffile.unsorted > $reffile
						sort $outfile > $logfile
					else
						cp $outfile $logfile
					fi
					# Check if the output required only a rowcount check ("-- rowcount-only-check" in query file)
					# This is deduced from the presence of *.diff files. If it is present, then a diff was done.
					# If it is not, a rowcount check was done when the original test ran. Do the same thing below
					# with the newer build of Octo.
					if [[ -e $sqlfile.diff ]]; then
						# .diff file exists. This means an actual diff was done. Do a diff in the new Octo build too.
						difffile="autoupgrade.$sqlfile.diff"
						diff $reffile $logfile > $difffile || true
						if [[ -s $difffile ]]; then
							echo "ERROR : [diff $reffile $logfile] returned non-zero diff. See $difffile for details" | tee -a ../errors.log
							echo "ERROR :   --> It is likely that bumping up FMT_PLAN_DEFINITION would fix such failures" | tee -a ../errors.log
							exit_status=1
							errors_found=1
						fi
					else
						# .diff file does not exist. This means only a rowcount check was done. Do the same check with the newer Octo build output.
						oldoctolines=$(wc -l $reffile | awk '{print $1}')
						newoctolines=$(wc -l $logfile | awk '{print $1}')
						if [[ $oldoctolines -ne $newoctolines ]]; then
							echo "ERROR : [$reffile has $oldoctolines lines but $logfile contains $newoctolines lines]" | tee -a ../errors.log
							exit_status=1
							errors_found=1
						fi
					fi
				fi
			done
			cd ..
			if [[ 0 == $errors_found ]]; then
				# No auto-upgrade related errors found in this bats test directory.
				# Delete this directory before moving on (reduces size of pipeline artifacts in case of failure).
				rm -rf $tstdir
			fi
		done
	else
		# Find out all "CREATE TABLE" queries in tests/fixtures/*.sql. Generate one query file for each.
		# Filter out lines like "\set ON_ERROR_STOP on" that are in tests/fixtures/postgres-*.sql files
		# as they confuse split_queries.py. Also filter out queries with errors that are in TERR*.sql files
		# and exclude `mysql-*-.sql` symbolic links from `auto-upgrade` cleanup.
		grep --exclude=mysql-*.sql -l "CREATE TABLE" ../tests/fixtures/*.sql | grep -v TERR | xargs cat | grep -v ON_ERROR_STOP > create_table.sql
		../tests/fixtures/sqllogic/split_queries.py create_table.sql "CREATE TABLE"
		# Create *.gld and *.dat files
		rm -f ./*.gld ./*.dat || true
		# Point src to newsrc so GDE works fine or else ZROSYNTAX error would be issued.
		rm -f src || true; ln -s newsrc src
		export ydb_gbldir="yottadb.gld"
		$ydb_dist/yottadb -run ^GDE <<FILE
		change -region DEFAULT -null_subscripts=true -record_size=1048576
		change -segment DEFAULT -file_name=mumps.dat
		add -region OCTOREG -dyn=OCTOSEG -null_subscripts=true -key_size=1019 -record_size=1048576
		add -segment OCTOSEG -file="octo.dat"
		add -segment AIMSEG -file="aim.dat" -access_method=MM -block_size=2048
		add -region AIMREG -dyn=AIMSEG -nojournal -key_size=1019 -null_subscripts=always -record_size=2048
		add -name %ydbocto* -region=OCTOREG
		add -name %ydbAIM* -region=AIMREG
FILE
		rm ./*.dat || true
		$ydb_dist/mupip create
		touch errors.log
		filediff() {
			local filename1="$1"
			local filename2="$2"
			local filename=""
			for dir in oldsrc newsrc
			do
				if [[ "oldsrc" == $dir ]]; then
					filename=$filename1
				else
					filename=$filename2
				fi
				# Replace variable parts of output (e.g. date/time/full-path-of-directory etc.)
				sed -i 's/[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}/DATE/g' ../$dir/$filename
				sed -i 's/[0-9]\{2\}:[0-9]\{2\}:[0-9]\{2\}/TIME/g' ../$dir/$filename
				sed -i 's/'$dir'/##SRCDIR##/g' ../$dir/$filename
				# Delete DBFILEXT messages as they contain pid and other variable output
				sed -i '/DBFILEXT/d' ../$dir/$filename
			done
			diff ../oldsrc/$filename1 ../newsrc/$filename2 > $filename2.diff || true
			if [[ -s $filename2.diff ]]; then
				echo "ERROR : [diff oldsrc/$filename1 newsrc/$filename2] returned non-zero diff" | tee -a ../errors.log
				echo "[cat $filename2.diff] output follows" | tee -a ../errors.log
				tee -a < $filename2.diff ../errors.log
				exit_status=1
			fi
		}
		run_octo() {
			local queryfile=$1
			local dir=$2
			# We do not want any failures in the "octo" invocation below to exit the script.
			# So disable the "set -e" setting temporarily for this step.
			set +e
			if [[ $3 == "vv" ]]; then
				./octo -vv -f $queryfile >& $queryfile.vv.out	# Need -vv to figure out generated M plan name
				# See comment in error block for TEST1 above for why exit code of 1 is accepted
				if [[ $? -gt 1 ]]; then
					echo "ERROR : [octo -f $dir/$queryfile] returned non-zero exit status : $?" | tee -a ../errors.log
					exit_status=1
				fi
				plan_name=$(grep _ydboctoP $queryfile.vv.out | sed 's/.*_ydboctoP/_ydboctoP/;s/].*//')
			fi
			./octo -f $queryfile >& $queryfile.out	# Run without -vv to get actual output (minus INFO/LP_ etc. output)
								# This will be used for filediff as it is deterministic.
			# Re-enable "set -e" now that "octo" invocation is done.
			set -e
		}
		cd oldsrc
		export ydb_routines=". _ydbocto.so $ydb_routines $ydb_dist/libyottadbutil.so"
		cp ../*.gld ../{mumps,octo}.dat ../create_table-*.sql .
		cp ./*.gld {mumps,octo}.dat create_table-*.sql ../newsrc
		for queryfile in create_table-*.sql
		do
			echo " --> Processing $queryfile"
			# Run the "CREATE TABLE" query in oldsrc directory to create the binary table definition
			cd ../oldsrc; rm -f ../src || true; ln -s oldsrc ../src
			run_octo $queryfile oldsrc novv
			# Determine table name and generate a query that selects all columns from that table
			tablename=$(grep -n "CREATE TABLE" $queryfile | grep -v "^--" | sed 's/.*CREATE TABLE //;s/(.*//;' | awk '{print $1}')
			echo "select * from $tablename;" > $queryfile.2
			# Copy over database files from oldsrc to newsrc
			cp {mumps,octo}.dat $queryfile.2 ../newsrc
			# Run the SELECT query in oldsrc directory
			run_octo $queryfile.2 oldsrc vv	# sets "plan_name" variable due to "vv"
			old_plan_name=$plan_name
			# Run an empty query file in the newsrc directory to force an auto upgrade of the binary table definitions
			cd ../newsrc; rm -f ../src || true; ln -s newsrc ../src
			echo "" > $queryfile.null
			run_octo $queryfile.null newsrc novv
			# Run the SELECT query in newsrc directory
			run_octo $queryfile.2 newsrc vv	# sets "plan_name" variable due to "vv"
			new_plan_name=$plan_name
			# Compare SELECT query output between oldsrc and newsrc. Should be none.
			# Allow for "[ERROR]" in $queryfile.2.out
			# But we also expect the exact same output with auto upgrade forced.
			filediff $queryfile.2.out $queryfile.2.out
			if [[ $old_plan_name != $new_plan_name ]]; then
				echo "ERROR : $queryfile : Plan name in oldsrc [$old_plan_name] differs from newsrc [$new_plan_name]" | tee -a ../errors.log
				exit_status=1
			fi
			if [[ ("" != "$new_plan_name") && ("" != "$old_plan_name") ]]; then
				# "$new_plan_name" can be "" for example if "CREATE TABLE" occurs in a comment in the query
				# file and the actual query is something else like a "DROP TABLE". In that case, skip this check.
				filediff "$old_plan_name" "$new_plan_name"
			fi
			# Need to DROP TABLE the current table in "oldsrc" directory to avoid reusing this plan for future
			# CREATE TABLE queries that point to the same table name (e.g. `Customers` vs `customers`) as that
			# can cause a failure in the pipeline job (see commit message for details).
			cd ../oldsrc; rm -f ../src || true; ln -s oldsrc ../src
			echo "DROP TABLE $tablename;" > "$queryfile.drop"
			run_octo "$queryfile.drop" oldsrc novv
			cd ../newsrc; rm -f ../src || true; ln -s newsrc ../src
		done
		cd ..
	fi
	# Set verbose mode back now that for loop is over (so we see each command as it gets executed)
	set -v
	set -x
fi

echo " -> exit $exit_status"
# Unset verbose mode before printing summary of failure results if any
set +x
set +v

if [[ 0 != "$exit_status" ]]; then
	if [[ "test-auto-upgrade" != "$jobname" ]]; then
		echo "# ----------------------------------------------------------"
		echo "# List of failed tests/subtests and their output directories"
		echo "# ----------------------------------------------------------"
		if ! [[ -s Testing/Temporary/LastTest.log ]]; then
			echo "# Detected script failure prior to BATS test execution. Please review script output to determine source."
		else
			grep -A 32 -E "not ok|Test: " Testing/Temporary/LastTest.log | grep -E "not ok|# Temporary|Test: " | grep -C 1 "not ok" | sed "s/^not/  &/;s/^#/  &/"
		fi
		echo "# -----------------------------"
	else
		echo "# ----------------------------------------------------------"
		echo "# List of errors (cat errors.log)"
		echo "# ----------------------------------------------------------"
		grep ERROR errors.log
	fi
fi

exit $exit_status

#!/usr/bin/env bash

#################################################################
#								#
# Copyright (c) 2020-2021 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

set -eu
set -o pipefail

ROOT="$(git rev-parse --show-toplevel)"

# Setup options
case "$0" in
	*octo) OCTO="$0.real";;
	*) echo "Unknown name $0, use 'rocto' or 'octo'"
esac

# Setup log file
short_commit="$($OCTO --version | grep "Git commit" | cut -d ' ' -f 3 | cut -c -5)"
log_file="$(mktemp /tmp/valgrind/"$short_commit".XXXXX.log)"

# Make sure to check for leaks even if interrupted or killed
check_leaks() {
	# 'uninitialised' is not a typo
	if grep "uninitialised value\|definitely lost in loss record" "$log_file"; then
		mv "$log_file" "$VALGRIND_LEAK_DIR"
	fi
}
trap check_leaks INT TERM EXIT

# Run valgrind
# TODO: see if --track-origins=yes --leak-check=full take unreasonable amounts of time
valgrind --log-file="$log_file" --suppressions="$ROOT/tools/ci/ignored-valgrind-errors.supp" "$OCTO" "$@"

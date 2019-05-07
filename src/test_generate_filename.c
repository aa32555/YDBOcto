/* Copyright (C) 2018-2019 YottaDB, LLC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "octo.h"
#include "octo_types.h"

#include "logical_plan.h"
#include "physical_plan.h"

#include "mmrhash.h"

static void test_valid_input(void **state) {
	hash128_state_t *hash_state;
	HASH128_STATE_INIT(hash_state, 0);
	char *key1 = "dummy1";
	char *key2 = "dummy2";
	char *filename = NULL;

	ydb_mmrhash_128_ingest(hash_state, (void*)key1, strlen(key1));
	ydb_mmrhash_128_ingest(hash_state, (void*)key2, strlen(key2));

	filename = generate_filename(hash_state, CrossReference);

	printf("test_valid_input: filename: %s\n", filename);
	assert_non_null(filename);
}

int main(void) {
	octo_init(0, NULL, FALSE);
	const struct CMUnitTest tests[] = {
		   cmocka_unit_test(test_valid_input),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}

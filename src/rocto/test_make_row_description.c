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

// Used to convert between network and host endian
#include <arpa/inet.h>

#include "rocto.h"
#include "message_formats.h"

static void test_null_input(void **state) {
	RowDescription *response = NULL;
	RowDescription *received_response = NULL;
	ErrorResponse *err = NULL;

	// Expect length field and number of parameters field
	int expected_length = sizeof(unsigned int) + sizeof(short int);

	response = make_row_description(NULL, 0);
	received_response = read_row_description((BaseMessage*)&response->type, &err);

	// Standard checks
	assert_null(err);
	assert_non_null(received_response);
	assert_int_equal(received_response->length, expected_length);
	assert_int_equal(received_response->num_parms, 0);

	free_row_description(response);
	free_row_description(received_response);
}

static void test_one_parms(void **state) {
	RowDescription *response = NULL;
	RowDescription *received_response = NULL;
	ErrorResponse *err = NULL;
	int num_parms = 1;
	RowDescriptionParm parms[num_parms];

	memset(parms, 0, sizeof(RowDescriptionParm) * num_parms);
	parms[0].name = "helloWorld";
	parms[0].table_id = 1;
	parms[0].column_id = 2;
	parms[0].data_type = 3;
	parms[0].data_type_size = 4;
	parms[0].type_modifier = 5;
	parms[0].format_code = 6;

	// RowDescription + RowDescriptionParms + string in each parm + null char
	int expected_length = sizeof(unsigned int) + sizeof(short int) + (sizeof(RowDescriptionParm) - sizeof(char*)) * num_parms
		+ strlen(parms[0].name) + sizeof(char);

	response = make_row_description(parms, num_parms);
	received_response = read_row_description((BaseMessage*)&response->type, &err);

	// Standard checks
	assert_null(err);
	assert_non_null(response);
	assert_int_equal(received_response->length, expected_length);
	assert_int_equal(received_response->num_parms, num_parms);

	assert_string_equal(received_response->parms[0].name, parms[0].name);
	assert_int_equal(received_response->parms[0].table_id, parms[0].table_id);
	assert_int_equal(received_response->parms[0].column_id, parms[0].column_id);
	assert_int_equal(received_response->parms[0].data_type, parms[0].data_type);
	assert_int_equal(received_response->parms[0].data_type_size, parms[0].data_type_size);
	assert_int_equal(received_response->parms[0].type_modifier, parms[0].type_modifier);
	assert_int_equal(received_response->parms[0].format_code, parms[0].format_code);

	free_row_description(response);
	free_row_description(received_response);
}

static void test_multi_parms(void **state) {
	RowDescription *response = NULL;
	RowDescription *received_response = NULL;
	ErrorResponse *err = NULL;
	int num_parms = 2;
	RowDescriptionParm parms[num_parms];

	memset(parms, 0, sizeof(RowDescriptionParm) * num_parms);
	parms[0].name = "helloWorld";
	parms[0].table_id = 1;
	parms[0].column_id = 2;
	parms[0].data_type = 3;
	parms[0].data_type_size = 4;
	parms[0].type_modifier = 5;
	parms[0].format_code = 6;
	parms[1].name = "helloUniverse";
	parms[1].table_id = 11;
	parms[1].column_id = 12;
	parms[1].data_type = 13;
	parms[1].data_type_size = 14;
	parms[1].type_modifier = 15;
	parms[1].format_code = 16;

	// RowDescription + RowDescriptionParms + string in each parm + null chars
	int expected_length = sizeof(unsigned int) + sizeof(short int) + (sizeof(RowDescriptionParm) - sizeof(char*)) * num_parms
		+ strlen(parms[0].name) + sizeof(char) + strlen(parms[1].name) + sizeof(char);

	response = make_row_description(parms, num_parms);
	received_response = read_row_description((BaseMessage*)&response->type, &err);

	// Standard checks
	assert_null(err);
	assert_non_null(response);
	assert_int_equal(received_response->length, expected_length);

	for (int i = 0; i < num_parms; i++) {
		assert_string_equal(received_response->parms[i].name, parms[i].name);
		assert_int_equal(received_response->parms[i].table_id, parms[i].table_id);
		assert_int_equal(received_response->parms[i].column_id, parms[i].column_id);
		assert_int_equal(received_response->parms[i].data_type, parms[i].data_type);
		assert_int_equal(received_response->parms[i].data_type_size, parms[i].data_type_size);
		assert_int_equal(received_response->parms[i].type_modifier, parms[i].type_modifier);
		assert_int_equal(received_response->parms[i].format_code, parms[i].format_code);
	}

	free_row_description(response);
	free_row_description(received_response);
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_null_input),
		cmocka_unit_test(test_one_parms),
		cmocka_unit_test(test_multi_parms),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}

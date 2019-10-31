/****************************************************************
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <endian.h>

// Used to convert between network and host endian
#include <arpa/inet.h>

#include <openssl/md5.h>

#include "octo.h"
#include "rocto.h"
#include "message_formats.h"
#include "helpers.h"

int __wrap_ydb_get_s(ydb_buffer_t *varname, int32_t subs_used, ydb_buffer_t *subsarray, ydb_buffer_t *ret_value) {
	if (0 == strncmp(varname->buf_addr, "$ZGBLDIR", varname->len_used)) {
		return 0;
	} else if (0 == strncmp(varname->buf_addr, "$zroutines", varname->len_used)){
		strcpy(ret_value->buf_addr, ".");
		ret_value->len_used = 1;
		return 0;
	}

	ydb_buffer_t *t = mock_ptr_type(ydb_buffer_t*);
	*ret_value = *t;
	return mock_type(int);
}

uint32_t __wrap_get_user_column_value(char *buffer, const uint32_t buf_len, const char *row, const uint32_t row_len,
		enum UserColumns column) {
	strncpy(buffer, mock_type(char*), buf_len);
	return mock_type(uint32_t);
}

int __wrap_md5_to_hex(char *md5_hash, char *hex, uint32_t hex_len) {
	strncpy(hex, mock_type(char*), hex_len);
	return mock_type(int);
}

int32_t __wrap_octo_log(int line, char *file, enum ERROR_LEVEL level, enum ERROR error, ...) {
	return mock_type(int32_t);
}

static void test_valid_input(void **state) {
	PasswordMessage *password_message;
	RoctoSession session;
	ydb_buffer_t session_id;

	YDB_LITERAL_TO_BUFFER("0", &session_id);
	session.session_id = &session_id;

	// Prepare startup message with username
	char *username = "user";
	StartupMessage *startup_message = make_startup_message(username);

	ydb_buffer_t user_info_subs;
	YDB_MALLOC_BUFFER(&user_info_subs, MAX_STR_CONST);
	// md5 hash of passworduser: 4d45974e13472b5a0be3533de4666414
	char *user_info = "1|user|super|inh|crer|cred|canl|repl|bypassrl|conn|md54d45974e13472b5a0be3533de4666414|valid";
	int done = 0;
	YDB_COPY_STRING_TO_BUFFER(user_info, &user_info_subs, done);
	will_return(__wrap_ydb_get_s, &user_info_subs);
	will_return(__wrap_ydb_get_s, YDB_OK);

	char *column_value = "md54d45974e13472b5a0be3533de4666414";
	will_return(__wrap_get_user_column_value, column_value);
	will_return(__wrap_get_user_column_value, strlen(column_value));

	// Wrap calls in make_password_message
	will_return(__wrap_md5_to_hex, "4d45974e13472b5a0be3533de4666414");
	will_return(__wrap_md5_to_hex, 0);
	will_return(__wrap_md5_to_hex, "8e998aaa66bd302e5592df3642c16f78");
	will_return(__wrap_md5_to_hex, 0);

	// Wrap call in handle_password_message
	will_return(__wrap_md5_to_hex, "8e998aaa66bd302e5592df3642c16f78");
	will_return(__wrap_md5_to_hex, 0);

	char *password = "password";
	char *salt = "salt";
	password_message = make_password_message(username, password, salt);

	int32_t result = handle_password_message(password_message, startup_message, salt);
	assert_int_equal(result, 0);

	free(password_message);
	free(startup_message->parameters);
	free(startup_message);
}

static void test_error_not_md5(void **state) {
	PasswordMessage *password_message;
	RoctoSession session;
	ydb_buffer_t session_id;

	YDB_LITERAL_TO_BUFFER("0", &session_id);
	session.session_id = &session_id;

	char *username = "user";
	StartupMessage *startup_message = make_startup_message(username);
	char *password = "password";
	char *salt = "salt";

	// Wrap calls in make_password_message
	will_return(__wrap_md5_to_hex, "4d45974e13472b5a0be3533de4666414");
	will_return(__wrap_md5_to_hex, 0);
	will_return(__wrap_md5_to_hex, "8e998aaa66bd302e5592df3642c16f78");
	will_return(__wrap_md5_to_hex, 0);
	will_return(__wrap_octo_log, 0);

	password_message = make_password_message(username, password, salt);
	password_message->password = "password";

	int32_t result = handle_password_message(password_message, startup_message, salt);
	assert_int_equal(result, 1);

	free(password_message);
	free(startup_message->parameters);
	free(startup_message);
}

static void test_error_user_info_lookup(void **state) {
	PasswordMessage *password_message;
	RoctoSession session;
	ydb_buffer_t session_id;

	YDB_LITERAL_TO_BUFFER("0", &session_id);
	session.session_id = &session_id;

	char *username = "user";
	StartupMessage *startup_message = make_startup_message(username);

	ydb_buffer_t user_info_subs;
	YDB_MALLOC_BUFFER(&user_info_subs, MAX_STR_CONST);
	// md5 hash of passworduser: 4d45974e13472b5a0be3533de4666414
	char *user_info = "1|user|super|inh|crer|cred|canl|repl|bypassrl|conn|md54d45974e13472b5a0be3533de4666414|valid";
	int done = 0;
	YDB_COPY_STRING_TO_BUFFER(user_info, &user_info_subs, done);
	will_return(__wrap_ydb_get_s, &user_info_subs);
	will_return(__wrap_ydb_get_s, YDB_ERR_LVUNDEF);

	char *salt = "salt";
	char *password = "password";

	// Wrap calls in make_password_message
	will_return(__wrap_md5_to_hex, "4d45974e13472b5a0be3533de4666414");
	will_return(__wrap_md5_to_hex, 0);
	will_return(__wrap_md5_to_hex, "8e998aaa66bd302e5592df3642c16f78");
	will_return(__wrap_md5_to_hex, 0);
	will_return(__wrap_octo_log, 0);

	password_message = make_password_message(username, password, salt);

	int32_t result = handle_password_message(password_message, startup_message, salt);
	assert_int_equal(result, 1);

	free(password_message);
	free(startup_message->parameters);
	free(startup_message);
}

static void test_error_hash_lookup(void **state) {
	PasswordMessage *password_message;
	RoctoSession session;
	ydb_buffer_t session_id;

	YDB_LITERAL_TO_BUFFER("0", &session_id);
	session.session_id = &session_id;

	char *username = "user";
	StartupMessage *startup_message = make_startup_message(username);

	ydb_buffer_t user_info_subs;
	YDB_MALLOC_BUFFER(&user_info_subs, MAX_STR_CONST);
	// md5 hash of passworduser: 4d45974e13472b5a0be3533de4666414
	char *user_info = "1|user|super|inh|crer|cred|canl|repl|bypassrl|conn|md54d45974e13472b5a0be3533de4666414|valid";
	int done = 0;
	YDB_COPY_STRING_TO_BUFFER(user_info, &user_info_subs, done);
	will_return(__wrap_ydb_get_s, &user_info_subs);
	will_return(__wrap_ydb_get_s, YDB_OK);

	char *column_value = "md54d45974e13472b5a0be3533de4666414";
	will_return(__wrap_get_user_column_value, column_value);
	will_return(__wrap_get_user_column_value, 0);

	// Wrap calls in make_password_message
	will_return(__wrap_md5_to_hex, "4d45974e13472b5a0be3533de4666414");
	will_return(__wrap_md5_to_hex, 0);
	will_return(__wrap_md5_to_hex, "8e998aaa66bd302e5592df3642c16f78");
	will_return(__wrap_md5_to_hex, 0);
	will_return(__wrap_octo_log, 0);

	char *salt = "salt";
	char *password = "password";
	password_message = make_password_message(username, password, salt);

	int32_t result = handle_password_message(password_message, startup_message, salt);
	assert_int_equal(result, 1);

	free(password_message);
	free(startup_message->parameters);
	free(startup_message);
}

static void test_error_hash_conversion(void **state) {
	PasswordMessage *password_message;
	RoctoSession session;
	ydb_buffer_t session_id;

	YDB_LITERAL_TO_BUFFER("0", &session_id);
	session.session_id = &session_id;

	char *username = "user";
	StartupMessage *startup_message = make_startup_message(username);

	ydb_buffer_t user_info_subs;
	YDB_MALLOC_BUFFER(&user_info_subs, MAX_STR_CONST);
	// md5 hash of passworduser: 4d45974e13472b5a0be3533de4666414
	char *user_info = "1|user|super|inh|crer|cred|canl|repl|bypassrl|conn|md54d45974e13472b5a0be3533de4666414|valid";
	int done = 0;
	YDB_COPY_STRING_TO_BUFFER(user_info, &user_info_subs, done);
	will_return(__wrap_ydb_get_s, &user_info_subs);
	will_return(__wrap_ydb_get_s, YDB_OK);

	char *column_value = "md54d45974e13472b5a0be3533de4666414";
	will_return(__wrap_get_user_column_value, column_value);
	will_return(__wrap_get_user_column_value, strlen(column_value));

	// Wrap calls in make_password_message
	will_return(__wrap_md5_to_hex, "4d45974e13472b5a0be3533de4666414");
	will_return(__wrap_md5_to_hex, 0);
	will_return(__wrap_md5_to_hex, "8e998aaa66bd302e5592df3642c16f78");
	will_return(__wrap_md5_to_hex, 0);

	char *salt = "salt";
	char *password = "password";
	password_message = make_password_message(username, password, salt);

	// Wrap calls in handle_password_message
	will_return(__wrap_md5_to_hex, "arbitrary");
	will_return(__wrap_md5_to_hex, 1);
	will_return(__wrap_octo_log, 0);

	int32_t result = handle_password_message(password_message, startup_message, salt);
	assert_int_equal(result, 1);

	free(password_message);
	free(startup_message->parameters);
	free(startup_message);
}

static void test_error_bad_password(void **state) {
	PasswordMessage *password_message;
	RoctoSession session;
	ydb_buffer_t session_id;

	YDB_LITERAL_TO_BUFFER("0", &session_id);
	session.session_id = &session_id;

	char *username = "user";
	StartupMessage *startup_message = make_startup_message(username);

	ydb_buffer_t user_info_subs;
	YDB_MALLOC_BUFFER(&user_info_subs, MAX_STR_CONST);
	// md5 hash of passworduser: 4d45974e13472b5a0be3533de4666414
	char *user_info = "1|user|super|inh|crer|cred|canl|repl|bypassrl|conn|md54d45974e13472b5a0be3533de4666414|valid";
	int done = 0;
	YDB_COPY_STRING_TO_BUFFER(user_info, &user_info_subs, done);
	will_return(__wrap_ydb_get_s, &user_info_subs);
	will_return(__wrap_ydb_get_s, YDB_OK);

	char *column_value = "md54d45974e13472b5a0be3533de4666414";
	will_return(__wrap_get_user_column_value, column_value);
	will_return(__wrap_get_user_column_value, strlen(column_value));

	// Wrap calls in make_password_message
	will_return(__wrap_md5_to_hex, "4d45974e13472b5a0be3533de4666414");
	will_return(__wrap_md5_to_hex, 0);
	will_return(__wrap_md5_to_hex, "8e998aaa66bd302e5592df3642c16f78");
	will_return(__wrap_md5_to_hex, 0);

	// Wrap calls in handle_password_message
	will_return(__wrap_md5_to_hex, "arbitrary");
	will_return(__wrap_md5_to_hex, 0);
	will_return(__wrap_octo_log, 0);

	char *salt = "salt";
	char *password = "balugawhales";
	password_message = make_password_message(username, password, salt);

	int32_t result = handle_password_message(password_message, startup_message, salt);
	assert_int_equal(result, 1);

	free(password_message);
	free(startup_message->parameters);
	free(startup_message);
}

int main(void) {
	octo_init(0, NULL);
	const struct CMUnitTest tests[] = {
		   cmocka_unit_test(test_valid_input),
		   cmocka_unit_test(test_error_not_md5),
		   cmocka_unit_test(test_error_user_info_lookup),
		   cmocka_unit_test(test_error_hash_lookup),
		   cmocka_unit_test(test_error_hash_conversion),
		   cmocka_unit_test(test_error_bad_password),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}

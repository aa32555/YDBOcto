/* Copyright (C) 2019 YottaDB, LLC
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
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <arpa/inet.h>

#include <openssl/ssl.h>
#include <openssl/conf.h>

#include "octo.h"
#include "rocto/rocto.h"
#include "rocto/message_formats.h"
#include "helpers.h"

int main(int argc, char **argv) {
	BaseMessage *base_message;
	StartupMessage *startup_message;
	SSLRequest *ssl_request;
	SSL_CTX *ossl_context;
	SSL *ossl_connection;
	int ossl_err;
	ErrorResponse *err;
	AuthenticationMD5Password *md5auth;
	AuthenticationOk *authok;
	ParameterStatus *parameter_status;
	struct sockaddr_in6 addressv6;
	struct sockaddr_in *address;
	int sfd, cfd, opt, addrlen, result, status;
	int cur_parm, i;
	pid_t child_id;
	char buffer[MAX_STR_CONST];
	RoctoSession session;
	StartupMessageParm message_parm;

	ydb_buffer_t ydb_buffers[2], *var_defaults, *var_sets, var_value;
	ydb_buffer_t *global_buffer = &(ydb_buffers[0]), *session_id_buffer = &(ydb_buffers[1]);
	ydb_buffer_t z_status, z_status_value;

	octo_init(argc, argv, TRUE);
	INFO(CUSTOM_ERROR, "rocto started");

	// Setup the address first so we know which protocol to use
	memset(&addressv6, 0, sizeof(struct sockaddr_in6));
	address = (struct sockaddr_in *)(&addressv6);
	address->sin_family = AF_INET;
	addrlen = sizeof(struct sockaddr_in6);
	if(inet_pton(AF_INET, config->rocto_config.address, &address->sin_addr) != 1) {
		addressv6.sin6_family = AF_INET6;
		if(inet_pton(AF_INET6, config->rocto_config.address, &addressv6.sin6_addr) != 1) {
			FATAL(ERR_BAD_ADDRESS, config->rocto_config.address);
		}
	}
	address->sin_port = htons(config->rocto_config.port);

	if((sfd = socket(address->sin_family, SOCK_STREAM, 0)) == -1) {
		FATAL(ERR_SYSCALL, "socket", errno, strerror(errno));
	}

	opt = 0;
	if(setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
		FATAL(ERR_SYSCALL, "setsockopt", errno, strerror(errno));
	}



	if(bind(sfd, (struct sockaddr *)&addressv6, sizeof(addressv6)) < 0) {
		FATAL(ERR_SYSCALL, "bind", errno, strerror(errno));
	}


	while (TRUE) {
		if(listen(sfd, 3) < 0) {
			FATAL(ERR_SYSCALL, "listen", errno, strerror(errno));
		}
		// printf("\nsfd: %d\naddress: %p\naddrlen: %lu\n", sfd, &address, addrlen);
		if((cfd = accept(sfd, (struct sockaddr *)&address, &addrlen)) < 0) {
			FATAL(ERR_SYSCALL, "accept", errno, strerror(errno));
		}
		child_id = fork();
		if(child_id != 0)
			continue;
		INFO(ERR_CLIENT_CONNECTED);
		// First we read the startup message, which has a special format
		// 2x32-bit ints
		session.connection_fd = cfd;
		session.ssl_active = FALSE;
		// Establish the connection first
		session.session_id = NULL;
		read_bytes(&session, buffer, MAX_STR_CONST, sizeof(int) * 2);
		// Attempt SSL connection, if configured
		if (0 == config->rocto_config.ssl_on) {
			ssl_request = NULL;
		} else {
			ssl_request = read_ssl_request(&session, buffer, sizeof(int) * 2, &err);
		}
		if (NULL != ssl_request) {
			result = send_bytes(&session, "S", sizeof(char));
			if (0 != result) {
				WARNING(ERR_ROCTO_SEND_FAILED, "failed to send SSL confirmation byte");
				SSL_shutdown(ossl_connection);
				SSL_free(ossl_connection);
				break;
			}
			// Initialize OpenSSL
			OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS, NULL);
			CONF_modules_load_file(NULL, NULL, 0);
			// Initialize SSL context, load certs/keys
			ossl_context = SSL_CTX_new(TLS_server_method());
			SSL_CTX_set_options(ossl_context, SSL_OP_SINGLE_DH_USE);	// Second arg is dummy option - fix later
			// Sets certificate for ALL connections in context - for single connection use "SSL_use_certificate..."
			result = SSL_CTX_use_certificate_chain_file(ossl_context, config->rocto_config.ssl_cert_file);
			if (result != 1) {
				WARNING(ERR_ROCTO_OSSL_CERT_LOAD, config->rocto_config.ssl_cert_file);
				break;
			}
			// Sets key for ALL connections in context - for single connection use "SSL_use_PrivateKey_file"
			result = SSL_CTX_use_PrivateKey_file(ossl_context, config->rocto_config.ssl_key_file, SSL_FILETYPE_PEM);
			if (result != 1) {
				WARNING(ERR_ROCTO_OSSL_KEY_LOAD, config->rocto_config.ssl_key_file);
				break;
			}
			// Set up SSL connection
			ossl_connection = SSL_new(ossl_context);
			session.ossl_connection = ossl_connection;
			if (NULL == ossl_connection) {
				WARNING(ERR_ROCTO_OSSL_CONN_FAILED);
				break;
			}
			SSL_set_fd(ossl_connection, cfd);
			// Send confirmation of SSL request
			ossl_err = SSL_accept(ossl_connection);
			if (0 >= ossl_err) {
				WARNING(ERR_ROCTO_OSSL_HANDSHAKE_FAILED);
				SSL_shutdown(ossl_connection);
				SSL_free(ossl_connection);
				break;
			}
			session.ssl_active = TRUE;
			read_bytes(&session, buffer, MAX_STR_CONST, sizeof(int) * 2);
		}
		// Attempt unencrypted connection if SSL not requested
		startup_message = read_startup_message(&session, buffer, sizeof(int) * 2, &err);
		if(startup_message == NULL) {
			send_message(&session, (BaseMessage*)(&err->type));
			free(err);
			break;
		}
		// Pretend to require md5 authentication
		md5auth = make_authentication_md5_password();
		result = send_message(&session, (BaseMessage*)(&md5auth->type));
		if(result)
			break;
		free(md5auth);

		// This next message is the user sending the password; ignore it
		base_message = read_message(&session, buffer, MAX_STR_CONST);
		if(base_message == NULL)
			break;

		// Ok
		authok = make_authentication_ok();
		send_message(&session, (BaseMessage*)(&authok->type));
		free(authok);

		// Enter the main loop
		global_buffer = &(ydb_buffers[0]);
		session_id_buffer = &(ydb_buffers[1]);
		YDB_LITERAL_TO_BUFFER("$ZSTATUS", &z_status);
		INIT_YDB_BUFFER(&z_status_value, MAX_STR_CONST);
		YDB_STRING_TO_BUFFER(config->global_names.session, global_buffer);
		INIT_YDB_BUFFER(session_id_buffer, MAX_STR_CONST);
		status = ydb_incr_s(global_buffer, 0, NULL, NULL, session_id_buffer);
		YDB_ERROR_CHECK(status, &z_status, &z_status_value);
		session.session_id = session_id_buffer;

		// Populate default parameters
		var_defaults = make_buffers(config->global_names.octo, 2, "variables", "");
		INIT_YDB_BUFFER(&var_defaults[2], MAX_STR_CONST);
		INIT_YDB_BUFFER(&var_value, MAX_STR_CONST);
		var_sets = make_buffers(config->global_names.session, 3, session.session_id->buf_addr,
				"variables", "");
		var_sets[3] = var_defaults[2];
		do {
			status = ydb_subscript_next_s(&var_defaults[0], 2, &var_defaults[1], &var_defaults[2]);
			if(status == YDB_ERR_NODEEND)
				break;
			YDB_ERROR_CHECK(status, &z_status, &z_status_value);
			status = ydb_get_s(&var_defaults[0], 2, &var_defaults[1], &var_value);
			YDB_ERROR_CHECK(status, &z_status, &z_status_value);
			var_sets[3] = var_defaults[2];
			status = ydb_set_s(&var_sets[0], 3, &var_sets[1], &var_value);
		} while(TRUE);

		// Set parameters
		for(cur_parm = 0; i < startup_message->num_parameters; i++) {
			set(startup_message->parameters[i].value, config->global_names.session, 3,
					session.session_id->buf_addr, "variables",
					startup_message->parameters[i].name);
		}

		var_sets[3].len_used = 0;
		do {
			status = ydb_subscript_next_s(&var_sets[0], 3, &var_sets[1], &var_sets[3]);
			if(status == YDB_ERR_NODEEND)
				break;
			YDB_ERROR_CHECK(status, &z_status, &z_status_value);
			var_sets[3].buf_addr[var_sets[3].len_used] = '\0';
			status = ydb_get_s(&var_sets[0], 3, &var_sets[1], &var_value);
			YDB_ERROR_CHECK(status, &z_status, &z_status_value);
			var_value.buf_addr[var_value.len_used] = '\0';
			message_parm.name = var_sets[3].buf_addr;
			message_parm.value = var_value.buf_addr;
			parameter_status = make_parameter_status(&message_parm);
			result = send_message(&session, (BaseMessage*)(&parameter_status->type));
			free(parameter_status);
			if(result) {
				return 0;
			}
		} while(TRUE);

		// Free temporary buffers
		free(var_defaults[2].buf_addr);
		free(var_value.buf_addr);
		free(var_defaults);
		free(var_sets);
		rocto_main_loop(&session);
		break;
	}


	return 0;
}

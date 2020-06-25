/****************************************************************
 *								*
 * Copyright (c) 2019-2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <netinet/tcp.h>

#include "octo.h"
#include "rocto/rocto.h"
#include "rocto/message_formats.h"
#include "helpers.h"

void handle_sigint(int sig, siginfo_t *info, void *context) {
	UNUSED(sig);
	UNUSED(info);
	UNUSED(context);
	rocto_session.session_ending = TRUE;
}

// Currently unused. May need to be used for CancelRequest handling in the future.
// NOTE: This code has been disabled due to the lack of signal forwarding support for SIGUSR1 to YDB
// void handle_sigusr1(int sig) {
// INFO(CUSTOM_ERROR, "SIGUSR1 RECEIVED");
// cancel_received = TRUE;
// }

#if YDB_TLS_AVAILABLE
#include "ydb_tls_interface.h"
#endif

int main(int argc, char **argv) {
	AuthenticationMD5Password *md5auth;
	AuthenticationOk *	   authok;
	BackendKeyData *	   backend_key_data;
	BaseMessage *		   base_message;
	CancelRequest *		   cancel_request;
	ParameterStatus *	   parameter_status;
	PasswordMessage *	   password_message;
	SSLRequest *		   ssl_request;
	StartupMessage *	   startup_message;
	StartupMessageParm	   message_parm;
	char			   buffer[MAX_STR_CONST];
	char			   host_buf[NI_MAXHOST], serv_buf[NI_MAXSERV];
	int			   cur_parm = 0;
	int			   sfd, cfd, opt, status = 0;
	int64_t			   mem_usage;
	pid_t			   child_id = 0;
	struct sigaction	   ctrlc_action;
	struct sockaddr_in *	   address = NULL;
	struct sockaddr_in6	   addressv6;
	ydb_buffer_t		   ydb_buffers[2], *var_defaults, *var_sets, var_value;
	ydb_buffer_t *		   session_buffer = &(ydb_buffers[0]), *session_id_buffer;
	ydb_buffer_t		   z_interrupt, z_interrupt_handler;
	ydb_buffer_t		   pid_subs[2], timestamp_buffer;
	ydb_buffer_t *		   pid_buffer = &pid_subs[0];
	socklen_t		   addrlen;
#if YDB_TLS_AVAILABLE
	gtm_tls_ctx_t *tls_context;
#endif

	// Initialize connection details in case errors prior to connections - needed before octo_init for Rocto error reporting
	rocto_session.ip = "IP_UNSET";
	rocto_session.port = "PORT_UNSET";

	// Do "octo_init" first as it initializes an environment variable ("ydb_lv_nullsubs") and that needs to be done
	// before any "ydb_init" call happens (as the latter reads this env var to know whether to allow nullsubs in lv or not)
	status = octo_init(argc, argv);
	if (0 != status)
		return status;

	opt = addrlen = 0;

	// Create buffers for managing secret keys for CancelRequests
	ydb_buffer_t secret_key_list_buffer, secret_key_buffer;
	char	     pid_str[INT32_TO_STRING_MAX], secret_key_str[INT32_TO_STRING_MAX], timestamp_str[INT64_TO_STRING_MAX];
	int	     secret_key = 0;
	YDB_LITERAL_TO_BUFFER("%ydboctoSecretKeyList", &secret_key_list_buffer);

	// Initialize a handler to respond to ctrl + c
	memset(&ctrlc_action, 0, sizeof(ctrlc_action));
	ctrlc_action.sa_flags = SA_SIGINFO;
	ctrlc_action.sa_sigaction = handle_sigint;
	status = sigaction(SIGINT, &ctrlc_action, NULL);
	if (0 != status)
		FATAL(ERR_SYSCALL, "sigaction", errno, strerror(errno));

	// NOTE: This code has been disabled due to the lack of signal forwarding support for SIGUSR1 to YDB
	// Initialize handler for SIGUSR1 in rocto C code
	// Currently unused. May need to be used for CancelRequest handling in the future.
	// memset(&sigusr1_action, 0, sizeof(sigusr1_action));
	// sigusr1_action.sa_flags = SA_SIGINFO;
	// sigusr1_action.sa_sigaction = (void *)handle_sigusr1;
	// status = sigaction(SIGUSR1, &sigusr1_action, NULL);
	// if (0 != status)
	// FATAL(ERR_SYSCALL, "sigaction", errno, strerror(errno));

	// Initialize SIGUSR1 handler in YDB
	status = ydb_init(); // YDB init needed for signal handler setup and gtm_tls_init call below */
	YDB_ERROR_CHECK(status);
	if (YDB_OK != status) {
		CLEANUP_CONFIG(config->config_file);
		return status;
	}
	YDB_LITERAL_TO_BUFFER("$ZINTERRUPT", &z_interrupt);
	YDB_LITERAL_TO_BUFFER("ZGOTO 1:run^%ydboctoCleanup", &z_interrupt_handler);
	status = ydb_set_s(&z_interrupt, 0, NULL, &z_interrupt_handler);
	YDB_ERROR_CHECK(status);
	if (YDB_OK != status) {
		CLEANUP_CONFIG(config->config_file);
		return status;
	}

	rocto_session.session_ending = FALSE;

	INFO(INFO_ROCTO_STARTED, config->rocto_config.port);

	// Disable sending log messages until all startup messages have been sent
	rocto_session.sending_message = TRUE;

	// Setup the address first so we know which protocol to use
	memset(&addressv6, 0, sizeof(struct sockaddr_in6));
	address = (struct sockaddr_in *)(&addressv6);
	address->sin_family = AF_INET;
	addrlen = sizeof(struct sockaddr_in6);
	status = inet_pton(AF_INET, config->rocto_config.address, &address->sin_addr);
	switch (status) {
	case 0:
		addressv6.sin6_family = AF_INET6;
		status = inet_pton(AF_INET6, config->rocto_config.address, &addressv6.sin6_addr);
		switch (status) {
		case 0:
			FATAL(ERR_BAD_ADDRESS, config->rocto_config.address);
			break;
		case 1:
			break;
		default:
			FATAL(ERR_SYSCALL, "inet_pton", errno, strerror(errno));
			break;
		}
		break;
	case 1:
		break;
	default:
		FATAL(ERR_SYSCALL, "inet_pton", errno, strerror(errno));
		break;
	}
	address->sin_port = htons(config->rocto_config.port);

	if (-1 == (sfd = socket(address->sin_family, SOCK_STREAM, 0))) {
		FATAL(ERR_SYSCALL, "socket", errno, strerror(errno));
	}
	opt = 1;
	if (-1 == setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
		FATAL(ERR_SYSCALL, "setsockopt", errno, strerror(errno));
	}

	if (!config->rocto_config.tcp_delay) {
		if (-1 == setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt))) {
			FATAL(ERR_SYSCALL, "setsockopt", errno, strerror(errno));
		}
	}

	if (bind(sfd, (struct sockaddr *)&addressv6, sizeof(addressv6)) < 0) {
		FATAL(ERR_SYSCALL, "bind", errno, strerror(errno));
	}

	// Spin off another thread to keep an eye on dead processes
	pthread_t thread_id;
	status = pthread_create(&thread_id, NULL, rocto_helper_waitpid, (void *)(&rocto_session));
	if (0 != status)
		FATAL(ERR_SYSCALL, "pthread_create", status, strerror(status));

	if (listen(sfd, 3) < 0) {
		FATAL(ERR_SYSCALL, "listen", errno, strerror(errno));
	}
#if YDB_TLS_AVAILABLE
	tls_context = INVALID_TLS_CONTEXT;
#endif
	ssl_request = NULL;
	while (!rocto_session.session_ending) {
		if ((cfd = accept(sfd, (struct sockaddr *)&address, &addrlen)) < 0) {
			if (rocto_session.session_ending) {
				break;
			}
			if (EINTR == errno) {
				continue;
			}
			ERROR(ERR_SYSCALL, "accept", errno, strerror(errno));
			continue;
		}

		// Create secret key and matching buffer
		syscall(SYS_getrandom, &secret_key, 4, 0);
		snprintf(secret_key_str, INT32_TO_STRING_MAX, "%u", secret_key);
		YDB_STRING_TO_BUFFER(secret_key_str, &secret_key_buffer);

		child_id = fork();
		if (0 != child_id) {
			uint64_t timestamp;

			if (0 > child_id) {
				ERROR(ERR_SYSCALL, "fork", errno, strerror(errno));
				continue;
			}
			INFO(INFO_ROCTO_SERVER_FORKED, child_id);
			// Create pid buffer
			snprintf(pid_str, INT32_TO_STRING_MAX, "%u", child_id);
			YDB_STRING_TO_BUFFER(pid_str, pid_buffer);
			// Add pid/secret key pair to list in listener/parent
			status = ydb_set_s(&secret_key_list_buffer, 1, pid_buffer, &secret_key_buffer);
			YDB_ERROR_CHECK(status);

			// Get timestamp of the new process
			timestamp = get_pid_start_time(child_id);
			if (0 == timestamp) {
				// Error emitted by callee get_pid_start_time()
				// No ErrorResponse is issued as this block is executed by the listener process,
				// i.e. we haven't yet established a client-server connection.
				// This means that the server won't be able to process cancel requests for this pid
				// due to the missing timestamp.
				break;
			}
			// Populate pid/timestamp buffers
			YDB_LITERAL_TO_BUFFER("timestamp", &pid_subs[1]);
			snprintf(timestamp_str, INT64_TO_STRING_MAX, "%zu", timestamp);
			YDB_STRING_TO_BUFFER(timestamp_str, &timestamp_buffer);
			// Add timestamp under PID key
			status = ydb_set_s(&secret_key_list_buffer, 2, &pid_subs[0], &timestamp_buffer);
			YDB_ERROR_CHECK(status);
			continue;
		}

		// Reset thread id to identify it as child process
		thread_id = 0;

		// First we read the startup message, which has a special format
		// 2x32-bit ints
		rocto_session.connection_fd = cfd;
		rocto_session.ssl_active = FALSE;
		if (config->rocto_config.use_dns) {
			status = getnameinfo((const struct sockaddr *)&address, addrlen, host_buf, NI_MAXHOST, serv_buf, NI_MAXSERV,
					     0);
		} else {
			status = getnameinfo((const struct sockaddr *)&address, addrlen, host_buf, NI_MAXHOST, serv_buf, NI_MAXSERV,
					     NI_NUMERICHOST | NI_NUMERICSERV);
		}
		if (0 != status) {
			ERROR(ERR_SYSCALL, "getnameinfo", errno, strerror(errno));
			break;
		}
		rocto_session.ip = host_buf;
		rocto_session.port = serv_buf;
		LOG_LOCAL_ONLY(INFO, ERR_CLIENT_CONNECTED, NULL);

		status = ydb_init(); // YDB init needed by gtm_tls_init call below */
		YDB_ERROR_CHECK(status);
		if (YDB_OK != status) {
			break;
		}
		// Establish the connection first
		rocto_session.session_id = NULL;
		read_bytes(&rocto_session, buffer, MAX_STR_CONST, sizeof(int) * 2);

		// Attempt TLS connection, if configured
		ssl_request = read_ssl_request(&rocto_session, buffer, sizeof(int) * 2);
		if ((NULL != ssl_request) && config->rocto_config.ssl_on) {
#if YDB_TLS_AVAILABLE
			int tls_errno;

			// gtm_tls_conn_info tls_connection;
			status = send_bytes(&rocto_session, "S", sizeof(char));
			if (0 != status) {
				ERROR(ERR_ROCTO_SEND_FAILED, "failed to send SSL confirmation byte");
				break;
			}
			// Initialize TLS context: load config, certs, keys, etc.
			tls_context = gtm_tls_init(GTM_TLS_API_VERSION, GTMTLS_OP_INTERACTIVE_MODE);
			if (INVALID_TLS_CONTEXT == tls_context) {
				tls_errno = gtm_tls_errno();
				if (0 < tls_errno) {
					ERROR(ERR_SYSCALL, "", tls_errno, strerror(tls_errno));
				} else {
					ERROR(ERR_ROCTO_TLS_INIT, gtm_tls_get_error());
				}
				break;
			}
			// Set up TLS socket
			gtm_tls_socket_t *tls_socket;
			tls_socket = gtm_tls_socket(tls_context, NULL, cfd, "OCTOSERVER", GTMTLS_OP_SOCKET_DEV);
			if (INVALID_TLS_SOCKET == tls_socket) {
				tls_errno = gtm_tls_errno();
				if (0 < tls_errno) {
					ERROR(ERR_SYSCALL, gtm_tls_get_error(), tls_errno, strerror(tls_errno));
				} else {
					ERROR(ERR_ROCTO_TLS_SOCKET, gtm_tls_get_error());
				}
				break;
			}
			// Accept incoming TLS connections
			do {
				status = gtm_tls_accept(tls_socket);
				if (0 != status) {
					if (-1 == status) {
						tls_errno = gtm_tls_errno();
						if (-1 == tls_errno) {
							ERROR(ERR_ROCTO_TLS_ACCEPT, gtm_tls_get_error());
						} else {
							ERROR(CUSTOM_ERROR, "unknown", tls_errno, strerror(tls_errno));
						}
						break;
					} else if (GTMTLS_WANT_READ == status) {
						ERROR(ERR_ROCTO_TLS_WANT_READ, NULL);
					} else if (GTMTLS_WANT_WRITE == status) {
						ERROR(ERR_ROCTO_TLS_WANT_WRITE, NULL);
					} else {
						ERROR(ERR_ROCTO_TLS_UNKNOWN, "failed to accept incoming connection(s)");
						break;
					}
				}
			} while ((GTMTLS_WANT_READ == status) || (GTMTLS_WANT_WRITE == status));
			rocto_session.tls_socket = tls_socket;
			rocto_session.ssl_active = TRUE;
			read_bytes(&rocto_session, buffer, MAX_STR_CONST, sizeof(int) * 2);
#endif
			// Attempt unencrypted connection if SSL is disabled
		} else if ((NULL != ssl_request) && !config->rocto_config.ssl_on) {
			status = send_bytes(&rocto_session, "N", sizeof(char));
			read_bytes(&rocto_session, buffer, MAX_STR_CONST, sizeof(int) * 2);
		} else if ((NULL == ssl_request) && config->rocto_config.ssl_required) {
			// Do not continue if TLS/SSL is required, but not requested by the client
			rocto_session.sending_message = FALSE; // Must enable message sending for client to be notified
			FATAL(ERR_ROCTO_TLS_REQUIRED, "");
			break;
		}

		// Check for CancelRequest and handle if so
		cancel_request = read_cancel_request(&rocto_session, buffer);
		if (NULL != cancel_request) {
			LOG_LOCAL_ONLY(INFO, ERR_ROCTO_QUERY_CANCELED, "");
			handle_cancel_request(cancel_request);
			// Shutdown connection immediately to free client prompt
			shutdown(rocto_session.connection_fd, SHUT_RDWR);
			break;
		}

		// Get actual (non-zero) pid of child/server
		child_id = getpid();
		snprintf(pid_str, INT32_TO_STRING_MAX, "%u", child_id);
		YDB_STRING_TO_BUFFER(pid_str, pid_buffer);
		// Clear all other secret key/pid pairs from other servers
		status = ydb_delete_s(&secret_key_list_buffer, 0, NULL, YDB_DEL_TREE);
		YDB_ERROR_CHECK(status);
		// Add pid/secret key pair to list in server/child
		status = ydb_set_s(&secret_key_list_buffer, 1, pid_buffer, &secret_key_buffer);
		YDB_ERROR_CHECK(status);

		startup_message = read_startup_message(&rocto_session, buffer, sizeof(int) * 2);
		if (NULL == startup_message) {
			break;
		}
		// Require md5 authentication
		char salt[4];
		md5auth = make_authentication_md5_password(&rocto_session, salt);
		status = send_message(&rocto_session, (BaseMessage *)(&md5auth->type));
		if (status) {
			ERROR(ERR_ROCTO_SEND_FAILED, "failed to send MD5 authentication required");
			free(md5auth);
			free(startup_message->parameters);
			free(startup_message);
			break;
		}
		free(md5auth);

		// This next message is the user sending the password
		int rocto_err = 0;
		base_message = read_message(&rocto_session, buffer, MAX_STR_CONST, &rocto_err);
		if (NULL == base_message) {
			if (-2 != rocto_err) {
				ERROR(ERR_ROCTO_READ_FAILED, "failed to read MD5 password");
			}
			free(startup_message->parameters);
			free(startup_message);
			break;
		}
		password_message = read_password_message(base_message);
		if (NULL == password_message) {
			free(startup_message->parameters);
			free(startup_message);
			break;
		}

		rocto_session.sending_message = FALSE;

		// Validate user credentials
		status = handle_password_message(password_message, startup_message, salt);
		free(password_message);
		if (0 != status) {
			free(startup_message->parameters);
			free(startup_message);
			break;
		}

		// Ok
		authok = make_authentication_ok();
		send_message(&rocto_session, (BaseMessage *)(&authok->type));
		free(authok);

		// Enter the main loop
		session_id_buffer = &(ydb_buffers[1]);
		YDB_STRING_TO_BUFFER(config->global_names.session, session_buffer);
		YDB_MALLOC_BUFFER(session_id_buffer, MAX_STR_CONST);
		status = ydb_incr_s(session_buffer, 0, NULL, NULL, session_id_buffer);
		YDB_ERROR_CHECK(status);
		if (YDB_OK != status) {
			YDB_FREE_BUFFER(session_id_buffer);
			break;
		}
		rocto_session.session_id = session_id_buffer;
		rocto_session.session_id->buf_addr[rocto_session.session_id->len_used] = '\0';

		// Populate default parameters
		var_defaults = make_buffers(config->global_names.octo, 2, "variables", "");
		YDB_MALLOC_BUFFER(&var_defaults[2], MAX_STR_CONST);
		YDB_MALLOC_BUFFER(&var_value, MAX_STR_CONST);
		var_sets = make_buffers(config->global_names.session, 3, rocto_session.session_id->buf_addr, "variables", "");
		var_sets[3] = var_defaults[2];
		do {
			status = ydb_subscript_next_s(&var_defaults[0], 2, &var_defaults[1], &var_defaults[2]);
			if (YDB_ERR_NODEEND == status) {
				status = YDB_OK;
				break;
			}
			if (0 != status)
				break;
			status = ydb_get_s(&var_defaults[0], 2, &var_defaults[1], &var_value);
			if (0 != status)
				break;
			var_sets[3] = var_defaults[2];
			status = ydb_set_s(&var_sets[0], 3, &var_sets[1], &var_value);
			if (0 != status)
				break;
		} while (TRUE);
		YDB_ERROR_CHECK(status);
		if (YDB_OK != status) {
			YDB_FREE_BUFFER(&var_defaults[2]);
			YDB_FREE_BUFFER(&var_value);
			YDB_FREE_BUFFER(session_id_buffer);
			free(var_defaults);
			free(var_sets);
			break;
		}
		// Set parameters
		for (cur_parm = 0; cur_parm < startup_message->num_parameters; cur_parm++) {
			ydb_buffer_t varname, subs_array[3], value;

			YDB_STRING_TO_BUFFER(config->global_names.session, &varname);
			YDB_STRING_TO_BUFFER(rocto_session.session_id->buf_addr, &subs_array[0]);
			YDB_LITERAL_TO_BUFFER("variables", &subs_array[1]);
			YDB_STRING_TO_BUFFER(startup_message->parameters[cur_parm].name, &subs_array[2]);
			YDB_STRING_TO_BUFFER(startup_message->parameters[cur_parm].value, &value);
			status = ydb_set_s(&varname, 3, subs_array, &value);
			YDB_ERROR_CHECK(status);
		}
		free(startup_message->parameters);
		free(startup_message);

		var_sets[3].len_used = 0;
		do {
			status = ydb_subscript_next_s(&var_sets[0], 3, &var_sets[1], &var_sets[3]);
			if (YDB_ERR_NODEEND == status) {
				status = YDB_OK;
				break;
			}
			YDB_ERROR_CHECK(status);
			if (0 != status)
				break;
			var_sets[3].buf_addr[var_sets[3].len_used] = '\0';
			status = ydb_get_s(&var_sets[0], 3, &var_sets[1], &var_value);
			YDB_ERROR_CHECK(status);
			if (0 != status)
				break;
			var_value.buf_addr[var_value.len_used] = '\0';
			message_parm.name = var_sets[3].buf_addr;
			message_parm.value = var_value.buf_addr;
			parameter_status = make_parameter_status(&message_parm);
			status = send_message(&rocto_session, (BaseMessage *)(&parameter_status->type));
			LOG_LOCAL_ONLY(INFO, INFO_ROCTO_PARAMETER_STATUS_SENT, message_parm.name, message_parm.value);
			free(parameter_status);
			if (status) {
				break;
			}
		} while (TRUE);

		// Free temporary buffers
		YDB_FREE_BUFFER(&var_defaults[2]);
		YDB_FREE_BUFFER(&var_value);
		free(var_defaults);
		free(var_sets);

		// Clean up after any errors from the above loop
		assert(0 == YDB_OK); // or else if `status` evaluated to true in the above loop, we would try to send another
				     // message on error
		if (YDB_OK != status) {
			YDB_FREE_BUFFER(session_id_buffer);
			break;
		}

		// Send secret key info to client
		backend_key_data = make_backend_key_data(secret_key, child_id);
		status = send_message(&rocto_session, (BaseMessage *)(&backend_key_data->type));
		free(backend_key_data);
		if (status) {
			YDB_FREE_BUFFER(session_id_buffer);
			break;
		}

		rocto_main_loop(&rocto_session);
		YDB_FREE_BUFFER(session_id_buffer);
		break;
	}
	if (0 != thread_id) {
		// We only want to close the sfd if we are the parent process which actually listens to
		// the socket; thread_id will be 0 if we are a child process
		close(sfd);
		status = pthread_join(thread_id, NULL);
		if (0 != status) {
			ERROR(ERR_SYSCALL, "failed to join helper_waitpid", status, strerror(status));
		}
	}
	// Since each iteration of the loop spawns a child process, each of which calls `gtm_tls_init`,
	// we call `gtm_tls_fini` for each child.
#if YDB_TLS_AVAILABLE
	if (INVALID_TLS_CONTEXT != tls_context) {
		gtm_tls_fini(&tls_context);
	}
#endif
	if (NULL != ssl_request) {
		free(ssl_request);
	}

	if (TRACE >= config->verbosity_level) {
		mem_usage = get_mem_usage();
		if (0 <= mem_usage) {
			TRACE(INFO_MEMORY_USAGE, mem_usage);
		} else {
			ERROR(ERR_MEMORY_USAGE, "");
		}
		status = ydb_ci("_ydboctoNodeDump");
		YDB_ERROR_CHECK(status);
	}

	CLEANUP_CONFIG(config->config_file);
	return status;
}

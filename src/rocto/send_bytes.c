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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

// Used to convert between network and host endian
#include <arpa/inet.h>

#include "rocto.h"
#include "message_formats.h"

int send_bytes(RoctoSession *session, char *message, size_t length) {
	int result = 0, ossl_error = 0;
	unsigned long ossl_error_code = 0;
	char *err = NULL;

	if (session->ssl_active) {
		result = SSL_write(session->ossl_connection, message, length);
		if (result <= 0 ) {
			ossl_error = SSL_get_error(session->ossl_connection, result);
			ossl_error_code = ERR_peek_last_error();
			err = ERR_error_string(ossl_error_code, err);
			if(errno == ECONNRESET) {
				return 1;
			}
			else if (ossl_error == SSL_ERROR_SYSCALL) {
				FATAL(ERR_SYSCALL, "unknown (OpenSSL)", errno, strerror(errno));
			}
			else {
				FATAL(ERR_ROCTO_OSSL_WRITE_FAILED, err);
			}
			return 1;
		}
	} else {
		result = send(session->connection_fd, message, length, 0);
		if(result < 0) {
			if(errno == ECONNRESET)
				return 1;
			FATAL(ERR_SYSCALL, "send", errno, strerror(errno));
			return 1;
		}
	}
	return 0;
}

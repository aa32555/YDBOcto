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
#include <assert.h>

// Used to convert between network and host endian
#include <arpa/inet.h>

#include "message_formats.h"

AuthenticationMD5Password *make_authentication_md5_password() {
	AuthenticationMD5Password *ret;

	ret = (AuthenticationMD5Password*)malloc(sizeof(AuthenticationMD5Password));
	memset(ret, 0, sizeof(AuthenticationMD5Password));

	ret->type = PSQL_AuthenticationMD5Password;
	ret->length = htonl(sizeof(int) + sizeof(int) + sizeof(char) * 4);
	ret->md5_required = htonl(5);
	/// TODO: this should be a random number
	ret->salt[0] = 12;
	ret->salt[1] = 12;
	ret->salt[2] = 12;
	ret->salt[3] = 12;

	return ret;
}

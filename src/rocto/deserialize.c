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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include <endian.h>

#include <libyottadb.h>

#include "octo.h"
#include "octo_types.h"
#include "message_formats.h"
#include "rocto.h"

// UUID record field sizes in nibbles
#define UUID_TIME_LOW	8
#define UUID_TIME_MID	4
#define UUID_TIME_HI_AND_VERSION	4
#define UUID_CLOCK_SEQ	4		// clock_seq_hi_and_res_clock_seq_low
#define UUID_NODE	12

// Expects hex length >= 3
char *byte_to_hex(char c, char *hex) {
	unsigned char high = 0, low = 0;
	// Isolate nibbles
	high = (unsigned char)c >> 4;
	low = c << 4;
	low >>= 4;

	// Map to ASCII digits or lower case a-f
	if (high < 10) {
		high += 48;
	} else {
		high += 87;
	}
	if (low < 10) {
		low += 48;
	} else {
		low += 87;
	}

	hex[0] = high;
	hex[1] = low;
	hex[2] = '\0';
	return hex;
}

// Convert raw MD5 hash to hex string
int md5_to_hex(const unsigned char *md5_hash, char *hex, uint32_t hex_len) {
	if (hex_len < 33) {	// Length of 16-byte md5 in hex, plus null terminator
		return 1;
	}
	int32_t i, j;
	for (i = 0, j = 0; i < 16; i++, j += 2) {	// MD5 hash length == 16
		byte_to_hex(md5_hash[i], &hex[j]);
	}
	hex[j] = '\0';
	return 0;
}

int64_t bin_to_bool(char *bin) {
	return bin[0];
}

int64_t bin_to_char(char *bin) {
	return bin[0];
}

int64_t bin_to_int16(char *bin) {
	int64_t new_int = 0;
	new_int = ntohs(*(int16_t*)bin);
	return new_int;
}

int64_t bin_to_int32(char *bin) {
	int64_t new_int = 0;
	new_int = ntohl(*(int32_t*)bin);
	return new_int;
}

int64_t bin_to_int64(char *bin) {
	int64_t new_int = 0;
	new_int = (int64_t)be64toh(*(uint64_t*)bin);
	return new_int;
}

int64_t bin_to_oid(char *bin) {
	int64_t new_int = 0;
	new_int = ntohl(*(int32_t*)bin);
	return new_int;
}

/*
float bin_to_float4(char *bin) {
}

double bin_to_float8(char *bin) {
}
*/

// TODO: This is currently ambiguous, the Go pq and rust-postgres drivers
// both handle this case by simply returning the input. However, the PostgreSQL
// documentation states that the bytea type must be a hex string preceded by '\x'
// or a (largely deprecated) 'escape string'
char *bin_to_bytea(char *bin) {
	return bin;
}

void bin_to_uuid(char *bin, char *buffer, int32_t buf_len) {
	const int32_t uuid_len = 36;	// 16 bytes * 2 nibbles/byte + 4 dashes
	assert(buf_len > uuid_len);

	// Fill out UUID time_low field
	char temp_16[3];	// count null
	int32_t i = 0, j = 0, offset = 0;
	offset += UUID_TIME_LOW;
	while (j < offset) {
		// One byte yields two hex nibble chars
		strncpy(&buffer[j], byte_to_hex(bin[i], temp_16), 2);
		j += 2;
		i++;
	}
	buffer[j] = '-';
	j++;
	offset += UUID_TIME_MID + 1;	// count dash
	while (j < offset) {
		// One byte yields two hex nibble chars
		strncpy(&buffer[j], byte_to_hex(bin[i], temp_16), 2);
		j += 2;
		i++;
	}
	buffer[j] = '-';
	j++;
	offset += UUID_TIME_HI_AND_VERSION + 1;		// count dash
	while (j < offset) {
		// One byte yields two hex nibble chars
		strncpy(&buffer[j], byte_to_hex(bin[i], temp_16), 2);
		j += 2;
		i++;
	}
	buffer[j] = '-';
	j++;
	offset += UUID_CLOCK_SEQ + 1;		// count dash
	while (j < offset) {
		// One byte yields two hex nibble chars
		strncpy(&buffer[j], byte_to_hex(bin[i], temp_16), 2);
		j += 2;
		i++;
	}
	buffer[j] = '-';
	j++;
	offset += UUID_NODE + 1;		// count dash
	while (j < offset) {
		// One byte yields two hex nibble chars
		strncpy(&buffer[j], byte_to_hex(bin[i], temp_16), 2);
		j += 2;
		i++;
	}
	assert(j == uuid_len);
	buffer[j] = '\0';
}

// HSTORE?

// VARBIT/BIT?

// TIME/TIMETZ?

// TIMESTAMP/TIMESTAMPTZ?

// DATE?

// MACADDR?

// ARRAY?

// RANGE?

// POINT?

// BOX?

// PATH?

// INET?

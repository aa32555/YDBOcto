/* Copyright (C) 2018 YottaDB, LLC
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
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#include "octo.h"
#include "octo_types.h"

#include "logical_plan.h"
#include "physical_plan.h"

char *generateFilename(EVP_MD_CTX *mdctx, FileType file_type) {
	const unsigned short max_filename_len = 31;
	unsigned short filename_len = 0;
	unsigned short filename_prefix_len = 0;
	unsigned short filename_suffix_len = 0;
	char *digest = NULL;
	char *filename = NULL;
	char *xref_prefix = "genOctoXref";
	char *output_plan_prefix = "genOctoOPlan";
	char *c = NULL;

	if((digest = (unsigned char *)OPENSSL_malloc(EVP_MD_size(EVP_md5()))) == NULL) {
		FATAL(ERR_LIBSSL_ERROR);
	}

	filename = (char*)malloc(max_file_name_len + sizeof(char));	// count null
	switch (file_type) {
		case CrossReference:
			filename_prefix_len = strlen(xref_prefix);
			filename_suffix_len = max_file_name_len - filename_prefix_len;
			break;
		case OutputPlan:
			filename_prefix_len = strlen(output_plan_prefix);
			filename_suffix_len = max_file_name_len - filename_prefix_len;
			// Hash function
			break;
		default:
			break;
	}

	c = filename;
	strcpy(c, xref_prefix, filename_prefix_len);
	c += filename_prefix_len;
	strcpy(c, digest, filename_suffix_len);

	snprintf(key->cross_reference_filename, filename_len, "genOctoXref%x%x",
			*(unsigned int*)tableNameHash, *(unsigned int*)columnNameHash);

	snprintf(filename, MAX_STR_CONST, "%s/%s.m", config->tmp_dir, key->cross_reference_filename);

	if(1 != EVP_DigestInit_ex(mdctx, EVP_md5(), NULL)) {
		FATAL(ERR_LIBSSL_ERROR);
	}

	if(1 != EVP_DigestUpdate(mdctx, message, message_len)) {
		FATAL(ERR_LIBSSL_ERROR);
	}


	if(1 != EVP_DigestFinal_ex(mdctx, *digest, digest_len)) {
		FATAL(ERR_LIBSSL_ERROR);
	}

	// sets *value to "m_routines/outputPlanABCHDKJHSDF123123.m"
}

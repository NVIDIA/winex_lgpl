/*
 * FILE:	sha2.h
 * AUTHOR:	Aaron D. Gifford - http://www.aarongifford.com/
 *
 * Copyright (c) 2000-2001, Aaron D. Gifford
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTOR(S) ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTOR(S) BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __SHA2_H__
#define __SHA2_H__

#include <basetsd.h>

/*** SHA-256/384/512 Various Length Definitions ***********************/
#define SHA256_BLOCK_LENGTH		64
#define SHA256_DIGEST_LENGTH		32
#define SHA256_DIGEST_STRING_LENGTH	(SHA256_DIGEST_LENGTH * 2 + 1)
#define SHA384_BLOCK_LENGTH		128
#define SHA384_DIGEST_LENGTH		48
#define SHA384_DIGEST_STRING_LENGTH	(SHA384_DIGEST_LENGTH * 2 + 1)
#define SHA512_BLOCK_LENGTH		128
#define SHA512_DIGEST_LENGTH		64
#define SHA512_DIGEST_STRING_LENGTH	(SHA512_DIGEST_LENGTH * 2 + 1)


/*** SHA-256/384/512 Context Structures *******************************/
typedef UINT8  sha2_byte;	/* Exactly 1 byte */
typedef UINT32 sha2_word32;	/* Exactly 4 bytes */
typedef UINT64 sha2_word64;	/* Exactly 8 bytes */

typedef struct _SHA256_CTX {
	sha2_word32	state[8];
	sha2_word64	bitcount;
	sha2_byte	buffer[SHA256_BLOCK_LENGTH];
} SHA256_CTX;
typedef struct _SHA512_CTX {
	sha2_word64	state[8];
	sha2_word64	bitcount[2];
	sha2_byte	buffer[SHA512_BLOCK_LENGTH];
} SHA512_CTX;

typedef SHA512_CTX SHA384_CTX;


/*** SHA-256/384/512 Function Prototypes ******************************/

void SHA256_Init(SHA256_CTX *);
void SHA256_Update(SHA256_CTX*, const sha2_byte*, size_t);
void SHA256_Final(sha2_byte[SHA256_DIGEST_LENGTH], SHA256_CTX*);
char* SHA256_End(SHA256_CTX*, char[SHA256_DIGEST_STRING_LENGTH]);
char* SHA256_Data(const sha2_byte*, size_t, char[SHA256_DIGEST_STRING_LENGTH]);

void SHA384_Init(SHA384_CTX*);
void SHA384_Update(SHA384_CTX*, const sha2_byte*, size_t);
void SHA384_Final(sha2_byte[SHA384_DIGEST_LENGTH], SHA384_CTX*);
char* SHA384_End(SHA384_CTX*, char[SHA384_DIGEST_STRING_LENGTH]);
char* SHA384_Data(const sha2_byte*, size_t, char[SHA384_DIGEST_STRING_LENGTH]);

void SHA512_Init(SHA512_CTX*);
void SHA512_Update(SHA512_CTX*, const sha2_byte*, size_t);
void SHA512_Final(sha2_byte[SHA512_DIGEST_LENGTH], SHA512_CTX*);
char* SHA512_End(SHA512_CTX*, char[SHA512_DIGEST_STRING_LENGTH]);
char* SHA512_Data(const sha2_byte*, size_t, char[SHA512_DIGEST_STRING_LENGTH]);

#endif /* __SHA2_H__ */

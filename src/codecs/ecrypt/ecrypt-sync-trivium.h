/* ecrypt-sync.h */

/* 
 * Header file for synchronous stream ciphers without authentication
 * mechanism.
 * 
 * *** Please only edit parts marked with "[edit]". ***
 */

#ifndef Trivium_SYNC
#define Trivium_SYNC

#include "ecrypt-portable.h"

/* ------------------------------------------------------------------------- */

/* Cipher parameters */

/* 
 * The name of your cipher.
 */
#define Trivium_NAME "TRIVIUM"                 /* [edit] */ 
#define Trivium_PROFILE "___H3"

/*
 * Specify which key and IV sizes are supported by your cipher. A user
 * should be able to enumerate the supported sizes by running the
 * following code:
 *
 * for (i = 0; Trivium_KEYSIZE(i) <= Trivium_MAXKEYSIZE; ++i)
 *   {
 *     keysize = Trivium_KEYSIZE(i);
 *
 *     ...
 *   }
 *
 * All sizes are in bits.
 */

#define Trivium_MAXKEYSIZE 80                  /* [edit] */
#define Trivium_KEYSIZE(i) (80 + (i)*32)       /* [edit] */

#define Trivium_MAXIVSIZE 80                   /* [edit] */
#define Trivium_IVSIZE(i) (32 + (i)*16)        /* [edit] */

/* ------------------------------------------------------------------------- */

/* Data structures */

/* 
 * Trivium_ctx is the structure containing the representation of the
 * internal state of your cipher. 
 */

typedef struct
{
  u64 init[2];
  u64 state[6];

} Trivium_ctx;

/* ------------------------------------------------------------------------- */

/* Mandatory functions */

/*
 * Key and message independent initialization. This function will be
 * called once when the program starts (e.g., to build expanded S-box
 * tables).
 */
void Trivium_init(void);

/*
 * Key setup. It is the user's responsibility to select the values of
 * keysize and ivsize from the set of supported values specified
 * above.
 */
void Trivium_keysetup(
  Trivium_ctx* ctx, 
  const u8* key, 
  u32 keysize,                /* Key size in bits. */ 
  u32 ivsize);                /* IV size in bits. */ 

/*
 * IV setup. After having called Trivium_keysetup(), the user is
 * allowed to call Trivium_ivsetup() different times in order to
 * encrypt/decrypt different messages with the same key but different
 * IV's.
 */
void Trivium_ivsetup(
  Trivium_ctx* ctx, 
  const u8* iv);

/*
 * Encryption/decryption of arbitrary length messages.
 *
 * For efficiency reasons, the API provides two types of
 * encrypt/decrypt functions. The Trivium_encrypt_bytes() function
 * (declared here) encrypts byte strings of arbitrary length, while
 * the Trivium_encrypt_blocks() function (defined later) only accepts
 * lengths which are multiples of Trivium_BLOCKLENGTH.
 * 
 * The user is allowed to make multiple calls to
 * Trivium_encrypt_blocks() to incrementally encrypt a long message,
 * but he is NOT allowed to make additional encryption calls once he
 * has called Trivium_encrypt_bytes() (unless he starts a new message
 * of course). For example, this sequence of calls is acceptable:
 *
 * Trivium_keysetup();
 *
 * Trivium_ivsetup();
 * Trivium_encrypt_blocks();
 * Trivium_encrypt_blocks();
 * Trivium_encrypt_bytes();
 *
 * Trivium_ivsetup();
 * Trivium_encrypt_blocks();
 * Trivium_encrypt_blocks();
 *
 * Trivium_ivsetup();
 * Trivium_encrypt_bytes();
 * 
 * The following sequence is not:
 *
 * Trivium_keysetup();
 * Trivium_ivsetup();
 * Trivium_encrypt_blocks();
 * Trivium_encrypt_bytes();
 * Trivium_encrypt_blocks();
 */

/*
 * By default Trivium_encrypt_bytes() and Trivium_decrypt_bytes() are
 * defined as macros which redirect the call to a single function
 * Trivium_process_bytes(). If you want to provide separate encryption
 * and decryption functions, please undef
 * Trivium_HAS_SINGLE_BYTE_FUNCTION.
 */
#define Trivium_HAS_SINGLE_BYTE_FUNCTION       /* [edit] */
#ifdef Trivium_HAS_SINGLE_BYTE_FUNCTION

#define Trivium_encrypt_bytes(ctx, plaintext, ciphertext, msglen)   \
  Trivium_process_bytes(0, ctx, plaintext, ciphertext, msglen)

#define Trivium_decrypt_bytes(ctx, ciphertext, plaintext, msglen)   \
  Trivium_process_bytes(1, ctx, ciphertext, plaintext, msglen)

void Trivium_process_bytes(
  int action,                 /* 0 = encrypt; 1 = decrypt; */
  Trivium_ctx* ctx, 
  const u8* input, 
  u8* output, 
  u32 msglen);                /* Message length in bytes. */ 

#else

void Trivium_encrypt_bytes(
  Trivium_ctx* ctx, 
  const u8* plaintext, 
  u8* ciphertext, 
  u32 msglen);                /* Message length in bytes. */ 

void Trivium_decrypt_bytes(
  Trivium_ctx* ctx, 
  const u8* ciphertext, 
  u8* plaintext, 
  u32 msglen);                /* Message length in bytes. */ 

#endif

/* ------------------------------------------------------------------------- */

/* Optional features */

/* 
 * For testing purposes it can sometimes be useful to have a function
 * which immediately generates keystream without having to provide it
 * with a zero plaintext. If your cipher cannot provide this function
 * (e.g., because it is not strictly a synchronous cipher), please
 * reset the Trivium_GENERATES_KEYSTREAM flag.
 */

#define Trivium_GENERATES_KEYSTREAM
#ifdef Trivium_GENERATES_KEYSTREAM

void Trivium_keystream_bytes(
  Trivium_ctx* ctx,
  u8* keystream,
  u32 length);                /* Length of keystream in bytes. */

#endif

/* ------------------------------------------------------------------------- */

/* Optional optimizations */

/* 
 * By default, the functions in this section are implemented using
 * calls to functions declared above. However, you might want to
 * implement them differently for performance reasons.
 */

/*
 * All-in-one encryption/decryption of (short) packets.
 *
 * The default definitions of these functions can be found in
 * "ecrypt-sync.c". If you want to implement them differently, please
 * undef the Trivium_USES_DEFAULT_ALL_IN_ONE flag.
 */
#define Trivium_USES_DEFAULT_ALL_IN_ONE        /* [edit] */

/*
 * Undef Trivium_HAS_SINGLE_PACKET_FUNCTION if you want to provide
 * separate packet encryption and decryption functions.
 */
#define Trivium_HAS_SINGLE_PACKET_FUNCTION     /* [edit] */
#ifdef Trivium_HAS_SINGLE_PACKET_FUNCTION

#define Trivium_encrypt_packet(                                        \
    ctx, iv, plaintext, ciphertext, mglen)                            \
  Trivium_process_packet(0,                                            \
    ctx, iv, plaintext, ciphertext, mglen)

#define Trivium_decrypt_packet(                                        \
    ctx, iv, ciphertext, plaintext, mglen)                            \
  Trivium_process_packet(1,                                            \
    ctx, iv, ciphertext, plaintext, mglen)

void Trivium_process_packet(
  int action,                 /* 0 = encrypt; 1 = decrypt; */
  Trivium_ctx* ctx, 
  const u8* iv,
  const u8* input, 
  u8* output, 
  u32 msglen);

#else

void Trivium_encrypt_packet(
  Trivium_ctx* ctx, 
  const u8* iv,
  const u8* plaintext, 
  u8* ciphertext, 
  u32 msglen);

void Trivium_decrypt_packet(
  Trivium_ctx* ctx, 
  const u8* iv,
  const u8* ciphertext, 
  u8* plaintext, 
  u32 msglen);

#endif

/*
 * Encryption/decryption of blocks.
 * 
 * By default, these functions are defined as macros. If you want to
 * provide a different implementation, please undef the
 * Trivium_USES_DEFAULT_BLOCK_MACROS flag and implement the functions
 * declared below.
 */

#define Trivium_BLOCKLENGTH 16                 /* [edit] */

#define Trivium_USES_DEFAULT_BLOCK_MACROS      /* [edit] */
#ifdef Trivium_USES_DEFAULT_BLOCK_MACROS

#define Trivium_encrypt_blocks(ctx, plaintext, ciphertext, blocks)  \
  Trivium_encrypt_bytes(ctx, plaintext, ciphertext,                 \
    (blocks) * Trivium_BLOCKLENGTH)

#define Trivium_decrypt_blocks(ctx, ciphertext, plaintext, blocks)  \
  Trivium_decrypt_bytes(ctx, ciphertext, plaintext,                 \
    (blocks) * Trivium_BLOCKLENGTH)

#ifdef Trivium_GENERATES_KEYSTREAM

#define Trivium_keystream_blocks(ctx, keystream, blocks)            \
  Trivium_keystream_bytes(ctx, keystream,                           \
    (blocks) * Trivium_BLOCKLENGTH)

#endif

#else

/*
 * Undef Trivium_HAS_SINGLE_BLOCK_FUNCTION if you want to provide
 * separate block encryption and decryption functions.
 */
#define Trivium_HAS_SINGLE_BLOCK_FUNCTION      /* [edit] */
#ifdef Trivium_HAS_SINGLE_BLOCK_FUNCTION

#define Trivium_encrypt_blocks(ctx, plaintext, ciphertext, blocks)     \
  Trivium_process_blocks(0, ctx, plaintext, ciphertext, blocks)

#define Trivium_decrypt_blocks(ctx, ciphertext, plaintext, blocks)     \
  Trivium_process_blocks(1, ctx, ciphertext, plaintext, blocks)

void Trivium_process_blocks(
  int action,                 /* 0 = encrypt; 1 = decrypt; */
  Trivium_ctx* ctx, 
  const u8* input, 
  u8* output, 
  u32 blocks);                /* Message length in blocks. */

#else

void Trivium_encrypt_blocks(
  Trivium_ctx* ctx, 
  const u8* plaintext, 
  u8* ciphertext, 
  u32 blocks);                /* Message length in blocks. */ 

void Trivium_decrypt_blocks(
  Trivium_ctx* ctx, 
  const u8* ciphertext, 
  u8* plaintext, 
  u32 blocks);                /* Message length in blocks. */ 

#endif

#ifdef Trivium_GENERATES_KEYSTREAM

void Trivium_keystream_blocks(
  Trivium_ctx* ctx,
  u8* keystream,
  u32 blocks);                /* Keystream length in blocks. */ 

#endif

#endif

/*
 * If your cipher can be implemented in different ways, you can use
 * the Trivium_VARIANT parameter to allow the user to choose between
 * them at compile time (e.g., gcc -DTrivium_VARIANT=3 ...). Please
 * only use this possibility if you really think it could make a
 * significant difference and keep the number of variants
 * (Trivium_MAXVARIANT) as small as possible (definitely not more than
 * 10). Note also that all variants should have exactly the same
 * external interface (i.e., the same Trivium_BLOCKLENGTH, etc.). 
 */
#define Trivium_MAXVARIANT 1                   /* [edit] */

#ifndef Trivium_VARIANT
#define Trivium_VARIANT 1
#endif

#if (Trivium_VARIANT > Trivium_MAXVARIANT)
#error this variant does not exist
#endif

/* ------------------------------------------------------------------------- */

#endif

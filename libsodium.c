
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sodium.h>
#include <stdint.h>
#include <string.h>

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "zend_exceptions.h"
#include "zend_API.h"

#if PHP_VERSION_ID < 70100
# define IS_VOID 0
#endif
#if PHP_VERSION_ID < 70200
# undef ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX
# define ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(name, return_reference, required_num_args, type, allow_null) \
    static const zend_internal_arg_info name[] = { \
        { (const char *)(zend_uintptr_t) required_num_args, NULL, type, return_reference, allow_null, 0 },
#endif
#ifndef ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX
# define ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(name, return_reference, num_args, type) \
    ZEND_BEGIN_ARG_INFO_EX(name, 0, return_reference, num_args)
#endif
#ifndef ZEND_ARG_INFO_WITH_DEFAULT_VALUE
# define ZEND_ARG_INFO_WITH_DEFAULT_VALUE(pass_by_ref, name, default_value) \
    ZEND_ARG_INFO(pass_by_ref, name)
#endif
#ifndef ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE
# define ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(pass_by_ref, name, type_hint, allow_null, default_value) \
        ZEND_ARG_TYPE_INFO(pass_by_ref, name, type_hint, allow_null)
#endif

#define PHP_SODIUM_ZSTR_TRUNCATE(zs, len) do { ZSTR_LEN(zs) = (len); ZSTR_VAL(zs)[len] = 0; } while(0)

static zend_always_inline zend_string *zend_string_checked_alloc(size_t len, int persistent)
{
    zend_string *zs;

    if (ZEND_MM_ALIGNED_SIZE(_ZSTR_STRUCT_SIZE(len)) < len) {
        zend_error_noreturn(E_ERROR, "Memory allocation too large (%zu bytes)", len);
    }
    zs = zend_string_alloc(len, persistent);
    ZSTR_VAL(zs)[len] = 0;

    return zs;
}

static zend_class_entry *sodium_exception_ce;

#if (defined(__amd64) || defined(__amd64__) || defined(__x86_64__) || defined(__i386__) || \
     defined(_M_AMD64) || defined(_M_IX86))
# define HAVE_AESGCM 1
#endif
#if SODIUM_LIBRARY_VERSION_MAJOR > 9 || (SODIUM_LIBRARY_VERSION_MAJOR == 9 && SODIUM_LIBRARY_VERSION_MINOR >= 2)
# define HAVE_AEAD_DETACHED 1
#endif

#ifndef crypto_aead_chacha20poly1305_IETF_KEYBYTES
# define crypto_aead_chacha20poly1305_IETF_KEYBYTES crypto_aead_chacha20poly1305_KEYBYTES
#endif
#ifndef crypto_aead_chacha20poly1305_IETF_NSECBYTES
# define crypto_aead_chacha20poly1305_IETF_NSECBYTES crypto_aead_chacha20poly1305_NSECBYTES
#endif
#ifndef crypto_aead_chacha20poly1305_IETF_ABYTES
# define crypto_aead_chacha20poly1305_IETF_ABYTES crypto_aead_chacha20poly1305_ABYTES
#endif

#if defined(crypto_secretstream_xchacha20poly1305_ABYTES) && SODIUM_LIBRARY_VERSION_MAJOR < 10
# undef crypto_secretstream_xchacha20poly1305_ABYTES
#endif

#ifndef crypto_pwhash_OPSLIMIT_MIN
# define crypto_pwhash_OPSLIMIT_MIN crypto_pwhash_OPSLIMIT_INTERACTIVE
#endif
#ifndef crypto_pwhash_MEMLIMIT_MIN
# define crypto_pwhash_MEMLIMIT_MIN crypto_pwhash_MEMLIMIT_INTERACTIVE
#endif
#ifndef crypto_pwhash_scryptsalsa208sha256_OPSLIMIT_MIN
# define crypto_pwhash_scryptsalsa208sha256_OPSLIMIT_MIN crypto_pwhash_scryptsalsa208sha256_OPSLIMIT_INTERACTIVE
#endif
#ifndef crypto_pwhash_scryptsalsa208sha256_MEMLIMIT_MIN
# define crypto_pwhash_scryptsalsa208sha256_MEMLIMIT_MIN crypto_pwhash_scryptsalsa208sha256_MEMLIMIT_INTERACTIVE
#endif

#ifndef PHP_FE_END
# define PHP_FE_END { NULL, NULL, NULL }
#endif

#include "php_libsodium.h"
#include "libsodium_arginfo.h"

#define PHP_FUNC_PROTO(X) PHP_FE(X, arginfo_ ## X)
#define PHP_FUNC_ALIAS(X, Y) PHP_FALIAS(X, Y, arginfo_ ## X)

static const zend_function_entry sodium_functions[] = {
    PHP_FUNC_PROTO(sodium_crypto_aead_aes256gcm_is_available)
#ifdef HAVE_AESGCM
    PHP_FUNC_PROTO(sodium_crypto_aead_aes256gcm_decrypt)
    PHP_FUNC_PROTO(sodium_crypto_aead_aes256gcm_encrypt)
    PHP_FUNC_PROTO(sodium_crypto_aead_aes256gcm_keygen)
# ifdef HAVE_AEAD_DETACHED
    PHP_FUNC_PROTO(sodium_crypto_aead_aes256gcm_decrypt_detached)
    PHP_FUNC_PROTO(sodium_crypto_aead_aes256gcm_encrypt_detached)
# endif
#endif
    PHP_FUNC_PROTO(sodium_crypto_aead_chacha20poly1305_decrypt)
    PHP_FUNC_PROTO(sodium_crypto_aead_chacha20poly1305_encrypt)
    PHP_FUNC_PROTO(sodium_crypto_aead_chacha20poly1305_keygen)
    PHP_FUNC_PROTO(sodium_crypto_aead_chacha20poly1305_ietf_decrypt)
    PHP_FUNC_PROTO(sodium_crypto_aead_chacha20poly1305_ietf_encrypt)
    PHP_FUNC_PROTO(sodium_crypto_aead_chacha20poly1305_ietf_keygen)
#ifdef HAVE_AEAD_DETACHED
    PHP_FUNC_PROTO(sodium_crypto_aead_chacha20poly1305_decrypt_detached)
    PHP_FUNC_PROTO(sodium_crypto_aead_chacha20poly1305_encrypt_detached)
    PHP_FUNC_PROTO(sodium_crypto_aead_chacha20poly1305_ietf_decrypt_detached)
    PHP_FUNC_PROTO(sodium_crypto_aead_chacha20poly1305_ietf_encrypt_detached)
#endif
#ifdef crypto_aead_xchacha20poly1305_IETF_NPUBBYTES
    PHP_FUNC_PROTO(sodium_crypto_aead_xchacha20poly1305_ietf_decrypt)
    PHP_FUNC_PROTO(sodium_crypto_aead_xchacha20poly1305_ietf_keygen)
    PHP_FUNC_PROTO(sodium_crypto_aead_xchacha20poly1305_ietf_encrypt)
# ifdef HAVE_AEAD_DETACHED
    PHP_FUNC_PROTO(sodium_crypto_aead_xchacha20poly1305_ietf_decrypt_detached)
    PHP_FUNC_PROTO(sodium_crypto_aead_xchacha20poly1305_ietf_encrypt_detached)
# endif
#endif
    PHP_FUNC_PROTO(sodium_crypto_auth)
    PHP_FUNC_PROTO(sodium_crypto_auth_keygen)
    PHP_FUNC_PROTO(sodium_crypto_auth_verify)
    PHP_FUNC_PROTO(sodium_crypto_box)
    PHP_FUNC_PROTO(sodium_crypto_box_keypair)
    PHP_FUNC_PROTO(sodium_crypto_box_seed_keypair)
    PHP_FUNC_PROTO(sodium_crypto_box_keypair_from_secretkey_and_publickey)
    PHP_FUNC_PROTO(sodium_crypto_box_open)
    PHP_FUNC_PROTO(sodium_crypto_box_publickey)
    PHP_FUNC_PROTO(sodium_crypto_box_publickey_from_secretkey)
    PHP_FUNC_PROTO(sodium_crypto_box_seal)
    PHP_FUNC_PROTO(sodium_crypto_box_seal_open)
    PHP_FUNC_PROTO(sodium_crypto_box_secretkey)
    PHP_FUNC_PROTO(sodium_crypto_kx_keypair)
    PHP_FUNC_PROTO(sodium_crypto_kx_publickey)
    PHP_FUNC_PROTO(sodium_crypto_kx_secretkey)
    PHP_FUNC_PROTO(sodium_crypto_kx_seed_keypair)
    PHP_FUNC_PROTO(sodium_crypto_kx_client_session_keys)
    PHP_FUNC_PROTO(sodium_crypto_kx_server_session_keys)
    PHP_FUNC_PROTO(sodium_crypto_generichash)
    PHP_FUNC_PROTO(sodium_crypto_generichash_keygen)
    PHP_FUNC_PROTO(sodium_crypto_generichash_init)
    PHP_FUNC_PROTO(sodium_crypto_generichash_update)
    PHP_FUNC_PROTO(sodium_crypto_generichash_final)
    PHP_FUNC_PROTO(sodium_crypto_kdf_derive_from_key)
    PHP_FUNC_PROTO(sodium_crypto_kdf_keygen)
#ifdef crypto_pwhash_SALTBYTES
    PHP_FUNC_PROTO(sodium_crypto_pwhash)
    PHP_FUNC_PROTO(sodium_crypto_pwhash_str)
    PHP_FUNC_PROTO(sodium_crypto_pwhash_str_verify)
#endif
#if SODIUM_LIBRARY_VERSION_MAJOR > 9 || (SODIUM_LIBRARY_VERSION_MAJOR == 9 && SODIUM_LIBRARY_VERSION_MINOR >= 6)
    PHP_FUNC_PROTO(sodium_crypto_pwhash_str_needs_rehash)
#endif
#ifdef crypto_pwhash_scryptsalsa208sha256_SALTBYTES
    PHP_FUNC_PROTO(sodium_crypto_pwhash_scryptsalsa208sha256)
    PHP_FUNC_PROTO(sodium_crypto_pwhash_scryptsalsa208sha256_str)
    PHP_FUNC_PROTO(sodium_crypto_pwhash_scryptsalsa208sha256_str_verify)
#endif
    PHP_FUNC_PROTO(sodium_crypto_scalarmult)
    PHP_FUNC_PROTO(sodium_crypto_secretbox)
    PHP_FUNC_PROTO(sodium_crypto_secretbox_keygen)
    PHP_FUNC_PROTO(sodium_crypto_secretbox_open)
#ifdef crypto_secretstream_xchacha20poly1305_ABYTES
    PHP_FUNC_PROTO(sodium_crypto_secretstream_xchacha20poly1305_keygen)
    PHP_FUNC_PROTO(sodium_crypto_secretstream_xchacha20poly1305_init_push)
    PHP_FUNC_PROTO(sodium_crypto_secretstream_xchacha20poly1305_push)
    PHP_FUNC_PROTO(sodium_crypto_secretstream_xchacha20poly1305_init_pull)
    PHP_FUNC_PROTO(sodium_crypto_secretstream_xchacha20poly1305_pull)
    PHP_FUNC_PROTO(sodium_crypto_secretstream_xchacha20poly1305_rekey)
#endif
    PHP_FUNC_PROTO(sodium_crypto_shorthash)
    PHP_FUNC_PROTO(sodium_crypto_shorthash_keygen)
    PHP_FUNC_PROTO(sodium_crypto_sign)
    PHP_FUNC_PROTO(sodium_crypto_sign_detached)
    PHP_FUNC_PROTO(sodium_crypto_sign_ed25519_pk_to_curve25519)
    PHP_FUNC_PROTO(sodium_crypto_sign_ed25519_sk_to_curve25519)
    PHP_FUNC_PROTO(sodium_crypto_sign_keypair)
    PHP_FUNC_PROTO(sodium_crypto_sign_keypair_from_secretkey_and_publickey)
    PHP_FUNC_PROTO(sodium_crypto_sign_open)
    PHP_FUNC_PROTO(sodium_crypto_sign_publickey)
    PHP_FUNC_PROTO(sodium_crypto_sign_secretkey)
    PHP_FUNC_PROTO(sodium_crypto_sign_publickey_from_secretkey)
    PHP_FUNC_PROTO(sodium_crypto_sign_seed_keypair)
    PHP_FUNC_PROTO(sodium_crypto_sign_verify_detached)
    PHP_FUNC_PROTO(sodium_crypto_stream)
    PHP_FUNC_PROTO(sodium_crypto_stream_keygen)
    PHP_FUNC_PROTO(sodium_crypto_stream_xor)
#ifdef crypto_stream_xchacha20_KEYBYTES
    PHP_FUNC_PROTO(sodium_crypto_stream_xchacha20)
    PHP_FUNC_PROTO(sodium_crypto_stream_xchacha20_xor)
#endif

    /* helpers */

    PHP_FUNC_PROTO(sodium_add)
    PHP_FUNC_PROTO(sodium_compare)
    PHP_FUNC_PROTO(sodium_increment)
    PHP_FUNC_PROTO(sodium_memcmp)
    PHP_FUNC_PROTO(sodium_memzero)
    PHP_FUNC_PROTO(sodium_pad)
    PHP_FUNC_PROTO(sodium_unpad)

    /* codecs */

    PHP_FUNC_PROTO(sodium_bin2hex)
    PHP_FUNC_PROTO(sodium_hex2bin)
#ifdef sodium_base64_VARIANT_ORIGINAL
    PHP_FUNC_PROTO(sodium_bin2base64)
    PHP_FUNC_PROTO(sodium_base642bin)
#endif

    /* aliases */

    PHP_FUNC_ALIAS(sodium_crypto_scalarmult_base, sodium_crypto_box_publickey_from_secretkey)

    PHP_FE_END
};

zend_module_entry sodium_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    "sodium",
    sodium_functions,
    PHP_MINIT(sodium),
    PHP_MSHUTDOWN(sodium),
    NULL,
    NULL,
    PHP_MINFO(sodium),
#if ZEND_MODULE_API_NO >= 20010901
    PHP_SODIUM_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_SODIUM
ZEND_GET_MODULE(sodium)
#endif

static zend_object *sodium_exception_create_object(zend_class_entry *ce) {
    zend_object *obj = zend_ce_exception->create_object(ce);
    zval rv, *trace;

#if ZEND_MODULE_API_NO >= 20200930
    zend_object *obj_zv = obj;
#else
    zval obj_zv[1];
    ZVAL_OBJ(obj_zv, obj);
#endif
    trace = zend_read_property(zend_get_exception_base(obj_zv), obj_zv, "trace", (sizeof "trace") - 1, 0, &rv);

    /* Remove argument information from backtrace to prevent information leaks */
    if (trace && Z_TYPE_P(trace) == IS_ARRAY) {
        zval *frame = NULL;
        ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(trace), frame) {
            if (Z_TYPE_P(frame) == IS_ARRAY) {
                zval *args = zend_hash_str_find(Z_ARRVAL_P(frame), "args", (sizeof "args") - 1);

                if (args != NULL) {
#ifdef ZVAL_EMPTY_ARRAY
                    zval_ptr_dtor(args);
                    ZVAL_EMPTY_ARRAY(args);
#else
                    if (Z_TYPE_P(args) == IS_ARRAY) {
                        zend_hash_clean(Z_ARRVAL_P(args));
                    }
#endif
                }
            }
        } ZEND_HASH_FOREACH_END();
    }
    return obj;
}

static void sodium_separate_string(zval *zv) {
    ZEND_ASSERT(Z_TYPE_P(zv) == IS_STRING);
    if (!Z_REFCOUNTED_P(zv) || Z_REFCOUNT_P(zv) > 1) {
        zend_string *copy = zend_string_init(Z_STRVAL_P(zv), Z_STRLEN_P(zv), 0);
        Z_TRY_DELREF_P(zv);
        ZVAL_STR(zv, copy);
    }
}

PHP_MINIT_FUNCTION(sodium)
{
    zend_class_entry ce;

    if (sodium_init() < 0) {
        zend_error(E_ERROR, "sodium_init()");
    }

    INIT_CLASS_ENTRY(ce, "SodiumException", NULL);
    sodium_exception_ce = zend_register_internal_class_ex(&ce, zend_ce_exception);
    sodium_exception_ce->create_object = sodium_exception_create_object;

    REGISTER_STRING_CONSTANT("SODIUM_LIBRARY_VERSION",
                             (char *) (void *) sodium_version_string(), CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_LIBRARY_MAJOR_VERSION",
                           sodium_library_version_major(), CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_LIBRARY_MINOR_VERSION",
                           sodium_library_version_minor(), CONST_CS | CONST_PERSISTENT);
#ifdef HAVE_AESGCM
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_AEAD_AES256GCM_KEYBYTES",
                           crypto_aead_aes256gcm_KEYBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_AEAD_AES256GCM_NSECBYTES",
                           crypto_aead_aes256gcm_NSECBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_AEAD_AES256GCM_NPUBBYTES",
                           crypto_aead_aes256gcm_NPUBBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_AEAD_AES256GCM_ABYTES",
                           crypto_aead_aes256gcm_ABYTES, CONST_CS | CONST_PERSISTENT);
#endif
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_KEYBYTES",
                           crypto_aead_chacha20poly1305_KEYBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_NSECBYTES",
                           crypto_aead_chacha20poly1305_NSECBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_NPUBBYTES",
                           crypto_aead_chacha20poly1305_NPUBBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_ABYTES",
                           crypto_aead_chacha20poly1305_ABYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_IETF_KEYBYTES",
                           crypto_aead_chacha20poly1305_IETF_KEYBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_IETF_NSECBYTES",
                           crypto_aead_chacha20poly1305_IETF_NSECBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_IETF_NPUBBYTES",
                           crypto_aead_chacha20poly1305_IETF_NPUBBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_IETF_ABYTES",
                           crypto_aead_chacha20poly1305_IETF_ABYTES, CONST_CS | CONST_PERSISTENT);
#ifdef crypto_aead_xchacha20poly1305_IETF_NPUBBYTES
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_AEAD_XCHACHA20POLY1305_IETF_KEYBYTES",
                           crypto_aead_xchacha20poly1305_IETF_KEYBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_AEAD_XCHACHA20POLY1305_IETF_NSECBYTES",
                           crypto_aead_xchacha20poly1305_IETF_NSECBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_AEAD_XCHACHA20POLY1305_IETF_NPUBBYTES",
                           crypto_aead_xchacha20poly1305_IETF_NPUBBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_AEAD_XCHACHA20POLY1305_IETF_ABYTES",
                           crypto_aead_xchacha20poly1305_IETF_ABYTES, CONST_CS | CONST_PERSISTENT);
#endif
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_AUTH_BYTES",
                           crypto_auth_BYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_AUTH_KEYBYTES",
                           crypto_auth_KEYBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_BOX_SEALBYTES",
                           crypto_box_SEALBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_BOX_SECRETKEYBYTES",
                           crypto_box_SECRETKEYBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_BOX_PUBLICKEYBYTES",
                           crypto_box_PUBLICKEYBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_BOX_KEYPAIRBYTES",
                           crypto_box_SECRETKEYBYTES + crypto_box_PUBLICKEYBYTES,
                           CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_BOX_MACBYTES",
                           crypto_box_MACBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_BOX_NONCEBYTES",
                           crypto_box_NONCEBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_BOX_SEEDBYTES",
                           crypto_box_SEEDBYTES, CONST_CS | CONST_PERSISTENT);
#ifndef crypto_kdf_BYTES_MIN
# define crypto_kdf_BYTES_MIN 16
# define crypto_kdf_BYTES_MAX 64
# define crypto_kdf_CONTEXTBYTES 8
# define crypto_kdf_KEYBYTES 32
#endif
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_KDF_BYTES_MIN",
                           crypto_kdf_BYTES_MIN, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_KDF_BYTES_MAX",
                           crypto_kdf_BYTES_MAX, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_KDF_CONTEXTBYTES",
                           crypto_kdf_CONTEXTBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_KDF_KEYBYTES",
                           crypto_kdf_KEYBYTES, CONST_CS | CONST_PERSISTENT);
#ifndef crypto_kx_SEEDBYTES
# define crypto_kx_SEEDBYTES 32
# define crypto_kx_SESSIONKEYBYTES 32
# define crypto_kx_PUBLICKEYBYTES 32
# define crypto_kx_SECRETKEYBYTES 32
#endif
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_KX_SEEDBYTES",
                           crypto_kx_SEEDBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_KX_SESSIONKEYBYTES",
                           crypto_kx_SESSIONKEYBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_KX_PUBLICKEYBYTES",
                           crypto_kx_PUBLICKEYBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_KX_SECRETKEYBYTES",
                           crypto_kx_SECRETKEYBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_KX_KEYPAIRBYTES",
                           crypto_kx_SECRETKEYBYTES + crypto_kx_PUBLICKEYBYTES,
                           CONST_CS | CONST_PERSISTENT);
#ifdef crypto_secretstream_xchacha20poly1305_ABYTES
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_SECRETSTREAM_XCHACHA20POLY1305_ABYTES",
                           crypto_secretstream_xchacha20poly1305_ABYTES,
                           CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_SECRETSTREAM_XCHACHA20POLY1305_HEADERBYTES",
                           crypto_secretstream_xchacha20poly1305_HEADERBYTES,
                           CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_SECRETSTREAM_XCHACHA20POLY1305_KEYBYTES",
                           crypto_secretstream_xchacha20poly1305_KEYBYTES,
                           CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_SECRETSTREAM_XCHACHA20POLY1305_MESSAGEBYTES_MAX",
                           crypto_secretstream_xchacha20poly1305_MESSAGEBYTES_MAX,
                           CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_SECRETSTREAM_XCHACHA20POLY1305_TAG_MESSAGE",
                           crypto_secretstream_xchacha20poly1305_TAG_MESSAGE,
                           CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_SECRETSTREAM_XCHACHA20POLY1305_TAG_PUSH",
                           crypto_secretstream_xchacha20poly1305_TAG_PUSH,
                           CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_SECRETSTREAM_XCHACHA20POLY1305_TAG_REKEY",
                           crypto_secretstream_xchacha20poly1305_TAG_REKEY,
                           CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_SECRETSTREAM_XCHACHA20POLY1305_TAG_FINAL",
                           crypto_secretstream_xchacha20poly1305_TAG_FINAL,
                           CONST_CS | CONST_PERSISTENT);
#endif
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_GENERICHASH_BYTES",
                           crypto_generichash_BYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_GENERICHASH_BYTES_MIN",
                           crypto_generichash_BYTES_MIN, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_GENERICHASH_BYTES_MAX",
                           crypto_generichash_BYTES_MAX, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_GENERICHASH_KEYBYTES",
                           crypto_generichash_KEYBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_GENERICHASH_KEYBYTES_MIN",
                           crypto_generichash_KEYBYTES_MIN, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_GENERICHASH_KEYBYTES_MAX",
                           crypto_generichash_KEYBYTES_MAX, CONST_CS | CONST_PERSISTENT);
#ifdef crypto_pwhash_SALTBYTES
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_PWHASH_ALG_ARGON2I13",
                           crypto_pwhash_ALG_ARGON2I13, CONST_CS | CONST_PERSISTENT);
# ifdef crypto_pwhash_ALG_ARGON2ID13
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_PWHASH_ALG_ARGON2ID13",
                           crypto_pwhash_ALG_ARGON2ID13, CONST_CS | CONST_PERSISTENT);
# endif
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_PWHASH_ALG_DEFAULT",
                           crypto_pwhash_ALG_DEFAULT, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_PWHASH_SALTBYTES",
                           crypto_pwhash_SALTBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_STRING_CONSTANT("SODIUM_CRYPTO_PWHASH_STRPREFIX",
                             crypto_pwhash_STRPREFIX, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_PWHASH_STRBYTES",
                           crypto_pwhash_STRBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_PWHASH_OPSLIMIT_INTERACTIVE",
                           crypto_pwhash_opslimit_interactive(), CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_PWHASH_MEMLIMIT_INTERACTIVE",
                           crypto_pwhash_memlimit_interactive(), CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_PWHASH_OPSLIMIT_MODERATE",
                           crypto_pwhash_opslimit_moderate(), CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_PWHASH_MEMLIMIT_MODERATE",
                           crypto_pwhash_memlimit_moderate(), CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_PWHASH_OPSLIMIT_SENSITIVE",
                           crypto_pwhash_opslimit_sensitive(), CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_PWHASH_MEMLIMIT_SENSITIVE",
                           crypto_pwhash_memlimit_sensitive(), CONST_CS | CONST_PERSISTENT);
#endif
#ifdef crypto_pwhash_scryptsalsa208sha256_SALTBYTES
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_PWHASH_SCRYPTSALSA208SHA256_SALTBYTES",
                           crypto_pwhash_scryptsalsa208sha256_SALTBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_STRING_CONSTANT("SODIUM_CRYPTO_PWHASH_SCRYPTSALSA208SHA256_STRPREFIX",
                             crypto_pwhash_scryptsalsa208sha256_STRPREFIX, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_PWHASH_SCRYPTSALSA208SHA256_OPSLIMIT_INTERACTIVE",
                           crypto_pwhash_scryptsalsa208sha256_opslimit_interactive(), CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_PWHASH_SCRYPTSALSA208SHA256_MEMLIMIT_INTERACTIVE",
                           crypto_pwhash_scryptsalsa208sha256_memlimit_interactive(), CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_PWHASH_SCRYPTSALSA208SHA256_OPSLIMIT_SENSITIVE",
                           crypto_pwhash_scryptsalsa208sha256_opslimit_sensitive(), CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_PWHASH_SCRYPTSALSA208SHA256_MEMLIMIT_SENSITIVE",
                           crypto_pwhash_scryptsalsa208sha256_memlimit_sensitive(), CONST_CS | CONST_PERSISTENT);
#endif
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_SCALARMULT_BYTES",
                           crypto_scalarmult_BYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_SCALARMULT_SCALARBYTES",
                           crypto_scalarmult_SCALARBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_SHORTHASH_BYTES",
                           crypto_shorthash_BYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_SHORTHASH_KEYBYTES",
                           crypto_shorthash_KEYBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_SECRETBOX_KEYBYTES",
                           crypto_secretbox_KEYBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_SECRETBOX_MACBYTES",
                           crypto_secretbox_MACBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_SECRETBOX_NONCEBYTES",
                           crypto_secretbox_NONCEBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_SIGN_BYTES",
                           crypto_sign_BYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_SIGN_SEEDBYTES",
                           crypto_sign_SEEDBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_SIGN_PUBLICKEYBYTES",
                           crypto_sign_PUBLICKEYBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_SIGN_SECRETKEYBYTES",
                           crypto_sign_SECRETKEYBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_SIGN_KEYPAIRBYTES",
                           crypto_sign_SECRETKEYBYTES + crypto_sign_PUBLICKEYBYTES,
                           CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_STREAM_NONCEBYTES",
                           crypto_stream_NONCEBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_STREAM_KEYBYTES",
                           crypto_stream_KEYBYTES, CONST_CS | CONST_PERSISTENT);
#ifdef crypto_stream_xchacha20_KEYBYTES
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_STREAM_XCHACHA20_NONCEBYTES",
                           crypto_stream_xchacha20_NONCEBYTES, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_CRYPTO_STREAM_XCHACHA20_KEYBYTES",
                           crypto_stream_xchacha20_KEYBYTES, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef sodium_base64_VARIANT_ORIGINAL
    REGISTER_LONG_CONSTANT("SODIUM_BASE64_VARIANT_ORIGINAL",
                           sodium_base64_VARIANT_ORIGINAL, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_BASE64_VARIANT_ORIGINAL_NO_PADDING",
                           sodium_base64_VARIANT_ORIGINAL_NO_PADDING, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_BASE64_VARIANT_URLSAFE",
                           sodium_base64_VARIANT_URLSAFE, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("SODIUM_BASE64_VARIANT_URLSAFE_NO_PADDING",
                           sodium_base64_VARIANT_URLSAFE_NO_PADDING, CONST_CS | CONST_PERSISTENT);
#endif
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(sodium)
{
    randombytes_close();
    return SUCCESS;
}

PHP_MINFO_FUNCTION(sodium)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "sodium support", "enabled");
    php_info_print_table_row(2, "sodium compiled version", PHP_SODIUM_VERSION);
    php_info_print_table_row(2, "libsodium headers version", SODIUM_VERSION_STRING);
    php_info_print_table_row(2, "libsodium library version", sodium_version_string());
    php_info_print_table_end();
}

PHP_FUNCTION(sodium_memzero)
{
    zval      *buf_zv;

    if (zend_parse_parameters(ZEND_NUM_ARGS(),
                              "z", &buf_zv) == FAILURE) {
        return;
    }
    ZVAL_DEREF(buf_zv);
    if (Z_TYPE_P(buf_zv) != IS_STRING) {
        zend_throw_exception(sodium_exception_ce, "a PHP string is required", 0);
        return;
    }
    if (Z_REFCOUNTED_P(buf_zv) && Z_REFCOUNT_P(buf_zv) == 1) {
        char *buf = Z_STRVAL(*buf_zv);
        size_t buf_len = Z_STRLEN(*buf_zv);
        if (buf_len > 0) {
            sodium_memzero(buf, (size_t) buf_len);
        }
    }
    convert_to_null(buf_zv);
}

PHP_FUNCTION(sodium_increment)
{
    zval          *val_zv;
    unsigned char *val;
    size_t         val_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(),
                              "z", &val_zv) == FAILURE) {
        return;
    }
    ZVAL_DEREF(val_zv);
    if (Z_TYPE_P(val_zv) != IS_STRING) {
        zend_throw_exception(sodium_exception_ce, "a PHP string is required", 0);
        return;
    }

    sodium_separate_string(val_zv);
    val = (unsigned char *) Z_STRVAL(*val_zv);
    val_len = Z_STRLEN(*val_zv);
    sodium_increment(val, val_len);
}

PHP_FUNCTION(sodium_add)
{
    zval          *val_zv;
    unsigned char *val;
    unsigned char *addv;
    size_t         val_len;
    size_t         addv_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(),
                              "zs", &val_zv, &addv, &addv_len) == FAILURE) {
        return;
    }
    ZVAL_DEREF(val_zv);
    if (Z_TYPE_P(val_zv) != IS_STRING) {
        zend_throw_exception(sodium_exception_ce, "PHP strings are required", 0);
        return;
    }

    sodium_separate_string(val_zv);
    val = (unsigned char *) Z_STRVAL(*val_zv);
    val_len = Z_STRLEN(*val_zv);
    if (val_len != addv_len) {
        zend_throw_exception(sodium_exception_ce, "values must have the same length", 0);
        return;
    }
    sodium_add(val, addv, val_len);
}

PHP_FUNCTION(sodium_memcmp)
{
    char      *buf1;
    char      *buf2;
    size_t     len1;
    size_t     len2;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss",
                              &buf1, &len1,
                              &buf2, &len2) == FAILURE) {
        return;
    }
    if (len1 != len2) {
        zend_throw_exception(sodium_exception_ce, "arguments have different sizes", 0);
        return;
    }
    RETURN_LONG(sodium_memcmp(buf1, buf2, len1));
}

PHP_FUNCTION(sodium_crypto_shorthash)
{
    zend_string   *hash;
    unsigned char *key;
    unsigned char *msg;
    size_t         key_len;
    size_t         msg_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss",
                              &msg, &msg_len,
                              &key, &key_len) == FAILURE) {
        return;
    }
    if (key_len != crypto_shorthash_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "key size should be SODIUM_CRYPTO_SHORTHASH_KEYBYTES bytes",
                   0);
        return;
    }
    hash = zend_string_checked_alloc(crypto_shorthash_BYTES, 0);
    if (crypto_shorthash((unsigned char *) ZSTR_VAL(hash), msg,
                         (unsigned long long) msg_len, key) != 0) {
        zend_string_free(hash);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }

    RETURN_STR(hash);
}

PHP_FUNCTION(sodium_crypto_secretbox)
{
    zend_string   *ciphertext;
    unsigned char *key;
    unsigned char *msg;
    unsigned char *nonce;
    size_t         key_len;
    size_t         msg_len;
    size_t         nonce_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sss",
                              &msg, &msg_len,
                              &nonce, &nonce_len,
                              &key, &key_len) == FAILURE) {
        return;
    }
    if (nonce_len != crypto_secretbox_NONCEBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "nonce size should be SODIUM_CRYPTO_SECRETBOX_NONCEBYTES bytes",
                   0);
        return;
    }
    if (key_len != crypto_secretbox_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "key size should be SODIUM_CRYPTO_SECRETBOX_KEYBYTES bytes",
                   0);
        return;
    }
    if (SIZE_MAX - msg_len <= crypto_secretbox_MACBYTES) {
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    ciphertext = zend_string_checked_alloc((size_t) msg_len + crypto_secretbox_MACBYTES, 0);
    if (crypto_secretbox_easy((unsigned char *) ZSTR_VAL(ciphertext),
                              msg, (unsigned long long) msg_len,
                              nonce, key) != 0) {
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }

    RETURN_STR(ciphertext);
}

PHP_FUNCTION(sodium_crypto_secretbox_open)
{
    zend_string   *msg;
    unsigned char *key;
    unsigned char *ciphertext;
    unsigned char *nonce;
    size_t         key_len;
    size_t         ciphertext_len;
    size_t         nonce_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sss",
                              &ciphertext, &ciphertext_len,
                              &nonce, &nonce_len,
                              &key, &key_len) == FAILURE) {
        return;
    }
    if (nonce_len != crypto_secretbox_NONCEBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "nonce size should be SODIUM_CRYPTO_SECRETBOX_NONCEBYTES bytes",
                   0);
        return;
    }
    if (key_len != crypto_secretbox_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "key size should be SODIUM_CRYPTO_SECRETBOX_KEYBYTES bytes",
                   0);
        return;
    }
    if (ciphertext_len < crypto_secretbox_MACBYTES) {
        RETURN_FALSE;
    }
    msg = zend_string_checked_alloc
        ((size_t) ciphertext_len - crypto_secretbox_MACBYTES, 0);
    if (crypto_secretbox_open_easy((unsigned char *) ZSTR_VAL(msg), ciphertext,
                                   (unsigned long long) ciphertext_len,
                                   nonce, key) != 0) {
        zend_string_free(msg);
        RETURN_FALSE;
    } else {
        RETURN_STR(msg);
    }
}

PHP_FUNCTION(sodium_crypto_generichash)
{
    zend_string   *hash;
    unsigned char *key = NULL;
    unsigned char *msg;
    zend_long      hash_len = crypto_generichash_BYTES;
    size_t         key_len = 0;
    size_t         msg_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|sl",
                              &msg, &msg_len,
                              &key, &key_len,
                              &hash_len) == FAILURE) {
        return;
    }
    if (hash_len < crypto_generichash_BYTES_MIN ||
        hash_len > crypto_generichash_BYTES_MAX) {
        zend_throw_exception(sodium_exception_ce, "unsupported output length", 0);
        return;
    }
    if (key_len != 0 &&
        (key_len < crypto_generichash_KEYBYTES_MIN ||
         key_len > crypto_generichash_KEYBYTES_MAX)) {
        zend_throw_exception(sodium_exception_ce, "unsupported key length", 0);
        return;
    }
    hash = zend_string_checked_alloc(hash_len, 0);
    if (crypto_generichash((unsigned char *) ZSTR_VAL(hash), (size_t) hash_len,
                           msg, (unsigned long long) msg_len,
                           key, (size_t) key_len) != 0) {
        zend_string_free(hash);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }

    RETURN_STR(hash);
}

PHP_FUNCTION(sodium_crypto_generichash_init)
{
    crypto_generichash_state  state_tmp;
    zend_string              *state;
    unsigned char            *key = NULL;
    size_t                    state_len = sizeof (crypto_generichash_state);
    zend_long                 hash_len = crypto_generichash_BYTES;
    size_t                    key_len = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|sl",
                              &key, &key_len,
                              &hash_len) == FAILURE) {
        return;
    }
    if (hash_len < crypto_generichash_BYTES_MIN ||
        hash_len > crypto_generichash_BYTES_MAX) {
        zend_throw_exception(sodium_exception_ce, "unsupported output length", 0);
        return;
    }
    if (key_len != 0 &&
        (key_len < crypto_generichash_KEYBYTES_MIN ||
         key_len > crypto_generichash_KEYBYTES_MAX)) {
        zend_throw_exception(sodium_exception_ce, "unsupported key length", 0);
        return;
    }
    memset(&state_tmp, 0, sizeof state_tmp);
    if (crypto_generichash_init((void *) &state_tmp, key, (size_t) key_len,
                                (size_t) hash_len) != 0) {
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }
    state = zend_string_checked_alloc(state_len, 0);
    memcpy(ZSTR_VAL(state), &state_tmp, state_len);
    sodium_memzero(&state_tmp, sizeof state_tmp);

    RETURN_STR(state);
}

PHP_FUNCTION(sodium_crypto_generichash_update)
{
    crypto_generichash_state  state_tmp;
    zval                     *state_zv;
    unsigned char            *msg;
    unsigned char            *state;
    size_t                    msg_len;
    size_t                    state_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "zs",
                              &state_zv, &msg, &msg_len) == FAILURE) {
        return;
    }
    ZVAL_DEREF(state_zv);
    if (Z_TYPE_P(state_zv) != IS_STRING) {
        zend_throw_exception(sodium_exception_ce, "a reference to a state is required", 0);
        return;
    }
    sodium_separate_string(state_zv);
    state = (unsigned char *) Z_STRVAL(*state_zv);
    state_len = Z_STRLEN(*state_zv);
    if (state_len != sizeof (crypto_generichash_state)) {
        zend_throw_exception(sodium_exception_ce, "incorrect state length", 0);
        return;
    }
    memcpy(&state_tmp, state, sizeof state_tmp);
    if (crypto_generichash_update((void *) &state_tmp, msg,
                                  (unsigned long long) msg_len) != 0) {
        sodium_memzero(&state_tmp, sizeof state_tmp);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }
    memcpy(state, &state_tmp, state_len);
    sodium_memzero(&state_tmp, sizeof state_tmp);

    RETURN_TRUE;
}

PHP_FUNCTION(sodium_crypto_generichash_final)
{
    crypto_generichash_state  state_tmp;
    zend_string              *hash;
    zval                     *state_zv;
    unsigned char            *state;
    size_t                    state_len;
    zend_long                 hash_len = crypto_generichash_BYTES;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|l",
                              &state_zv, &hash_len) == FAILURE) {
        return;
    }
    ZVAL_DEREF(state_zv);
    if (Z_TYPE_P(state_zv) != IS_STRING) {
        zend_throw_exception(sodium_exception_ce, "a reference to a state is required", 0);
        return;
    }
    sodium_separate_string(state_zv);
    state = (unsigned char *) Z_STRVAL(*state_zv);
    state_len = Z_STRLEN(*state_zv);
    if (state_len != sizeof (crypto_generichash_state)) {
        zend_throw_exception(sodium_exception_ce, "incorrect state length", 0);
        return;
    }
    if (hash_len < crypto_generichash_BYTES_MIN ||
        hash_len > crypto_generichash_BYTES_MAX) {
        zend_throw_exception(sodium_exception_ce, "unsupported output length", 0);
        return;
    }
    hash = zend_string_checked_alloc(hash_len, 0);
    memcpy(&state_tmp, state, sizeof state_tmp);
    if (crypto_generichash_final((void *) &state_tmp,
                                 (unsigned char *) ZSTR_VAL(hash),
                                 (size_t) hash_len) != 0) {
        sodium_memzero(&state_tmp, sizeof state_tmp);
        zend_string_free(hash);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }
    sodium_memzero(&state_tmp, sizeof state_tmp);
    sodium_memzero(state, state_len);
    convert_to_null(state_zv);

    RETURN_STR(hash);
}

PHP_FUNCTION(sodium_crypto_box_keypair)
{
    zend_string *keypair;
    size_t       keypair_len;

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }
    keypair_len = crypto_box_SECRETKEYBYTES + crypto_box_PUBLICKEYBYTES;
    keypair = zend_string_checked_alloc(keypair_len, 0);
    if (crypto_box_keypair((unsigned char *) ZSTR_VAL(keypair) +
                           crypto_box_SECRETKEYBYTES,
                           (unsigned char *) ZSTR_VAL(keypair)) != 0) {
        zend_string_free(keypair);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }

    RETURN_STR(keypair);
}

PHP_FUNCTION(sodium_crypto_box_seed_keypair)
{
    zend_string   *keypair;
    unsigned char *seed;
    size_t         keypair_len;
    size_t         seed_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s",
                              &seed, &seed_len) == FAILURE) {
        return;
    }
    if (seed_len != crypto_box_SEEDBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "seed should be SODIUM_CRYPTO_BOX_SEEDBYTES bytes",
                   0);
        return;
    }
    keypair_len = crypto_box_SECRETKEYBYTES + crypto_box_PUBLICKEYBYTES;
    keypair = zend_string_checked_alloc(keypair_len, 0);
    if (crypto_box_seed_keypair((unsigned char *) ZSTR_VAL(keypair) +
                                 crypto_box_SECRETKEYBYTES,
                                 (unsigned char *) ZSTR_VAL(keypair),
                                 seed) != 0) {
        zend_string_free(keypair);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }

    RETURN_STR(keypair);
}

PHP_FUNCTION(sodium_crypto_box_keypair_from_secretkey_and_publickey)
{
    zend_string *keypair;
    char        *publickey;
    char        *secretkey;
    size_t       keypair_len;
    size_t       publickey_len;
    size_t       secretkey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss",
                              &secretkey, &secretkey_len,
                              &publickey, &publickey_len) == FAILURE) {
        return;
    }
    if (secretkey_len != crypto_box_SECRETKEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "secretkey should be SODIUM_CRYPTO_BOX_SECRETKEYBYTES bytes",
                   0);
        return;
    }
    if (publickey_len != crypto_box_PUBLICKEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "publickey should be SODIUM_CRYPTO_BOX_PUBLICKEYBYTES bytes",
                   0);
        return;
    }
    keypair_len = crypto_box_SECRETKEYBYTES + crypto_box_PUBLICKEYBYTES;
    keypair = zend_string_checked_alloc(keypair_len, 0);
    memcpy(ZSTR_VAL(keypair), secretkey, crypto_box_SECRETKEYBYTES);
    memcpy(ZSTR_VAL(keypair) + crypto_box_SECRETKEYBYTES, publickey,
           crypto_box_PUBLICKEYBYTES);

    RETURN_STR(keypair);
}

PHP_FUNCTION(sodium_crypto_box_secretkey)
{
    zend_string   *secretkey;
    unsigned char *keypair;
    size_t         keypair_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s",
                              &keypair, &keypair_len) == FAILURE) {
        return;
    }
    if (keypair_len !=
        crypto_box_SECRETKEYBYTES + crypto_box_PUBLICKEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "keypair should be SODIUM_CRYPTO_BOX_KEYPAIRBYTES bytes",
                   0);
        return;
    }
    secretkey = zend_string_checked_alloc(crypto_box_SECRETKEYBYTES, 0);
    memcpy(ZSTR_VAL(secretkey), keypair, crypto_box_SECRETKEYBYTES);

    RETURN_STR(secretkey);
}

PHP_FUNCTION(sodium_crypto_box_publickey)
{
    zend_string   *publickey;
    unsigned char *keypair;
    size_t         keypair_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s",
                              &keypair, &keypair_len) == FAILURE) {
        return;
    }
    if (keypair_len !=
        crypto_box_SECRETKEYBYTES + crypto_box_PUBLICKEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "keypair should be SODIUM_CRYPTO_BOX_KEYPAIRBYTES bytes",
                   0);
        return;
    }
    publickey = zend_string_checked_alloc(crypto_box_PUBLICKEYBYTES, 0);
    memcpy(ZSTR_VAL(publickey), keypair + crypto_box_SECRETKEYBYTES,
           crypto_box_PUBLICKEYBYTES);

    RETURN_STR(publickey);
}

PHP_FUNCTION(sodium_crypto_box_publickey_from_secretkey)
{
    zend_string   *publickey;
    unsigned char *secretkey;
    size_t         secretkey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s",
                              &secretkey, &secretkey_len) == FAILURE) {
        return;
    }
    if (secretkey_len != crypto_box_SECRETKEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "key should be SODIUM_CRYPTO_BOX_SECRETKEYBYTES bytes",
                   0);
        return;
    }
    publickey = zend_string_checked_alloc(crypto_box_PUBLICKEYBYTES, 0);
    (void) sizeof(int[crypto_scalarmult_BYTES ==
                      crypto_box_PUBLICKEYBYTES ? 1 : -1]);
    (void) sizeof(int[crypto_scalarmult_SCALARBYTES ==
                      crypto_box_SECRETKEYBYTES ? 1 : -1]);
    crypto_scalarmult_base((unsigned char *) ZSTR_VAL(publickey), secretkey);

    RETURN_STR(publickey);
}

PHP_FUNCTION(sodium_crypto_box)
{
    zend_string   *ciphertext;
    unsigned char *keypair;
    unsigned char *msg;
    unsigned char *nonce;
    unsigned char *publickey;
    unsigned char *secretkey;
    size_t         keypair_len;
    size_t         msg_len;
    size_t         nonce_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sss",
                              &msg, &msg_len,
                              &nonce, &nonce_len,
                              &keypair, &keypair_len) == FAILURE) {
        return;
    }
    if (nonce_len != crypto_box_NONCEBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "nonce size should be SODIUM_CRYPTO_BOX_NONCEBYTES bytes",
                   0);
        return;
    }
    if (keypair_len != crypto_box_SECRETKEYBYTES + crypto_box_PUBLICKEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "keypair size should be SODIUM_CRYPTO_BOX_KEYPAIRBYTES bytes",
                   0);
        return;
    }
    secretkey = keypair;
    publickey = keypair + crypto_box_SECRETKEYBYTES;
    if (SIZE_MAX - msg_len <= crypto_box_MACBYTES) {
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    ciphertext = zend_string_checked_alloc((size_t) msg_len + crypto_box_MACBYTES, 0);
    if (crypto_box_easy((unsigned char *) ZSTR_VAL(ciphertext), msg,
                        (unsigned long long) msg_len,
                        nonce, publickey, secretkey) != 0) {
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }

    RETURN_STR(ciphertext);
}

PHP_FUNCTION(sodium_crypto_box_open)
{
    zend_string   *msg;
    unsigned char *ciphertext;
    unsigned char *keypair;
    unsigned char *nonce;
    unsigned char *publickey;
    unsigned char *secretkey;
    size_t         ciphertext_len;
    size_t         keypair_len;
    size_t         nonce_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sss",
                              &ciphertext, &ciphertext_len,
                              &nonce, &nonce_len,
                              &keypair, &keypair_len) == FAILURE) {
        return;
    }
    if (nonce_len != crypto_box_NONCEBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "nonce size should be SODIUM_CRYPTO_BOX_NONCEBYTES bytes",
                   0);
        return;
    }
    if (keypair_len != crypto_box_SECRETKEYBYTES + crypto_box_PUBLICKEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "keypair size should be SODIUM_CRYPTO_BOX_KEYPAIRBYTES bytes",
                   0);
        return;
    }
    secretkey = keypair;
    publickey = keypair + crypto_box_SECRETKEYBYTES;
    if (ciphertext_len < crypto_box_MACBYTES) {
        RETURN_FALSE;
    }
    msg = zend_string_checked_alloc((size_t) ciphertext_len - crypto_box_MACBYTES, 0);
    if (crypto_box_open_easy((unsigned char *) ZSTR_VAL(msg), ciphertext,
                             (unsigned long long) ciphertext_len,
                             nonce, publickey, secretkey) != 0) {
        zend_string_free(msg);
        RETURN_FALSE;
    } else {
        RETURN_STR(msg);
    }
}

PHP_FUNCTION(sodium_crypto_box_seal)
{
    zend_string   *ciphertext;
    unsigned char *msg;
    unsigned char *publickey;
    size_t         msg_len;
    size_t         publickey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss",
                              &msg, &msg_len,
                              &publickey, &publickey_len) == FAILURE) {
        return;
    }
    if (publickey_len != crypto_box_PUBLICKEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "public key size should be SODIUM_CRYPTO_BOX_PUBLICKEYBYTES bytes",
                   0);
        return;
    }
    if (SIZE_MAX - msg_len <= crypto_box_SEALBYTES) {
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    ciphertext = zend_string_checked_alloc((size_t) msg_len + crypto_box_SEALBYTES, 0);
    if (crypto_box_seal((unsigned char *) ZSTR_VAL(ciphertext), msg,
                        (unsigned long long) msg_len, publickey) != 0) {
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }

    RETURN_STR(ciphertext);
}

PHP_FUNCTION(sodium_crypto_box_seal_open)
{
    zend_string   *msg;
    unsigned char *ciphertext;
    unsigned char *keypair;
    unsigned char *publickey;
    unsigned char *secretkey;
    size_t         ciphertext_len;
    size_t         keypair_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss",
                              &ciphertext, &ciphertext_len,
                              &keypair, &keypair_len) == FAILURE) {
        return;
    }
    if (keypair_len != crypto_box_SECRETKEYBYTES + crypto_box_PUBLICKEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "keypair size should be SODIUM_CRYPTO_BOX_KEYPAIRBYTES bytes",
                   0);
        return;
    }
    secretkey = keypair;
    publickey = keypair + crypto_box_SECRETKEYBYTES;
    if (ciphertext_len < crypto_box_SEALBYTES) {
        RETURN_FALSE;
    }
    msg = zend_string_checked_alloc((size_t) ciphertext_len - crypto_box_SEALBYTES, 0);
    if (crypto_box_seal_open((unsigned char *) ZSTR_VAL(msg), ciphertext,
                             (unsigned long long) ciphertext_len,
                             publickey, secretkey) != 0) {
        zend_string_free(msg);
        RETURN_FALSE;
    } else {
        RETURN_STR(msg);
    }
}

PHP_FUNCTION(sodium_crypto_sign_keypair)
{
    zend_string *keypair;
    size_t       keypair_len;

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }
    keypair_len = crypto_sign_SECRETKEYBYTES + crypto_sign_PUBLICKEYBYTES;
    keypair = zend_string_checked_alloc(keypair_len, 0);
    if (crypto_sign_keypair((unsigned char *) ZSTR_VAL(keypair) +
                            crypto_sign_SECRETKEYBYTES,
                            (unsigned char *) ZSTR_VAL(keypair)) != 0) {
        zend_string_free(keypair);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }

    RETURN_STR(keypair);
}

PHP_FUNCTION(sodium_crypto_sign_seed_keypair)
{
    zend_string   *keypair;
    unsigned char *seed;
    size_t         keypair_len;
    size_t         seed_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s",
                              &seed, &seed_len) == FAILURE) {
        return;
    }
    if (seed_len != crypto_sign_SEEDBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "seed should be SODIUM_CRYPTO_SIGN_SEEDBYTES bytes",
                   0);
        return;
    }
    keypair_len = crypto_sign_SECRETKEYBYTES + crypto_sign_PUBLICKEYBYTES;
    keypair = zend_string_checked_alloc(keypair_len, 0);
    if (crypto_sign_seed_keypair((unsigned char *) ZSTR_VAL(keypair) +
                                 crypto_sign_SECRETKEYBYTES,
                                 (unsigned char *) ZSTR_VAL(keypair),
                                 seed) != 0) {
        zend_string_free(keypair);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }

    RETURN_STR(keypair);
}

PHP_FUNCTION(sodium_crypto_sign_keypair_from_secretkey_and_publickey)
{
    zend_string *keypair;
    char        *publickey;
    char        *secretkey;
    size_t       keypair_len;
    size_t       publickey_len;
    size_t       secretkey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss",
                              &secretkey, &secretkey_len,
                              &publickey, &publickey_len) == FAILURE) {
        return;
    }
    if (secretkey_len != crypto_sign_SECRETKEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "secretkey should be SODIUM_CRYPTO_SIGN_SECRETKEYBYTES bytes",
                   0);
        return;
    }
    if (publickey_len != crypto_sign_PUBLICKEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "publickey should be SODIUM_CRYPTO_SIGN_PUBLICKEYBYTES bytes",
                   0);
        return;
    }
    keypair_len = crypto_sign_SECRETKEYBYTES + crypto_sign_PUBLICKEYBYTES;
    keypair = zend_string_checked_alloc(keypair_len, 0);
    memcpy(ZSTR_VAL(keypair), secretkey, crypto_sign_SECRETKEYBYTES);
    memcpy(ZSTR_VAL(keypair) + crypto_sign_SECRETKEYBYTES, publickey,
           crypto_sign_PUBLICKEYBYTES);

    RETURN_STR(keypair);
}

PHP_FUNCTION(sodium_crypto_sign_publickey_from_secretkey)
{
    zend_string *publickey;
    char        *secretkey;
    size_t       secretkey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s",
                              &secretkey, &secretkey_len) == FAILURE) {
        return;
    }
    if (secretkey_len != crypto_sign_SECRETKEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "secretkey should be SODIUM_CRYPTO_SIGN_SECRETKEYBYTES bytes",
                   0);
        return;
    }
    publickey = zend_string_checked_alloc(crypto_sign_PUBLICKEYBYTES, 0);

    if (crypto_sign_ed25519_sk_to_pk((unsigned char *) ZSTR_VAL(publickey),
                                     (const unsigned char *) secretkey) != 0) {
        zend_throw_exception(sodium_exception_ce,
                   "internal error", 0);
        return;
    }

    RETURN_STR(publickey);
}

PHP_FUNCTION(sodium_crypto_sign_secretkey)
{
    zend_string   *secretkey;
    unsigned char *keypair;
    size_t         keypair_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s",
                              &keypair, &keypair_len) == FAILURE) {
        return;
    }
    if (keypair_len !=
        crypto_sign_SECRETKEYBYTES + crypto_sign_PUBLICKEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "keypair should be SODIUM_CRYPTO_SIGN_KEYPAIRBYTES bytes",
                   0);
        return;
    }
    secretkey = zend_string_checked_alloc(crypto_sign_SECRETKEYBYTES, 0);
    memcpy(ZSTR_VAL(secretkey), keypair, crypto_sign_SECRETKEYBYTES);

    RETURN_STR(secretkey);
}

PHP_FUNCTION(sodium_crypto_sign_publickey)
{
    zend_string   *publickey;
    unsigned char *keypair;
    size_t         keypair_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s",
                              &keypair, &keypair_len) == FAILURE) {
        return;
    }
    if (keypair_len !=
        crypto_sign_SECRETKEYBYTES + crypto_sign_PUBLICKEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "keypair should be SODIUM_CRYPTO_SIGN_KEYPAIRBYTES bytes",
                   0);
        return;
    }
    publickey = zend_string_checked_alloc(crypto_sign_PUBLICKEYBYTES, 0);
    memcpy(ZSTR_VAL(publickey), keypair + crypto_sign_SECRETKEYBYTES,
           crypto_sign_PUBLICKEYBYTES);

    RETURN_STR(publickey);
}

PHP_FUNCTION(sodium_crypto_sign)
{
    zend_string        *msg_signed;
    unsigned char      *msg;
    unsigned char      *secretkey;
    unsigned long long  msg_signed_real_len;
    size_t              msg_len;
    size_t              msg_signed_len;
    size_t              secretkey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss",
                              &msg, &msg_len,
                              &secretkey, &secretkey_len) == FAILURE) {
        return;
    }
    if (secretkey_len != crypto_sign_SECRETKEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "secret key size should be SODIUM_CRYPTO_SIGN_SECRETKEYBYTES bytes",
                   0);
        return;
    }
    if (SIZE_MAX - msg_len <= crypto_sign_BYTES) {
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    msg_signed_len = msg_len + crypto_sign_BYTES;
    msg_signed = zend_string_checked_alloc((size_t) msg_signed_len, 0);
    if (crypto_sign((unsigned char *) ZSTR_VAL(msg_signed),
                    &msg_signed_real_len, msg,
                    (unsigned long long) msg_len, secretkey) != 0) {
        zend_string_free(msg_signed);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }
    if (msg_signed_real_len >= SIZE_MAX || msg_signed_real_len > msg_signed_len) {
        zend_string_free(msg_signed);
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    PHP_SODIUM_ZSTR_TRUNCATE(msg_signed, (size_t) msg_signed_real_len);

    RETURN_STR(msg_signed);
}

PHP_FUNCTION(sodium_crypto_sign_open)
{
    zend_string        *msg;
    unsigned char      *msg_signed;
    unsigned char      *publickey;
    unsigned long long  msg_real_len;
    size_t              msg_len;
    size_t              msg_signed_len;
    size_t              publickey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss",
                              &msg_signed, &msg_signed_len,
                              &publickey, &publickey_len) == FAILURE) {
        return;
    }
    if (publickey_len != crypto_sign_PUBLICKEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "public key size should be SODIUM_CRYPTO_SIGN_PUBLICKEYBYTES bytes",
                   0);
        return;
    }
    msg_len = msg_signed_len;
    if (msg_len >= SIZE_MAX) {
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    msg = zend_string_checked_alloc((size_t) msg_len, 0);
    if (crypto_sign_open((unsigned char *) ZSTR_VAL(msg), &msg_real_len,
                         msg_signed, (unsigned long long) msg_signed_len,
                         publickey) != 0) {
        zend_string_free(msg);
        RETURN_FALSE;
    }
    if (msg_real_len >= SIZE_MAX || msg_real_len > msg_signed_len) {
        zend_string_free(msg);
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    PHP_SODIUM_ZSTR_TRUNCATE(msg, (size_t) msg_real_len);

    RETURN_STR(msg);
}

PHP_FUNCTION(sodium_crypto_sign_detached)
{
    zend_string        *signature;
    unsigned char      *msg;
    unsigned char      *secretkey;
    unsigned long long  signature_real_len;
    size_t              msg_len;
    size_t              secretkey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss",
                              &msg, &msg_len,
                              &secretkey, &secretkey_len) == FAILURE) {
        return;
    }
    if (secretkey_len != crypto_sign_SECRETKEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "secret key size should be SODIUM_CRYPTO_SIGN_SECRETKEYBYTES bytes",
                   0);
        return;
    }
    signature = zend_string_checked_alloc((size_t) crypto_sign_BYTES, 0);
    memset(ZSTR_VAL(signature), 0, (size_t) crypto_sign_BYTES);
    if (crypto_sign_detached((unsigned char *) ZSTR_VAL(signature),
                             &signature_real_len, msg,
                             (unsigned long long) msg_len, secretkey) != 0) {
        zend_string_free(signature);
        zend_throw_exception(sodium_exception_ce, "signature creation failed", 0);
        return;
    }
    if (signature_real_len <= 0U || signature_real_len > crypto_sign_BYTES) {
        zend_string_free(signature);
        zend_throw_exception(sodium_exception_ce, "signature has a bogus size", 0);
        return;
    }
    PHP_SODIUM_ZSTR_TRUNCATE(signature, (size_t) signature_real_len);

    RETURN_STR(signature);
}

PHP_FUNCTION(sodium_crypto_sign_verify_detached)
{
    unsigned char *msg;
    unsigned char *publickey;
    unsigned char *signature;
    size_t         msg_len;
    size_t         publickey_len;
    size_t         signature_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sss",
                              &signature, &signature_len,
                              &msg, &msg_len,
                              &publickey, &publickey_len) == FAILURE) {
        return;
    }
    if (signature_len != crypto_sign_BYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "signature size should be SODIUM_CRYPTO_SIGN_BYTES bytes",
                   0);
        return;
    }
    if (publickey_len != crypto_sign_PUBLICKEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "public key size should be SODIUM_CRYPTO_SIGN_PUBLICKEYBYTES bytes",
                   0);
        return;
    }
    if (crypto_sign_verify_detached(signature,
                                    msg, (unsigned long long) msg_len,
                                    publickey) != 0) {
        RETURN_FALSE;
    }
    RETURN_TRUE;
}

PHP_FUNCTION(sodium_crypto_stream)
{
    zend_string   *ciphertext;
    unsigned char *key;
    unsigned char *nonce;
    zend_long      ciphertext_len;
    size_t         key_len;
    size_t         nonce_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "lss",
                              &ciphertext_len,
                              &nonce, &nonce_len,
                              &key, &key_len) == FAILURE) {
        return;
    }
    if (ciphertext_len <= 0 || ciphertext_len >= SIZE_MAX) {
        zend_throw_exception(sodium_exception_ce, "ciphertext length must be greater than 0", 0);
        return;
    }
    if (nonce_len != crypto_stream_NONCEBYTES) {
        zend_throw_exception(sodium_exception_ce, "nonce should be SODIUM_CRYPTO_STREAM_NONCEBYTES bytes", 0);
        return;
    }
    if (key_len != crypto_stream_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce, "key should be SODIUM_CRYPTO_STREAM_KEYBYTES bytes", 0);
        return;
    }
    ciphertext = zend_string_checked_alloc((size_t) ciphertext_len, 0);
    if (crypto_stream((unsigned char *) ZSTR_VAL(ciphertext),
                      (unsigned long long) ciphertext_len, nonce, key) != 0) {
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }

    RETURN_STR(ciphertext);
}

PHP_FUNCTION(sodium_crypto_stream_xor)
{
    zend_string   *ciphertext;
    unsigned char *key;
    unsigned char *msg;
    unsigned char *nonce;
    size_t         ciphertext_len;
    size_t         key_len;
    size_t         msg_len;
    size_t         nonce_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sss",
                              &msg, &msg_len,
                              &nonce, &nonce_len,
                              &key, &key_len) == FAILURE) {
        return;
    }
    if (nonce_len != crypto_stream_NONCEBYTES) {
        zend_throw_exception(sodium_exception_ce, "nonce should be SODIUM_CRYPTO_STREAM_NONCEBYTES bytes", 0);
        return;
    }
    if (key_len != crypto_stream_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce, "key should be SODIUM_CRYPTO_STREAM_KEYBYTES bytes", 0);
        return;
    }
    ciphertext_len = msg_len;
    ciphertext = zend_string_checked_alloc((size_t) ciphertext_len, 0);
    if (crypto_stream_xor((unsigned char *) ZSTR_VAL(ciphertext), msg,
                          (unsigned long long) msg_len, nonce, key) != 0) {
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }

    RETURN_STR(ciphertext);
}

#ifdef crypto_stream_xchacha20_KEYBYTES
PHP_FUNCTION(sodium_crypto_stream_xchacha20)
{
    zend_string   *ciphertext;
    unsigned char *key;
    unsigned char *nonce;
    zend_long      ciphertext_len;
    size_t         key_len;
    size_t         nonce_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "lss",
                              &ciphertext_len,
                              &nonce, &nonce_len,
                              &key, &key_len) == FAILURE) {
        return;
    }
    if (ciphertext_len <= 0 || ciphertext_len >= SIZE_MAX) {
        zend_throw_exception(sodium_exception_ce, "ciphertext length must be greater than 0", 0);
        return;
    }
    if (nonce_len != crypto_stream_xchacha20_NONCEBYTES) {
        zend_throw_exception(sodium_exception_ce, "nonce should be SODIUM_CRYPTO_STREAM_XCHACHA20_NONCEBYTES bytes", 0);
        return;
    }
    if (key_len != crypto_stream_xchacha20_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce, "key should be SODIUM_CRYPTO_STREAM_XCHACHA20_KEYBYTES bytes", 0);
        return;
    }
    ciphertext = zend_string_checked_alloc((size_t) ciphertext_len, 0);
    if (crypto_stream_xchacha20((unsigned char *) ZSTR_VAL(ciphertext),
                                (unsigned long long) ciphertext_len, nonce, key) != 0) {
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }

    RETURN_STR(ciphertext);
}

PHP_FUNCTION(sodium_crypto_stream_xchacha20_xor)
{
    zend_string   *ciphertext;
    unsigned char *key;
    unsigned char *msg;
    unsigned char *nonce;
    size_t         ciphertext_len;
    size_t         key_len;
    size_t         msg_len;
    size_t         nonce_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sss",
    &msg, &msg_len,
    &nonce, &nonce_len,
    &key, &key_len) == FAILURE) {
        return;
    }
    if (nonce_len != crypto_stream_xchacha20_NONCEBYTES) {
        zend_throw_exception(sodium_exception_ce, "nonce should be SODIUM_CRYPTO_STREAM_XCHACHA20_NONCEBYTES bytes", 0);
        return;
    }
    if (key_len != crypto_stream_xchacha20_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce, "key should be SODIUM_CRYPTO_STREAM_XCHACHA20_KEYBYTES bytes", 0);
        return;
    }
    ciphertext_len = msg_len;
    ciphertext = zend_string_checked_alloc((size_t) ciphertext_len, 0);
    if (crypto_stream_xchacha20_xor((unsigned char *) ZSTR_VAL(ciphertext), msg,
    (unsigned long long) msg_len, nonce, key) != 0) {
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }

    RETURN_STR(ciphertext);
}
#endif

#ifdef crypto_pwhash_SALTBYTES
PHP_FUNCTION(sodium_crypto_pwhash)
{
    zend_string   *hash;
    unsigned char *salt;
    char          *passwd;
    zend_long      hash_len;
    zend_long      memlimit;
    zend_long      opslimit;
    zend_long      alg;
    size_t         passwd_len;
    size_t         salt_len;
    int            ret;

    alg = (zend_long) crypto_pwhash_ALG_DEFAULT;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "lssll|l",
                              &hash_len,
                              &passwd, &passwd_len,
                              &salt, &salt_len,
                              &opslimit, &memlimit, &alg) == FAILURE ||
        hash_len <= 0 || hash_len >= 0xffffffff ||
        passwd_len >= 0xffffffff ||
        opslimit <= 0 || memlimit <= 0 || memlimit > SIZE_MAX) {
        zend_throw_exception(sodium_exception_ce, "invalid parameters", 0);
        return;
    }
    if (alg != crypto_pwhash_ALG_ARGON2I13
# ifdef crypto_pwhash_ALG_ARGON2ID13
        && alg != crypto_pwhash_ALG_ARGON2ID13
# endif
        && alg != crypto_pwhash_ALG_DEFAULT) {
        zend_throw_exception(sodium_exception_ce, "unsupported password hashing algorithm", 0);
        return;
    }
    if (passwd_len <= 0) {
        zend_error(E_WARNING, "empty password");
    }
    if (salt_len != crypto_pwhash_SALTBYTES) {
        zend_throw_exception(sodium_exception_ce, "salt should be SODIUM_CRYPTO_PWHASH_SALTBYTES bytes", 0);
        return;
    }
    if (opslimit < crypto_pwhash_OPSLIMIT_MIN) {
        zend_throw_exception(sodium_exception_ce,
                             "number of operations for the password hashing function is too low", 0);
    }
    if (memlimit < crypto_pwhash_MEMLIMIT_MIN) {
        zend_throw_exception(sodium_exception_ce,
                             "maximum memory for the password hashing function is too low", 0);
    }
    hash = zend_string_checked_alloc((size_t) hash_len, 0);
    ret = -1;
# ifdef crypto_pwhash_ALG_ARGON2ID13
    if (alg == crypto_pwhash_ALG_ARGON2ID13) {
        ret = crypto_pwhash_argon2id
            ((unsigned char *) ZSTR_VAL(hash), (unsigned long long) hash_len,
              passwd, (unsigned long long) passwd_len, salt,
              (unsigned long long) opslimit, (size_t) memlimit, (int) alg);
    }
# endif
    if (ret == -1) {
        ret = crypto_pwhash
            ((unsigned char *) ZSTR_VAL(hash), (unsigned long long) hash_len,
              passwd, (unsigned long long) passwd_len, salt,
              (unsigned long long) opslimit, (size_t) memlimit, (int) alg);
    }
    if (ret != 0) {
        zend_string_free(hash);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }

    RETURN_STR(hash);
}

PHP_FUNCTION(sodium_crypto_pwhash_str)
{
    zend_string *hash_str;
    char        *passwd;
    zend_long    memlimit;
    zend_long    opslimit;
    size_t       passwd_len;
    size_t       len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sll",
                              &passwd, &passwd_len,
                              &opslimit, &memlimit) == FAILURE ||
        opslimit <= 0 || memlimit <= 0 || memlimit > SIZE_MAX ||
        passwd_len >= 0xffffffff) {
        zend_throw_exception(sodium_exception_ce,
                   "invalid parameters", 0);
        return;
    }
    if (passwd_len <= 0) {
        zend_error(E_WARNING, "empty password");
    }
    if (opslimit < crypto_pwhash_OPSLIMIT_MIN) {
        zend_throw_exception(sodium_exception_ce,
                             "number of operations for the password hashing function is too low", 0);
    }
    if (memlimit < crypto_pwhash_MEMLIMIT_MIN) {
        zend_throw_exception(sodium_exception_ce,
                             "maximum memory for the password hashing function is too low", 0);
    }
    hash_str = zend_string_checked_alloc(crypto_pwhash_STRBYTES - 1, 0);
    if (crypto_pwhash_str
        (ZSTR_VAL(hash_str), passwd, (unsigned long long) passwd_len,
         (unsigned long long) opslimit, (size_t) memlimit) != 0) {
        zend_string_free(hash_str);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }

    len = strlen(ZSTR_VAL(hash_str));
    PHP_SODIUM_ZSTR_TRUNCATE(hash_str, len);

    RETURN_STR(hash_str);
}

#if SODIUM_LIBRARY_VERSION_MAJOR > 9 || (SODIUM_LIBRARY_VERSION_MAJOR == 9 && SODIUM_LIBRARY_VERSION_MINOR >= 6)
PHP_FUNCTION(sodium_crypto_pwhash_str_needs_rehash)
{
    char      *hash_str;
    zend_long  memlimit;
    zend_long  opslimit;
    size_t     hash_str_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sll",
                              &hash_str, &hash_str_len,
                              &opslimit, &memlimit) == FAILURE) {
        return;
    }
    if (crypto_pwhash_str_needs_rehash(hash_str, opslimit, memlimit) == 0) {
        RETURN_FALSE;
    }
    RETURN_TRUE;
}
#endif

PHP_FUNCTION(sodium_crypto_pwhash_str_verify)
{
    char      *hash_str;
    char      *passwd;
    size_t     hash_str_len;
    size_t     passwd_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss",
                              &hash_str, &hash_str_len,
                              &passwd, &passwd_len) == FAILURE) {
        zend_throw_exception(sodium_exception_ce,
                   "invalid parameters", 0);
        return;
    }
    if (passwd_len >= 0xffffffff) {
        zend_throw_exception(sodium_exception_ce,
                   "unsupported password length", 0);
        return;
    }
    if (passwd_len <= 0) {
        zend_error(E_WARNING, "empty password");
    }
    if (crypto_pwhash_str_verify
        (hash_str, passwd, (unsigned long long) passwd_len) == 0) {
        RETURN_TRUE;
    }
    RETURN_FALSE;
}
#endif

#ifdef crypto_pwhash_scryptsalsa208sha256_SALTBYTES
PHP_FUNCTION(sodium_crypto_pwhash_scryptsalsa208sha256)
{
    zend_string   *hash;
    unsigned char *salt;
    char          *passwd;
    zend_long      hash_len;
    zend_long      memlimit;
    zend_long      opslimit;
    size_t         passwd_len;
    size_t         salt_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "lssll",
                              &hash_len,
                              &passwd, &passwd_len,
                              &salt, &salt_len,
                              &opslimit, &memlimit) == FAILURE) {
        zend_throw_exception(sodium_exception_ce, "invalid parameters", 0);
        return;
    }
    if (hash_len <= 0 || hash_len >= SIZE_MAX || hash_len > 0x1fffffffe0ULL) {
        zend_throw_exception(sodium_exception_ce, "hash length must be greater than 0", 0);
        return;
    }
    if (opslimit <= 0) {
        zend_throw_exception(sodium_exception_ce, "ops limit must be greater than 0", 0);
        return;
    }
    if (memlimit <= 0 || memlimit > SIZE_MAX) {
        zend_throw_exception(sodium_exception_ce, "memory limit must be greater than 0", 0);
        return;
    }
    if (passwd_len <= 0) {
        zend_error(E_WARNING, "empty password");
    }
    if (salt_len != crypto_pwhash_scryptsalsa208sha256_SALTBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "salt should be SODIUM_CRYPTO_PWHASH_SCRYPTSALSA208SHA256_SALTBYTES bytes",
                   0);
        return;
    }
    if (opslimit < crypto_pwhash_scryptsalsa208sha256_OPSLIMIT_INTERACTIVE) {
        zend_throw_exception(sodium_exception_ce,
                             "number of operations for the scrypt function is too low", 0);
    }
    if (memlimit < crypto_pwhash_scryptsalsa208sha256_MEMLIMIT_INTERACTIVE) {
        zend_throw_exception(sodium_exception_ce,
                             "maximum memory for the scrypt function is too low", 0);
    }
    php_error(E_DEPRECATED, "crypto_pwhash_scryptsalsa208sha256 is deprecated, "
              "please use the high-level crypto_pwhash() API");
    hash = zend_string_checked_alloc((size_t) hash_len, 0);
    if (crypto_pwhash_scryptsalsa208sha256
        ((unsigned char *) ZSTR_VAL(hash), (unsigned long long) hash_len,
         passwd, (unsigned long long) passwd_len, salt,
         (unsigned long long) opslimit, (size_t) memlimit) != 0) {
        zend_string_free(hash);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }

    RETURN_STR(hash);
}

PHP_FUNCTION(sodium_crypto_pwhash_scryptsalsa208sha256_str)
{
    zend_string *hash_str;
    char        *passwd;
    zend_long    memlimit;
    zend_long    opslimit;
    size_t       passwd_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sll",
                              &passwd, &passwd_len,
                              &opslimit, &memlimit) == FAILURE) {
        zend_throw_exception(sodium_exception_ce, "invalid parameters", 0);
        return;
    }
    if (opslimit <= 0) {
        zend_throw_exception(sodium_exception_ce, "ops limit must be greater than 0", 0);
        return;
    }
    if (memlimit <= 0 || memlimit > SIZE_MAX) {
        zend_throw_exception(sodium_exception_ce, "memory limit must be greater than 0", 0);
        return;
    }
    if (passwd_len <= 0) {
        zend_error(E_WARNING, "empty password");
    }
    if (opslimit < crypto_pwhash_scryptsalsa208sha256_OPSLIMIT_INTERACTIVE) {
        zend_throw_exception(sodium_exception_ce,
                             "number of operations for the scrypt function is too low", 0);
    }
    if (memlimit < crypto_pwhash_scryptsalsa208sha256_MEMLIMIT_INTERACTIVE) {
        zend_throw_exception(sodium_exception_ce,
                             "maximum memory for the scrypt function is too low", 0);
    }
    php_error(E_DEPRECATED, "crypto_pwhash_scryptsalsa208sha256 is deprecated, "
              "please use the high-level crypto_pwhash() API");
    hash_str = zend_string_checked_alloc
        (crypto_pwhash_scryptsalsa208sha256_STRBYTES - 1, 0);
    if (crypto_pwhash_scryptsalsa208sha256_str
        (ZSTR_VAL(hash_str), passwd, (unsigned long long) passwd_len,
         (unsigned long long) opslimit, (size_t) memlimit) != 0) {
        zend_string_free(hash_str);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }

    RETURN_STR(hash_str);
}

PHP_FUNCTION(sodium_crypto_pwhash_scryptsalsa208sha256_str_verify)
{
    char      *hash_str;
    char      *passwd;
    size_t     hash_str_len;
    size_t     passwd_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss",
                              &hash_str, &hash_str_len,
                              &passwd, &passwd_len) == FAILURE) {
        zend_throw_exception(sodium_exception_ce, "invalid parameters", 0);
        return;
    }
    if (passwd_len <= 0) {
        zend_error(E_WARNING, "empty password");
    }
    if (hash_str_len != crypto_pwhash_scryptsalsa208sha256_STRBYTES - 1) {
        zend_error(E_WARNING, "wrong size for the hashed password");
        RETURN_FALSE;
    }
    php_error(E_DEPRECATED, "crypto_pwhash_scryptsalsa208sha256 is deprecated, "
              "please use the high-level crypto_pwhash() API");
    if (crypto_pwhash_scryptsalsa208sha256_str_verify
        (hash_str, passwd, (unsigned long long) passwd_len) == 0) {
        RETURN_TRUE;
    }
    RETURN_FALSE;
}
#endif

PHP_FUNCTION(sodium_crypto_aead_aes256gcm_is_available)
{
    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }
#ifdef HAVE_AESGCM
    RETURN_BOOL(crypto_aead_aes256gcm_is_available());
#else
    RETURN_FALSE;
#endif
}

#ifdef HAVE_AESGCM
PHP_FUNCTION(sodium_crypto_aead_aes256gcm_encrypt)
{
    zend_string        *ciphertext;
    unsigned char      *ad;
    unsigned char      *msg;
    unsigned char      *npub;
    unsigned char      *secretkey;
    unsigned long long  ciphertext_real_len;
    size_t              ad_len;
    size_t              ciphertext_len;
    size_t              msg_len;
    size_t              npub_len;
    size_t              secretkey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ssss",
                              &msg, &msg_len,
                              &ad, &ad_len,
                              &npub, &npub_len,
                              &secretkey, &secretkey_len) == FAILURE) {
        return;
    }
    if (npub_len != crypto_aead_aes256gcm_NPUBBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "public nonce size should be "
                   "SODIUM_CRYPTO_AEAD_AES256GCM_NPUBBYTES bytes",
                   0);
        return;
    }
    if (secretkey_len != crypto_aead_aes256gcm_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "secret key size should be "
                   "SODIUM_CRYPTO_AEAD_AES256GCM_KEYBYTES bytes",
                   0);
        return;
    }
    if (SIZE_MAX - msg_len <= crypto_aead_aes256gcm_ABYTES) {
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    if ((unsigned long long) msg_len > (16ULL * ((1ULL << 32) - 2ULL)) - crypto_aead_aes256gcm_ABYTES) {
        zend_throw_exception(sodium_exception_ce, "message too long for a single key", 0);
        return;
    }
    ciphertext_len = msg_len + crypto_aead_aes256gcm_ABYTES;
    ciphertext = zend_string_checked_alloc((size_t) ciphertext_len, 0);
    if (crypto_aead_aes256gcm_encrypt
        ((unsigned char *) ZSTR_VAL(ciphertext), &ciphertext_real_len, msg,
         (unsigned long long) msg_len,
         ad, (unsigned long long) ad_len, NULL, npub, secretkey) != 0) {
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }
    if (ciphertext_real_len <= 0U || ciphertext_real_len >= SIZE_MAX ||
        ciphertext_real_len > ciphertext_len) {
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    PHP_SODIUM_ZSTR_TRUNCATE(ciphertext, (size_t) ciphertext_real_len);

    RETURN_STR(ciphertext);
}

# ifdef HAVE_AEAD_DETACHED
PHP_FUNCTION(sodium_crypto_aead_aes256gcm_encrypt_detached)
{
    zend_string        *ciphertext;
    zend_string        *mac;
    unsigned char      *ad;
    unsigned char      *msg;
    unsigned char      *npub;
    unsigned char      *secretkey;
    unsigned long long  mac_real_len;
    size_t              ad_len;
    size_t              ciphertext_len;
    size_t              mac_len;
    size_t              msg_len;
    size_t              npub_len;
    size_t              secretkey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ssss",
                              &msg, &msg_len,
                              &ad, &ad_len,
                              &npub, &npub_len,
                              &secretkey, &secretkey_len) == FAILURE) {
        return;
    }
    if (npub_len != crypto_aead_aes256gcm_NPUBBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "public nonce size should be "
                   "SODIUM_CRYPTO_AEAD_AES256GCM_NPUBBYTES bytes",
                   0);
        return;
    }
    if (secretkey_len != crypto_aead_aes256gcm_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "secret key size should be "
                   "SODIUM_CRYPTO_AEAD_AES256GCM_KEYBYTES bytes",
                   0);
        return;
    }
    if ((unsigned long long) msg_len > (16ULL * ((1ULL << 32) - 2ULL))) {
        zend_throw_exception(sodium_exception_ce, "message too long for a single key", 0);
        return;
    }
    ciphertext_len = msg_len;
    ciphertext = zend_string_checked_alloc((size_t) ciphertext_len, 0);

    mac_len = crypto_aead_aes256gcm_ABYTES;
    mac = zend_string_checked_alloc((size_t) mac_len, 0);

    if (crypto_aead_aes256gcm_encrypt_detached((unsigned char *) ZSTR_VAL(ciphertext),
                                               (unsigned char *) ZSTR_VAL(mac),
                                               &mac_real_len,
                                               msg, (unsigned long long) msg_len,
                                               ad, (unsigned long long) ad_len,
                                               NULL, npub, secretkey) != 0) {
        zend_string_free(mac);
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }
    if (mac_real_len <= 0U || mac_real_len >= SIZE_MAX || mac_real_len > mac_len) {
        zend_string_free(mac);
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    PHP_SODIUM_ZSTR_TRUNCATE(mac, (size_t) mac_real_len);

    array_init(return_value);
    add_next_index_str(return_value, ciphertext);
    add_next_index_str(return_value, mac);
}
# endif

PHP_FUNCTION(sodium_crypto_aead_aes256gcm_decrypt)
{
    zend_string        *msg;
    unsigned char      *ad;
    unsigned char      *ciphertext;
    unsigned char      *npub;
    unsigned char      *secretkey;
    unsigned long long  msg_real_len;
    size_t              ad_len;
    size_t              ciphertext_len;
    size_t              msg_len;
    size_t              npub_len;
    size_t              secretkey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ssss",
                              &ciphertext, &ciphertext_len,
                              &ad, &ad_len,
                              &npub, &npub_len,
                              &secretkey, &secretkey_len) == FAILURE) {
        return;
    }
    if (npub_len != crypto_aead_aes256gcm_NPUBBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "public nonce size should be "
                   "SODIUM_CRYPTO_AEAD_AES256GCM_NPUBBYTES bytes",
                   0);
        return;
    }
    if (secretkey_len != crypto_aead_aes256gcm_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "secret key size should be "
                   "SODIUM_CRYPTO_AEAD_AES256GCM_KEYBYTES bytes",
                   0);
        return;
    }
    if (ciphertext_len < crypto_aead_aes256gcm_ABYTES) {
        RETURN_FALSE;
    }
    if (ciphertext_len - crypto_aead_aes256gcm_ABYTES > 16ULL * ((1ULL << 32) - 2ULL)) {
        zend_throw_exception(sodium_exception_ce, "message too long for a single key", 0);
        return;
    }
    msg_len = ciphertext_len;
    if (msg_len >= SIZE_MAX) {
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    msg = zend_string_checked_alloc((size_t) msg_len, 0);
    if (crypto_aead_aes256gcm_decrypt
        ((unsigned char *) ZSTR_VAL(msg), &msg_real_len, NULL,
         ciphertext, (unsigned long long) ciphertext_len,
         ad, (unsigned long long) ad_len, npub, secretkey) != 0) {
        zend_string_free(msg);
        RETURN_FALSE;
    }
    if (msg_real_len >= SIZE_MAX || msg_real_len > msg_len) {
        zend_string_free(msg);
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    PHP_SODIUM_ZSTR_TRUNCATE(msg, (size_t) msg_real_len);

    RETURN_STR(msg);
}

# ifdef HAVE_AEAD_DETACHED
PHP_FUNCTION(sodium_crypto_aead_aes256gcm_decrypt_detached)
{
    zend_string        *msg;
    unsigned char      *ad;
    unsigned char      *ciphertext;
    unsigned char      *mac;
    unsigned char      *npub;
    unsigned char      *secretkey;
    size_t              ad_len;
    size_t              ciphertext_len;
    size_t              mac_len;
    size_t              msg_len;
    size_t              npub_len;
    size_t              secretkey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sssss",
                              &ciphertext, &ciphertext_len,
                              &mac, &mac_len,
                              &ad, &ad_len,
                              &npub, &npub_len,
                              &secretkey, &secretkey_len) == FAILURE) {
        return;
    }
    if (npub_len != crypto_aead_aes256gcm_NPUBBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "public nonce size should be "
                   "SODIUM_CRYPTO_AEAD_AES256GCM_NPUBBYTES bytes",
                   0);
        return;
    }
    if (secretkey_len != crypto_aead_aes256gcm_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "secret key size should be "
                   "SODIUM_CRYPTO_AEAD_AES256GCM_KEYBYTES bytes",
                   0);
        return;
    }
    if (mac_len != crypto_aead_aes256gcm_ABYTES) {
        RETURN_FALSE;
    }
    if (ciphertext_len > 16ULL * ((1ULL << 32) - 2ULL)) {
        zend_throw_exception(sodium_exception_ce, "message too long for a single key", 0);
        return;
    }
    msg_len = ciphertext_len;
    msg = zend_string_checked_alloc((size_t) msg_len, 0);
    if (crypto_aead_aes256gcm_decrypt_detached
        ((unsigned char *) ZSTR_VAL(msg),
         NULL,
         ciphertext, (unsigned long long) ciphertext_len,
         mac,
         ad, (unsigned long long) ad_len,
         npub, secretkey) != 0) {
        zend_string_free(msg);
        RETURN_FALSE;
    }
    RETURN_STR(msg);
}
# endif
#endif

PHP_FUNCTION(sodium_crypto_aead_chacha20poly1305_encrypt)
{
    zend_string        *ciphertext;
    unsigned char      *ad;
    unsigned char      *msg;
    unsigned char      *npub;
    unsigned char      *secretkey;
    unsigned long long  ciphertext_real_len;
    size_t              ad_len;
    size_t              ciphertext_len;
    size_t              msg_len;
    size_t              npub_len;
    size_t              secretkey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ssss",
                              &msg, &msg_len,
                              &ad, &ad_len,
                              &npub, &npub_len,
                              &secretkey, &secretkey_len) == FAILURE) {
        return;
    }
    if (npub_len != crypto_aead_chacha20poly1305_NPUBBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "public nonce size should be "
                   "SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_NPUBBYTES bytes",
                   0);
        return;
    }
    if (secretkey_len != crypto_aead_chacha20poly1305_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "secret key size should be "
                   "SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_KEYBYTES bytes",
                   0);
        return;
    }
    if (SIZE_MAX - msg_len <= crypto_aead_chacha20poly1305_ABYTES) {
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    ciphertext_len = msg_len + crypto_aead_chacha20poly1305_ABYTES;
    ciphertext = zend_string_checked_alloc((size_t) ciphertext_len, 0);
    if (crypto_aead_chacha20poly1305_encrypt
        ((unsigned char *) ZSTR_VAL(ciphertext), &ciphertext_real_len, msg,
         (unsigned long long) msg_len,
         ad, (unsigned long long) ad_len, NULL, npub, secretkey) != 0) {
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }
    if (ciphertext_real_len <= 0U || ciphertext_real_len >= SIZE_MAX ||
        ciphertext_real_len > ciphertext_len) {
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    PHP_SODIUM_ZSTR_TRUNCATE(ciphertext, (size_t) ciphertext_real_len);

    RETURN_STR(ciphertext);
}

#ifdef HAVE_AEAD_DETACHED
PHP_FUNCTION(sodium_crypto_aead_chacha20poly1305_encrypt_detached)
{
    zend_string        *ciphertext;
    zend_string        *mac;
    unsigned char      *ad;
    unsigned char      *msg;
    unsigned char      *npub;
    unsigned char      *secretkey;
    unsigned long long  mac_real_len;
    size_t              ad_len;
    size_t              ciphertext_len;
    size_t              mac_len;
    size_t              msg_len;
    size_t              npub_len;
    size_t              secretkey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ssss",
                              &msg, &msg_len,
                              &ad, &ad_len,
                              &npub, &npub_len,
                              &secretkey, &secretkey_len) == FAILURE) {
        return;
    }
    if (npub_len != crypto_aead_chacha20poly1305_NPUBBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "public nonce size should be "
                   "SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_NPUBBYTES bytes",
                   0);
        return;
    }
    if (secretkey_len != crypto_aead_chacha20poly1305_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "secret key size should be "
                   "SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_KEYBYTES bytes",
                   0);
        return;
    }
    ciphertext_len = msg_len;
    ciphertext = zend_string_checked_alloc((size_t) ciphertext_len, 0);
    mac_len = crypto_aead_chacha20poly1305_ABYTES;
    mac = zend_string_checked_alloc((size_t) mac_len, 0);
    if (crypto_aead_chacha20poly1305_encrypt_detached((unsigned char *) ZSTR_VAL(ciphertext),
                                                      (unsigned char *) ZSTR_VAL(mac),
                                                      &mac_real_len, msg,
                                                      (unsigned long long) msg_len,
                                                      ad, (unsigned long long) ad_len,
                                                      NULL, npub, secretkey) != 0) {
        zend_string_free(mac);
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }
    if (mac_real_len <= 0U || mac_real_len >= SIZE_MAX || mac_real_len > mac_len) {
        zend_string_free(mac);
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    PHP_SODIUM_ZSTR_TRUNCATE(mac, (size_t) mac_real_len);

    array_init(return_value);
    add_next_index_str(return_value, ciphertext);
    add_next_index_str(return_value, mac);
}
#endif

PHP_FUNCTION(sodium_crypto_aead_chacha20poly1305_decrypt)
{
    zend_string        *msg;
    unsigned char      *ad;
    unsigned char      *ciphertext;
    unsigned char      *npub;
    unsigned char      *secretkey;
    unsigned long long  msg_real_len;
    size_t              ad_len;
    size_t              ciphertext_len;
    size_t              msg_len;
    size_t              npub_len;
    size_t              secretkey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ssss",
                              &ciphertext, &ciphertext_len,
                              &ad, &ad_len,
                              &npub, &npub_len,
                              &secretkey, &secretkey_len) == FAILURE) {
        return;
    }
    if (npub_len != crypto_aead_chacha20poly1305_NPUBBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "public nonce size should be "
                   "SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_NPUBBYTES bytes",
                   0);
        return;
    }
    if (secretkey_len != crypto_aead_chacha20poly1305_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "secret key size should be "
                   "SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_KEYBYTES bytes",
                   0);
        return;
    }
    if (ciphertext_len < crypto_aead_chacha20poly1305_ABYTES) {
        RETURN_FALSE;
    }
    msg_len = ciphertext_len;
    if (msg_len >= SIZE_MAX) {
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    msg = zend_string_checked_alloc((size_t) msg_len, 0);
    if (crypto_aead_chacha20poly1305_decrypt
        ((unsigned char *) ZSTR_VAL(msg), &msg_real_len, NULL,
         ciphertext, (unsigned long long) ciphertext_len,
         ad, (unsigned long long) ad_len, npub, secretkey) != 0) {
        zend_string_free(msg);
        RETURN_FALSE;
    }
    if (msg_real_len >= SIZE_MAX || msg_real_len > msg_len) {
        zend_string_free(msg);
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    PHP_SODIUM_ZSTR_TRUNCATE(msg, (size_t) msg_real_len);

    RETURN_STR(msg);
}

#ifdef HAVE_AEAD_DETACHED
PHP_FUNCTION(sodium_crypto_aead_chacha20poly1305_decrypt_detached)
{
    zend_string        *msg;
    unsigned char      *ad;
    unsigned char      *ciphertext;
    unsigned char      *mac;
    unsigned char      *npub;
    unsigned char      *secretkey;
    size_t              ad_len;
    size_t              ciphertext_len;
    size_t              mac_len;
    size_t              msg_len;
    size_t              npub_len;
    size_t              secretkey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sssss",
                              &ciphertext, &ciphertext_len,
                              &mac, &mac_len,
                              &ad, &ad_len,
                              &npub, &npub_len,
                              &secretkey, &secretkey_len) == FAILURE) {
        return;
    }
    if (npub_len != crypto_aead_chacha20poly1305_NPUBBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "public nonce size should be "
                   "SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_NPUBBYTES bytes",
                   0);
        return;
    }
    if (secretkey_len != crypto_aead_chacha20poly1305_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "secret key size should be "
                   "SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_KEYBYTES bytes",
                   0);
        return;
    }
    if (mac_len != crypto_aead_chacha20poly1305_ABYTES) {
        RETURN_FALSE;
    }
    msg_len = ciphertext_len;
    msg = zend_string_checked_alloc((size_t) msg_len, 0);
    if (crypto_aead_chacha20poly1305_decrypt_detached
        ((unsigned char *) ZSTR_VAL(msg),
         NULL,
         ciphertext, (unsigned long long) ciphertext_len,
         mac,
         ad, (unsigned long long) ad_len,
         npub, secretkey) != 0) {
        zend_string_free(msg);
        RETURN_FALSE;
    }

    RETURN_STR(msg);
}
#endif

PHP_FUNCTION(sodium_crypto_aead_chacha20poly1305_ietf_encrypt)
{
    zend_string        *ciphertext;
    unsigned char      *ad;
    unsigned char      *msg;
    unsigned char      *npub;
    unsigned char      *secretkey;
    unsigned long long  ciphertext_real_len;
    size_t              ad_len;
    size_t              ciphertext_len;
    size_t              msg_len;
    size_t              npub_len;
    size_t              secretkey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ssss",
                              &msg, &msg_len,
                              &ad, &ad_len,
                              &npub, &npub_len,
                              &secretkey, &secretkey_len) == FAILURE) {
        return;
    }
    if (npub_len != crypto_aead_chacha20poly1305_IETF_NPUBBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "public nonce size should be "
                   "SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_IETF_NPUBBYTES bytes",
                   0);
        return;
    }
    if (secretkey_len != crypto_aead_chacha20poly1305_IETF_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "secret key size should be "
                   "SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_IETF_KEYBYTES bytes",
                   0);
        return;
    }
    if (SIZE_MAX - msg_len <= crypto_aead_chacha20poly1305_IETF_ABYTES) {
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    if ((unsigned long long) msg_len > 64ULL * (1ULL << 32) - 64ULL) {
        zend_throw_exception(sodium_exception_ce, "message too long for a single key", 0);
        return;
    }
    ciphertext_len = msg_len + crypto_aead_chacha20poly1305_IETF_ABYTES;
    ciphertext = zend_string_checked_alloc((size_t) ciphertext_len, 0);
    if (crypto_aead_chacha20poly1305_ietf_encrypt
        ((unsigned char *) ZSTR_VAL(ciphertext), &ciphertext_real_len, msg,
         (unsigned long long) msg_len,
         ad, (unsigned long long) ad_len, NULL, npub, secretkey) != 0) {
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }
    if (ciphertext_real_len <= 0U || ciphertext_real_len >= SIZE_MAX ||
        ciphertext_real_len > ciphertext_len) {
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    PHP_SODIUM_ZSTR_TRUNCATE(ciphertext, (size_t) ciphertext_real_len);

    RETURN_STR(ciphertext);
}

#ifdef HAVE_AEAD_DETACHED
PHP_FUNCTION(sodium_crypto_aead_chacha20poly1305_ietf_encrypt_detached)
{
    zend_string        *ciphertext;
    zend_string        *mac;
    unsigned char      *ad;
    unsigned char      *msg;
    unsigned char      *npub;
    unsigned char      *secretkey;
    unsigned long long  mac_real_len;
    size_t              ad_len;
    size_t              ciphertext_len;
    size_t              mac_len;
    size_t              msg_len;
    size_t              npub_len;
    size_t              secretkey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ssss",
                              &msg, &msg_len,
                              &ad, &ad_len,
                              &npub, &npub_len,
                              &secretkey, &secretkey_len) == FAILURE) {
        return;
    }
    if (npub_len != crypto_aead_chacha20poly1305_IETF_NPUBBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "public nonce size should be "
                   "SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_IETF_NPUBBYTES bytes",
                   0);
        return;
    }
    if (secretkey_len != crypto_aead_chacha20poly1305_IETF_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "secret key size should be "
                   "SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_IETF_KEYBYTES bytes",
                   0);
        return;
    }
    ciphertext_len = msg_len;
    ciphertext = zend_string_checked_alloc((size_t) ciphertext_len, 0);
    mac_len = crypto_aead_chacha20poly1305_IETF_ABYTES;
    mac = zend_string_checked_alloc((size_t) mac_len, 0);
    if (crypto_aead_chacha20poly1305_ietf_encrypt_detached((unsigned char *) ZSTR_VAL(ciphertext),
                                                           (unsigned char *) ZSTR_VAL(mac),
                                                           &mac_real_len, msg,
                                                           (unsigned long long) msg_len,
                                                           ad, (unsigned long long) ad_len,
                                                           NULL, npub, secretkey) != 0) {
        zend_string_free(mac);
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }
    if (mac_real_len <= 0U || mac_real_len >= SIZE_MAX || mac_real_len > mac_len) {
        zend_string_free(mac);
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    PHP_SODIUM_ZSTR_TRUNCATE(mac, (size_t) mac_real_len);

    array_init(return_value);
    add_next_index_str(return_value, ciphertext);
    add_next_index_str(return_value, mac);
}
#endif

PHP_FUNCTION(sodium_crypto_aead_chacha20poly1305_ietf_decrypt)
{
    zend_string        *msg;
    unsigned char      *ad;
    unsigned char      *ciphertext;
    unsigned char      *npub;
    unsigned char      *secretkey;
    unsigned long long  msg_real_len;
    size_t              ad_len;
    size_t              ciphertext_len;
    size_t              msg_len;
    size_t              npub_len;
    size_t              secretkey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ssss",
                              &ciphertext, &ciphertext_len,
                              &ad, &ad_len,
                              &npub, &npub_len,
                              &secretkey, &secretkey_len) == FAILURE) {
        return;
    }
    if (npub_len != crypto_aead_chacha20poly1305_IETF_NPUBBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "public nonce size should be "
                   "SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_IETF_NPUBBYTES bytes",
                   0);
        return;
    }
    if (secretkey_len != crypto_aead_chacha20poly1305_IETF_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "secret key size should be "
                   "SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_IETF_KEYBYTES bytes",
                   0);
        return;
    }
    msg_len = ciphertext_len;
    if (msg_len >= SIZE_MAX) {
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    if (ciphertext_len < crypto_aead_chacha20poly1305_IETF_ABYTES) {
        RETURN_FALSE;
    }
    if ((unsigned long long) ciphertext_len -
        crypto_aead_chacha20poly1305_IETF_ABYTES > 64ULL * (1ULL << 32) - 64ULL) {
        zend_throw_exception(sodium_exception_ce, "message too long for a single key", 0);
        return;
    }
    msg = zend_string_checked_alloc((size_t) msg_len, 0);
    if (crypto_aead_chacha20poly1305_ietf_decrypt
        ((unsigned char *) ZSTR_VAL(msg), &msg_real_len, NULL,
         ciphertext, (unsigned long long) ciphertext_len,
         ad, (unsigned long long) ad_len, npub, secretkey) != 0) {
        zend_string_free(msg);
        RETURN_FALSE;
    }
    if (msg_real_len >= SIZE_MAX || msg_real_len > msg_len) {
        zend_string_free(msg);
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    PHP_SODIUM_ZSTR_TRUNCATE(msg, (size_t) msg_real_len);

    RETURN_STR(msg);
}

#ifdef HAVE_AEAD_DETACHED
PHP_FUNCTION(sodium_crypto_aead_chacha20poly1305_ietf_decrypt_detached)
{
    zend_string        *msg;
    unsigned char      *ad;
    unsigned char      *ciphertext;
    unsigned char      *mac;
    unsigned char      *npub;
    unsigned char      *secretkey;
    size_t              ad_len;
    size_t              ciphertext_len;
    size_t              mac_len;
    size_t              msg_len;
    size_t              npub_len;
    size_t              secretkey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sssss",
                              &ciphertext, &ciphertext_len,
                              &mac, &mac_len,
                              &ad, &ad_len,
                              &npub, &npub_len,
                              &secretkey, &secretkey_len) == FAILURE) {
        return;
    }
    if (npub_len != crypto_aead_chacha20poly1305_IETF_NPUBBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "public nonce size should be "
                   "SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_IETF_NPUBBYTES bytes",
                   0);
        return;
    }
    if (secretkey_len != crypto_aead_chacha20poly1305_IETF_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "secret key size should be "
                   "SODIUM_CRYPTO_AEAD_CHACHA20POLY1305_IETF_KEYBYTES bytes",
                   0);
        return;
    }
    if (mac_len != crypto_aead_chacha20poly1305_IETF_ABYTES) {
        RETURN_FALSE;
    }
    msg_len = ciphertext_len;
    msg = zend_string_checked_alloc((size_t) msg_len, 0);
    if (crypto_aead_chacha20poly1305_ietf_decrypt_detached
        ((unsigned char *) ZSTR_VAL(msg),
         NULL,
         ciphertext, (unsigned long long) ciphertext_len,
         mac,
         ad, (unsigned long long) ad_len,
         npub, secretkey) != 0) {
        zend_string_free(msg);
        RETURN_FALSE;
    }
    RETURN_STR(msg);
}
#endif

#ifdef crypto_aead_xchacha20poly1305_IETF_NPUBBYTES
PHP_FUNCTION(sodium_crypto_aead_xchacha20poly1305_ietf_encrypt)
{
    zend_string        *ciphertext;
    unsigned char      *ad;
    unsigned char      *msg;
    unsigned char      *npub;
    unsigned char      *secretkey;
    unsigned long long  ciphertext_real_len;
    size_t              ad_len;
    size_t              ciphertext_len;
    size_t              msg_len;
    size_t              npub_len;
    size_t              secretkey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ssss",
                              &msg, &msg_len,
                              &ad, &ad_len,
                              &npub, &npub_len,
                              &secretkey, &secretkey_len) == FAILURE) {
        return;
    }
    if (npub_len != crypto_aead_xchacha20poly1305_IETF_NPUBBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "public nonce size should be "
                   "SODIUM_CRYPTO_AEAD_XCHACHA20POLY1305_IETF_NPUBBYTES bytes",
                   0);
        return;
    }
    if (secretkey_len != crypto_aead_xchacha20poly1305_IETF_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "secret key size should be "
                   "SODIUM_CRYPTO_AEAD_XCHACHA20POLY1305_IETF_KEYBYTES bytes",
                   0);
        return;
    }
    if (SIZE_MAX - msg_len <= crypto_aead_xchacha20poly1305_IETF_ABYTES) {
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    ciphertext_len = msg_len + crypto_aead_xchacha20poly1305_IETF_ABYTES;
    ciphertext = zend_string_checked_alloc((size_t) ciphertext_len, 0);
    if (crypto_aead_xchacha20poly1305_ietf_encrypt
        ((unsigned char *) ZSTR_VAL(ciphertext), &ciphertext_real_len, msg,
         (unsigned long long) msg_len,
         ad, (unsigned long long) ad_len, NULL, npub, secretkey) != 0) {
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }
    if (ciphertext_real_len <= 0U || ciphertext_real_len >= SIZE_MAX ||
        ciphertext_real_len > ciphertext_len) {
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    PHP_SODIUM_ZSTR_TRUNCATE(ciphertext, (size_t) ciphertext_real_len);

    RETURN_STR(ciphertext);
}

# ifdef HAVE_AEAD_DETACHED
PHP_FUNCTION(sodium_crypto_aead_xchacha20poly1305_ietf_encrypt_detached)
{
    zend_string        *ciphertext;
    zend_string        *mac;
    unsigned char      *ad;
    unsigned char      *msg;
    unsigned char      *npub;
    unsigned char      *secretkey;
    unsigned long long  mac_real_len;
    size_t              ad_len;
    size_t              ciphertext_len;
    size_t              mac_len;
    size_t              msg_len;
    size_t              npub_len;
    size_t              secretkey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ssss",
                              &msg, &msg_len,
                              &ad, &ad_len,
                              &npub, &npub_len,
                              &secretkey, &secretkey_len) == FAILURE) {
        return;
    }
    if (npub_len != crypto_aead_xchacha20poly1305_IETF_NPUBBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "public nonce size should be "
                   "SODIUM_CRYPTO_AEAD_XCHACHA20POLY1305_IETF_NPUBBYTES bytes",
                   0);
        return;
    }
    if (secretkey_len != crypto_aead_xchacha20poly1305_IETF_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "secret key size should be "
                   "SODIUM_CRYPTO_AEAD_XCHACHA20POLY1305_IETF_KEYBYTES bytes",
                   0);
        return;
    }
    ciphertext_len = msg_len;
    ciphertext = zend_string_checked_alloc((size_t) ciphertext_len, 0);
    mac_len = crypto_aead_xchacha20poly1305_IETF_ABYTES;
    mac = zend_string_checked_alloc((size_t) mac_len, 0);
    if (crypto_aead_xchacha20poly1305_ietf_encrypt_detached((unsigned char *) ZSTR_VAL(ciphertext),
                                                            (unsigned char *) ZSTR_VAL(mac),
                                                            &mac_real_len, msg,
                                                            (unsigned long long) msg_len,
                                                            ad, (unsigned long long) ad_len,
                                                            NULL, npub, secretkey) != 0) {
        zend_string_free(mac);
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }
    if (mac_real_len <= 0U || mac_real_len >= SIZE_MAX || mac_real_len > mac_len) {
        zend_string_free(mac);
        zend_string_free(ciphertext);
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    PHP_SODIUM_ZSTR_TRUNCATE(mac, (size_t) mac_real_len);

    array_init(return_value);
    add_next_index_str(return_value, ciphertext);
    add_next_index_str(return_value, mac);
}
# endif

PHP_FUNCTION(sodium_crypto_aead_xchacha20poly1305_ietf_decrypt)
{
    zend_string        *msg;
    unsigned char      *ad;
    unsigned char      *ciphertext;
    unsigned char      *npub;
    unsigned char      *secretkey;
    unsigned long long  msg_real_len;
    size_t              ad_len;
    size_t              ciphertext_len;
    size_t              msg_len;
    size_t              npub_len;
    size_t              secretkey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ssss",
                              &ciphertext, &ciphertext_len,
                              &ad, &ad_len,
                              &npub, &npub_len,
                              &secretkey, &secretkey_len) == FAILURE) {
        return;
    }
    if (npub_len != crypto_aead_xchacha20poly1305_IETF_NPUBBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "public nonce size should be "
                   "SODIUM_CRYPTO_AEAD_XCHACHA20POLY1305_IETF_NPUBBYTES bytes",
                   0);
        return;
    }
    if (secretkey_len != crypto_aead_xchacha20poly1305_IETF_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "secret key size should be "
                   "SODIUM_CRYPTO_AEAD_XCHACHA20POLY1305_IETF_KEYBYTES bytes",
                   0);
        return;
    }
    if (ciphertext_len < crypto_aead_xchacha20poly1305_IETF_ABYTES) {
        RETURN_FALSE;
    }
    msg_len = ciphertext_len;
    if (msg_len - crypto_aead_xchacha20poly1305_IETF_ABYTES >= SIZE_MAX) {
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    if ((unsigned long long) ciphertext_len -
        crypto_aead_xchacha20poly1305_IETF_ABYTES > 64ULL * (1ULL << 32) - 64ULL) {
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    msg = zend_string_checked_alloc((size_t) msg_len, 0);
    if (crypto_aead_xchacha20poly1305_ietf_decrypt
        ((unsigned char *) ZSTR_VAL(msg), &msg_real_len, NULL,
         ciphertext, (unsigned long long) ciphertext_len,
         ad, (unsigned long long) ad_len, npub, secretkey) != 0) {
        zend_string_free(msg);
        RETURN_FALSE;
    }
    if (msg_real_len >= SIZE_MAX || msg_real_len > msg_len) {
        zend_string_free(msg);
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    PHP_SODIUM_ZSTR_TRUNCATE(msg, (size_t) msg_real_len);

    RETURN_STR(msg);
}

# ifdef HAVE_AEAD_DETACHED
PHP_FUNCTION(sodium_crypto_aead_xchacha20poly1305_ietf_decrypt_detached)
{
    zend_string        *msg;
    unsigned char      *ad;
    unsigned char      *ciphertext;
    unsigned char      *mac;
    unsigned char      *npub;
    unsigned char      *secretkey;
    size_t              ad_len;
    size_t              ciphertext_len;
    size_t              mac_len;
    size_t              msg_len;
    size_t              npub_len;
    size_t              secretkey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sssss",
                              &ciphertext, &ciphertext_len,
                              &mac, &mac_len,
                              &ad, &ad_len,
                              &npub, &npub_len,
                              &secretkey, &secretkey_len) == FAILURE) {
        return;
    }
    if (npub_len != crypto_aead_xchacha20poly1305_IETF_NPUBBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "public nonce size should be "
                   "SODIUM_CRYPTO_AEAD_XCHACHA20POLY1305_IETF_NPUBBYTES bytes",
                   0);
        return;
    }
    if (secretkey_len != crypto_aead_xchacha20poly1305_IETF_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "secret key size should be "
                   "SODIUM_CRYPTO_AEAD_XCHACHA20POLY1305_IETF_KEYBYTES bytes",
                   0);
        return;
    }
    if (mac_len != crypto_aead_xchacha20poly1305_IETF_ABYTES) {
        RETURN_FALSE;
    }
    msg_len = ciphertext_len;
    msg = zend_string_checked_alloc((size_t) msg_len, 0);
    if (crypto_aead_xchacha20poly1305_ietf_decrypt_detached
        ((unsigned char *) ZSTR_VAL(msg),
         NULL,
         ciphertext, (unsigned long long) ciphertext_len,
         mac,
         ad, (unsigned long long) ad_len,
         npub, secretkey) != 0) {
        zend_string_free(msg);
        RETURN_FALSE;
    }
    RETURN_STR(msg);
}
# endif
#endif

PHP_FUNCTION(sodium_bin2hex)
{
    zend_string   *hex;
    unsigned char *bin;
    size_t         bin_len;
    size_t         hex_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s",
                              &bin, &bin_len) == FAILURE) {
        return;
    }
    if (bin_len >= SIZE_MAX / 2U) {
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    hex_len = bin_len * 2U;
    hex = zend_string_checked_alloc((size_t) hex_len, 0);
    sodium_bin2hex(ZSTR_VAL(hex), hex_len + 1U, bin, bin_len);

    RETURN_STR(hex);
}

PHP_FUNCTION(sodium_hex2bin)
{
    zend_string   *bin;
    const char    *end;
    char          *hex;
    char          *ignore = NULL;
    size_t         bin_real_len;
    size_t         bin_len;
    size_t         hex_len;
    size_t         ignore_len = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|s",
                              &hex, &hex_len,
                              &ignore, &ignore_len) == FAILURE) {
        return;
    }
    bin_len = hex_len / 2;
    bin = zend_string_checked_alloc(bin_len, 0);
    if (sodium_hex2bin((unsigned char *) ZSTR_VAL(bin), bin_len, hex, hex_len,
                       ignore, &bin_real_len, &end) != 0 ||
        end != hex + hex_len) {
        zend_string_free(bin);
        zend_throw_exception(sodium_exception_ce, "invalid hex string", 0);
        return;
    }
    if (bin_real_len >= SIZE_MAX || bin_real_len > bin_len) {
        zend_string_free(bin);
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    PHP_SODIUM_ZSTR_TRUNCATE(bin, (size_t) bin_real_len);

    RETURN_STR(bin);
}

#ifdef sodium_base64_VARIANT_ORIGINAL
PHP_FUNCTION(sodium_bin2base64)
{
    zend_string   *b64;
    unsigned char *bin;
    zend_long      variant;
    size_t         bin_len;
    size_t         b64_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sl",
                              &bin, &bin_len, &variant) == FAILURE) {
        return;
    }
    if ((((unsigned int) variant) & ~ 0x6U) != 0x1U) {
        zend_throw_exception(sodium_exception_ce,
                             "invalid base64 variant identifier", 0);
        return;
    }
    if (bin_len >= SIZE_MAX / 4U * 3U - 3U - 1U) {
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    b64_len = sodium_base64_ENCODED_LEN(bin_len, variant);
    b64 = zend_string_checked_alloc((size_t) b64_len - 1U, 0);
    sodium_bin2base64(ZSTR_VAL(b64), b64_len, bin, bin_len, (int) variant);

    RETURN_STR(b64);
}

PHP_FUNCTION(sodium_base642bin)
{
    zend_string   *bin;
    char          *b64;
    const char    *end;
    char          *ignore = NULL;
    zend_long      variant;
    size_t         bin_real_len;
    size_t         bin_len;
    size_t         b64_len;
    size_t         ignore_len = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sl|s",
                              &b64, &b64_len, &variant,
                              &ignore, &ignore_len) == FAILURE) {
        return;
    }
    if ((((unsigned int) variant) & ~ 0x6U) != 0x1U) {
        zend_throw_exception(sodium_exception_ce,
                             "invalid base64 variant identifier", 0);
        return;
    }
    bin_len = b64_len / 4U * 3U + 2U;
    bin = zend_string_checked_alloc(bin_len, 0);
    if (sodium_base642bin((unsigned char *) ZSTR_VAL(bin), bin_len,
                          b64, b64_len,
                          ignore, &bin_real_len, &end, (int) variant) != 0 ||
        end != b64 + b64_len) {
        zend_string_free(bin);
        zend_throw_exception(sodium_exception_ce, "invalid base64 string", 0);
        return;
    }
    if (bin_real_len >= SIZE_MAX || bin_real_len > bin_len) {
        zend_string_free(bin);
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    PHP_SODIUM_ZSTR_TRUNCATE(bin, (size_t) bin_real_len);

    RETURN_STR(bin);
}
#endif

PHP_FUNCTION(sodium_crypto_scalarmult)
{
    zend_string   *q;
    unsigned char *n;
    unsigned char *p;
    size_t         n_len;
    size_t         p_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss",
                              &n, &n_len, &p, &p_len) == FAILURE) {
        return;
    }
    if (n_len != crypto_scalarmult_SCALARBYTES ||
        p_len != crypto_scalarmult_BYTES) {
        zend_throw_exception(sodium_exception_ce, "scalar and point must be "
                   "SODIUM_CRYPTO_SCALARMULT_SCALARBYTES bytes",
                   0);
        return;
    }
    q = zend_string_checked_alloc(crypto_scalarmult_BYTES, 0);
    if (crypto_scalarmult((unsigned char *) ZSTR_VAL(q), n, p) != 0) {
        zend_string_free(q);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }

    RETURN_STR(q);
}

PHP_FUNCTION(sodium_crypto_kx_seed_keypair)
{
    unsigned char *sk;
    unsigned char *pk;
    unsigned char *seed;
    size_t         seed_len;
    zend_string   *keypair;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s",
                              &seed, &seed_len) == FAILURE) {
        return;
    }
    if (seed_len != crypto_kx_SEEDBYTES) {
        zend_throw_exception(sodium_exception_ce, "seed must be SODIUM_CRYPTO_KX_SEEDBYTES bytes", 0);
        return;
    }
    (void) sizeof(int[crypto_scalarmult_SCALARBYTES == crypto_kx_PUBLICKEYBYTES ? 1 : -1]);
    (void) sizeof(int[crypto_scalarmult_SCALARBYTES == crypto_kx_SECRETKEYBYTES ? 1 : -1]);
    keypair = zend_string_checked_alloc(crypto_kx_SECRETKEYBYTES + crypto_kx_PUBLICKEYBYTES, 0);
    sk = (unsigned char *) ZSTR_VAL(keypair);
    pk = sk + crypto_kx_SECRETKEYBYTES;
    crypto_generichash(sk, crypto_kx_SECRETKEYBYTES,
                       seed, crypto_kx_SEEDBYTES, NULL, 0);
    if (crypto_scalarmult_base(pk, sk) != 0) {
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }

    RETURN_STR(keypair);
}

PHP_FUNCTION(sodium_crypto_kx_keypair)
{
    unsigned char *sk;
    unsigned char *pk;
    zend_string   *keypair;

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }
    keypair = zend_string_checked_alloc(crypto_kx_SECRETKEYBYTES + crypto_kx_PUBLICKEYBYTES, 0);
    sk = (unsigned char *) ZSTR_VAL(keypair);
    pk = sk + crypto_kx_SECRETKEYBYTES;
    randombytes_buf(sk, crypto_kx_SECRETKEYBYTES);
    if (crypto_scalarmult_base(pk, sk) != 0) {
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }
    RETURN_STR(keypair);
}

PHP_FUNCTION(sodium_crypto_kx_secretkey)
{
    zend_string   *secretkey;
    unsigned char *keypair;
    size_t         keypair_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s",
                              &keypair, &keypair_len) == FAILURE) {
        return;
    }
    if (keypair_len !=
        crypto_kx_SECRETKEYBYTES + crypto_kx_PUBLICKEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "keypair should be SODIUM_CRYPTO_KX_KEYPAIRBYTES bytes",
                   0);
        return;
    }
    secretkey = zend_string_checked_alloc(crypto_kx_SECRETKEYBYTES, 0);
    memcpy(ZSTR_VAL(secretkey), keypair, crypto_kx_SECRETKEYBYTES);

    RETURN_STR(secretkey);
}

PHP_FUNCTION(sodium_crypto_kx_publickey)
{
    zend_string   *publickey;
    unsigned char *keypair;
    size_t         keypair_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s",
                              &keypair, &keypair_len) == FAILURE) {
        return;
    }
    if (keypair_len !=
        crypto_kx_SECRETKEYBYTES + crypto_kx_PUBLICKEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "keypair should be SODIUM_CRYPTO_KX_KEYPAIRBYTES bytes",
                   0);
        return;
    }
    publickey = zend_string_checked_alloc(crypto_kx_PUBLICKEYBYTES, 0);
    memcpy(ZSTR_VAL(publickey), keypair + crypto_kx_SECRETKEYBYTES,
           crypto_kx_PUBLICKEYBYTES);

    RETURN_STR(publickey);
}

PHP_FUNCTION(sodium_crypto_kx_client_session_keys)
{
    crypto_generichash_state h;
    unsigned char  q[crypto_scalarmult_BYTES];
    unsigned char *keypair;
    unsigned char *client_sk;
    unsigned char *client_pk;
    unsigned char *server_pk;
    unsigned char  session_keys[2 * crypto_kx_SESSIONKEYBYTES];
    size_t         keypair_len;
    size_t         server_pk_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss",
                              &keypair, &keypair_len,
                              &server_pk, &server_pk_len) == FAILURE) {
        return;
    }
    if (keypair_len != crypto_kx_SECRETKEYBYTES + crypto_kx_PUBLICKEYBYTES) {
        zend_throw_exception(sodium_exception_ce, "keypair must be SODIUM_CRYPTO_KX_KEYPAIRBYTES bytes", 0);
        return;
    }
    if (server_pk_len != crypto_kx_PUBLICKEYBYTES) {
        zend_throw_exception(sodium_exception_ce, "public keys must be SODIUM_CRYPTO_KX_PUBLICKEYBYTES bytes", 0);
        return;
    }
    client_sk = &keypair[0];
    client_pk = &keypair[crypto_kx_SECRETKEYBYTES];
    (void) sizeof(int[crypto_scalarmult_SCALARBYTES == crypto_kx_PUBLICKEYBYTES ? 1 : -1]);
    (void) sizeof(int[crypto_scalarmult_SCALARBYTES == crypto_kx_SECRETKEYBYTES ? 1 : -1]);
    if (crypto_scalarmult(q, client_sk, server_pk) != 0) {
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }
    crypto_generichash_init(&h, NULL, 0U, 2 * crypto_kx_SESSIONKEYBYTES);
    crypto_generichash_update(&h, q, sizeof q);
    sodium_memzero(q, sizeof q);
    crypto_generichash_update(&h, client_pk, crypto_kx_PUBLICKEYBYTES);
    crypto_generichash_update(&h, server_pk, crypto_kx_PUBLICKEYBYTES);
    crypto_generichash_final(&h, session_keys, 2 * crypto_kx_SESSIONKEYBYTES);
    sodium_memzero(&h, sizeof h);
    array_init(return_value);
    add_next_index_stringl(return_value,
                           (const char *) session_keys,
                           crypto_kx_SESSIONKEYBYTES);
    add_next_index_stringl(return_value,
                           (const char *) session_keys + crypto_kx_SESSIONKEYBYTES,
                           crypto_kx_SESSIONKEYBYTES);
}

PHP_FUNCTION(sodium_crypto_kx_server_session_keys)
{
    crypto_generichash_state h;
    unsigned char  q[crypto_scalarmult_BYTES];
    unsigned char *keypair;
    unsigned char *server_sk;
    unsigned char *server_pk;
    unsigned char *client_pk;
    unsigned char  session_keys[2 * crypto_kx_SESSIONKEYBYTES];
    size_t         keypair_len;
    size_t         client_pk_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss",
                              &keypair, &keypair_len,
                              &client_pk, &client_pk_len) == FAILURE) {
        return;
    }
    if (keypair_len != crypto_kx_SECRETKEYBYTES + crypto_kx_PUBLICKEYBYTES) {
        zend_throw_exception(sodium_exception_ce, "keypair must be SODIUM_CRYPTO_KX_KEYPAIRBYTES bytes", 0);
        return;
    }
    if (client_pk_len != crypto_kx_PUBLICKEYBYTES) {
        zend_throw_exception(sodium_exception_ce, "public keys must be SODIUM_CRYPTO_KX_PUBLICKEYBYTES bytes", 0);
        return;
    }
    server_sk = &keypair[0];
    server_pk = &keypair[crypto_kx_SECRETKEYBYTES];
    (void) sizeof(int[crypto_scalarmult_SCALARBYTES == crypto_kx_PUBLICKEYBYTES ? 1 : -1]);
    (void) sizeof(int[crypto_scalarmult_SCALARBYTES == crypto_kx_SECRETKEYBYTES ? 1 : -1]);
    if (crypto_scalarmult(q, server_sk, client_pk) != 0) {
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }
    crypto_generichash_init(&h, NULL, 0U, 2 * crypto_kx_SESSIONKEYBYTES);
    crypto_generichash_update(&h, q, sizeof q);
    sodium_memzero(q, sizeof q);
    crypto_generichash_update(&h, client_pk, crypto_kx_PUBLICKEYBYTES);
    crypto_generichash_update(&h, server_pk, crypto_kx_PUBLICKEYBYTES);
    crypto_generichash_final(&h, session_keys, 2 * crypto_kx_SESSIONKEYBYTES);
    sodium_memzero(&h, sizeof h);
    array_init(return_value);
    add_next_index_stringl(return_value,
                           (const char *) session_keys + crypto_kx_SESSIONKEYBYTES,
                           crypto_kx_SESSIONKEYBYTES);
    add_next_index_stringl(return_value,
                           (const char *) session_keys,
                           crypto_kx_SESSIONKEYBYTES);
}

PHP_FUNCTION(sodium_crypto_auth)
{
    zend_string *mac;
    char        *key;
    char        *msg;
    size_t       msg_len;
    size_t       key_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss",
                              &msg, &msg_len,
                              &key, &key_len) == FAILURE) {
        return;
    }
    if (key_len != crypto_auth_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce, "key must be SODIUM_CRYPTO_AUTH_KEYBYTES bytes", 0);
        return;
    }
    mac = zend_string_checked_alloc(crypto_auth_BYTES, 0);
    if (crypto_auth((unsigned char *) ZSTR_VAL(mac),
                    (const unsigned char *) msg, msg_len,
                    (const unsigned char *) key) != 0) {
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }

    RETURN_STR(mac);
}

PHP_FUNCTION(sodium_crypto_auth_verify)
{
    char      *mac;
    char      *key;
    char      *msg;
    size_t     mac_len;
    size_t     msg_len;
    size_t     key_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sss",
                              &mac, &mac_len,
                              &msg, &msg_len,
                              &key, &key_len) == FAILURE) {
        return;
    }
    if (key_len != crypto_auth_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce, "key must be SODIUM_CRYPTO_AUTH_KEYBYTES bytes", 0);
        return;
    }
    if (mac_len != crypto_auth_BYTES) {
        zend_throw_exception(sodium_exception_ce, "authentication tag must be SODIUM_CRYPTO_AUTH_BYTES bytes", 0);
        return;
    }
    if (crypto_auth_verify((const unsigned char *) mac,
                           (const unsigned char *) msg, msg_len,
                           (const unsigned char *) key) != 0) {
        RETURN_FALSE;
    }
    RETURN_TRUE;
}

PHP_FUNCTION(sodium_crypto_sign_ed25519_sk_to_curve25519)
{
    zend_string *ecdhkey;
    char        *eddsakey;
    size_t       eddsakey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s",
                              &eddsakey, &eddsakey_len) == FAILURE) {
        return;
    }
    if (eddsakey_len != crypto_sign_SECRETKEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "Ed25519 key should be SODIUM_CRYPTO_SIGN_SECRETKEYBYTES bytes",
                   0);
        return;
    }
    ecdhkey = zend_string_checked_alloc(crypto_box_SECRETKEYBYTES, 0);

    if (crypto_sign_ed25519_sk_to_curve25519((unsigned char *) ZSTR_VAL(ecdhkey),
                                             (const unsigned char *) eddsakey) != 0) {
        zend_throw_exception(sodium_exception_ce, "crypto_sign_ed25519_sk_to_curve25519()", 0);
        return;
    }

    RETURN_STR(ecdhkey);
}

PHP_FUNCTION(sodium_crypto_sign_ed25519_pk_to_curve25519)
{
    zend_string *ecdhkey;
    char        *eddsakey;
    size_t       eddsakey_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s",
                              &eddsakey, &eddsakey_len) == FAILURE) {
        return;
    }
    if (eddsakey_len != crypto_sign_PUBLICKEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "Ed25519 key should be SODIUM_CRYPTO_SIGN_PUBLICKEYBYTES bytes",
                   0);
        return;
    }
    ecdhkey = zend_string_checked_alloc(crypto_sign_PUBLICKEYBYTES, 0);

    if (crypto_sign_ed25519_pk_to_curve25519((unsigned char *) ZSTR_VAL(ecdhkey),
                                             (const unsigned char *) eddsakey) != 0) {
        zend_throw_exception(sodium_exception_ce, "crypto_sign_ed25519_pk_to_curve25519()", 0);
        return;
    }

    RETURN_STR(ecdhkey);
}

PHP_FUNCTION(sodium_compare)
{
    char      *buf1;
    char      *buf2;
    size_t     len1;
    size_t     len2;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss",
                              &buf1, &len1,
                              &buf2, &len2) == FAILURE) {
        return;
    }
    if (len1 != len2) {
        zend_throw_exception(sodium_exception_ce, "arguments have different sizes", 0);
    } else {
        RETURN_LONG(sodium_compare((const unsigned char *) buf1,
                                   (const unsigned char *) buf2, (size_t) len1));
    }
}

#ifdef HAVE_AESGCM
PHP_FUNCTION(sodium_crypto_aead_aes256gcm_keygen)
{
    unsigned char key[crypto_aead_aes256gcm_KEYBYTES];

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }
    randombytes_buf(key, sizeof key);
    RETURN_STRINGL((const char *) key, sizeof key);
}
#endif

PHP_FUNCTION(sodium_crypto_aead_chacha20poly1305_keygen)
{
    unsigned char key[crypto_aead_chacha20poly1305_KEYBYTES];

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }
    randombytes_buf(key, sizeof key);
    RETURN_STRINGL((const char *) key, sizeof key);
}

PHP_FUNCTION(sodium_crypto_aead_chacha20poly1305_ietf_keygen)
{
    unsigned char key[crypto_aead_chacha20poly1305_IETF_KEYBYTES];

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }
    randombytes_buf(key, sizeof key);
    RETURN_STRINGL((const char *) key, sizeof key);
}

#ifdef crypto_aead_xchacha20poly1305_IETF_NPUBBYTES
PHP_FUNCTION(sodium_crypto_aead_xchacha20poly1305_ietf_keygen)
{
    unsigned char key[crypto_aead_xchacha20poly1305_IETF_KEYBYTES];

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }
    randombytes_buf(key, sizeof key);
    RETURN_STRINGL((const char *) key, sizeof key);
}
#endif

PHP_FUNCTION(sodium_crypto_auth_keygen)
{
    unsigned char key[crypto_auth_KEYBYTES];

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }
    randombytes_buf(key, sizeof key);
    RETURN_STRINGL((const char *) key, sizeof key);
}

PHP_FUNCTION(sodium_crypto_generichash_keygen)
{
    unsigned char key[crypto_generichash_KEYBYTES];

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }
    randombytes_buf(key, sizeof key);
    RETURN_STRINGL((const char *) key, sizeof key);
}

PHP_FUNCTION(sodium_crypto_kdf_keygen)
{
    unsigned char key[crypto_kdf_KEYBYTES];

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }
    randombytes_buf(key, sizeof key);
    RETURN_STRINGL((const char *) key, sizeof key);
}

PHP_FUNCTION(sodium_crypto_secretbox_keygen)
{
    unsigned char key[crypto_secretbox_KEYBYTES];

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }
    randombytes_buf(key, sizeof key);
    RETURN_STRINGL((const char *) key, sizeof key);
}

PHP_FUNCTION(sodium_crypto_shorthash_keygen)
{
    unsigned char key[crypto_shorthash_KEYBYTES];

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }
    randombytes_buf(key, sizeof key);
    RETURN_STRINGL((const char *) key, sizeof key);
}

PHP_FUNCTION(sodium_crypto_stream_keygen)
{
    unsigned char key[crypto_stream_KEYBYTES];

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }
    randombytes_buf(key, sizeof key);
    RETURN_STRINGL((const char *) key, sizeof key);
}

#ifdef crypto_stream_xchacha20_KEYBYTES
PHP_FUNCTION(sodium_crypto_stream_xchacha20_keygen)
{
    unsigned char key[crypto_stream_xchacha20_KEYBYTES];

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }
    randombytes_buf(key, sizeof key);
    RETURN_STRINGL((const char *) key, sizeof key);
}
#endif

PHP_FUNCTION(sodium_crypto_kdf_derive_from_key)
{
    unsigned char  ctx_padded[crypto_generichash_blake2b_PERSONALBYTES];
#ifndef crypto_kdf_PRIMITIVE
    unsigned char  salt[crypto_generichash_blake2b_SALTBYTES];
#endif
    char          *ctx;
    char          *key;
    zend_string   *subkey;
    zend_long      subkey_id;
    zend_long      subkey_len;
    size_t         ctx_len;
    size_t         key_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "llss",
                              &subkey_len,
                              &subkey_id,
                              &ctx, &ctx_len,
                              &key, &key_len) == FAILURE) {
        return;
    }
    if (subkey_len < crypto_kdf_BYTES_MIN) {
        zend_throw_exception(sodium_exception_ce, "subkey cannot be smaller than SODIUM_CRYPTO_KDF_BYTES_MIN", 0);
        return;
    }
    if (subkey_len > crypto_kdf_BYTES_MAX || subkey_len > SIZE_MAX) {
        zend_throw_exception(sodium_exception_ce, "subkey cannot be larger than SODIUM_CRYPTO_KDF_BYTES_MAX", 0);
        return;
    }
    if (subkey_id < 0) {
        zend_throw_exception(sodium_exception_ce, "subkey_id cannot be negative", 0);
        return;
    }
    if (ctx_len != crypto_kdf_CONTEXTBYTES) {
        zend_throw_exception(sodium_exception_ce, "context should be SODIUM_CRYPTO_KDF_CONTEXTBYTES bytes", 0);
        return;
    }
    if (key_len != crypto_kdf_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce, "key should be SODIUM_CRYPTO_KDF_KEYBYTES bytes", 0);
        return;
    }
    memcpy(ctx_padded, ctx, crypto_kdf_CONTEXTBYTES);
    memset(ctx_padded + crypto_kdf_CONTEXTBYTES, 0, sizeof ctx_padded - crypto_kdf_CONTEXTBYTES);
    subkey = zend_string_checked_alloc((size_t) subkey_len, 0);
#ifdef crypto_kdf_PRIMITIVE
    crypto_kdf_derive_from_key((unsigned char *) ZSTR_VAL(subkey),
                               (size_t) subkey_len, (uint64_t) subkey_id,
                               ctx, (const unsigned char *) key);
#else
    salt[0] = (unsigned char) (((uint64_t) subkey_id)      );
    salt[1] = (unsigned char) (((uint64_t) subkey_id) >>  8);
    salt[2] = (unsigned char) (((uint64_t) subkey_id) >> 16);
    salt[3] = (unsigned char) (((uint64_t) subkey_id) >> 24);
    salt[4] = (unsigned char) (((uint64_t) subkey_id) >> 32);
    salt[5] = (unsigned char) (((uint64_t) subkey_id) >> 40);
    salt[6] = (unsigned char) (((uint64_t) subkey_id) >> 48);
    salt[7] = (unsigned char) (((uint64_t) subkey_id) >> 56);
    memset(salt + 8, 0, (sizeof salt) - 8);
    crypto_generichash_blake2b_salt_personal((unsigned char *) ZSTR_VAL(subkey),
                                             (size_t) subkey_len,
                                             NULL, 0,
                                             (const unsigned char *) key,
                                             crypto_kdf_KEYBYTES,
                                             salt, ctx_padded);
#endif

    RETURN_STR(subkey);
}

PHP_FUNCTION(sodium_pad)
{
    zend_string    *padded;
    char           *unpadded;
    zend_long       blocksize;
    volatile size_t st;
    size_t          i, j, k;
    size_t          unpadded_len;
    size_t          xpadlen;
    size_t          xpadded_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sl",
                              &unpadded, &unpadded_len, &blocksize) == FAILURE) {
        return;
    }
    if (blocksize <= 0) {
        zend_throw_exception(sodium_exception_ce, "block size cannot be less than 1", 0);
        return;
    }
    if (blocksize > SIZE_MAX) {
        zend_throw_exception(sodium_exception_ce, "block size is too large", 0);
        return;
    }
    xpadlen = blocksize - 1U;
    if ((blocksize & (blocksize - 1U)) == 0U) {
        xpadlen -= unpadded_len & ((size_t) blocksize - 1U);
    } else {
        xpadlen -= unpadded_len % (size_t) blocksize;
    }
    if ((size_t) SIZE_MAX - unpadded_len <= xpadlen) {
        zend_throw_exception(sodium_exception_ce, "input is too large", 0);
        return;
    }
    xpadded_len = unpadded_len + xpadlen;
    padded = zend_string_checked_alloc(xpadded_len + 1U, 0);
    if (unpadded_len > 0) {
        st = 1U;
        i = 0U;
        k = unpadded_len;
        for (j = 0U; j <= xpadded_len; j++) {
            ZSTR_VAL(padded)[j] = unpadded[i];
            k -= st;
            st = (size_t) (~(((( (((uint64_t) k) >> 48) | (((uint64_t) k) >> 32) |
                                 (k >> 16) | k) & 0xffff) - 1U) >> 16)) & 1U;
            i += st;
        }
    }
#if SODIUM_LIBRARY_VERSION_MAJOR > 9 || (SODIUM_LIBRARY_VERSION_MAJOR == 9 && SODIUM_LIBRARY_VERSION_MINOR >= 6)
    if (sodium_pad(NULL, (unsigned char *) ZSTR_VAL(padded), unpadded_len,
                   (size_t) blocksize, xpadded_len + 1U) != 0) {
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }
#else
    {
        char                   *tail;
        volatile unsigned char  mask;
        unsigned char           barrier_mask;

        tail = &ZSTR_VAL(padded)[xpadded_len];
        mask = 0U;
        for (i = 0; i < blocksize; i++) {
            barrier_mask = (unsigned char)
                (((i ^ xpadlen) - 1U) >> ((sizeof(size_t) - 1U) * CHAR_BIT));
            tail[-i] = (tail[-i] & mask) | (0x80 & barrier_mask);
            mask |= barrier_mask;
        }
    }
#endif

    RETURN_STR(padded);
}

PHP_FUNCTION(sodium_unpad)
{
    zend_string *unpadded;
    char        *padded;
    size_t       padded_len;
    size_t       unpadded_len;
    zend_long    blocksize;
    int          ret;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sl",
                              &padded, &padded_len, &blocksize) == FAILURE) {
        return;
    }
    if (blocksize <= 0) {
        zend_throw_exception(sodium_exception_ce, "block size cannot be less than 1", 0);
        return;
    }
    if (blocksize > SIZE_MAX) {
        zend_throw_exception(sodium_exception_ce, "block size is too large", 0);
        return;
    }
    if (padded_len < blocksize) {
        zend_throw_exception(sodium_exception_ce, "invalid padding", 0);
        return;
    }

#if SODIUM_LIBRARY_VERSION_MAJOR > 9 || (SODIUM_LIBRARY_VERSION_MAJOR == 9 && SODIUM_LIBRARY_VERSION_MINOR >= 6)
    ret = sodium_unpad(&unpadded_len, (const unsigned char *) padded,
                       padded_len, (size_t) blocksize);
#else
    {
        const char      *tail;
        unsigned char    acc = 0U;
        unsigned char    c;
        unsigned char    valid = 0U;
        volatile size_t  pad_len = 0U;
        size_t           i;
        size_t           is_barrier;

        tail = &padded[padded_len - 1U];

        for (i = 0U; i < (size_t) blocksize; i++) {
            c = tail[-i];
            is_barrier =
                (( (acc - 1U) & (pad_len - 1U) & ((c ^ 0x80) - 1U) ) >> 8) & 1U;
            acc |= c;
            pad_len |= i & (1U + ~is_barrier);
            valid |= (unsigned char) is_barrier;
        }
        unpadded_len = padded_len - 1U - pad_len;
        ret = (int) (valid - 1U);
    }
#endif
    if (ret != 0 || unpadded_len > LONG_MAX) {
        zend_throw_exception(sodium_exception_ce, "invalid padding", 0);
        return;
    }
    unpadded = zend_string_init(padded, padded_len, 0);
    PHP_SODIUM_ZSTR_TRUNCATE(unpadded, unpadded_len);

    RETURN_STR(unpadded);
}

#ifdef crypto_secretstream_xchacha20poly1305_ABYTES
PHP_FUNCTION(sodium_crypto_secretstream_xchacha20poly1305_keygen)
{
    unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES];

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }
    randombytes_buf(key, sizeof key);
    RETURN_STRINGL((const char *) key, sizeof key);
}

PHP_FUNCTION(sodium_crypto_secretstream_xchacha20poly1305_init_push)
{
    crypto_secretstream_xchacha20poly1305_state  state;
    unsigned char                                header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    unsigned char                               *key;
    size_t                                       key_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s",
                              &key, &key_len) == FAILURE) {
        return;
    }
    if (key_len != crypto_secretstream_xchacha20poly1305_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "key size should be SODIUM_CRYPTO_SECRETSTREAM_XCHACHA20POLY1305_KEYBYTES bytes",
                   0);
        return;
    }
    if (crypto_secretstream_xchacha20poly1305_init_push(&state,
                                                        header, key) != 0) {
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }
    array_init(return_value);
    add_next_index_stringl(return_value, (const char *) &state, sizeof state);
    add_next_index_stringl(return_value, (const char *) header, sizeof header);
}

PHP_FUNCTION(sodium_crypto_secretstream_xchacha20poly1305_push)
{
    zval               *state_zv;
    zend_string        *c;
    unsigned char      *ad = NULL;
    unsigned char      *msg;
    unsigned char      *state;
    unsigned long long  c_real_len;
    zend_long           tag = crypto_secretstream_xchacha20poly1305_TAG_MESSAGE;
    size_t              ad_len = (size_t) 0U;
    size_t              c_len;
    size_t              msg_len;
    size_t              state_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "zs|sl",
                              &state_zv,
                              &msg, &msg_len, &ad, &ad_len, &tag) == FAILURE) {
        return;
    }
    ZVAL_DEREF(state_zv);
    if (Z_TYPE_P(state_zv) != IS_STRING) {
        zend_throw_exception(sodium_exception_ce, "a reference to a state is required", 0);
        return;
    }
    sodium_separate_string(state_zv);
    state = (unsigned char *) Z_STRVAL(*state_zv);
    state_len = Z_STRLEN(*state_zv);
    if (state_len != sizeof (crypto_secretstream_xchacha20poly1305_state)) {
        zend_throw_exception(sodium_exception_ce, "incorrect state length", 0);
        return;
    }
    if (msg_len > crypto_secretstream_xchacha20poly1305_MESSAGEBYTES_MAX ||
        msg_len > SIZE_MAX - crypto_secretstream_xchacha20poly1305_ABYTES) {
        zend_throw_exception(sodium_exception_ce, "message cannot be larger than SODIUM_CRYPTO_SECRETSTREAM_XCHACHA20POLY1305_MESSAGEBYTES_MAX bytes", 0);
        return;
    }
    if (tag < 0 || tag > 255) {
        zend_throw_exception(sodium_exception_ce, "unsupported value for the tag", 0);
        return;
    }
    c_len = msg_len + crypto_secretstream_xchacha20poly1305_ABYTES;
    c = zend_string_checked_alloc((size_t) c_len, 0);
    if (crypto_secretstream_xchacha20poly1305_push
        ((void *) state, (unsigned char *) ZSTR_VAL(c), &c_real_len,
         msg, (unsigned long long) msg_len, ad, (unsigned long long) ad_len,
         (unsigned char) tag) != 0) {
        zend_string_free(c);
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }
    if (c_real_len <= 0U || c_real_len >= SIZE_MAX || c_real_len > c_len) {
        zend_string_free(c);
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    PHP_SODIUM_ZSTR_TRUNCATE(c, (size_t) c_real_len);

    RETURN_STR(c);
}

PHP_FUNCTION(sodium_crypto_secretstream_xchacha20poly1305_init_pull)
{
    crypto_secretstream_xchacha20poly1305_state  state;
    unsigned char                               *header;
    unsigned char                               *key;
    size_t                                       header_len;
    size_t                                       key_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss",
                              &header, &header_len,
                              &key, &key_len) == FAILURE) {
        return;
    }
    if (header_len != crypto_secretstream_xchacha20poly1305_HEADERBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "header size should be SODIUM_CRYPTO_SECRETSTREAM_XCHACHA20POLY1305_HEADERBYTES bytes",
                   0);
        return;
    }
    if (key_len != crypto_secretstream_xchacha20poly1305_KEYBYTES) {
        zend_throw_exception(sodium_exception_ce,
                   "key size should be SODIUM_CRYPTO_SECRETSTREAM_XCHACHA20POLY1305_KEYBYTES bytes",
                   0);
        return;
    }
    if (crypto_secretstream_xchacha20poly1305_init_pull(&state,
                                                        header, key) != 0) {
        zend_throw_exception(sodium_exception_ce, "internal error", 0);
        return;
    }
    RETURN_STRINGL((const char *) &state, sizeof state);
}

PHP_FUNCTION(sodium_crypto_secretstream_xchacha20poly1305_pull)
{
    zval               *state_zv;
    zend_string        *msg;
    unsigned char      *ad = NULL;
    unsigned char      *c;
    unsigned char      *state;
    unsigned long long  msg_real_len;
    size_t              ad_len = (size_t) 0U;
    size_t              msg_len;
    size_t              c_len;
    size_t              state_len;
    unsigned char       tag;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "zs|s",
                              &state_zv,
                              &c, &c_len, &ad, &ad_len) == FAILURE) {
        return;
    }
    ZVAL_DEREF(state_zv);
    if (Z_TYPE_P(state_zv) != IS_STRING) {
        zend_throw_exception(sodium_exception_ce, "a reference to a state is required", 0);
        return;
    }
    sodium_separate_string(state_zv);
    state = (unsigned char *) Z_STRVAL(*state_zv);
    state_len = Z_STRLEN(*state_zv);
    if (state_len != sizeof (crypto_secretstream_xchacha20poly1305_state)) {
        zend_throw_exception(sodium_exception_ce, "incorrect state length", 0);
        return;
    }
    if (c_len < crypto_secretstream_xchacha20poly1305_ABYTES) {
        RETURN_FALSE;
    }
    msg_len = c_len - crypto_secretstream_xchacha20poly1305_ABYTES;
    msg = zend_string_checked_alloc((size_t) msg_len, 0);
    if (crypto_secretstream_xchacha20poly1305_pull
        ((void *) state, (unsigned char *) ZSTR_VAL(msg), &msg_real_len, &tag,
         c, (unsigned long long) c_len, ad, (unsigned long long) ad_len) != 0) {
        zend_string_free(msg);
        RETURN_FALSE;
    }
    if (msg_real_len >= SIZE_MAX || msg_real_len > msg_len) {
        zend_string_free(msg);
        zend_throw_exception(sodium_exception_ce, "arithmetic overflow", 0);
        return;
    }
    PHP_SODIUM_ZSTR_TRUNCATE(msg, (size_t) msg_real_len);
    array_init(return_value);
    add_next_index_str(return_value, msg);
    add_next_index_long(return_value, (long) tag);
}

PHP_FUNCTION(sodium_crypto_secretstream_xchacha20poly1305_rekey)
{
    zval          *state_zv;
    unsigned char *state;
    size_t         state_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &state_zv) == FAILURE) {
        return;
    }
    ZVAL_DEREF(state_zv);
    if (Z_TYPE_P(state_zv) != IS_STRING) {
        zend_throw_exception(sodium_exception_ce, "a reference to a state is required", 0);
        return;
    }
    sodium_separate_string(state_zv);
    state = (unsigned char *) Z_STRVAL(*state_zv);
    state_len = Z_STRLEN(*state_zv);
    if (state_len != sizeof (crypto_secretstream_xchacha20poly1305_state)) {
        zend_throw_exception(sodium_exception_ce, "incorrect state length", 0);
        return;
    }
    crypto_secretstream_xchacha20poly1305_rekey((void *) state);
}
#endif

/*  OpenSSL dynamic importer ('sslimport.c')
 *  Copyright 2010 TransGaming Inc.
 *
 *
 *  provides a dynamic import of the native OpenSSL library.
 */
#include "config.h"
#include "wine/openssl.h"
#include "wine/port.h"


/* the SSL headers and libraries are available at compile time => try to use them */
#ifdef SSL_AVAILABLE

# ifndef SIZEOFARRAY
#  define SIZEOFARRAY(x)    (sizeof(x) / sizeof((x)[0]))
# endif


/* keep lists of tested (and valid) versions of the native OpenSSL and crypto libraries
   for each platform.  The most preferrable to be loaded must be first in each list. */
# define LIB_NAME_LENGTH 40
# ifndef SONAME_LIBSSL
#  ifdef __APPLE__
static const char g_sslLibs[][LIB_NAME_LENGTH] = {  "libssl.0.9.8.dylib",
                                                    "libssl.0.9.7.dylib",
                                                    "libssl.dylib"  };
#  else
static const char g_sslLibs[][LIB_NAME_LENGTH] = {  "libssl.so.0.9.8",
                                                    "libssl.so.8",
                                                    "libssl.so.7",
                                                    "libssl.so.6",
                                                    "libssl.so.0.9.7",
                                                    "libssl.so.5",
                                                    "libssl.so.4",
                                                    "libssl.so.0",
                                                    "libssl.so"  };
#  endif
# else
static const char g_sslLibs[][LIB_NAME_LENGTH] = {SONAME_LIBSSL};
# endif

# ifndef SONAME_LIBCRYPTO
#  ifdef __APPLE__
static const char g_cryptoLibs[][LIB_NAME_LENGTH] = {   "libcrypto.0.9.8.dylib",
                                                        "libcrypto.0.9.7.dylib",
                                                        "libcrypto.dylib"   };
#  else
static const char g_cryptoLibs[][LIB_NAME_LENGTH] = {   "libcrypto.so.0.9.8",
                                                        "libcrypto.so.8",
                                                        "libcrypto.so.7",
                                                        "libcrypto.so.6",
                                                        "libcrypto.so.0.9.7",
                                                        "libcrypto.so.5",
                                                        "libcrypto.so.4",
                                                        "libcrypto.so.0",
                                                        "libcrypto.so"   };
#  endif
# else
static const char g_cryptoLibs[][LIB_NAME_LENGTH] = {SONAME_LIB_CRYPTO};
# endif


/* global instance of the OpenSSL function table structure.  All of the OpenSSL functions
   will be accessed through this (through macros defined in 'wine/openssl.h'). */
OpenSSL g_openSSL = {0};



/* OPENSSL_loadOpenSSL(): attempt to load the OpenSSL and Crypto libraries dynamically
     and import the required functions.  Returns zero on success, a positive count of
     the number of missing imports if some or all functions could not be imported, and
     a negative error code if one of the libraries could not be loaded.  This function
     must be called before attempting to call any OpenSSL functions.  Upon successful
     import, the OpenSSL library will be initialized and all error strings will be
     loaded. */
int OPENSSL_loadOpenSSL()
{
    int i;
    int ret = OPENSSL_ERR_NONE;


    /* already initialized everything */
    if (OPENSSL_isOpenSSLLoaded())
    {
        g_openSSL.refCount++;
        return g_openSSL.missingFunctions;
    }

    /* try to load the OpenSSL libraries in order or preference */
    for (i = 0; i < SIZEOFARRAY(g_sslLibs); i++)
    {
        g_openSSL.openSSLModule = wine_dlopen(g_sslLibs[i], RTLD_NOW, NULL, 0);

        if (g_openSSL.openSSLModule)
            break;
    }

    if (g_openSSL.openSSLModule == NULL)
    {
        ret = OPENSSL_ERR_SSL_LOAD_FAILED;
        goto Fail;
    }


    for (i = 0; i < SIZEOFARRAY(g_cryptoLibs); i++)
    {
        g_openSSL.cryptoModule = wine_dlopen(g_cryptoLibs[i], RTLD_NOW, NULL, 0);

        if (g_openSSL.cryptoModule)
            break;
    }

    if (g_openSSL.cryptoModule == NULL)
    {
        ret = OPENSSL_ERR_CRYPTO_LOAD_FAILED;
        goto Fail;
    }


#define GETPROC(mod, x) \
        do{ \
            g_openSSL.p##x = wine_dlsym(g_openSSL.mod##Module, #x, NULL, 0); \
            if (g_openSSL.p##x == NULL) \
            { \
                fprintf(stderr, "err:opensslimport:%s(): failed to load symbol %s() from the module '%s'\n", __FUNCTION__, #x, #mod); \
                g_openSSL.missingFunctions++; \
            } \
        } while (0)

    GETPROC(openSSL, CRYPTO_free);
    GETPROC(openSSL, OPENSSL_add_all_algorithms_noconf);
    GETPROC(openSSL, OPENSSL_add_all_algorithms_conf);
    GETPROC(openSSL, PKCS12_parse);
    GETPROC(openSSL, PKCS12_verify_mac);
    GETPROC(openSSL, PKCS12_free);
    GETPROC(openSSL, SSL_connect);
    GETPROC(openSSL, SSL_CTX_ctrl);
    GETPROC(openSSL, SSL_CTX_use_certificate);
    GETPROC(openSSL, SSL_CTX_use_PrivateKey);
    GETPROC(openSSL, SSL_CTX_get_timeout);
    GETPROC(openSSL, SSL_CTX_set_timeout);
    GETPROC(openSSL, SSL_CTX_set_default_verify_paths);
    GETPROC(openSSL, SSL_CTX_new);
    GETPROC(openSSL, SSL_CTX_free);
    GETPROC(openSSL, SSL_free);
    GETPROC(openSSL, SSL_get_error);
    GETPROC(openSSL, SSL_get_peer_certificate);
    GETPROC(openSSL, SSL_get_verify_result);
    GETPROC(openSSL, SSL_library_init);
    GETPROC(openSSL, SSL_load_error_strings);
    GETPROC(openSSL, SSL_new);
    GETPROC(openSSL, SSL_pending);
    GETPROC(openSSL, SSL_read);
    GETPROC(openSSL, SSL_set_accept_state);
    GETPROC(openSSL, SSL_set_bio);
    GETPROC(openSSL, SSL_set_connect_state);
    GETPROC(openSSL, SSL_set_fd);
    GETPROC(openSSL, SSL_shutdown);
    GETPROC(openSSL, SSL_state);
    GETPROC(openSSL, SSL_use_certificate);
    GETPROC(openSSL, SSL_use_PrivateKey);
    GETPROC(openSSL, SSL_write);
    GETPROC(openSSL, SSLv23_method);

    GETPROC(crypto, BIO_ctrl);
    GETPROC(crypto, BIO_ctrl_get_read_request);
    GETPROC(crypto, BIO_ctrl_pending);
    GETPROC(crypto, BIO_ctrl_wpending);
    GETPROC(crypto, BIO_free);
    GETPROC(crypto, BIO_new_bio_pair);
    GETPROC(crypto, BIO_new_fp);
    GETPROC(crypto, BIO_new_mem_buf);
    GETPROC(crypto, BIO_read);
    GETPROC(crypto, BIO_write);
    GETPROC(crypto, BN_bn2bin);
    GETPROC(crypto, d2i_PKCS12_bio);
    GETPROC(crypto, d2i_X509);
    GETPROC(crypto, ERR_error_string);
    GETPROC(crypto, ERR_error_string_n);
    GETPROC(crypto, ERR_get_error);
    GETPROC(crypto, ERR_load_crypto_strings);
    GETPROC(crypto, EVP_PKEY_free);
    GETPROC(crypto, EVP_PKEY_size);
    GETPROC(crypto, EVP_PKEY_type);
    GETPROC(crypto, i2d_PublicKey);
    GETPROC(crypto, i2d_X509);
    GETPROC(crypto, X509_free);
    GETPROC(crypto, X509_get_pubkey);
#undef GETPROC

    /* initialize the library */
    g_openSSL.pSSL_library_init();
    g_openSSL.pOPENSSL_add_all_algorithms_conf();
    g_openSSL.pSSL_load_error_strings();
    g_openSSL.pERR_load_crypto_strings();

    g_openSSL.refCount++;
    return g_openSSL.missingFunctions;

Fail:
    OPENSSL_unloadOpenSSL();
    return ret;
}

/* OPENSSL_isOpenSSLLoaded(): checks if the OpenSSL library has been loaded already.
     Returns 0 if the library has not been loaded or failed to load.  Returns 1 if
     the library was successfully loaded and all symbols were successfully imported. */
int OPENSSL_isOpenSSLLoaded()
{
    return (g_openSSL.openSSLModule != NULL) &&
           (g_openSSL.cryptoModule != NULL);
}

/* OPENSSL_unloadOpenSSL(): unloads the OpenSSL library and clears out the structure.
     This function does not strictly need to be called, but should be before shutting
     down anything that initialized it. */
int OPENSSL_unloadOpenSSL()
{
    g_openSSL.refCount--;

    /* there are still outstanding references to the library => don't unload yet */
    if (g_openSSL.refCount > 0)
        return 0;


    /* nothing else is referencing the library => close everything and clean up */
    if (g_openSSL.openSSLModule)
        wine_dlclose(g_openSSL.openSSLModule, NULL, 0);

    if (g_openSSL.cryptoModule)
        wine_dlclose(g_openSSL.cryptoModule, NULL, 0);

    /* reset the import struct since it's now invalid */
    memset(&g_openSSL, 0, sizeof(OpenSSL));
    return 0;
}


/* OpenSSL is not available => stub out the library loading functions */
#else  /* !SSL_AVAILABLE */

/* OPENSSL_loadOpenSSL(): fails with OPENSSL_ERR_SSL_LOAD_FAILED. */
int OPENSSL_loadOpenSSL()
{
    return OPENSSL_ERR_SSL_LOAD_FAILED;
}

/* OPENSSL_isOpenSSLLoaded(): fails with 0. */
int OPENSSL_isOpenSSLLoaded()
{
    return 0;
}

/* OPENSSL_unloadOpenSSL(): fails with OPENSSL_ERR_SSL_LOAD_FAILED. */
int OPENSSL_unloadOpenSSL()
{
    return OPENSSL_ERR_SSL_LOAD_FAILED;
}

#endif  /* !SSL_AVAILABLE */

/*
 *  OpenSSL dynamic import library ('openssl.h')
 *  Copyright (c) 2010-2015 NVIDIA CORPORATION. All rights reserved.
 *
 *
 *  This header provides access to a dynamically loaded version of OpenSSL.  This
 *  allows for control over the imported version of the native library and provides
 *  a common load point for the library.  This header must be included before any
 *  Windows headers are included as some OpenSSL symbols conflict with windows
 *  symbols.  This header must also be included *after* 'config.h' has been
 *  included.
 *
 *  Checks for the availability of the library should either look for the symbol
 *  SSL_AVAILABLE, or call the OPENSSL_isOpenSSLLoaded() function.  OpenSSL types,
 *  functions, and macros should not be used unless SSL_AVAILABLE is defined.
 *
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
*/
#ifndef  __OPENSSL_IMPORT_H__
# define __OPENSSL_IMPORT_H__

/* NOTE: 'config.h' must be included before this file in order to have the OpenSSL symbols defined */
# ifndef __WINE_CONFIG_H
#  error "'config.h' must be included before attempting to include 'wine/openssl.h'"
# endif

# ifdef HAVE_OPENSSL_SSL_H
#  include <openssl/ssl.h>
#  include <openssl/crypto.h>
#  include <openssl/err.h>
#  include <openssl/evp.h>
#  include <openssl/bio.h>
#  include <openssl/pkcs12.h>
#  include <openssl/x509.h>
#  include <openssl/x509v3.h>
#  undef FAR
#  undef DSA

/* the symbol X509_NAME is defined in 'wincrypt.h' and conflicts with an OpenSSL type.
   We'll rename it here to avoid that conflict.  A few other names have similar issues. */
#  define X509_NAME_WIN         ((LPCSTR)7)
#  define X509_CERT_PAIR_WIN    ((LPCSTR)53)
#  define X509_EXTENSIONS_WIN   ((LPCSTR)5)
#  define PKCS7_SIGNER_INFO_WIN ((LPCSTR)500)
#  undef  X509_NAME
#  undef  X509_CERT_PAIR_WIN
#  undef  X509_EXTENSIONS
#  undef  PKCS7_SIGNER_INFO

/* symbol to be used to check for the availability of OpenSSL functions */
#  define SSL_AVAILABLE 1


/* struct OpenSSL: provides storage for function pointers into the OpenSSL and crypto
     libraries.  These are loaded dynamically at run time (using OPENSSL_loadOpenSSL()).
     The instance of the struct is reference counted and the libraries will only be
     unloaded (using OPENSSL_unloadOpenSSL()) once all references have been released.
     All functions must be imported successfully to be considered a complete load.
     However, the OPENSSL_loadOpenSSL() function will still succeed if not all functions
     could be imported (the caller is responsible for checking if they are available
     before calling into each function).

     NOTE: be very careful when adding functions to this table.  OpenSSL uses a huge number
           of macros for its documented functions and they can obviously not be imported.
           Unfortunately, they cannot be used as defined either because that will lead to
           a runtime error (likely a crash or runtime link error) when trying to call the
           functions used in the macros.  Even if the runtime linker is able to load a
           module for the missing symbols, there is no guarantee that it will have loaded
           the same module as we have here.  If the function that gets implicitly linked
           accesses any global state of OpenSSL, it will either fail or give undefined
           results (thereby making the error VERY hard to track down).  A solution needs
           to be implemented to make it safe to call macro implemented functions while 
           using the dynamically loaded libraries. */
typedef struct
{
    void *openSSLModule;
    void *cryptoModule;
    int   missingFunctions;
    int   refCount;

#  define MAKE_FUNCPTR(f) typeof(f) * p##f

    /* OpenSSL's libssl functions that we use */
    MAKE_FUNCPTR(CRYPTO_free);
    MAKE_FUNCPTR(OPENSSL_add_all_algorithms_conf);
    MAKE_FUNCPTR(OPENSSL_add_all_algorithms_noconf);
    MAKE_FUNCPTR(PKCS12_free);
    MAKE_FUNCPTR(PKCS12_parse);
    MAKE_FUNCPTR(PKCS12_verify_mac);
    MAKE_FUNCPTR(SSL_connect);
    MAKE_FUNCPTR(SSL_CTX_ctrl);
    MAKE_FUNCPTR(SSL_CTX_free);
    MAKE_FUNCPTR(SSL_CTX_get_timeout);
    MAKE_FUNCPTR(SSL_CTX_new);
    MAKE_FUNCPTR(SSL_CTX_set_default_verify_paths);
    MAKE_FUNCPTR(SSL_CTX_set_timeout);
    MAKE_FUNCPTR(SSL_CTX_use_certificate);
    MAKE_FUNCPTR(SSL_CTX_use_PrivateKey);
    MAKE_FUNCPTR(SSL_free);
    MAKE_FUNCPTR(SSL_get_error);
    MAKE_FUNCPTR(SSL_get_peer_certificate);
    MAKE_FUNCPTR(SSL_get_verify_result);
    MAKE_FUNCPTR(SSL_library_init);
    MAKE_FUNCPTR(SSL_load_error_strings);
    MAKE_FUNCPTR(SSL_new);
    MAKE_FUNCPTR(SSL_pending);
    MAKE_FUNCPTR(SSL_read);
    MAKE_FUNCPTR(SSL_set_accept_state);
    MAKE_FUNCPTR(SSL_set_bio);
    MAKE_FUNCPTR(SSL_set_connect_state);
    MAKE_FUNCPTR(SSL_set_fd);
    MAKE_FUNCPTR(SSL_shutdown);
    MAKE_FUNCPTR(SSL_state);
    MAKE_FUNCPTR(SSL_use_certificate);
    MAKE_FUNCPTR(SSL_use_PrivateKey);
    MAKE_FUNCPTR(SSL_write);
    MAKE_FUNCPTR(SSLv23_method);


    /* OpenSSL's libcrypto functions that we use */
    MAKE_FUNCPTR(BIO_ctrl);
    MAKE_FUNCPTR(BIO_ctrl_get_read_request);
    MAKE_FUNCPTR(BIO_ctrl_pending);
    MAKE_FUNCPTR(BIO_ctrl_wpending);
    MAKE_FUNCPTR(BIO_free);
    MAKE_FUNCPTR(BIO_new_bio_pair);
    MAKE_FUNCPTR(BIO_new_fp);
    MAKE_FUNCPTR(BIO_new_mem_buf);
    MAKE_FUNCPTR(BIO_read);
    MAKE_FUNCPTR(BIO_write);
    MAKE_FUNCPTR(BN_bn2bin);
    MAKE_FUNCPTR(d2i_X509);
    MAKE_FUNCPTR(d2i_PKCS12_bio);
    MAKE_FUNCPTR(ERR_error_string);
    MAKE_FUNCPTR(ERR_error_string_n);
    MAKE_FUNCPTR(ERR_get_error);
    MAKE_FUNCPTR(ERR_load_crypto_strings);
    MAKE_FUNCPTR(EVP_PKEY_free);
    MAKE_FUNCPTR(EVP_PKEY_size);
    MAKE_FUNCPTR(EVP_PKEY_type);
    MAKE_FUNCPTR(i2d_X509);
    MAKE_FUNCPTR(i2d_PublicKey);
    MAKE_FUNCPTR(X509_free);
    MAKE_FUNCPTR(X509_get_pubkey);
#  undef MAKE_FUNCPTR
} OpenSSL;

/* global instance of the OpenSSL function table that all calls are made through */
extern OpenSSL g_openSSL;


/* helper macros to override the loaded OpenSSL functions.  Note that it is left up to the
   caller's responsibility to check for SSL_AVAILABLE before calling any of the functions
   defined here.  Individual functions can also be checked for availability by comparing
   them to NULL. */
# define CRYPTO_free                        g_openSSL.pCRYPTO_free
# define OPENSSL_add_all_algorithms_noconf  g_openSSL.pOPENSSL_add_all_algorithms_noconf
# define OPENSSL_add_all_algorithms_conf    g_openSSL.pOPENSSL_add_all_algorithms_conf
# define PKCS12_parse                       g_openSSL.pPKCS12_parse
# define PKCS12_verify_mac                  g_openSSL.pPKCS12_verify_mac
# define PKCS12_free                        g_openSSL.pPKCS12_free
# define SSL_library_init                   g_openSSL.pSSL_library_init
# define SSL_load_error_strings             g_openSSL.pSSL_load_error_strings
# define SSL_connect                        g_openSSL.pSSL_connect
# define SSL_CTX_ctrl                       g_openSSL.pSSL_CTX_ctrl
# define SSL_CTX_free                       g_openSSL.pSSL_CTX_free
# define SSL_CTX_get_timeout                g_openSSL.pSSL_CTX_get_timeout
# define SSL_CTX_new                        g_openSSL.pSSL_CTX_new
# define SSL_CTX_set_default_verify_paths   g_openSSL.pSSL_CTX_set_default_verify_paths
# define SSL_CTX_set_timeout                g_openSSL.pSSL_CTX_set_timeout
# define SSL_CTX_use_certificate            g_openSSL.pSSL_CTX_use_certificate
# define SSL_CTX_use_PrivateKey             g_openSSL.pSSL_CTX_use_PrivateKey
# define SSL_free                           g_openSSL.pSSL_free
# define SSL_get_error                      g_openSSL.pSSL_get_error
# define SSL_get_peer_certificate           g_openSSL.pSSL_get_peer_certificate
# define SSL_get_verify_result              g_openSSL.pSSL_get_verify_result
# define SSL_new                            g_openSSL.pSSL_new
# define SSL_pending                        g_openSSL.pSSL_pending
# define SSL_read                           g_openSSL.pSSL_read
# define SSL_set_accept_state               g_openSSL.pSSL_set_accept_state
# define SSL_set_bio                        g_openSSL.pSSL_set_bio
# define SSL_set_connect_state              g_openSSL.pSSL_set_connect_state
# define SSL_set_fd                         g_openSSL.pSSL_set_fd
# define SSL_shutdown                       g_openSSL.pSSL_shutdown
# define SSL_state                          g_openSSL.pSSL_state
# define SSL_use_certificate                g_openSSL.pSSL_use_certificate
# define SSL_use_PrivateKey                 g_openSSL.pSSL_use_PrivateKey
# define SSL_write                          g_openSSL.pSSL_write
# define SSLv23_method                      g_openSSL.pSSLv23_method

# define BIO_ctrl                           g_openSSL.pBIO_ctrl
# define BIO_ctrl_get_read_request          g_openSSL.pBIO_ctrl_get_read_request
# define BIO_ctrl_pending                   g_openSSL.pBIO_ctrl_pending
# define BIO_ctrl_wpending                  g_openSSL.pBIO_ctrl_wpending
# define BIO_free                           g_openSSL.pBIO_free
# define BIO_new_bio_pair                   g_openSSL.pBIO_new_bio_pair
# define BIO_new_fp                         g_openSSL.pBIO_new_fp
# define BIO_new_mem_buf                    g_openSSL.pBIO_new_mem_buf
# define BIO_read                           g_openSSL.pBIO_read
# define BIO_write                          g_openSSL.pBIO_write
# define BN_bn2bin                          g_openSSL.pBN_bn2bin
# define d2i_X509                           g_openSSL.pd2i_X509
# define d2i_PKCS12_bio                     g_openSSL.pd2i_PKCS12_bio
# define ERR_error_string                   g_openSSL.pERR_error_string
# define ERR_error_string_n                 g_openSSL.pERR_error_string_n
# define ERR_get_error                      g_openSSL.pERR_get_error
# define ERR_load_crypto_strings            g_openSSL.pERR_load_crypto_strings
# define EVP_PKEY_free                      g_openSSL.pEVP_PKEY_free
# define EVP_PKEY_size                      g_openSSL.pEVP_PKEY_size
# define EVP_PKEY_type                      g_openSSL.pEVP_PKEY_type
# define i2d_PublicKey                      g_openSSL.pi2d_PublicKey
# define i2d_X509                           g_openSSL.pi2d_X509
# define X509_get_pubkey                    g_openSSL.pX509_get_pubkey
# define X509_free                          g_openSSL.pX509_free


# else      /* !HAVE_OPENSSL */
#  undef SSL_AVAILABLE

/* create a dummy struct for the OpenSSL type when it is not available at compile time. */
typedef struct{ int unused; } OpenSSL;

/* these macros allow the OpenSSL call to compile if it is not included in the build, but it will likely
   cause issues if actually called.  There should always be checks for whether OpenSSL is available before
   calling any of its functions. */
# define CRYPTO_free(args...)
# define OPENSSL_add_all_algorithms_conf(args...)
# define OPENSSL_add_all_algorithms_noconf(args...)
# define PKCS12_free(args...)
# define PKCS12_parse(args...)
# define PKCS12_verify_mac(args...)
# define SSL_connect(args...)
# define SSL_CTX_ctrl(args...)
# define SSL_CTX_free(args...)
# define SSL_CTX_new(args...)
# define SSL_CTX_use_certificate(args...)
# define SSL_CTX_use_PrivateKey(args...)
# define SSL_CTX_get_timeout(args...)
# define SSL_CTX_set_default_verify_paths(args...)
# define SSL_CTX_set_timeout(args...)
# define SSL_free(args...)
# define SSL_get_error(args...)
# define SSL_get_peer_certificate(args...)
# define SSL_get_verify_result(args...)
# define SSL_library_init(args...)
# define SSL_load_error_strings(args...)
# define SSL_new(args...)
# define SSL_pending(args...)
# define SSL_read(args...)
# define SSL_set_accept_state(args...)
# define SSL_set_bio(args...)
# define SSL_set_connect_state(args...)
# define SSL_set_fd(args...)
# define SSL_shutdown(args...)
# define SSL_state(args...)
# define SSL_use_certificate(args...)
# define SSL_use_PrivateKey(args...)
# define SSL_write(args...)
# define SSLv23_method(args...)

# define BIO_ctrl(args...)
# define BIO_ctrl_get_read_request(args...)
# define BIO_ctrl_pending(args...)
# define BIO_ctrl_wpending(args...)
# define BIO_free(args...)
# define BIO_new_fp(args...)
# define BIO_new_bio_pair(args...)
# define BIO_new_mem_buf(args...)
# define BIO_read(args...)
# define BIO_write(args...)
# define BN_bn2bin(args...)
# define d2i_PKCS12_bio(args...)
# define d2i_X509(args...)
# define ERR_error_string(args...)
# define ERR_error_string_n(args...)
# define ERR_get_error(args...)
# define ERR_load_crypto_strings(args...)
# define EVP_PKEY_free(args...)
# define EVP_PKEY_size(args...)
# define EVP_PKEY_type(args...)
# define i2d_PublicKey(args...)
# define i2d_X509(args...)
# define X509_free(args...)
# define X509_get_pubkey(args...)


# endif     /* !HAVE_OPENSSL */


/* enum OpenSSLLoadError: error codes returned by OPENSSL_loadOpenSSL() when one of the
     libraries could not be loaded.  These error codes can also be returned by the
     OPENSSL_unloadOpenSSL() function on failure. */
typedef enum
{
    OPENSSL_ERR_NONE = 0,
    OPENSSL_ERR_SSL_LOAD_FAILED = -1,
    OPENSSL_ERR_CRYPTO_LOAD_FAILED = -2,
} OpenSSLLoadError;


/* OPENSSL_loadOpenSSL(): attempt to load the OpenSSL and Crypto libraries dynamically
     and import the required functions.  Returns zero on success, a positive count of
     the number of missing imports if some or all functions could not be imported, and
     a negative error code if one of the libraries could not be loaded.  This function
     must be called before attempting to call any OpenSSL functions.  Upon successful
     import, the OpenSSL library will be initialized and all error strings will be
     loaded. */
int OPENSSL_loadOpenSSL();

/* OPENSSL_isOpenSSLLoaded(): checks if the OpenSSL library has been loaded already.
     Returns 0 if the library has not been loaded or failed to load.  Returns 1 if
     the library was successfully loaded and all symbols were successfully imported. */
int OPENSSL_isOpenSSLLoaded();

/* OPENSSL_unloadOpenSSL(): unloads the OpenSSL library and clears out the structure.
     This function does not strictly need to be called, but should be before shutting
     down anything that initialized it. */
int OPENSSL_unloadOpenSSL();

#endif

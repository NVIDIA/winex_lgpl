/*
 * Definitions for the client side of the Wine server communication
 *
 * Copyright (C) 1998 Alexandre Julliard
 */

#ifndef __WINE_WINE_SERVER_H
#define __WINE_WINE_SERVER_H

#include <assert.h>

#include "wine/thread.h"
#include "wine/exception.h"
#include "wine/server_protocol.h"

struct wine_server_request
{
    struct wine_server_request* next;
    struct wine_server_request* prev;

    BOOL reply_received;
    unsigned int saved_buffer_pos;

    EXCEPTION_FRAME frame;

    union generic_request req;
};


struct __server_iovec
{
    const void  *ptr;
    unsigned int size;
};

#define __SERVER_MAX_DATA 4

struct __server_request_info
{
    union
    {
        union generic_request req;    /* request structure */
        union generic_reply   reply;  /* reply structure */
    } u;
    size_t                size;       /* size of request structure */
    unsigned int          data_count; /* count of request data pointers */
    void                 *reply_data; /* reply data pointer */
    struct __server_iovec data[__SERVER_MAX_DATA];  /* request variable size data */
};

extern unsigned int wine_server_call( void *req_ptr );
extern void wine_server_send_fd( int fd );
extern int wine_server_recv_fd( handle_t handle );
extern int wine_server_handle_to_fd( handle_t handle, unsigned int access, int *unix_fd,
                                     enum fd_type *type, int *flags );

void wine_server_init_req( struct wine_server_request *req,
			   enum request request_type, size_t variable_size );
void wine_server_end_req( struct wine_server_request *req);

/* do a server call and set the last error code */
inline static unsigned int wine_server_call_err( void *req_ptr )
{
    unsigned int res = wine_server_call( req_ptr );
    if (res) SetLastError( RtlNtStatusToDosError(res) );
    return res;
}

/* get the size of the variable part of the returned reply */
inline static size_t wine_server_reply_size( const void *reply )
{
    return ((struct reply_header *)reply)->reply_size;
}

/* add some data to be sent along with the request */
inline static void wine_server_add_data( void *req_ptr, const void *ptr, unsigned int size )
{
    struct __server_request_info * const req = req_ptr;
    if (size)
    {
        assert( req->data_count < __SERVER_MAX_DATA );
        req->data[req->data_count].ptr = ptr;
        req->data[req->data_count++].size = size;
        req->u.req.request_header.request_size += size;
    }
}

/* set the pointer and max size for the reply var data */
inline static void wine_server_set_reply( void *req_ptr, void *ptr, unsigned int max_size )
{
    struct __server_request_info * const req = req_ptr;
    req->reply_data = ptr;
    req->u.req.request_header.reply_size = max_size;
}

/* convert a handle to a server precision handle or return
 * a 0xffffffe0 handle if the cast is invalid */
inline static INT_PTR wine_server_handle( HANDLE handle )
{
    return ( (INT_PTR)handle == (int)handle ) ? (INT_PTR)handle : 0xffffffe0;
}

/* convert a server precision handle to a standard handle */
inline static HANDLE extend_server_handle( handle_t handle )
{
    return (HANDLE)(INT_PTR)handle;
}

#define SERVER_START_REQ(type) \
    do { \
        struct __server_request_info __req; \
        struct type##_request * const req = &__req.u.req.type##_request; \
        const struct type##_reply * const reply = &__req.u.reply.type##_reply; \
        __req.u.req.request_header.req = REQ_##type; \
        __req.u.req.request_header.request_size = 0; \
        __req.u.req.request_header.reply_size = 0; \
        __req.size = sizeof(*req); \
        __req.data_count = 0; \
        (void)reply; \
        do

#define SERVER_END_REQ \
        while(0); \
    } while(0)

/* SHM wineserver definitions */
extern BOOL call_supported_through_shared_memory( int req );
extern BOOL server_scheduler_active( void );
void set_shared_memory_reserved(size_t size, void *addr);
extern void setup_shared_memory_wineserver (int shm_type, size_t shm_size,
                                            void *shm_addr);

/* non-exported functions */
extern void server_protocol_error( const char *err, ... ) WINE_NORETURN;
extern void server_protocol_perror( const char *err ) WINE_NORETURN;
extern const char *get_config_dir(void);
extern void CLIENT_InitServer(void);
extern void CLIENT_InitServerDone(void);
extern void CLIENT_InitThread (BOOL IsMainThread);
extern void CLIENT_BootDone( int debug_level );
extern int CLIENT_IsBootThread(void);
extern int wait_server_reply (const void *cookie);

#endif  /* __WINE_WINE_SERVER_H */

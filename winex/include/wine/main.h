/* Taken from WineHQ, shared structure between the preloader
 * and the rest of wine */

#ifndef __WINE_LOADER_MAIN_H
#define __WINE_LOADER_MAIN_H

/* you can only mark one area as serveraddr for now */
#define PRELOAD_INFO_NULL       0x1
#define PRELOAD_INFO_SERVERADDR 0x2

struct wine_preload_info
{
    void  *addr;
    size_t size;
    int    info;
};

#endif /* __WINE_LOADER_MAIN_H */


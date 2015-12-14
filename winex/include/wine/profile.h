/* 
 * Wine profiling interface
 *
 * Copyright 2004 TransGaming Technologies Inc.
 */

#ifndef __WINE_WINE_PROFILE_H
#define __WINE_WINE_PROFILE_H

#ifdef __cplusplus
extern "C" {
#endif

/* #define WINE_PROFILE_ON */

#define RESET_FLAG     0x00000001  /* resets counter on query */
#define HUD_FLAG       0x00000002  /* display timer on hud */
#define LOG_FLAG       0x00000004  /* display to log */
#define COUNTER_FLAG   0x00000008  /* timer is used as a counter */
#define HISTORY_FLAG   0x00000010  /* keep and display running history */
#define PERCENT_FLAG   0x00000020  /* calculate timer as a percent of frame-time */
#define NESTED_FLAG    0x10000000  

typedef struct _query_info {
  char data[128];
} query_info;

#ifdef WINE_PROFILE_ON

#include "wine/compiler_defines.h"

/* for internal use only */
#define WINE_GET_REF(name) \
  static int timer_ref; \
  if( WINE_EXPECT( timer_ref == 0, 0 ) ) { \
    timer_ref = wine_get_timer_ref(name); \
  } 
  
/* these are for external profiling usage */
# define WINE_START_TIMER(name) \
  do { \
    WINE_GET_REF(name) \
    wine_start_timer(timer_ref);  \
  } while(0)
# define WINE_STOP_TIMER(name) \
  do { \
    WINE_GET_REF(name) \
    wine_stop_timer(timer_ref);  \
  } while(0)
# define WINE_INCREMENT_COUNTER(name, value) \
  do { \
    static int timer_ref; \
    if( WINE_EXPECT( timer_ref == 0, 0 ) ) { \
      timer_ref = wine_get_timer_ref(name); \
      wine_set_timer_flags(timer_ref, COUNTER_FLAG, COUNTER_FLAG); \
    } \
    wine_increment_counter(timer_ref, value);  \
  } while(0)
#define WINE_TIMER_FLAGS_SET(name, flags) \
  do { \
    WINE_GET_REF(name) \
    wine_set_timer_flags(timer_ref, flags, flags);  \
  } while(0)
#define WINE_TIMER_FLAGS_UNSET(name, flags) \
  do { \
    WINE_GET_REF(name) \
    wine_set_timer_flags(timer_ref, 0, flags);  \
  } while(0) 
#else
/* if profiling is off, make these no-ops */
# define WINE_START_TIMER(name) do {} while(0)
# define WINE_STOP_TIMER(name) do {} while(0)
# define WINE_INCREMENT_COUNTER(name, value) do {} while(0)
# define WINE_TIMER_FLAGS_SET(name, flags) do {} while(0) 
# define WINE_TIMER_FLAGS_UNSET(name, flags) do {} while(0)
#endif /* WINE_PROFILE_ON */

int wine_get_timer_ref(const char *name);
void wine_start_timer(int ref);
void wine_stop_timer(int ref);
void wine_increment_counter(int ref, int value);
void wine_set_timer_flags(int ref, unsigned int orflags, unsigned int mask);
int wine_query_counters(query_info *info, int size, unsigned int flags);
int wine_query_timers(query_info *info, int size, unsigned int flags);

#ifdef __cplusplus
}
#endif

#endif /* __WINE_WINE_PROFILE_H */

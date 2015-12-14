/*   Wine Profiling Tools 
 *
 * Copyright (c) 2004-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
*/
#include "config.h"

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wine/debug.h"
#include "wine/profile.h"

#define GROUP_SEP   ':'
#define MAX_NAME_LEN 50
#define HIST_SIZE    64
#define MAX_GROUP_SIZE 10

WINE_DECLARE_DEBUG_CHANNEL(history);
WINE_DEFAULT_DEBUG_CHANNEL(history);

typedef struct 
{
  char name[MAX_NAME_LEN];
  unsigned long total;
  struct timeval start;
  unsigned int flags;
  unsigned int cur;
  unsigned long history[HIST_SIZE];
  unsigned long mean;
  unsigned long stddev; 
  int group_count;
  int group_refs[MAX_GROUP_SIZE];
} timer;

static inline void timer_init(timer *t) 
{
  t->flags = RESET_FLAG | HUD_FLAG /* | LOG_FLAG */| HISTORY_FLAG | PERCENT_FLAG;
  t->total = 0;
  memset(t->history, '\0', sizeof(t->history));
  t->mean = 0;
  t->stddev = 0;
  t->cur = 0;
  t->group_count = 0;
}

static inline void timer_reset(timer *t) 
{
  if( t->flags & HISTORY_FLAG ) {
    unsigned long val = t->history[t->cur];
    /* update stats for value being removed */
    t->mean -= val;
    t->stddev -= val*val;
    /* add current total to stats */
    val = t->total;
    t->mean += val;
    t->stddev += val*val;
    t->history[t->cur] = val;
    t->cur = (t->cur+1)%HIST_SIZE;
  }

  t->total = 0;
}

static inline void timer_set_flags(timer *t, unsigned int orflags, unsigned int mask)
{
  t->flags = (t->flags & ~mask) | orflags;
}

static inline void timer_start(timer *t)
{
  gettimeofday(&t->start, NULL);
}

static inline void timer_stop(timer *t)
{
  struct timeval stop;
  gettimeofday(&stop, NULL);
  t->total += ((stop.tv_sec * 1000000 + stop.tv_usec) - 
               (t->start.tv_sec * 1000000 + t->start.tv_usec));
}

static inline void timer_increment(timer *t, int val)
{
  t->total += val;
}

static int num_timers = 32;
static timer *gbl_timers = NULL;
static int *gbl_timer_order = NULL;
static int last_used_timer=1;

int wine_get_timer_ref(const char *name)
{
  int i;
  int new_ref;
  char *sep;
  char safe_name[MAX_NAME_LEN];
  int safe_len;

  /* create or resize the timer array */
  if( gbl_timers == NULL || (last_used_timer == num_timers) ) {
    num_timers *= 2;
    gbl_timers = realloc( gbl_timers, sizeof(timer)*num_timers );
    gbl_timer_order = realloc( gbl_timer_order, sizeof(int)*num_timers );
    /* timer 0 is used internally as a frame timer */
    if( last_used_timer == 1 ) {
      timer_init( &gbl_timers[0] );
    }
  }

  /* make a copy of the name and make sure it doesn't have trailing semicolons */
  safe_len = min(MAX_NAME_LEN-1, strlen(name));
  while( (safe_len > 0) && (name[safe_len-1] == GROUP_SEP) ) {
    safe_len--;
  }
  strncpy(safe_name, name, safe_len);
  safe_name[safe_len] = '\0';
  
  /* look for an existing timer */
  for( i=1; i<last_used_timer; i++ ) {
    if( strncmp( safe_name, gbl_timers[i].name, MAX_NAME_LEN-1 ) == 0 ) {
      return i;
    }
  }
  
  /* create a new timer */
  strncpy( gbl_timers[last_used_timer].name, safe_name, MAX_NAME_LEN-1 );
  gbl_timers[last_used_timer].name[MAX_NAME_LEN-1] = '\0';
  timer_init(&gbl_timers[last_used_timer]);
  new_ref = last_used_timer++;

  /* find the appropriate insertion point - list is always alpha order */
  for( i=1; i<new_ref; i++ ) {
    if( strncmp(safe_name, gbl_timers[gbl_timer_order[i]].name, MAX_NAME_LEN-1) < 0 ) {
      break;
    }
  }
  /* insert at point i */
  memmove(&gbl_timer_order[i+1], &gbl_timer_order[i], sizeof(int)*(new_ref-i));
  gbl_timer_order[i] = new_ref;

  /* add the timer to the appropriate group if necessary */
  if( (sep = strrchr( safe_name, GROUP_SEP )) != NULL ) {
    char group_name[MAX_NAME_LEN];
    int base_timer_ref, len;
    timer *base_timer;

    len = sep - safe_name;
    strncpy( group_name, safe_name, len );
    group_name[len] = '\0';
    /* find (or create) the base timer for the group */
    base_timer_ref = wine_get_timer_ref(group_name);
    base_timer = &gbl_timers[base_timer_ref];
    if( base_timer->group_count < MAX_GROUP_SIZE ) {
      base_timer->group_refs[base_timer->group_count] = new_ref;
      base_timer->group_count++;
    } else {
      ERR("group size exceeded for %s\n", group_name);
    }
  }

  return new_ref;
}

void wine_start_timer(int ref)
{
  timer_start(&gbl_timers[ref]);
}

void wine_stop_timer(int ref)
{
  timer_stop(&gbl_timers[ref]);
}
  
void wine_increment_counter(int ref, int value )
{
  timer_increment(&gbl_timers[ref], value);
}

void wine_set_timer_flags(int ref, unsigned int orflags, unsigned int mask)
{
  int i,j;

  timer_set_flags(&gbl_timers[ref], orflags, mask);

  /* propage the COUNTER_FLAG to any parent groups */
  for( i=last_used_timer-1; i>0; i-- ) {
    timer *t = &gbl_timers[gbl_timer_order[i]];
    for( j=0; j<t->group_count; j++ ) {
      if( gbl_timers[t->group_refs[j]].flags & COUNTER_FLAG ) {
        timer_set_flags(t, COUNTER_FLAG, COUNTER_FLAG);
      }
    } 
  }
}

int wine_query_counters(query_info *info, int size, unsigned int flags)
{
  int i,idx, count=0;
  char time[20], stats[40];
  
  /* prep the display - in sorted order */
  for( idx=1; idx<last_used_timer; idx++ ) {
    i = gbl_timer_order[idx];
    if( gbl_timers[i].flags & COUNTER_FLAG ) {
      sprintf(time, "%8lu", gbl_timers[i].total);
      sprintf(stats, "(%6lu)", gbl_timers[i].mean / HIST_SIZE);

      if( (gbl_timers[i].flags & HUD_FLAG) && (count < size)) {
	snprintf(info[count].data, 128, "%s %s %s", time,
	    (gbl_timers[i].flags & HISTORY_FLAG) ? stats : "      ",
	    gbl_timers[i].name);
	info[count].data[127] = '\0';
	count++;
      }
      if( (gbl_timers[i].flags & LOG_FLAG) && TRACE_ON(history)) {
	DPRINTF("%s %s %s\n", time,
	    (gbl_timers[i].flags & HISTORY_FLAG) ? stats : "      ",
	    gbl_timers[i].name);
      }
      if( (flags & RESET_FLAG) && (gbl_timers[i].flags & RESET_FLAG) ) {
	timer_reset( &gbl_timers[i] );
      }
    }
  }
  return count;
}

int wine_query_timers(query_info *info, int size, unsigned int flags)
{
  int i,j,idx, count=0;
  float frame_time;
  char time[20], percent[20], stats[40];

  /* Have we been initialized with any timers yet? */
  if( gbl_timers == NULL ) return 0;
  
  timer_stop( &gbl_timers[0] );
  frame_time = (float)gbl_timers[0].mean / HIST_SIZE;
  if( size > 0 ) {
    sprintf(info[count].data, "%8ld (%6lu)  Total time", 
	gbl_timers[0].total, gbl_timers[0].mean / HIST_SIZE );
    count++;
  }
  if( TRACE_ON(history) && (gbl_timers[0].flags & LOG_FLAG) ) {
    DPRINTF("%8ld (%6lu)  Total time\n", 
	gbl_timers[0].total, gbl_timers[0].mean / HIST_SIZE );
  }

  /* compute the group times - the timers are sorted by name, so aggregates
   * are first, followed by sub-groups, etc, to the deepest level.
   * We must total all sub-groups before moving to the higher level, and if
   * we traverse the group list backwards it all works out for us */
  for( i=last_used_timer-1; i>0; i-- ) {
    timer *t = &gbl_timers[gbl_timer_order[i]];
    for( j=0; j<t->group_count; j++ ) {
      timer_increment(t, gbl_timers[t->group_refs[j]].total);
    } 
  }

  /* prep the display - in sorted order */
  for( idx=1; idx<last_used_timer; idx++ ) {
    i = gbl_timer_order[idx];
    if( !(gbl_timers[i].flags & COUNTER_FLAG) ) {
      sprintf(time, "%8lu", gbl_timers[i].total);
      sprintf(stats, "(%6lu)", gbl_timers[i].mean / HIST_SIZE);
      sprintf(percent, "%5.2f%%", ((float)gbl_timers[i].mean/HIST_SIZE)/frame_time *100.f);
      
      if( (gbl_timers[i].flags & HUD_FLAG) && (count < size)) {
	snprintf(info[count].data, 128, "%s %s %s %s", time,
	    (gbl_timers[i].flags & HISTORY_FLAG) ? stats : "      ",
	    (gbl_timers[i].flags & PERCENT_FLAG) ? percent : "      ", 
	    gbl_timers[i].name);
	info[count].data[127] = '\0';
	count++;
      }
      if( (gbl_timers[i].flags & LOG_FLAG) && TRACE_ON(history) ) {
	DPRINTF("%s %s %s %s\n", time,
	    (gbl_timers[i].flags & HISTORY_FLAG) ? stats : "      ",
	    (gbl_timers[i].flags & PERCENT_FLAG) ? percent : "      ", 
	    gbl_timers[i].name);
      }
      if( (flags & RESET_FLAG) && (gbl_timers[i].flags & RESET_FLAG) ) {
	timer_reset( &gbl_timers[i] );
      }
    }
  }

  if( flags & RESET_FLAG ) timer_reset( &gbl_timers[0] );
  timer_start( &gbl_timers[0] );
  return count;
}


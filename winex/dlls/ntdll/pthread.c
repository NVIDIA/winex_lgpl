/*
 * pthread emulation for re-entrant libcs
 *
 * We can't use pthreads directly, so why not let libcs
 * that want pthreads use Wine's own threading instead...
 *
 * Copyright 1999 Ove K�ven
 */

#include "config.h"

#ifndef USE_PTHREADS

#define _GNU_SOURCE /* we may need to override some GNU extensions */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dlfcn.h>

#include "winbase.h"
#include "thread.h"
#include "winternl.h"

#if defined(HAVE_MUTEX_M_KIND)
#define MUTEX_KIND(m) m->__m_kind
#elif defined(HAVE_MUTEX_KIND)
#define MUTEX_KIND(m) m->__data.__kind
#else
#error "Missing pthread_mutex_t kind field."
#endif

static int init_done;
static void create_atfork_cs(void);
static void grab_libc(void);

void PTHREAD_init_done(void)
{
    create_atfork_cs(); /* Must be called before setting init_done */
    init_done = 1;
    grab_libc();
}

/* Currently this probably works only for glibc2,
 * which checks for the presence of double-underscore-prepended
 * pthread primitives, and use them if available.
 * If they are not available, the libc defaults to
 * non-threadsafe operation (not good). */

#if defined(__GLIBC__)
#include <pthread.h>
#include <signal.h>

#ifdef NEED_UNDERSCORE_PREFIX
# define PREFIX "_"
#else
# define PREFIX
#endif

#define PSTR(str) PREFIX #str

/* adapt as necessary (a construct like this is used in glibc sources) */
#define strong_alias(orig, alias) \
 asm(".globl " PSTR(alias) "\n" \
     "\t.set " PSTR(alias) "," PSTR(orig))

/* strong_alias does not work on external symbols (.o format limitation?),
 * so for those, we need to use the pogo stick */
#if defined(__i386__) && !defined(__PIC__)
/* FIXME: PIC */
#define jump_alias(orig, alias) \
 asm(".globl " PSTR(alias) "\n" \
     "\t.type " PSTR(alias) ",@function\n" \
     PSTR(alias) ":\n" \
     "\tjmp " PSTR(orig))
#endif

static void *libc_handle = NULL;
static pid_t (*libc_fork)(void);
static int (*libc_sigaction)(int signum,
                             const struct sigaction *act,
                             struct sigaction *oldact);

/* NOTE: This is a truly extremely incredibly ugly hack!
 * But it does seem to work... */

/* assume that pthread_mutex_t has room for at least one pointer,
 * and hope that the users of pthread_mutex_t considers it opaque
 * (never checks what's in it)
 * also: assume that static initializer sets pointer to NULL
 */
typedef struct {
  CRITICAL_SECTION *critsect;
} *wine_mutex;

/* see wine_mutex above for comments */
typedef struct {
  RTL_RWLOCK *lock;
} *wine_rwlock;

typedef struct _wine_cleanup {
  void (*routine)(void *);
  void *arg;
} *wine_cleanup;

typedef const void *key_data;

#define FIRST_KEY 0
#define MAX_KEYS 16 /* libc6 doesn't use that many, but... */

#ifdef NO_TRACE_MSGS
#define P_OUTPUT(stuff)
#else
#define P_OUTPUT(stuff) write(2,stuff,strlen(stuff))
#endif

/* #define P_TRACE(stuff) P_OUTPUT(stuff) */
#define P_TRACE(stuff)

static void* init_tsd[4];
static int libc_internal_tsd_set(int key, const void *pointer);

static void grab_libc(void)
{
  unsigned u;
#ifdef FIND_LIBC
  void *libc_open;
  Dl_info info;
#endif /* FIND_LIBC */

  if (libc_handle) return;

  for (u=0; u<4; u++)
    libc_internal_tsd_set(u, init_tsd[u]);

#ifdef FIND_LIBC
  /* to find the real name of libc, we can either detect it with configure
   * and compile the name into wine, or detect it at runtime, like this */
  libc_open = dlsym(RTLD_NEXT, "fopen");
  dladdr(libc_open, &info); /* <== GNU extension */

  /* now we can grab a libc handle */
  libc_handle = dlopen(info.dli_fname, RTLD_LAZY);
#else
  /* we may not need the real libc handle if we just grab the symbols
   * as early as possible (before x11drv is loaded) */
  libc_handle = RTLD_NEXT;
#endif /* FIND_LIBC */

  /* and thus the entry points we need */
  libc_fork = dlsym(libc_handle, "fork");
  libc_sigaction = dlsym(libc_handle, "sigaction");
}

void __pthread_initialize(void)
{
  P_TRACE("pthread_init\n");
}

struct pthread_thread_init {
	 void* (*start_routine)(void*);
	 void* arg;
};

static DWORD CALLBACK pthread_thread_start(LPVOID data)
{
  struct pthread_thread_init init = *(struct pthread_thread_init*)data;
  HeapFree(GetProcessHeap(),0,data);
  return (DWORD)init.start_routine(init.arg);
}

int pthread_create(pthread_t* thread, const pthread_attr_t* attr, void*
        (*start_routine)(void *), void* arg)
{
  HANDLE hThread;
  struct pthread_thread_init* idata = HeapAlloc(GetProcessHeap(), 0,
		sizeof(struct pthread_thread_init));

  P_TRACE("pthread_create\n");

  if (!idata){
    P_OUTPUT("FIXME: pthread_create, HeapAlloc failed, expect badness\n");
  }

  idata->start_routine = start_routine;
  idata->arg = arg;
  hThread = CreateThread(NULL, 0, pthread_thread_start, idata, 0, thread);

  if(hThread)
    CloseHandle(hThread);
  else
  {
    HeapFree(GetProcessHeap(),0,idata); /* free idata struct on failure */
    return EAGAIN;
  }

  return 0;
}

int pthread_cancel(pthread_t thread)
{
  HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, thread);

  P_TRACE("pthread_cancel\n");

  if(!TerminateThread(hThread, 0))
  {
    CloseHandle(hThread);
    return EINVAL;      /* return error */
  }

  CloseHandle(hThread);

  return 0;             /* return success */
}

int pthread_join(pthread_t thread, void **value_ptr)
{
  HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, thread);

  P_TRACE("pthread_join\n");

  WaitForSingleObject(hThread, INFINITE);
  if(!GetExitCodeThread(hThread, (LPDWORD)value_ptr))
  {
    CloseHandle(hThread);
    return EINVAL; /* FIXME: make this more correctly match */
  }                /* windows errors */

  CloseHandle(hThread);
  return 0;
}

/*FIXME: not sure what to do with this one... */
int pthread_detach(pthread_t thread)
{
  P_OUTPUT("FIXME:pthread_detach\n");
  return 0;
}

/* FIXME: we have no equivalents in win32 for the policys */
/* so just keep this as a stub */
int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy)
{
  P_OUTPUT("FIXME:pthread_attr_setschedpolicy\n");
  return 0;
}

/* FIXME: no win32 equivalent for scope */
int pthread_attr_setscope(pthread_attr_t *attr, int scope)
{
  P_OUTPUT("FIXME:pthread_attr_setscope\n");
  return 0; /* return success */
}

/* FIXME: no win32 equivalent for schedule param */
int pthread_attr_setschedparam(pthread_attr_t *attr,
    const struct sched_param *param)
{
  P_OUTPUT("FIXME:pthread_attr_setschedparam\n");
  return 0; /* return success */
}

int __pthread_once(pthread_once_t *once_control, void (*init_routine)(void))
{
  static pthread_once_t the_once = PTHREAD_ONCE_INIT;
  LONG once_now = *(LONG *)&the_once;

  P_TRACE("pthread_once\n");

  if (InterlockedCompareExchange((LONG*)once_control, once_now+1, once_now) == once_now)
    (*init_routine)();
  return 0;
}
strong_alias(__pthread_once, pthread_once);

void __pthread_kill_other_threads_np(void)
{
    /* we don't need to do anything here */
}
strong_alias(__pthread_kill_other_threads_np, pthread_kill_other_threads_np);

/***** atfork *****/

#define MAX_ATFORK 8  /* libc doesn't need that many anyway */

static CRITICAL_SECTION atfork_section;
typedef void (*atfork_handler)();
static atfork_handler atfork_prepare[MAX_ATFORK];
static atfork_handler atfork_parent[MAX_ATFORK];
static atfork_handler atfork_child[MAX_ATFORK];
static int atfork_count;

static void create_atfork_cs(void)
{
  InitializeCriticalSection( &atfork_section );
  CRITICAL_SECTION_NAME( &atfork_section, "atfork_section" );
}

int __pthread_atfork(void (*prepare)(void),
		     void (*parent)(void),
		     void (*child)(void))
{
    P_TRACE("pthread_atfork\n");

    if (init_done) EnterCriticalSection( &atfork_section );
    assert( atfork_count < MAX_ATFORK );
    atfork_prepare[atfork_count] = prepare;
    atfork_parent[atfork_count] = parent;
    atfork_child[atfork_count] = child;
    atfork_count++;
    if (init_done) LeaveCriticalSection( &atfork_section );
    return 0;
}
strong_alias(__pthread_atfork, pthread_atfork);

pid_t __fork(void)
{
    pid_t pid;
    int i;

    P_TRACE("fork\n");

    if (init_done) EnterCriticalSection( &atfork_section );
    grab_libc();
    /* prepare handlers are called in reverse insertion order */
    for (i = atfork_count - 1; i >= 0; i--) if (atfork_prepare[i]) atfork_prepare[i]();
    if (!(pid = (*libc_fork)()))
    {
        if (init_done) InitializeCriticalSection( &atfork_section );
        for (i = 0; i < atfork_count; i++) if (atfork_child[i]) atfork_child[i]();
    }
    else
    {
        for (i = 0; i < atfork_count; i++) if (atfork_parent[i]) atfork_parent[i]();
        if (init_done) LeaveCriticalSection( &atfork_section );
    }
    return pid;
}
strong_alias(__fork, fork);

/***** MUTEXES *****/

int __pthread_mutex_init(pthread_mutex_t *mutex,
                        const pthread_mutexattr_t *mutexattr)
{
  P_TRACE("pthread_mutex_init\n");

  /* glibc has a tendency to initialize mutexes very often, even
     in situations where they are not really used later on.

     As for us, initializing a mutex is very expensive, we postpone
     the real initialization until the time the mutex is first used. */

  ((wine_mutex)mutex)->critsect = NULL;
  return 0;
}
strong_alias(__pthread_mutex_init, pthread_mutex_init);

static void mutex_real_init( pthread_mutex_t *mutex )
{
  CRITICAL_SECTION *critsect = HeapAlloc(GetProcessHeap(), 0, sizeof(CRITICAL_SECTION));
  if (!critsect)
  {
     P_OUTPUT("FIXME: mutex_real_init, HeapAlloc failed, expect badness\n");
  }

  InitializeCriticalSection(critsect);

  if (InterlockedCompareExchangePointer((void**)&(((wine_mutex)mutex)->critsect),critsect,NULL) != NULL) {
    /* too late, some other thread already did it */
    DeleteCriticalSection(critsect);
    HeapFree(GetProcessHeap(), 0, critsect);
  }
}

int __pthread_mutex_lock(pthread_mutex_t *mutex)
{
  CRITICAL_SECTION *cs;

  P_TRACE("pthread_mutex_lock\n");

  if (!init_done) return 0;
  if (!((wine_mutex)mutex)->critsect)
    mutex_real_init( mutex );

  cs = ((wine_mutex)mutex)->critsect;
  EnterCriticalSection(cs);
  if (MUTEX_KIND (mutex) != PTHREAD_MUTEX_RECURSIVE_NP) {
    if (cs->RecursionCount > 1) {
      /* we're supposed to deadlock if it's PTHREAD_MUTEX_FAST_NP,
       * but I won't bother implementing that.
       * Just pretend it's always PTHREAD_MUTEX_ERRORCHECK_NP. */
      LeaveCriticalSection(cs);
      return EDEADLK;
    }
  }
  return 0;
}
strong_alias(__pthread_mutex_lock, pthread_mutex_lock);

int __pthread_mutex_trylock(pthread_mutex_t *mutex)
{
  CRITICAL_SECTION *cs;

  P_TRACE("pthread_mutex_trylock\n");

  if (!init_done) return 0;
  if (!((wine_mutex)mutex)->critsect)
    mutex_real_init( mutex );

  cs = ((wine_mutex)mutex)->critsect;
  if (!TryEnterCriticalSection(cs)) {
    return EBUSY;
  }
  if (MUTEX_KIND (mutex) != PTHREAD_MUTEX_RECURSIVE_NP) {
    if (cs->RecursionCount > 1) {
      LeaveCriticalSection(cs);
      return EBUSY;
    }
  }
  return 0;
}
strong_alias(__pthread_mutex_trylock, pthread_mutex_trylock);

int __pthread_mutex_unlock(pthread_mutex_t *mutex)
{
  CRITICAL_SECTION *cs;

  P_TRACE("pthread_mutex_unlock\n");

  if (!((wine_mutex)mutex)->critsect) return 0;
  cs = ((wine_mutex)mutex)->critsect;
  if (cs->OwningThread != GetCurrentThreadId()) {
      if (MUTEX_KIND (mutex) == PTHREAD_MUTEX_ERRORCHECK_NP)
      return EPERM;
    return 0;
  }
  LeaveCriticalSection(cs);
  return 0;
}
strong_alias(__pthread_mutex_unlock, pthread_mutex_unlock);

int __pthread_mutex_destroy(pthread_mutex_t *mutex)
{
  P_TRACE("pthread_mutex_destroy\n");

  if (!((wine_mutex)mutex)->critsect) return 0;
  if (((wine_mutex)mutex)->critsect->RecursionCount) {
#if 0 /* there seems to be a bug in libc6 that makes this a bad idea */
    return EBUSY;
#else
    while (((wine_mutex)mutex)->critsect->RecursionCount)
      LeaveCriticalSection(((wine_mutex)mutex)->critsect);
#endif /* force off */
  }
  DeleteCriticalSection(((wine_mutex)mutex)->critsect);
  HeapFree(GetProcessHeap(), 0, ((wine_mutex)mutex)->critsect);
  return 0;
}
strong_alias(__pthread_mutex_destroy, pthread_mutex_destroy);


/***** MUTEX ATTRIBUTES *****/
/* just dummies, since critical sections are always recursive */

int __pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
  return 0;
}
strong_alias(__pthread_mutexattr_init, pthread_mutexattr_init);

int __pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
  return 0;
}
strong_alias(__pthread_mutexattr_destroy, pthread_mutexattr_destroy);

int __pthread_mutexattr_setkind_np(pthread_mutexattr_t *attr, int kind)
{
  return 0;
}
strong_alias(__pthread_mutexattr_setkind_np, pthread_mutexattr_setkind_np);

int __pthread_mutexattr_getkind_np(pthread_mutexattr_t *attr, int *kind)
{
  *kind = PTHREAD_MUTEX_RECURSIVE_NP;
  return 0;
}
strong_alias(__pthread_mutexattr_getkind_np, pthread_mutexattr_getkind_np);

int __pthread_mutexattr_settype(pthread_mutexattr_t *attr, int kind)
{
  return 0;
}
strong_alias(__pthread_mutexattr_settype, pthread_mutexattr_settype);

int __pthread_mutexattr_gettype(pthread_mutexattr_t *attr, int *kind)
{
  *kind = PTHREAD_MUTEX_RECURSIVE_NP;
  return 0;
}
strong_alias(__pthread_mutexattr_gettype, pthread_mutexattr_gettype);


/***** THREAD-SPECIFIC VARIABLES (KEYS) *****/

int __pthread_key_create(pthread_key_t *key, void (*destr_function)(void *))
{
  static LONG keycnt = FIRST_KEY;

  P_TRACE("pthread_key_create\n");

  *key = InterlockedExchangeAdd(&keycnt, 1);
  return 0;
}
strong_alias(__pthread_key_create, pthread_key_create);

int __pthread_key_delete(pthread_key_t key)
{
  P_TRACE("pthread_key_delete\n");

  return 0;
}
strong_alias(__pthread_key_delete, pthread_key_delete);

int __pthread_setspecific(pthread_key_t key, const void *pointer)
{
  TEB *teb;

  P_TRACE("pthread_setspecific\n");

  teb = NtCurrentTeb();
  if (!teb->pthread_data) {
    teb->pthread_data = calloc(MAX_KEYS,sizeof(key_data));
  }
  ((key_data*)(teb->pthread_data))[key] = pointer;
  return 0;
}
strong_alias(__pthread_setspecific, pthread_setspecific);

void *__pthread_getspecific(pthread_key_t key)
{
  TEB *teb;

  P_TRACE("pthread_getspecific\n");

  teb = NtCurrentTeb();
  if (!teb) return NULL;
  if (!teb->pthread_data) return NULL;
  return (void *)(((key_data*)(teb->pthread_data))[key]);
}
strong_alias(__pthread_getspecific, pthread_getspecific);

static int libc_internal_tsd_set(int key, const void *pointer)
{
  P_TRACE("libc_internal_tsd_set\n");
  if (key >= 4) {
    P_OUTPUT("too few libc keys\n");
    return 0;
  }
  if (init_done) {
    NtCurrentTeb()->pthread_data2[key] = (void*)pointer;
    return 0;
  }
  else {
    init_tsd[key] = (void*)pointer;
    return 0;
  }
}
int (*__libc_internal_tsd_set)(int key, const void *pointer) = libc_internal_tsd_set;

static void *libc_internal_tsd_get(int key)
{
  P_TRACE("libc_internal_tsd_get\n");
  if (key >= 4) {
    P_OUTPUT("too few libc keys\n");
    return NULL;
  }
  if (init_done) {
    return NtCurrentTeb()->pthread_data2[key];
  }
  else {
    return init_tsd[key];
  }
}
void *(*__libc_internal_tsd_get)(int key) = libc_internal_tsd_get;


/***** "EXCEPTION" FRAMES *****/
/* not implemented right now */

void _pthread_cleanup_push(struct _pthread_cleanup_buffer *buffer, void (*routine)(void *), void *arg)
{
  ((wine_cleanup)buffer)->routine = routine;
  ((wine_cleanup)buffer)->arg = arg;
}

void _pthread_cleanup_pop(struct _pthread_cleanup_buffer *buffer, int execute)
{
  if (execute) (*(((wine_cleanup)buffer)->routine))(((wine_cleanup)buffer)->arg);
}

void _pthread_cleanup_push_defer(struct _pthread_cleanup_buffer *buffer, void (*routine)(void *), void *arg)
{
  _pthread_cleanup_push(buffer, routine, arg);
}

void _pthread_cleanup_pop_restore(struct _pthread_cleanup_buffer *buffer, int execute)
{
  _pthread_cleanup_pop(buffer, execute);
}


/***** CONDITIONS *****/
/* not implemented right now */

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *cond_attr)
{
  P_OUTPUT("FIXME:pthread_cond_init\n");
  return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
  P_OUTPUT("FIXME:pthread_cond_destroy\n");
  return 0;
}

int pthread_cond_signal(pthread_cond_t *cond)
{
  P_OUTPUT("FIXME:pthread_cond_signal\n");
  return 0;
}

int pthread_cond_broadcast(pthread_cond_t *cond)
{
  P_OUTPUT("FIXME:pthread_cond_broadcast\n");
  return 0;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
  P_OUTPUT("FIXME:pthread_cond_wait\n");
  return 0;
}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime)
{
  P_OUTPUT("FIXME:pthread_cond_timedwait\n");
  return 0;
}

/**** CONDITION ATTRIBUTES *****/
/* not implemented right now */

int pthread_condattr_init(pthread_condattr_t *attr)
{
  return 0;
}

int pthread_condattr_destroy(pthread_condattr_t *attr)
{
  return 0;
}

#if (__GLIBC__ == 2) && (__GLIBC_MINOR__ >= 2)
/***** READ-WRITE LOCKS *****/

static void rwlock_real_init(pthread_rwlock_t *rwlock)
{
  RTL_RWLOCK *lock = HeapAlloc(GetProcessHeap(), 0, sizeof(RTL_RWLOCK));
  if (!lock) {
    P_OUTPUT("FIXME: rwlock_real_init, HeapAlloc failed, expect badness\n");
  }

  RtlInitializeResource(lock);

  if (InterlockedCompareExchangePointer((void**)&(((wine_rwlock)rwlock)->lock),lock,NULL) != NULL) {
    /* too late, some other thread already did it */
    RtlDeleteResource(lock);
    HeapFree(GetProcessHeap(), 0, lock);
  }
}

int __pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *rwlock_attr)
{
  P_TRACE("pthread_rwlock_init\n");

  ((wine_rwlock)rwlock)->lock = NULL;
  return 0;
}
strong_alias(__pthread_rwlock_init, pthread_rwlock_init);

int __pthread_rwlock_destroy(pthread_rwlock_t *rwlock)
{
  P_TRACE("pthread_rwlock_destroy\n");

  if (!((wine_rwlock)rwlock)->lock) return 0;
  RtlDeleteResource(((wine_rwlock)rwlock)->lock);
  HeapFree(GetProcessHeap(), 0, ((wine_rwlock)rwlock)->lock);
  return 0;
}
strong_alias(__pthread_rwlock_destroy, pthread_rwlock_destroy);

int __pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)
{
  P_TRACE("pthread_rwlock_rdlock\n");

  if (!init_done) return 0;
  if (!((wine_rwlock)rwlock)->lock)
    rwlock_real_init( rwlock );

  while(TRUE)
    if (RtlAcquireResourceShared(((wine_rwlock)rwlock)->lock, TRUE))
      return 0;
}
strong_alias(__pthread_rwlock_rdlock, pthread_rwlock_rdlock);

int __pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock)
{
  P_TRACE("pthread_rwlock_tryrdlock\n");

  if (!init_done) return 0;
  if (!((wine_rwlock)rwlock)->lock)
    rwlock_real_init( rwlock );

  if (!RtlAcquireResourceShared(((wine_rwlock)rwlock)->lock, FALSE)) {
    errno = EBUSY;
    return -1;
  }
  return 0;
}
strong_alias(__pthread_rwlock_tryrdlock, pthread_rwlock_tryrdlock);

int __pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)
{
  P_TRACE("pthread_wrlock\n");

  if (!init_done) return 0;
  if (!((wine_rwlock)rwlock)->lock)
    rwlock_real_init( rwlock );

  while(TRUE)
    if (RtlAcquireResourceExclusive(((wine_rwlock)rwlock)->lock, TRUE))
      return 0;
}
strong_alias(__pthread_rwlock_wrlock, pthread_rwlock_wrlock);

int __pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock)
{
  P_TRACE("pthread_trywrlock\n");

  if (!init_done) return 0;
  if (!((wine_rwlock)rwlock)->lock)
    rwlock_real_init( rwlock );

  if (!RtlAcquireResourceExclusive(((wine_rwlock)rwlock)->lock, FALSE)) {
    errno = EBUSY;
    return -1;
  }
  return 0;
}
strong_alias(__pthread_rwlock_trywrlock, pthread_rwlock_trywrlock);

int __pthread_rwlock_unlock(pthread_rwlock_t *rwlock)
{
  P_TRACE("pthread_rwlock_unlock\n");

  if (!((wine_rwlock)rwlock)->lock) return 0;
  RtlReleaseResource( ((wine_rwlock)rwlock)->lock );
  return 0;
}
strong_alias(__pthread_rwlock_unlock, pthread_rwlock_unlock);

/**** READ-WRITE LOCK ATTRIBUTES *****/
/* not implemented right now */

int pthread_rwlockattr_init(pthread_rwlockattr_t *attr)
{
  return 0;
}

int __pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr)
{
  return 0;
}
strong_alias(__pthread_rwlockattr_destroy, pthread_rwlockattr_destroy);

int pthread_rwlockattr_getkind_np(const pthread_rwlockattr_t *attr, int *pref)
{
  *pref = 0;
  return 0;
}

int pthread_rwlockattr_setkind_np(pthread_rwlockattr_t *attr, int pref)
{
  return 0;
}
#endif /* glibc 2.2 */

/***** MISC *****/

pthread_t pthread_self(void)
{
  P_TRACE("pthread_self\n");
  return (pthread_t)NtCurrentTeb()->pthread_id;
}

int pthread_equal(pthread_t thread1, pthread_t thread2)
{
  P_TRACE("pthread_equal\n");
  return (DWORD)thread1 == (DWORD)thread2;
}

void pthread_exit(void *retval)
{
  P_TRACE("pthread_exit\n");
  /* FIXME: pthread cleanup */
  ExitThread((DWORD)retval);
}

int pthread_setcanceltype(int type, int *oldtype)
{
  if (oldtype) *oldtype = PTHREAD_CANCEL_ASYNCHRONOUS;
  return 0;
}

/***** ANTI-OVERRIDES *****/
/* pthreads tries to override these, point them back to libc */

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
  P_TRACE("sigaction\n");
  grab_libc();
  return (*libc_sigaction)(signum, act, oldact);
}

#else /* __GLIBC__ */

static void grab_libc(void)
{
}

static void create_atfork_cs(void)
{
}

#endif /* __GLIBC__ */

#endif /* USE_PTHREADS */

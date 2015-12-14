/*
 * RPCSS named pipe server
 *
 * Copyright (C) 2002 Greg Turner
 */

#include <assert.h>

#include "rpcss.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole);

static HANDLE np_server_end;
static HANDLE np_server_work_event;
static CRITICAL_SECTION np_server_cs;
static LONG srv_thread_count;
static BOOL server_live;

LONG RPCSS_SrvThreadCount(void)
{
  return srv_thread_count;
}

BOOL RPCSS_UnBecomePipeServer(void)
{
  BOOL rslt = TRUE;
  DWORD wait_result;
  HANDLE master_mutex = RPCSS_GetMasterMutex();

  WINE_TRACE("\n");

  wait_result = WaitForSingleObject(master_mutex, MASTER_MUTEX_TIMEOUT);

  switch (wait_result) {
    case WAIT_ABANDONED: /* ? */
    case WAIT_OBJECT_0:
      /* we have ownership */
      break;
    case WAIT_FAILED:
    case WAIT_TIMEOUT:
    default:
      WINE_ERR("This should never happen: couldn't enter mutex.\n");
      /* this is totally unacceptable.  no graceful out exists */
      assert(FALSE);
  }

  /* now that we have the master mutex, we can safely stop
     listening on the pipe.  Before we proceed, we do a final
     check that it's OK to shut down to ensure atomicity */

  if (!RPCSS_ReadyToDie())
    rslt = FALSE;
  else {
    WINE_TRACE("shutting down pipe.\n");
    server_live = FALSE;
    if (!CloseHandle(np_server_end))
      WINE_WARN("Failed to close named pipe.\n");
    if (!CloseHandle(np_server_work_event))
      WINE_WARN("Failed to close the event handle.\n");
    DeleteCriticalSection(&np_server_cs);
  }

  if (!ReleaseMutex(master_mutex))
    WINE_ERR("Unable to leave master mutex!??\n");

  return rslt;
}

void RPCSS_ServerProcessRANMessage(PRPCSS_NP_MESSAGE pMsg, PRPCSS_NP_REPLY pReply)
{
  WINE_TRACE("\n");
  /* we do absolutely nothing, but on the server end,
     the lazy timeout is reset as a result of our connection. */
  RPCSS_SetMaxLazyTimeout(pMsg->message.ranmsg.timeout);
  RPCSS_SetLazyTimeRemaining(RPCSS_GetMaxLazyTimeout());
  pReply->as_uint = 0;
}

void RPCSS_ServerProcessREGISTEREPMessage(PRPCSS_NP_MESSAGE pMsg, PRPCSS_NP_REPLY pReply,
  char *vardata)
{
  WINE_TRACE("\n");

  RPCSS_RegisterRpcEndpoints(
    pMsg->message.registerepmsg.iface,
    pMsg->message.registerepmsg.object_count,
    pMsg->message.registerepmsg.binding_count,
    pMsg->message.registerepmsg.no_replace,
    vardata,
    pMsg->vardata_payload_size
  );

  /* no reply */
  pReply->as_uint = 0;
}

void RPCSS_ServerProcessUNREGISTEREPMessage(PRPCSS_NP_MESSAGE pMsg,
  PRPCSS_NP_REPLY pReply, char *vardata)
{
  WINE_TRACE("\n");

  RPCSS_UnregisterRpcEndpoints(
    pMsg->message.unregisterepmsg.iface,
    pMsg->message.unregisterepmsg.object_count,
    pMsg->message.unregisterepmsg.binding_count,
    vardata,
    pMsg->vardata_payload_size
  );

  /* no reply */
  pReply->as_uint = 0;
}

void RPCSS_ServerProcessRESOLVEEPMessage(PRPCSS_NP_MESSAGE pMsg,
  PRPCSS_NP_REPLY pReply, char *vardata)
{
  WINE_TRACE("\n");

  /* for now, reply is placed into *pReply.as_string, on success, by RPCSS_ResolveRpcEndpoints */
  ZeroMemory(pReply->as_string, MAX_RPCSS_NP_REPLY_STRING_LEN);
  RPCSS_ResolveRpcEndpoints(
    pMsg->message.resolveepmsg.iface,
    pMsg->message.resolveepmsg.object,
    vardata,
    pReply->as_string
  );
}

void RPCSS_ServerProcessMessage(PRPCSS_NP_MESSAGE pMsg, PRPCSS_NP_REPLY pReply, char *vardata)
{
  WINE_TRACE("\n");
  switch (pMsg->message_type) {
    case RPCSS_NP_MESSAGE_TYPEID_RANMSG:
      RPCSS_ServerProcessRANMessage(pMsg, pReply);
      break;
    case RPCSS_NP_MESSAGE_TYPEID_REGISTEREPMSG:
      RPCSS_ServerProcessREGISTEREPMessage(pMsg, pReply, vardata);
      break;
    case RPCSS_NP_MESSAGE_TYPEID_UNREGISTEREPMSG:
      RPCSS_ServerProcessUNREGISTEREPMessage(pMsg, pReply, vardata);
      break;
    case RPCSS_NP_MESSAGE_TYPEID_RESOLVEEPMSG:
      RPCSS_ServerProcessRESOLVEEPMessage(pMsg, pReply, vardata);
      break;
    default:
      WINE_ERR("Message type unknown!!  No action taken.\n");
  }
}

/* each message gets its own thread.  this is it. */
VOID HandlerThread(LPVOID lpvPipeHandle)
{
  RPCSS_NP_MESSAGE msg, vardata_payload_msg;
  char *c, *vardata = NULL;
  RPCSS_NP_REPLY reply;
  DWORD bytesread, written;
  BOOL success, had_payload = FALSE;
  HANDLE mypipe;

  mypipe = (HANDLE) lpvPipeHandle;

  WINE_TRACE("mypipe: %p\n", mypipe);

  success = ReadFile(
    mypipe,                   /* pipe handle */
    (char *) &msg,            /* message buffer */
    sizeof(RPCSS_NP_MESSAGE), /* message buffer size */
    &bytesread,               /* receives number of bytes read */
    NULL                      /* not overlapped */
  );

  if (msg.vardata_payload_size) {
    had_payload = TRUE;
    /* this fudge space allows us not to worry about exceeding the buffer space
       on the last read */
    vardata = LocalAlloc(LPTR, (msg.vardata_payload_size) + VARDATA_PAYLOAD_BYTES);
    if (!vardata) {
      WINE_ERR("vardata memory allocation failure.\n");
      success = FALSE;
    } else {
      for ( c = vardata; (c - vardata) < msg.vardata_payload_size;
            c += VARDATA_PAYLOAD_BYTES) {
        success = ReadFile(
	  mypipe,
	  (char *) &vardata_payload_msg,
	  sizeof(RPCSS_NP_MESSAGE),
	  &bytesread,
	  NULL
	);
	if ( (!success) || (bytesread != sizeof(RPCSS_NP_MESSAGE)) ||
             (vardata_payload_msg.message_type != RPCSS_NP_MESSAGE_TYPEID_VARDATAPAYLOADMSG) ) {
	  WINE_ERR("vardata payload read failure! (s=%s,br=%ld,exp_br=%d,mt=%u,mt_exp=%u\n",
	    success ? "TRUE" : "FALSE", bytesread, sizeof(RPCSS_NP_MESSAGE),
	    vardata_payload_msg.message_type, RPCSS_NP_MESSAGE_TYPEID_VARDATAPAYLOADMSG);
	  success = FALSE;
	  break;
	}
        CopyMemory(c, vardata_payload_msg.message.vardatapayloadmsg.payload, VARDATA_PAYLOAD_BYTES);
	WINE_TRACE("payload read.\n");
      }
    }
  }

  if (success && (bytesread == sizeof(RPCSS_NP_MESSAGE))) {
    WINE_TRACE("read success.\n");
    /* process the message and send a reply, serializing requests. */
    EnterCriticalSection(&np_server_cs);
    WINE_TRACE("processing message.\n");
    RPCSS_ServerProcessMessage(&msg, &reply, vardata);
    LeaveCriticalSection(&np_server_cs);

    if (had_payload) LocalFree(vardata);

    WINE_TRACE("message processed, sending reply....\n");

    success = WriteFile(
      mypipe,                 /* pipe handle */
      (char *) &reply,        /* reply buffer */
      sizeof(RPCSS_NP_REPLY), /* reply buffer size */
      &written,               /* receives number of bytes written */
      NULL                    /* not overlapped */
    );

    if ( (!success) || (written != sizeof(RPCSS_NP_REPLY)) )
      WINE_WARN("Message reply failed. (successs=%s, br=%ld, exp_br=%d)\n",
        success ? "TRUE" : "FALSE", written, sizeof(RPCSS_NP_REPLY));
    else
      WINE_TRACE("Reply sent successfully.\n");
  } else
    WINE_WARN("Message receipt failed.\n");

  FlushFileBuffers(mypipe);
  DisconnectNamedPipe(mypipe);
  CloseHandle(mypipe);
  InterlockedDecrement(&srv_thread_count);
}

VOID NPMainWorkThread(LPVOID ignored)
{
  BOOL connected;
  HANDLE hthread, master_mutex = RPCSS_GetMasterMutex();
  DWORD threadid, wait_result;

  WINE_TRACE("\n");

  while (server_live) {
    connected = ConnectNamedPipe(np_server_end, NULL) ?
      TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

    if (connected) {
      /* is "work" the act of connecting pipes, or the act of serving
         requests successfully?  for now I will make it the former. */
      if (!SetEvent(np_server_work_event))
        WINE_WARN("failed to signal np_server_work_event.\n");

      /* Create a thread for this client.  */
      InterlockedIncrement(&srv_thread_count);
      hthread = CreateThread(
        NULL,                      /* no security attribute */
        0,                         /* default stack size */
        (LPTHREAD_START_ROUTINE) HandlerThread,
        (LPVOID) np_server_end,    /* thread parameter */
        0,                         /* not suspended */
        &threadid                  /* returns thread ID  (not used) */
      );

      if (hthread) {
        WINE_TRACE("Spawned handler thread: %p\n", hthread);
        CloseHandle(hthread);

        /* for safety's sake, hold the mutex while we switch the pipe */

        wait_result = WaitForSingleObject(master_mutex, MASTER_MUTEX_TIMEOUT);

        switch (wait_result) {
          case WAIT_ABANDONED: /* ? */
          case WAIT_OBJECT_0:
            /* we have ownership */
            break;
          case WAIT_FAILED:
          case WAIT_TIMEOUT:
          default:
            /* huh? */
	    wait_result = WAIT_FAILED;
        }

        if (wait_result == WAIT_FAILED) {
          WINE_ERR("Couldn't enter master mutex.  Expect prolems.\n");
        } else {
	  /* now create a new named pipe instance to listen on */
          np_server_end = CreateNamedPipe(
            NAME_RPCSS_NAMED_PIPE,                                 /* pipe name */
            PIPE_ACCESS_DUPLEX,                                    /* pipe open mode */
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, /* pipe-specific modes */
            PIPE_UNLIMITED_INSTANCES,                              /* maximum instances */
            sizeof(RPCSS_NP_REPLY),                                /* output buffer size */
            sizeof(RPCSS_NP_MESSAGE),                              /* input buffer size */
            2000,                                                  /* time-out interval */
            NULL                                                   /* SD */
          );

          if (np_server_end == INVALID_HANDLE_VALUE) {
            WINE_ERR("Failed to recreate named pipe!\n");
            /* not sure what to do? */
            assert(FALSE);
          }

          if (!ReleaseMutex(master_mutex))
	    WINE_ERR("Uh oh.  Couldn't leave master mutex.  Expect deadlock.\n");
	}
      } else {
        WINE_ERR("Failed to spawn handler thread!\n");
        DisconnectNamedPipe(np_server_end);
        InterlockedDecrement(&srv_thread_count);
      }
    }
  }
  WINE_TRACE("Server thread shutdown.\n");
}

HANDLE RPCSS_NPConnect(void)
{
  HANDLE the_pipe = NULL;
  DWORD dwmode, wait_result;
  HANDLE master_mutex = RPCSS_GetMasterMutex();

  WINE_TRACE("\n");

  while (TRUE) {

    wait_result = WaitForSingleObject(master_mutex, MASTER_MUTEX_TIMEOUT);
    switch (wait_result) {
      case WAIT_ABANDONED:
      case WAIT_OBJECT_0:
        break;
      case WAIT_FAILED:
      case WAIT_TIMEOUT:
      default:
        WINE_ERR("This should never happen: couldn't enter mutex.\n");
        return (HANDLE) NULL;
    }

    /* try to open the client side of the named pipe. */
    the_pipe = CreateFileA(
      NAME_RPCSS_NAMED_PIPE,           /* pipe name */
      GENERIC_READ | GENERIC_WRITE,    /* r/w access */
      0,                               /* no sharing */
      NULL,                            /* no security attributes */
      OPEN_EXISTING,                   /* open an existing pipe */
      0,                               /* default attributes */
      NULL                             /* no template file */
    );

    if (the_pipe != INVALID_HANDLE_VALUE)
      break;

    if (GetLastError() != ERROR_PIPE_BUSY) {
      WINE_WARN("Unable to open named pipe %s (assuming unavailable).\n",
        wine_dbgstr_a(NAME_RPCSS_NAMED_PIPE));
      the_pipe = NULL;
      break;
    }

    WINE_WARN("Named pipe busy (will wait)\n");

    if (!ReleaseMutex(master_mutex))
      WINE_ERR("Failed to release master mutex.  Expect deadlock.\n");

    /* wait for the named pipe.  We are only
       willing to wait only 5 seconds.  It should be available /very/ soon. */
    if (! WaitNamedPipeA(NAME_RPCSS_NAMED_PIPE, MASTER_MUTEX_WAITNAMEDPIPE_TIMEOUT))
    {
      WINE_ERR("Named pipe unavailable after waiting.  Something is probably wrong.\n");
      return NULL;
    }

  }

  if (the_pipe) {
    dwmode = PIPE_READMODE_MESSAGE;
    /* SetNamedPipeHandleState not implemented ATM, but still seems to work somehow. */
    if (! SetNamedPipeHandleState(the_pipe, &dwmode, NULL, NULL))
      WINE_WARN("Failed to set pipe handle state\n");
  }

  if (!ReleaseMutex(master_mutex))
    WINE_ERR("Uh oh, failed to leave the RPC Master Mutex!\n");

  return the_pipe;
}

BOOL RPCSS_SendReceiveNPMsg(HANDLE np, PRPCSS_NP_MESSAGE msg, PRPCSS_NP_REPLY reply)
{
  DWORD count;

  WINE_TRACE("(np == %p, msg == %p, reply == %p)\n", np, msg, reply);

  if (! WriteFile(np, msg, sizeof(RPCSS_NP_MESSAGE), &count, NULL)) {
    WINE_ERR("write failed.\n");
    return FALSE;
  }

  if (count != sizeof(RPCSS_NP_MESSAGE)) {
    WINE_ERR("write count mismatch.\n");
    return FALSE;
  }

  if (! ReadFile(np, reply, sizeof(RPCSS_NP_REPLY), &count, NULL)) {
    WINE_ERR("read failed.\n");
    return FALSE;
  }

  if (count != sizeof(RPCSS_NP_REPLY)) {
    WINE_ERR("read count mismatch. got %ld, expected %u.\n", count, sizeof(RPCSS_NP_REPLY));
    return FALSE;
  }

  /* message execution was successful */
  return TRUE;
}

BOOL RPCSS_BecomePipeServer(void)
{
  RPCSS_NP_MESSAGE msg;
  RPCSS_NP_REPLY reply;
  BOOL rslt = TRUE;
  HANDLE client_handle, hthread, master_mutex = RPCSS_GetMasterMutex();
  DWORD threadid, wait_result;

  WINE_TRACE("\n");

  wait_result = WaitForSingleObject(master_mutex, MASTER_MUTEX_TIMEOUT);

  switch (wait_result) {
    case WAIT_ABANDONED: /* ? */
    case WAIT_OBJECT_0:
      /* we have ownership */
      break;
    case WAIT_FAILED:
    case WAIT_TIMEOUT:
    default:
      WINE_ERR("Couldn't enter master mutex.\n");
      return FALSE;
  }

  /* now we have the master mutex.  during this time we will
   *
   *   o check if an rpcss already listens on the pipe.  If so,
   *     we will tell it we were invoked, which will cause the
   *     other end to update its timeouts.  After, we just return
   *     false.
   *
   *   o otherwise, we establish the pipe for ourselves and get
   *     ready to listen on it
   */

  if ((client_handle = RPCSS_NPConnect()) != NULL) {
    msg.message_type = RPCSS_NP_MESSAGE_TYPEID_RANMSG;
    msg.message.ranmsg.timeout = RPCSS_GetMaxLazyTimeout();
    msg.vardata_payload_size = 0;
    if (!RPCSS_SendReceiveNPMsg(client_handle, &msg, &reply))
      WINE_ERR("Something is amiss: RPC_SendReceive failed.\n");
    rslt = FALSE;
  }
  if (rslt) {
    np_server_work_event = CreateEventA(NULL, FALSE, FALSE, "RpcNpServerWorkEvent");
    if (np_server_work_event == NULL) {
      /* dunno what we can do then */
      WINE_ERR("Unable to create the np_server_work_event\n");
      assert(FALSE);
    }
    CRITICAL_SECTION_DEFINE(&np_server_cs);

    np_server_end = CreateNamedPipe(
      NAME_RPCSS_NAMED_PIPE,                                   /* pipe name */
      PIPE_ACCESS_DUPLEX,                                      /* pipe open mode */
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,   /* pipe-specific modes */
      PIPE_UNLIMITED_INSTANCES,                                /* maximum number of instances */
      sizeof(RPCSS_NP_REPLY),                                  /* output buffer size */
      sizeof(RPCSS_NP_MESSAGE),                                /* input buffer size */
      2000,                                                    /* time-out interval */
      NULL                                                     /* SD */
    );

    if (np_server_end == INVALID_HANDLE_VALUE) {
      WINE_ERR("Failed to create named pipe!\n");
      DeleteCriticalSection(&np_server_cs);
      if (!CloseHandle(np_server_work_event)) /* we will leak the handle... */
        WINE_WARN("Failed to close np_server_work_event handle!\n");
      np_server_work_event = NULL;
      np_server_end = NULL;
      rslt = FALSE;
    }
  }

  server_live = rslt;

  if (rslt) {
    /* OK, now spawn the (single) server thread */
    hthread = CreateThread(
      NULL,                      /* no security attribute */
      0,                         /* default stack size */
      (LPTHREAD_START_ROUTINE) NPMainWorkThread,
      (LPVOID) NULL,             /* thread parameter */
      0,                         /* not suspended */
      &threadid                  /* returns thread ID  (not used) */
    );
    if (hthread) {
      WINE_TRACE("Created server thread.\n");
      CloseHandle(hthread);
    } else {
      WINE_ERR("Serious error: unable to create server thread!\n");
      if (!CloseHandle(np_server_work_event)) /* we will leak the handle... */
        WINE_WARN("Failed to close np_server_work_event handle!\n");
      if (!CloseHandle(np_server_end)) /* we will leak the handle... */
        WINE_WARN("Unable to close named pipe handle!\n");
      DeleteCriticalSection(&np_server_cs);
      np_server_end = NULL;
      np_server_work_event = NULL;
      rslt = FALSE;
      server_live = FALSE;
    }
  }
  if (!ReleaseMutex(master_mutex))
    WINE_ERR("Unable to leave master mutex!??\n");

  return rslt;
}

BOOL RPCSS_NPDoWork(void)
{
  DWORD waitresult = WaitForSingleObject(np_server_work_event, 1000);

  if (waitresult == WAIT_TIMEOUT)
    return FALSE;
  if (waitresult == WAIT_OBJECT_0)
    return TRUE;

  return FALSE;
}

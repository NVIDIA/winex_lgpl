

#ifndef _GDIPLUSINIT_H
#define _GDIPLUSINIT_H

enum DebugEventLevel
{
    DebugEventLevelFatal,
    DebugEventLevelWarning
};

typedef VOID (WINAPI *DebugEventProc)(enum DebugEventLevel, CHAR *);
typedef Status (WINAPI *NotificationHookProc)(ULONG_PTR *);
typedef void (WINAPI *NotificationUnhookProc)(ULONG_PTR);

struct GdiplusStartupInput
{
    UINT32 GdiplusVersion;
    DebugEventProc DebugEventCallback;
    BOOL SuppressBackgroundThread;
    BOOL SuppressExternalCodecs;

#ifdef __cplusplus
    GdiplusStartupInput(DebugEventProc debugEventCallback = NULL,
        BOOL suppressBackgroundThread = FALSE,
        BOOL suppressExternalCodecs = FALSE)
    {
        GdiplusVersion = 1;
        DebugEventCallback = debugEventCallback;
        SuppressBackgroundThread = suppressBackgroundThread;
        SuppressExternalCodecs = suppressExternalCodecs;
    }
#endif
};

struct GdiplusStartupOutput
{
    NotificationHookProc NotificationHook;
    NotificationUnhookProc NotificationUnhook;
};

#ifdef __cplusplus
extern "C" {
#endif

Status WINAPI GdiplusStartup(ULONG_PTR *, const struct GdiplusStartupInput *, struct GdiplusStartupOutput *);
void WINAPI GdiplusShutdown(ULONG_PTR);

#ifdef __cplusplus
}
#endif

#endif

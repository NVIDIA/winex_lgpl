/* external SDLDRV functions/typedefs.
 * use for code etc. thats not in dlls/SDLDRV
 * Copyright (c) 2007-2015 NVIDIA CORPORATION. All rights reserved.
 */

#ifndef __SDLDRV_EXT_H
#define __SDLDRV_EXT_H


/* Some typedefs for SDLDRV integration */
typedef enum
{
   kHiddenUINotUsed = 0,
   kHiddenUIStarting,
   kHiddenUIStopping,
   kHiddenUIRunning
} HiddenUICallBackState;

typedef void (*pfnMainThreadHiddenUICallback)(HiddenUICallBackState state);
typedef void (*SDLDRV_Hide3DView_Func)(int newWidth, int newHeight, pfnMainThreadHiddenUICallback uiCallback);
typedef void (*SDLDRV_Restore3DView_Func)();
typedef void (*SDLDRV_SwitchOutToBrowserURL_Func)(char *urlString);

extern SDLDRV_Hide3DView_Func SDLDRV_Hide3DView;
extern SDLDRV_Restore3DView_Func SDLDRV_Restore3DView;
extern SDLDRV_SwitchOutToBrowserURL_Func SDLDRV_SwitchOutToBrowserURL;

#endif /* __SDLDRV_EXT_H */

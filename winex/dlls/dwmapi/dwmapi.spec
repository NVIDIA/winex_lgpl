name    dwmapi
type    win32
init    DllMain

100 stub @
101 stub @
102 stdcall DwmEnableComposition (long)
103 stub @
104 stub @
105 stub @
106 stub @
107 stub @
108 stub @
109 stub @
110 stub @
111 stub @
112 stub @
113 stub @

115 stub @
116 stub @
117 stub @
118 stub @
119 stub @
120 stub @

@ stub DwmAttachMilContent
@ stdcall DwmDefWindowProc(long long long long ptr)
@ stub DwmDetachMilContent
@ stdcall DwmEnableBlurBehindWindow(ptr ptr)
@ stdcall DwmEnableMMCSS(long)
@ stdcall DwmExtendFrameIntoClientArea(long ptr)
@ stdcall DwmFlush()
@ stdcall DwmGetColorizationColor(ptr long)
@ stub DwmGetCompositionTimingInfo
@ stdcall DwmGetGraphicsStreamClient(long ptr)
@ stdcall DwmGetGraphicsStreamTransformHint(long ptr)
@ stdcall DwmGetTransportAttributes(ptr ptr ptr)
@ stdcall DwmGetWindowAttribute(ptr long ptr long)
@ stdcall DwmIsCompositionEnabled(ptr)
@ stub DwmModifyPreviousDxFrameDuration
@ stub DwmQueryThumbnailSourceSize
@ stub DwmRegisterThumbnail
@ stub DwmSetDxFrameDuration
@ stub DwmSetPresentParameters
@ stdcall DwmSetWindowAttribute(long long ptr long)
@ stdcall DwmUnregisterThumbnail(long)
@ stub DwmUpdateThumbnailProperties

name	mouse
type	win16
owner	user32
rsrc	resources/mouse.res

1 pascal16 Inquire(ptr) MOUSE_Inquire
2 pascal16 Enable(segptr) MOUSE_Enable
3 pascal16 Disable() MOUSE_Disable
4 stub MOUSEGETINTVECT
5 stub GETSETMOUSEDATA
#Control Panel thinks this is implemented if it is available
#6 stub CPLAPPLET
7 stub POWEREVENTPROC

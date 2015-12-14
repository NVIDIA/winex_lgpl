name credui
type win32
init DllMain

import comctl32.dll
import user32.dll
import advapi32.dll

@ stub CredUICmdLinePromptForCredentialsA
@ stub CredUICmdLinePromptForCredentialsW
@ stub CredUIConfirmCredentialsA
@ stdcall CredUIConfirmCredentialsW(wstr long)
@ stdcall CredUIInitControls()
@ stub CredUIParseUserNameA
@ stdcall CredUIParseUserNameW(wstr ptr long ptr long)
@ stub CredUIPromptForCredentialsA
@ stdcall CredUIPromptForCredentialsW(ptr wstr ptr long ptr long ptr long ptr long)
@ stdcall CredUIReadSSOCredA(str ptr)
@ stdcall CredUIReadSSOCredW(wstr ptr)
@ stdcall CredUIStoreSSOCredA(str str str long)
@ stdcall CredUIStoreSSOCredW(wstr wstr wstr long)
@ stub DllCanUnloadNow
@ stub DllGetClassObject
@ stub DllRegisterServer
@ stub DllUnregisterServer

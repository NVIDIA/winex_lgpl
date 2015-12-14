

#ifndef _GDIPLUSMEM_H
#define _GDIPLUSMEM_H

#define WINGDIPAPI __stdcall

#ifdef __cplusplus
extern "C" {
#endif

void* WINGDIPAPI GdipAlloc(SIZE_T);
void WINGDIPAPI GdipFree(void*);

#ifdef __cplusplus
}
#endif

#endif

name oleaut32
type win32
rsrc version.res
init OLEAUT32_main

import -delay comctl32.dll
import ole32.dll
import rpcrt4.dll
import user32.dll
import gdi32.dll
import advapi32.dll
import kernel32.dll
import ntdll.dll


2 stdcall SysAllocString(wstr) SysAllocString
3 stdcall SysReAllocString(ptr wstr) SysReAllocString
4 stdcall SysAllocStringLen(wstr long) SysAllocStringLen
5 stdcall SysReAllocStringLen(ptr ptr long) SysReAllocStringLen
6 stdcall SysFreeString(wstr) SysFreeString
7 stdcall SysStringLen(wstr) SysStringLen
8 stdcall VariantInit(ptr) VariantInit
9 stdcall VariantClear(ptr) VariantClear
10 stdcall VariantCopy(ptr ptr) VariantCopy
11 stdcall VariantCopyInd(ptr ptr) VariantCopyInd
12 stdcall VariantChangeType(ptr ptr long long) VariantChangeType
13 stdcall VariantTimeToDosDateTime(double ptr ptr) VariantTimeToDosDateTime
14 stdcall DosDateTimeToVariantTime(long long ptr) DosDateTimeToVariantTime
15 stdcall SafeArrayCreate(long long ptr) SafeArrayCreate
16 stdcall SafeArrayDestroy(ptr) SafeArrayDestroy
17 stdcall SafeArrayGetDim(ptr) SafeArrayGetDim
18 stdcall SafeArrayGetElemsize(ptr) SafeArrayGetElemsize
19 stdcall SafeArrayGetUBound(ptr long long) SafeArrayGetUBound
20 stdcall SafeArrayGetLBound(ptr long long) SafeArrayGetLBound
21 stdcall SafeArrayLock(ptr) SafeArrayLock
22 stdcall SafeArrayUnlock(ptr) SafeArrayUnlock
23 stdcall SafeArrayAccessData(ptr ptr) SafeArrayAccessData
24 stdcall SafeArrayUnaccessData(ptr) SafeArrayUnaccessData
25 stdcall SafeArrayGetElement(ptr ptr ptr) SafeArrayGetElement
26 stdcall SafeArrayPutElement(ptr ptr ptr) SafeArrayPutElement
27 stdcall SafeArrayCopy(ptr ptr) SafeArrayCopy
28 stdcall DispGetParam(ptr long long ptr ptr) DispGetParam
29 stdcall DispGetIDsOfNames(ptr ptr long ptr) DispGetIDsOfNames
30 stdcall DispInvoke(ptr ptr long long ptr ptr ptr ptr) DispInvoke
31 stdcall CreateDispTypeInfo(ptr long ptr) CreateDispTypeInfo
32 stdcall CreateStdDispatch(ptr ptr ptr ptr) CreateStdDispatch
33 stdcall RegisterActiveObject(ptr ptr long ptr) RegisterActiveObject
34 stdcall RevokeActiveObject(long ptr) RevokeActiveObject
35 stdcall GetActiveObject(ptr ptr ptr) GetActiveObject
36 stdcall SafeArrayAllocDescriptor(long ptr) SafeArrayAllocDescriptor
37 stdcall SafeArrayAllocData(ptr) SafeArrayAllocData
38 stdcall SafeArrayDestroyDescriptor(ptr) SafeArrayDestroyDescriptor
39 stdcall SafeArrayDestroyData(ptr) SafeArrayDestroyData
40 stdcall SafeArrayRedim(ptr ptr) SafeArrayRedim
41 stdcall SafeArrayAllocDescriptorEx(long long ptr) SafeArrayAllocDescriptorEx
42 stub SafeArrayCreateEx
43 stub SafeArrayCreateVectorEx
44 stub SafeArraySetRecordInfo
45 stub SafeArrayGetRecordInfo
46 stdcall VarParseNumFromStr(wstr long long ptr ptr) VarParseNumFromStr
47 stdcall VarNumFromParseNum(ptr ptr long ptr) VarNumFromParseNum
48 stdcall VarI2FromUI1(long ptr) VarI2FromUI1
49 stdcall VarI2FromI4(long ptr) VarI2FromI4
50 stdcall VarI2FromR4(long ptr) VarI2FromR4
51 stdcall VarI2FromR8(double ptr) VarI2FromR8
52 stdcall VarI2FromCy(double ptr) VarI2FromCy
53 stdcall VarI2FromDate(double ptr) VarI2FromDate
54 stdcall VarI2FromStr(wstr long long ptr) VarI2FromStr
55 stub VarI2FromDisp
56 stdcall VarI2FromBool(long ptr) VarI2FromBool
57 stub SafeArraySetIID
58 stdcall VarI4FromUI1(long ptr) VarI4FromUI1
59 stdcall VarI4FromI2(long ptr) VarI4FromI2
60 stdcall VarI4FromR4(long ptr) VarI4FromR4
61 stdcall VarI4FromR8(double ptr) VarI4FromR8
62 stdcall VarI4FromCy(double ptr) VarI4FromCy
63 stdcall VarI4FromDate(double ptr) VarI4FromDate
64 stdcall VarI4FromStr(wstr long long ptr) VarI4FromStr
65 stub VarI4FromDisp
66 stdcall VarI4FromBool(long ptr) VarI4FromBool
67 stub SafeArrayGetIID
68 stdcall VarR4FromUI1(long ptr) VarR4FromUI1
69 stdcall VarR4FromI2(long ptr) VarR4FromI2
70 stdcall VarR4FromI4(long ptr) VarR4FromI4
71 stdcall VarR4FromR8(double ptr) VarR4FromR8
72 stdcall VarR4FromCy(double ptr) VarR4FromCy
73 stdcall VarR4FromDate(double ptr) VarR4FromDate
74 stdcall VarR4FromStr(wstr long long ptr) VarR4FromStr
75 stub VarR4FromDisp
76 stdcall VarR4FromBool(long ptr) VarR4FromBool
77 stdcall SafeArrayGetVarType(ptr ptr) SafeArrayGetVarType
78 stdcall VarR8FromUI1(long ptr) VarR8FromUI1
79 stdcall VarR8FromI2(long ptr) VarR8FromI2
80 stdcall VarR8FromI4(long ptr) VarR8FromI4
81 stdcall VarR8FromR4(long ptr) VarR8FromR4
82 stdcall VarR8FromCy(double ptr) VarR8FromCy
83 stdcall VarR8FromDate(double ptr) VarR8FromDate
84 stdcall VarR8FromStr(wstr long long ptr) VarR8FromStr
85 stub VarR8FromDisp
86 stdcall VarR8FromBool(long ptr) VarR8FromBool
87 stub VarFormat # stdcall (ptr ptr long long long ptr)
88 stdcall VarDateFromUI1(long ptr) VarDateFromUI1
89 stdcall VarDateFromI2(long ptr) VarDateFromI2
90 stdcall VarDateFromI4(long ptr) VarDateFromI4
91 stdcall VarDateFromR4(long ptr) VarDateFromR4
92 stdcall VarDateFromR8(double ptr) VarDateFromR8
93 stdcall VarDateFromCy(double ptr) VarDateFromCy
94 stdcall VarDateFromStr(wstr long long ptr) VarDateFromStr
95 stub VarDateFromDisp
96 stdcall VarDateFromBool(long ptr) VarDateFromBool
97 stub VarFormatDateTime # stdcall (ptr long long ptr)
98 stdcall VarCyFromUI1(long ptr) VarCyFromUI1
99 stdcall VarCyFromI2(long ptr) VarCyFromI2
100 stdcall VarCyFromI4(long ptr) VarCyFromI4
101 stdcall VarCyFromR4(long ptr) VarCyFromR4
102 stdcall VarCyFromR8(double ptr) VarCyFromR8
103 stdcall VarCyFromDate(double ptr) VarCyFromDate
104 stdcall VarCyFromStr(ptr long long ptr) VarCyFromStr
105 stub VarCyFromDisp
106 stdcall VarCyFromBool(long ptr) VarCyFromBool
107 stub VarFormatNumber # stdcall (ptr long long long long long ptr)
108 stdcall VarBstrFromUI1(long long long ptr) VarBstrFromUI1
109 stdcall VarBstrFromI2(long long long ptr) VarBstrFromI2
110 stdcall VarBstrFromI4(long long long ptr) VarBstrFromI4
111 stdcall VarBstrFromR4(long long long ptr) VarBstrFromR4
112 stdcall VarBstrFromR8(double long long ptr) VarBstrFromR8
113 stdcall VarBstrFromCy(double long long ptr) VarBstrFromCy
114 stdcall VarBstrFromDate(double long long ptr) VarBstrFromDate
115 stdcall VarBstrFromDisp(ptr long long ptr) VarBstrFromDisp
116 stdcall VarBstrFromBool(long long long ptr) VarBstrFromBool
117 stub VarFormatPercent # stdcall (ptr long long long long long ptr)
118 stdcall VarBoolFromUI1(long ptr) VarBoolFromUI1
119 stdcall VarBoolFromI2(long ptr) VarBoolFromI2
120 stdcall VarBoolFromI4(long ptr) VarBoolFromI4
121 stdcall VarBoolFromR4(long ptr) VarBoolFromR4
122 stdcall VarBoolFromR8(double ptr) VarBoolFromR8
123 stdcall VarBoolFromDate(double ptr) VarBoolFromDate
124 stdcall VarBoolFromCy(double ptr) VarBoolFromCy
125 stdcall VarBoolFromStr(wstr long long ptr) VarBoolFromStr
126 stub VarBoolFromDisp
127 stub VarFormatCurrency # stdcall (ptr long long long long long ptr)
128 stub VarWeekdayName # stdcall (long long long long ptr)
129 stub VarMonthName # stdcall (long long long ptr)
130 stdcall VarUI1FromI2(long ptr) VarUI1FromI2
131 stdcall VarUI1FromI4(long ptr) VarUI1FromI4
132 stdcall VarUI1FromR4(long ptr) VarUI1FromR4
133 stdcall VarUI1FromR8(double ptr) VarUI1FromR8
134 stdcall VarUI1FromCy(double ptr) VarUI1FromCy
135 stdcall VarUI1FromDate(double ptr) VarUI1FromDate
136 stdcall VarUI1FromStr(wstr long long ptr) VarUI1FromStr
137 stub VarUI1FromDisp
138 stdcall VarUI1FromBool(long ptr) VarUI1FromBool
139 stub VarFormatFromTokens # stdcall (ptr ptr ptr long ptr long)
140 stub VarTokenizeFormatString # stdcall (ptr ptr long long long long ptr)
141 stub VarAdd # stdcall (ptr ptr ptr)
142 stub VarAnd # stdcall (ptr ptr ptr)
143 stub VarDiv # stdcall (ptr ptr ptr)
144 stdcall DllCanUnloadNow() OLEAUT32_DllCanUnloadNow
145 stdcall DllGetClassObject(ptr ptr ptr) OLEAUT32_DllGetClassObject
146 stub DispCallFunc
147 stdcall VariantChangeTypeEx(ptr ptr long long long) VariantChangeTypeEx
148 stdcall SafeArrayPtrOfIndex(ptr ptr ptr) SafeArrayPtrOfIndex
149 stdcall SysStringByteLen(ptr) SysStringByteLen
150 stdcall SysAllocStringByteLen(ptr long) SysAllocStringByteLen
151 stdcall DllRegisterServer() OLEAUT32_DllRegisterServer
152 stub VarEqv # stdcall (ptr ptr ptr)
153 stub VarIdiv # stdcall (ptr ptr ptr)
154 stub VarImp # stdcall (ptr ptr ptr)
155 stub VarMod # stdcall (ptr ptr ptr)
156 stub VarMul # stdcall (ptr ptr ptr)
157 stub VarOr # stdcall (ptr ptr ptr)
158 stub VarPow # stdcall (ptr ptr ptr)
159 stub VarSub # stdcall (ptr ptr ptr)
160 stdcall CreateTypeLib(long wstr ptr) CreateTypeLib
161 stdcall LoadTypeLib (wstr ptr) LoadTypeLib
162 stdcall LoadRegTypeLib (ptr long long long ptr) LoadRegTypeLib
163 stdcall RegisterTypeLib(ptr wstr wstr) RegisterTypeLib
164 stdcall QueryPathOfRegTypeLib(ptr long long long ptr) QueryPathOfRegTypeLib
165 stdcall LHashValOfNameSys(long long wstr) LHashValOfNameSys
166 stdcall LHashValOfNameSysA(long long str) LHashValOfNameSysA
167 stub VarXor # stdcall (ptr ptr ptr)
168 stub VarAbs # stdcall (ptr ptr)
169 stub VarFix # stdcall (ptr ptr)
170 stdcall OaBuildVersion() OaBuildVersion
171 stub ClearCustData
172 stub VarInt # stdcall (ptr ptr)
173 stub VarNeg # stdcall (ptr ptr)
174 stub VarNot # stdcall (ptr ptr)
175 stub VarRound # stdcall (ptr long ptr)
176 stub VarCmp # stdcall (ptr ptr long long)
177 stub VarDecAdd # stdcall (ptr ptr ptr)
178 stub VarDecDiv # stdcall (ptr ptr ptr)
179 stub VarDecMul # stdcall (ptr ptr ptr)
180 stub CreateTypeLib2
181 stub VarDecSub # stdcall (ptr ptr ptr)
182 stub VarDecAbs # stdcall (ptr ptr)
183 stdcall LoadTypeLibEx (wstr long ptr) LoadTypeLibEx
184 stdcall SystemTimeToVariantTime(ptr ptr) SystemTimeToVariantTime
185 stdcall VariantTimeToSystemTime(double ptr) VariantTimeToSystemTime
186 stdcall UnRegisterTypeLib (ptr long long long long) UnRegisterTypeLib
187 stub VarDecFix # stdcall (ptr ptr)
188 stub VarDecInt # stdcall (ptr ptr)
189 stub VarDecNeg # stdcall (ptr ptr)
190 stub VarDecFromUI1
191 stub VarDecFromI2
192 stub VarDecFromI4
193 stub VarDecFromR4
194 stub VarDecFromR8
195 stub VarDecFromDate
196 stub VarDecFromCy
197 stub VarDecFromStr
198 stub VarDecFromDisp
199 stub VarDecFromBool
200 forward GetErrorInfo ole32.GetErrorInfo
201 forward SetErrorInfo ole32.SetErrorInfo
202 forward CreateErrorInfo ole32.CreateErrorInfo
203 stub VarDecRound # stdcall (ptr long ptr)
204 stub VarDecCmp # stdcall (ptr ptr)
205 stdcall VarI2FromI1(long ptr) VarI2FromI1
206 stdcall VarI2FromUI2(long ptr) VarI2FromUI2
207 stdcall VarI2FromUI4(long ptr) VarI2FromUI4
208 stub VarI2FromDec
209 stdcall VarI4FromI1(long ptr) VarI4FromI1
210 stdcall VarI4FromUI2(long ptr) VarI4FromUI2
211 stdcall VarI4FromUI4(long ptr) VarI4FromUI4
212 stub VarI4FromDec
213 stdcall VarR4FromI1(long ptr) VarR4FromI1
214 stdcall VarR4FromUI2(long ptr) VarR4FromUI2
215 stdcall VarR4FromUI4(long ptr) VarR4FromUI4
216 stub VarR4FromDec
217 stdcall VarR8FromI1(long ptr) VarR8FromI1
218 stdcall VarR8FromUI2(long ptr) VarR8FromUI2
219 stdcall VarR8FromUI4(long ptr) VarR8FromUI4
220 stub VarR8FromDec
221 stdcall VarDateFromI1(long ptr) VarDateFromI1
222 stdcall VarDateFromUI2(long ptr) VarDateFromUI2
223 stdcall VarDateFromUI4(long ptr) VarDateFromUI4
224 stub VarDateFromDec
225 stdcall VarCyFromI1(long ptr) VarCyFromI1
226 stdcall VarCyFromUI2(long ptr) VarCyFromUI2
227 stdcall VarCyFromUI4(long ptr) VarCyFromUI4
228 stub VarCyFromDec
229 stdcall VarBstrFromI1(long long long ptr) VarBstrFromI1
230 stdcall VarBstrFromUI2(long long long ptr) VarBstrFromUI2
231 stdcall VarBstrFromUI4(long long long ptr) VarBstrFromUI4
232 stub VarBstrFromDec
233 stdcall VarBoolFromI1(long ptr) VarBoolFromI1
234 stdcall VarBoolFromUI2(long ptr) VarBoolFromUI2
235 stdcall VarBoolFromUI4(long ptr) VarBoolFromUI4
236 stub VarBoolFromDec
237 stdcall VarUI1FromI1(long ptr) VarUI1FromI1
238 stdcall VarUI1FromUI2(long ptr) VarUI1FromUI2
239 stdcall VarUI1FromUI4(long ptr) VarUI1FromUI4
240 stub VarUI1FromDec
241 stub VarDecFromI1
242 stub VarDecFromUI2
243 stub VarDecFromUI4
244 stdcall VarI1FromUI1(long ptr) VarI1FromUI1
245 stdcall VarI1FromI2(long ptr) VarI1FromI2
246 stdcall VarI1FromI4(long ptr) VarI1FromI4
247 stdcall VarI1FromR4(long ptr) VarI1FromR4
248 stdcall VarI1FromR8(double ptr) VarI1FromR8
249 stdcall VarI1FromDate(double ptr) VarI1FromDate
250 stdcall VarI1FromCy(double ptr) VarI1FromCy
251 stdcall VarI1FromStr(wstr long long ptr) VarI1FromStr
252 stub VarI1FromDisp
253 stdcall VarI1FromBool(long ptr) VarI1FromBool
254 stdcall VarI1FromUI2(long ptr) VarI1FromUI2
255 stdcall VarI1FromUI4(long ptr) VarI1FromUI4
256 stub VarI1FromDec
257 stdcall VarUI2FromUI1(long ptr) VarUI2FromUI1
258 stdcall VarUI2FromI2(long ptr) VarUI2FromI2
259 stdcall VarUI2FromI4(long ptr) VarUI2FromI4
260 stdcall VarUI2FromR4(long ptr) VarUI2FromR4
261 stdcall VarUI2FromR8(double ptr) VarUI2FromR8
262 stdcall VarUI2FromDate(double ptr) VarUI2FromDate
263 stdcall VarUI2FromCy(double ptr) VarUI2FromCy
264 stdcall VarUI2FromStr(wstr long long ptr) VarUI2FromStr
265 stub VarUI2FromDisp
266 stdcall VarUI2FromBool(long ptr) VarUI2FromBool
267 stdcall VarUI2FromI1(long ptr) VarUI2FromI1
268 stdcall VarUI2FromUI4(long ptr) VarUI2FromUI4
269 stub VarUI2FromDec
270 stdcall VarUI4FromUI1(long ptr) VarUI4FromUI1
271 stdcall VarUI4FromI2(long ptr) VarUI4FromI2
272 stdcall VarUI4FromI4(long ptr) VarUI4FromI4
273 stdcall VarUI4FromR4(long ptr) VarUI4FromR4
274 stdcall VarUI4FromR8(double ptr) VarUI4FromR8
275 stdcall VarUI4FromDate(double ptr) VarUI4FromDate
276 stdcall VarUI4FromCy(double ptr) VarUI4FromCy
277 stdcall VarUI4FromStr(wstr long long ptr) VarUI4FromStr
278 stub VarUI4FromDisp
279 stdcall VarUI4FromBool(long ptr) VarUI4FromBool
280 stdcall VarUI4FromI1(long ptr) VarUI4FromI1
281 stdcall VarUI4FromUI2(long ptr) VarUI4FromUI2
282 stub VarUI4FromDec
283 stdcall BSTR_UserSize(ptr long ptr) BSTR_UserSize
284 stdcall BSTR_UserMarshal(ptr ptr ptr) BSTR_UserMarshal
285 stdcall BSTR_UserUnmarshal(ptr ptr ptr) BSTR_UserUnmarshal
286 stdcall BSTR_UserFree(ptr ptr) BSTR_UserFree
287 stdcall VARIANT_UserSize(ptr long ptr) VARIANT_UserSize
288 stdcall VARIANT_UserMarshal(ptr ptr ptr) VARIANT_UserMarshal
289 stdcall VARIANT_UserUnmarshal(ptr ptr ptr) VARIANT_UserUnmarshal
290 stdcall VARIANT_UserFree(ptr ptr) VARIANT_UserFree
291 stub LPSAFEARRAY_UserSize
292 stub LPSAFEARRAY_UserMarshal
293 stub LPSAFEARRAY_UserUnmarshal
294 stub LPSAFEARRAY_UserFree
295 stub LPSAFEARRAY_Size
296 stub LPSAFEARRAY_Marshal
297 stub LPSAFEARRAY_Unmarshal
298 stub VarDecCmpR8 # stdcall (ptr double)
299 stub VarCyAdd
300 stdcall DllUnregisterServer() OLEAUT32_DllUnregisterServer
301 stub OACreateTypeLib2
# 302 unused
303 stub VarCyMul
304 stub VarCyMulI4
305 stub VarCySub
306 stub VarCyAbs
307 stub VarCyFix
308 stub VarCyInt
309 stub VarCyNeg
310 stub VarCyRound
311 stub VarCyCmp
312 stub VarCyCmpR8
313 stdcall VarBstrCat(ptr ptr ptr) VarBstrCat
314 stdcall VarBstrCmp(ptr ptr long long) VarBstrCmp
315 stub VarR8Pow # stdcall (double double ptr)
316 stub VarR4CmpR8
317 stub VarR8Round # stdcall (double long ptr)
318 stdcall VarCat(ptr ptr ptr) VarCat
319 stub VarDateFromUdateEx # stdcall (ptr long long ptr)
# 320-321 unused
322 stub GetRecordInfoFromGuids # stdcall (ptr long long long ptr ptr)
323 stub GetRecordInfoFromTypeInfo # stdcall (ptr ptr)
# 324 unused
325 stub SetVarConversionLocaleSetting
326 stub GetVarConversionLocaleSetting
327 stub SetOaNoCache
# 328 unused
329 stub VarCyMulI8
330 stdcall VarDateFromUdate(ptr long ptr) VarDateFromUdate
331 stdcall VarUdateFromDate(double long ptr) VarUdateFromDate
332 stub GetAltMonthNames
333 stub VarI8FromUI1
334 stub VarI8FromI2
335 stub VarI8FromR4
336 stub VarI8FromR8
337 stub VarI8FromCy
338 stub VarI8FromDate
339 stub VarI8FromStr
340 stub VarI8FromDisp
341 stub VarI8FromBool
342 stub VarI8FromI1
343 stub VarI8FromUI2
344 stub VarI8FromUI4
345 stub VarI8FromDec
346 stub VarI2FromI8
347 stub VarI2FromUI8
348 stub VarI4FromI8
349 stub VarI4FromUI8
# 350-359 unused
360 stub VarR4FromI8
361 stub VarR4FromUI8
362 stub VarR8FromI8
363 stub VarR8FromUI8
364 stub VarDateFromI8
365 stub VarDateFromUI8
366 stub VarCyFromI8
367 stub VarCyFromUI8
368 stub VarBstrFromI8
369 stdcall VarBstrFromUI8(double long long ptr)
370 stub VarBoolFromI8
371 stub VarBoolFromUI8
372 stub VarUI1FromI8
373 stub VarUI1FromUI8
374 stub VarDecFromI8
375 stub VarDecFromUI8
376 stub VarI1FromI8
377 stub VarI1FromUI8
378 stub VarUI2FromI8
379 stub VarUI2FromUI8

# 380-400 unused in XP
380 stub UserHWND_from_local
381 stub UserHWND_to_local
382 stub UserHWND_free_inst
383 stub UserHWND_free_local
384 stub UserBSTR_from_local
385 stub UserBSTR_to_local
386 stub UserBSTR_free_inst
387 stub UserBSTR_free_local
388 stub UserVARIANT_from_local
389 stub UserVARIANT_to_local
390 stub UserVARIANT_free_inst
391 stub UserVARIANT_free_local
392 stub UserEXCEPINFO_from_local
393 stub UserEXCEPINFO_to_local
394 stub UserEXCEPINFO_free_inst
395 stub UserEXCEPINFO_free_local
396 stub UserMSG_from_local
397 stub UserMSG_to_local
398 stub UserMSG_free_inst
399 stub UserMSG_free_local

401 stdcall OleLoadPictureEx(ptr long long long long long long ptr) OleLoadPictureEx
402 stub OleLoadPictureFileEx
# 403-410 unused
411 stdcall SafeArrayCreateVector(long long long) SafeArrayCreateVector
412 stdcall SafeArrayCopyData(ptr ptr) SafeArrayCopyData
413 stub VectorFromBstr
414 stub BstrFromVector
415 stdcall OleIconToCursor(long long) OleIconToCursor
416 stdcall OleCreatePropertyFrameIndirect(ptr) OleCreatePropertyFrameIndirect
417 stdcall OleCreatePropertyFrame(ptr long long ptr long ptr long ptr ptr long ptr) OleCreatePropertyFrame
418 stdcall OleLoadPicture(ptr long long ptr ptr) OleLoadPicture
419 stdcall OleCreatePictureIndirect(ptr ptr long ptr) OleCreatePictureIndirect
420 stdcall OleCreateFontIndirect(ptr ptr ptr) OleCreateFontIndirect
421 stdcall OleTranslateColor(long long long) OleTranslateColor
422 stub OleLoadPictureFile
423 stub OleSavePictureFile
424 stub OleLoadPicturePath
425 stub VarUI4FromI8
426 stub VarUI4FromUI8
427 stub VarI8FromUI8
428 stub VarUI8FromI8
429 stub VarUI8FromUI1
430 stub VarUI8FromI2
431 stub VarUI8FromR4
432 stub VarUI8FromR8
433 stub VarUI8FromCy
434 stub VarUI8FromDate
435 stub VarUI8FromStr
436 stub VarUI8FromDisp
437 stub VarUI8FromBool
438 stub VarUI8FromI1
439 stub VarUI8FromUI2
440 stub VarUI8FromUI4
441 stub VarUI8FromDec
442 stub RegisterTypeLibForUser
443 stub UnRegisterTypeLibForUser

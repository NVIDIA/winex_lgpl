/*
 * Apple CoreAudio based MP3 MSACM driver 
 * 
 *    mp3_audioconverter.h - header for mac-specific functions
 *
 *      Copyright (C) 2003 TransGaming Technologies Inc.
 *
 */


int APPLEMP3_AudioConverterInit(void **data, int samplesPerSec, int channels);
void APPLEMP3_AudioConverterUnInit(void *data);
void APPLEMP3_AudioConverterReset(void *data);
void APPLEMP3_AudioConverterDecodeData(void     *data,
                                      DWORD    inInputDataSize,
                                      void*    inInputData,
                                      DWORD*   ioOutputDataSize,
                                      void*    outOutputData);

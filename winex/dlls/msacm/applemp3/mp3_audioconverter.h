/*
 * Apple CoreAudio based MP3 MSACM driver 
 * 
 *    mp3_audioconverter.h - header for mac-specific functions
 *
 * Copyright (c) 2003-2015 NVIDIA CORPORATION. All rights reserved.
 *
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
*/


int APPLEMP3_AudioConverterInit(void **data, int samplesPerSec, int channels);
void APPLEMP3_AudioConverterUnInit(void *data);
void APPLEMP3_AudioConverterReset(void *data);
void APPLEMP3_AudioConverterDecodeData(void     *data,
                                      DWORD    inInputDataSize,
                                      void*    inInputData,
                                      DWORD*   ioOutputDataSize,
                                      void*    outOutputData);

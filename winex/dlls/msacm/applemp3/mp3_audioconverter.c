/*
 * Apple CoreAudio based MP3 MSACM driver
 * 
 *    mp3_audioconverter.c - code for mac-specific functions
 *
 * Copyright (c) 2003-2015 NVIDIA CORPORATION. All rights reserved.
 *
 *
 *  A couple of things to note: 
 *   Apple's idea of a 'frame' is # of bytes for ONE sample of data across all channels
 *   Apple's idea of a 'packet' is the natural format of data for the given file format,
 *   which is equivalent to a 'frame' in MP3 format parlance!  We use apple's definitions
 *   here, not MP3 format definitions, so don't confuse Apple's 'Frame' with an MP3 'Frame'!
 *
 *   We now support VBR mp3s, as we are parsing the mp3 frame (aka packet) headers directly.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifdef __APPLE__

#include "wine/debug.h"

#if TG_MAC_OS_X_SDK_VERSION >= TG_MAC_OS_X_VERSION_SNOWLEOPARD
/* Local compiler redefinitions to account for new inclusions of COM files in 10.6's AudioToolbox */
#define ULONG ULONG_AUDIOTOOLBOX
#define HRESULT HRESULT_AUDIOTOOLBOX
#undef REFIID
#define REFIID REFIID_AUDIOTOOLBOX
#endif

#include <CoreServices/CoreServices.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <stdio.h>

DEFAULT_DEBUG_CHANNEL(applemp3);

typedef struct tagIntAppleMP3Data
{
    AudioConverterRef            mConverter;
    UInt32                       mChannels;
    AudioStreamPacketDescription mDataPacketDescription;
    
    /* Input Data Cache */
    UInt8*                       mCachePtr;
    UInt32                       mCacheMaxSize;
    UInt32                       mCacheStart;
    UInt32                       mCacheEnd;
    
    /* Data Cache for callback */
    UInt8*                       mCallbackCachePtr;
    UInt32                       mCallbackCacheMaxSize;
} IntAppleMP3Data;

#define CACHE_INIT_SIZE 1024*50

/* these are defined in applemp3.c */
void LockCoreAudio();
void UnlockCoreAudio();

/***********************************************************************
 *           APPLEMP3_AudioConverterUnInit
 */
void APPLEMP3_AudioConverterUnInit(IntAppleMP3Data **data)
{
    TRACE("%p\n", data);

    if (data[0]->mConverter) {
        LockCoreAudio();
        AudioConverterDispose (data[0]->mConverter);
        UnlockCoreAudio();
    }

    /* Free the caches */
    if (data[0]->mCachePtr)
        free (data[0]->mCachePtr);
    if (data[0]->mCallbackCachePtr)
        free (data[0]->mCallbackCachePtr);
    
    free(*data);
    *data = NULL;
}

/***********************************************************************
 *           APPLEMP3_AudioConverterReset
 */
void APPLEMP3_AudioConverterReset(IntAppleMP3Data **data)
{
    TRACE("%p\n", data);

    data[0]->mCacheStart = 0;
    data[0]->mCacheEnd = 0;
}


/***********************************************************************
 *           APPLEMP3_AudioConverterInit
 */

int APPLEMP3_AudioConverterInit(IntAppleMP3Data **data, int samplesPerSec, int channels)
{
    OSStatus result = noErr;
    AudioStreamBasicDescription srcDesc;
    AudioStreamBasicDescription	dstDesc;

    /* Note the calloc - every bit of our data is zeroed */
    if (*data == NULL)
    {
    	*data = calloc(1, sizeof(IntAppleMP3Data));    	
    }
    else
    {
	APPLEMP3_AudioConverterUnInit(data);
	*data = calloc(1, sizeof(IntAppleMP3Data));
    }
    
    if (data[0] == NULL)
        return -1;
    
    /* Number of channels of input data */
    data[0]->mChannels = channels;
    
    TRACE("%p samples: %d channels: %d \n", data, samplesPerSec, channels);
    
    /* Set up the src and dst descriptions */
    
    srcDesc.mSampleRate = samplesPerSec;
    srcDesc.mFormatID = kAudioFormatMPEGLayer3;
    srcDesc.mFormatFlags = 0;
    srcDesc.mBytesPerPacket = 0;
    srcDesc.mFramesPerPacket = 1152;
    srcDesc.mBytesPerFrame = 0;
    srcDesc.mChannelsPerFrame = channels;
    srcDesc.mBitsPerChannel = 0;
    srcDesc.mReserved = 0;

    dstDesc.mSampleRate = samplesPerSec;
    dstDesc.mFormatID = kAudioFormatLinearPCM;
    dstDesc.mFormatFlags = kLinearPCMFormatFlagIsBigEndian | 
                           kLinearPCMFormatFlagIsSignedInteger;
    dstDesc.mBytesPerPacket = channels * 2;
    dstDesc.mFramesPerPacket = 1;
    dstDesc.mBytesPerFrame = channels * 2;
    dstDesc.mChannelsPerFrame = channels;
    dstDesc.mBitsPerChannel = 16;
    dstDesc.mReserved = 0;

    /* Create the AudioConverter */
    LockCoreAudio();
    result = AudioConverterNew (&srcDesc, &dstDesc, &data[0]->mConverter);    
    UnlockCoreAudio();
    if (result != noErr)
    {
        ERR("Error creating AudioConverter\n");
        return FALSE;
    }

    /* Alloc the input cache at initial size - it might need to grow later */
    data[0]->mCachePtr = malloc(CACHE_INIT_SIZE);
    
    if (data[0]->mCachePtr)
    {
        data[0]->mCacheMaxSize = CACHE_INIT_SIZE;
        TRACE("AudioConverter and cache successfully created\n");
        return TRUE;
    }
    else
    {
        ERR("Error creating conversion cache\n");
        return FALSE;
    }

}



/***********************************************************************
 *           APPLEMP3_GetBits
 * 
 *           This function gets the specified bits out of a 32-bit word.
 *           Doesn't check for bad parameters!
 */
static inline UInt32 APPLEMP3_GetBits(UInt32 input, UInt32 bitIndex, UInt32 numBits)
{
    /* Shift up by bitIndex, then down to put the data in the lower part
       of the output.  Since everything is unsigned, we don't get any 
       sign conversions to mess things up. */
    return (input << bitIndex) >> (32 - numBits);   
}


/***********************************************************************
 *           APPLEMP3_FindMP3PacketSize
 * 
 *           This function takes the start of an MP3 frame (aka 'packet')
 *           as input and returns the packet size in bytes.  If the input 
 *           pointer does not point to the start of an MP3 frame, the 
 *           function will return 0 to indicate an error.  
 *
 *           MP3 format info from:
 *             http://mpgedit.org/mpgedit/mpeg_format/mpeghdr.htm
 */

    /* Tables */
static const UInt32 V1L1_bitrates[16] =  {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0};
static const UInt32 V1L2_bitrates[16] =  {0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, 0};
static const UInt32 V1L3_bitrates[16] =  {0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 0};
static const UInt32 V2L1_bitrates[16] =  {0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256, 0};
static const UInt32 V2L23_bitrates[16] = {0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0};

static const UInt32 V1_samplerates[4] =  {44100, 48000, 32000, 0};
static const UInt32 V2_samplerates[4] =  {22050, 24000, 16000, 0};
static const UInt32 V25_samplerates[4] = {11025, 12000,  8000, 0};

UInt32 APPLEMP3_FindMP3PacketSize(void* inInputData)
{
    /* Parse the mp3 data to find the packet size */
    UInt32 header = *((UInt32*)inInputData);
    UInt32 sync, mpegid, layer, bitrate_index, samplerate_index, pad; 
    UInt32 bitrate = 0;
    UInt32 samplerate = 0;
    UInt16 packetSize = 0;
 
    /* Get the sync bits */    
    sync = APPLEMP3_GetBits(header, 0, 11);
    
    /* Verify the sync bits */
    if (sync != 0x7ff)
    {
        WARN("Bad MP3 Frame Header Sync Bits\n");
        return 0;
    }
    
    /* get all the other stuff */
    mpegid = APPLEMP3_GetBits(header, 11, 2);
    layer = APPLEMP3_GetBits(header, 13, 2);
    bitrate_index = APPLEMP3_GetBits(header, 16, 4);
    samplerate_index = APPLEMP3_GetBits(header, 20, 2);
    pad = APPLEMP3_GetBits(header, 22, 1);
    
    /* Get the bitrate */
    if (mpegid == 3)
    {
        if (layer == 3)      /* MPEG 1, Layer 1 */
            bitrate = V1L1_bitrates[bitrate_index];
        else if (layer == 2) /* MPEG 1, Layer 2 */
            bitrate = V1L2_bitrates[bitrate_index];
        else if (layer == 1) /* MPEG 1, Layer 3 */
            bitrate = V1L3_bitrates[bitrate_index];        
    }
    else
    {
        if (layer == 3)      /* MPEG 2, Layer 1 */
            bitrate = V2L1_bitrates[bitrate_index];
        else if (layer == 2) /* MPEG 2, Layer 2 */
            bitrate = V2L23_bitrates[bitrate_index];
        else if (layer == 1) /* MPEG 2, Layer 3 */
            bitrate = V2L23_bitrates[bitrate_index];            
    }
    
    /* Bitrate is always in thousands */
    bitrate = bitrate * 1000;
    
    /* Get the samplerate */
    if (mpegid == 3)      /* MPEG 1 */
        samplerate = V1_samplerates[samplerate_index];        
    else if (mpegid == 2) /* MPEG 2 */
        samplerate = V2_samplerates[samplerate_index];        
    else if (mpegid == 0) /* MPEG 2.5 */
        samplerate = V25_samplerates[samplerate_index];        

    /* Verify the rates */
    if ((samplerate == 0) || (bitrate == 0) )
    {
        ERR("Bad MP3 Frame Header - zero samplerate or biterate \n");
        return 0;
    }
        
    /* Calculate the packet size */
    if (layer == 3)      /* Layer 1 */
        packetSize = ((12 * bitrate / samplerate) + pad) * 4;
    else
        packetSize = (144 * bitrate / samplerate) + pad;

    return packetSize;    
}


/***********************************************************************
 *           APPLEMP3_CacheInputData
 * 
 *           Cache incoming data in the input buffer
 */

OSStatus APPLEMP3_CacheInputData(IntAppleMP3Data       *data,
                                 UInt32               inInputDataSize,
                                 void*                inInputData)
{
    /* Ok, we're going to add the incoming data to our cache.  If we look like we're going about
       to go over the end of the cache, we'll move the cache data back to the beginning of the buffer.
       If even that's not good enough, we'll realloc the cache to make it big enough for the data in 
       question */

    TRACE("%p About to add %lu bytes to cache.  Cache Start now: %ld Cache End now: %ld\n", data, inInputDataSize, data->mCacheStart, data->mCacheEnd);
    
    /* First, check if it WON'T in the existing cache */
    if (!(inInputDataSize <= (data->mCacheMaxSize - data->mCacheEnd)))
    {
        /* Ok, it didn't fit initially.  Regardless of what we do next, we're going to have to move the
           existing good data back to the start of the cache buffer, so do so now */
        memmove(data->mCachePtr, data->mCachePtr + data->mCacheStart, (data->mCacheEnd - data->mCacheStart));
        data->mCacheEnd = (data->mCacheEnd - data->mCacheStart);
        data->mCacheStart = 0;
        TRACE("Rolled back cache.  Cache Start now: %ld Cache End now: %ld\n", data->mCacheStart, data->mCacheEnd);
    }
    
    /* Now, check again if we WILL fit in the existing cache, now that it's potentially been emptied */
    if (inInputDataSize <= (data->mCacheMaxSize - data->mCacheEnd))
    {
        /* It fits, copy in the data and return success */
        memcpy(data->mCachePtr + data->mCacheEnd, inInputData, inInputDataSize);
        data->mCacheEnd += inInputDataSize;     
        TRACE("Data has been Added.  Cache Start now: %ld Cache End now: %ld\n", data->mCacheStart, data->mCacheEnd);
        return noErr;
    }
    
    /* Ok, we now know that we need to make the cache bigger.  Do so.  Hopefully this will be very very 
       rare!  Make the new cache big enoug to hold the new data plus another CACHE_INIT_SIZE buffer */
    data->mCacheMaxSize = data->mCacheEnd + inInputDataSize + CACHE_INIT_SIZE;
    data->mCachePtr = realloc(data->mCachePtr, data->mCacheMaxSize);
    TRACE("Cache too small - needed realloc to %ld bytes.  Cache Start now: %ld Cache End now: %ld Cache Ptr now: %p\n", 
          data->mCacheMaxSize, data->mCacheStart, data->mCacheEnd, data->mCachePtr);   
    
    if (!data->mCachePtr)
    {
        /* uh oh! */
        data->mCacheMaxSize = 0;
        ERR("Couldn't realloc mp3 cache!\n");        
        return memFullErr;
    }

    /* That done, copy in the new data and return success */
    memcpy(data->mCachePtr + data->mCacheEnd, inInputData, inInputDataSize);
    data->mCacheEnd += inInputDataSize;     
    TRACE("Data has been Added.  Cache Start now: %ld Cache End now: %ld\n", data->mCacheStart, data->mCacheEnd);
    return noErr;
}                              


/***********************************************************************
 *           APPLEMP3_AudioConverterInputDataProc
 */
OSStatus APPLEMP3_AudioConverterComplexInputDataProc( AudioConverterRef                   inAudioConverter,
                                                      UInt32*                             ioNumberDataPackets,
                                                      AudioBufferList*                    ioData,
                                                      AudioStreamPacketDescription**      outDataPacketDescription,
                                                      void*                               inUserData)

{
    IntAppleMP3Data* data = (IntAppleMP3Data*)inUserData; 
    UInt32 packetSize; 
    
    TRACE("%p request num packets: %lu Cache Start: %lu Cache End: %lu \n", data, *ioNumberDataPackets, data->mCacheStart, data->mCacheEnd);
    
    /* Ensure that we have enough data to calculate the packet size */
    if ((data->mCacheEnd - data->mCacheStart) < 4)
    {
        TRACE("Not enough data to calculate packet size yet\n");
        ioData->mBuffers[0].mData = NULL;
        ioData->mBuffers[0].mDataByteSize = 0;
        *ioNumberDataPackets = 0;
        return eofErr;
    }

    /* Find the packetsize for the current packet */
    packetSize = APPLEMP3_FindMP3PacketSize(data->mCachePtr + data->mCacheStart);
    TRACE("mp3 packetsize: %lu \n", packetSize);
    
    /* If we don't have enough data cached yet, return an error */
    if ((packetSize == 0) || ((data->mCacheEnd - data->mCacheStart) < packetSize))
    {
        TRACE("Not enough data cached yet or zero packetSize \n");
        ioData->mBuffers[0].mData = NULL;
        ioData->mBuffers[0].mDataByteSize = 0;
        *ioNumberDataPackets = 0;
        return eofErr;
    }
    
    /* We *always* provide just a single data packet, since if the mp3 file uses vbr data we would 
       have a much more difficult time filling in the outDataPacketDescriptions. */ 
     *ioNumberDataPackets = 1;
        
    /* Ok, here we have at least enough data for one packet.  Add it to the output cache, making sure 
       that the output cache is big enough first */
    if (packetSize > data->mCallbackCacheMaxSize)
    {
        if (data->mCallbackCachePtr)
            free(data->mCallbackCachePtr);
        
        data->mCallbackCacheMaxSize = packetSize;
        data->mCallbackCachePtr = malloc(packetSize);
    }
    memcpy(data->mCallbackCachePtr, data->mCachePtr + data->mCacheStart, packetSize);
    
    /* Advance the cache start position by the number of bytes we're sending */
    data->mCacheStart += packetSize;
    
    /* Set up the pointers for output data */
    ioData->mBuffers[0].mData = data->mCallbackCachePtr;
    ioData->mBuffers[0].mDataByteSize = packetSize;

    /* What to do with this?... */
/*    
    data->mDataPacketDescription.mStartOffset = 0;
    data->mDataPacketDescription.mVariableFramesInPacket = 0;
    data->mDataPacketDescription.mDataByteSize = packetSize;
    *outDataPacketDescription = &data->mDataPacketDescription;
*/    
    *outDataPacketDescription = NULL;
    
    return noErr;
}



/***********************************************************************
 *           APPLEMP3_AudioConverterDecodeData
 */

void APPLEMP3_AudioConverterDecodeData(IntAppleMP3Data     *data,
                                       UInt32    inInputDataSize,
                                       void*     inInputData,
                                       UInt32*   ioOutputDataSize,
                                       void*     outOutputData)
{
    OSStatus         err = noErr;
    UInt32           numFrames;
    AudioBufferList  bufferList;
    UInt32           dataRead = 0;
    UInt32           dataRemaining = *ioOutputDataSize;
    UInt32           maxBufferSize  = dataRemaining;
    UInt32           curPos = 0;
    
    TRACE("data: %p inputDataSize: %lu outputDataSize: %lu\n", data, inInputDataSize, *ioOutputDataSize);

    /* If we have new data, add it to the cache */
    if (inInputDataSize)
        err = APPLEMP3_CacheInputData(data, inInputDataSize, inInputData);
    
    if (err != noErr)
    {
        ERR("Could not cache mp3 input data!\n");
        *ioOutputDataSize = 0;
        return;
    }

    /* Loop, filling in output data as we get it */
    *ioOutputDataSize = 0;
    while (dataRemaining >= (1152 * 2 * data->mChannels))
    {
        bufferList.mNumberBuffers = 1;
        bufferList.mBuffers[0].mData = outOutputData + curPos;
        bufferList.mBuffers[0].mDataByteSize = maxBufferSize - curPos;
        numFrames = 1152; /* one mp3 packet at a time - an mp3 packet produces 1152 samples */
    
        LockCoreAudio();
        err = AudioConverterFillComplexBuffer(data->mConverter, 
                                              APPLEMP3_AudioConverterComplexInputDataProc,
                                              data, 
                                              &numFrames,
                                              &bufferList,
                                              NULL);
        UnlockCoreAudio();
       
        dataRead = bufferList.mBuffers[0].mDataByteSize;
        curPos += dataRead;
        *ioOutputDataSize += dataRead;
        dataRemaining -= dataRead;
        TRACE("Tried to fill buffer: data: %p outputDataSize: %lu frames: %lu err: %ld\n", data, dataRead, numFrames, err);

        if (dataRead == 0)
        {
            /* Time to stop */
            TRACE("No more output available\n");
            return;
        }
    }
}


#else /* __APPLE__ */

#endif /* __APPLE__ */



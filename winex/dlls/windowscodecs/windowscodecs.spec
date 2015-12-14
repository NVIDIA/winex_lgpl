name windowscodecs
init DllMain
type win32

import kernel32.dll
import ole32.dll
import advapi32.dll

@ stdcall DllGetClassObject(ptr ptr ptr)
@ stub IEnumString_Next_WIC_Proxy
@ stub IEnumString_Reset_WIC_Proxy
@ stub IPropertyBag2_Write_Proxy
@ stub IWICBitmapClipper_Initialize_Proxy
@ stub IWICBitmapCodecInfo_DoesSupportAnimation_Proxy
@ stub IWICBitmapCodecInfo_DoesSupportLossless_Proxy
@ stub IWICBitmapCodecInfo_DoesSupportMultiframe_Proxy
@ stub IWICBitmapCodecInfo_GetContainerFormat_Proxy
@ stub IWICBitmapCodecInfo_GetDeviceManufacturer_Proxy
@ stub IWICBitmapCodecInfo_GetDeviceModels_Proxy
@ stub IWICBitmapCodecInfo_GetFileExtensions_Proxy
@ stub IWICBitmapCodecInfo_GetMimeTypes_Proxy
@ stub IWICBitmapDecoder_CopyPalette_Proxy
@ stub IWICBitmapDecoder_GetColorContexts_Proxy
@ stub IWICBitmapDecoder_GetDecoderInfo_Proxy
@ stub IWICBitmapDecoder_GetFrameCount_Proxy
@ stub IWICBitmapDecoder_GetFrame_Proxy
@ stub IWICBitmapDecoder_GetMetadataQueryReader_Proxy
@ stub IWICBitmapDecoder_GetPreview_Proxy
@ stub IWICBitmapDecoder_GetThumbnail_Proxy
@ stub IWICBitmapEncoder_Commit_Proxy
@ stub IWICBitmapEncoder_CreateNewFrame_Proxy
@ stub IWICBitmapEncoder_GetEncoderInfo_Proxy
@ stub IWICBitmapEncoder_GetMetadataQueryWriter_Proxy
@ stub IWICBitmapEncoder_Initialize_Proxy
@ stub IWICBitmapEncoder_SetPalette_Proxy
@ stub IWICBitmapEncoder_SetThumbnail_Proxy
@ stub IWICBitmapFlipRotator_Initialize_Proxy
@ stub IWICBitmapFrameDecode_GetColorContexts_Proxy
@ stub IWICBitmapFrameDecode_GetMetadataQueryReader_Proxy
@ stub IWICBitmapFrameDecode_GetThumbnail_Proxy
@ stub IWICBitmapFrameEncode_Commit_Proxy
@ stub IWICBitmapFrameEncode_GetMetadataQueryWriter_Proxy
@ stub IWICBitmapFrameEncode_Initialize_Proxy
@ stub IWICBitmapFrameEncode_SetColorContexts_Proxy
@ stub IWICBitmapFrameEncode_SetResolution_Proxy
@ stub IWICBitmapFrameEncode_SetSize_Proxy
@ stub IWICBitmapFrameEncode_SetThumbnail_Proxy
@ stub IWICBitmapFrameEncode_WriteSource_Proxy
@ stub IWICBitmapLock_GetDataPointer_STA_Proxy
@ stub IWICBitmapLock_GetStride_Proxy
@ stub IWICBitmapScaler_Initialize_Proxy
@ stub IWICBitmapSource_CopyPalette_Proxy
@ stub IWICBitmapSource_CopyPixels_Proxy
@ stub IWICBitmapSource_GetPixelFormat_Proxy
@ stub IWICBitmapSource_GetResolution_Proxy
@ stub IWICBitmapSource_GetSize_Proxy
@ stub IWICBitmap_Lock_Proxy
@ stub IWICBitmap_SetPalette_Proxy
@ stub IWICBitmap_SetResolution_Proxy
@ stub IWICColorContext_InitializeFromMemory_Proxy
@ stub IWICComponentFactory_CreateMetadataWriterFromReader_Proxy
@ stub IWICComponentFactory_CreateQueryWriterFromBlockWriter_Proxy
@ stub IWICComponentInfo_GetAuthor_Proxy
@ stub IWICComponentInfo_GetCLSID_Proxy
@ stub IWICComponentInfo_GetFriendlyName_Proxy
@ stub IWICComponentInfo_GetSpecVersion_Proxy
@ stub IWICComponentInfo_GetVersion_Proxy
@ stub IWICFastMetadataEncoder_Commit_Proxy
@ stub IWICFastMetadataEncoder_GetMetadataQueryWriter_Proxy
@ stub IWICFormatConverter_Initialize_Proxy
@ stub IWICImagingFactory_CreateBitmapClipper_Proxy
@ stub IWICImagingFactory_CreateBitmapFlipRotator_Proxy
@ stub IWICImagingFactory_CreateBitmapFromHBITMAP_Proxy
@ stub IWICImagingFactory_CreateBitmapFromHICON_Proxy
@ stub IWICImagingFactory_CreateBitmapFromMemory_Proxy
@ stub IWICImagingFactory_CreateBitmapFromSource_Proxy
@ stub IWICImagingFactory_CreateBitmapScaler_Proxy
@ stub IWICImagingFactory_CreateBitmap_Proxy
@ stub IWICImagingFactory_CreateComponentInfo_Proxy
@ stub IWICImagingFactory_CreateDecoderFromFileHandle_Proxy
@ stub IWICImagingFactory_CreateDecoderFromFilename_Proxy
@ stub IWICImagingFactory_CreateDecoderFromStream_Proxy
@ stub IWICImagingFactory_CreateEncoder_Proxy
@ stub IWICImagingFactory_CreateFastMetadataEncoderFromDecoder_Proxy
@ stub IWICImagingFactory_CreateFastMetadataEncoderFromFrameDecode_Proxy
@ stub IWICImagingFactory_CreateFormatConverter_Proxy
@ stub IWICImagingFactory_CreatePalette_Proxy
@ stub IWICImagingFactory_CreateQueryWriterFromReader_Proxy
@ stub IWICImagingFactory_CreateQueryWriter_Proxy
@ stub IWICImagingFactory_CreateStream_Proxy
@ stub IWICMetadataBlockReader_GetCount_Proxy
@ stub IWICMetadataBlockReader_GetReaderByIndex_Proxy
@ stub IWICMetadataQueryReader_GetContainerFormat_Proxy
@ stub IWICMetadataQueryReader_GetEnumerator_Proxy
@ stub IWICMetadataQueryReader_GetLocation_Proxy
@ stub IWICMetadataQueryReader_GetMetadataByName_Proxy
@ stub IWICMetadataQueryWriter_RemoveMetadataByName_Proxy
@ stub IWICMetadataQueryWriter_SetMetadataByName_Proxy
@ stub IWICPalette_GetColorCount_Proxy
@ stub IWICPalette_GetColors_Proxy
@ stub IWICPalette_GetType_Proxy
@ stub IWICPalette_HasAlpha_Proxy
@ stub IWICPalette_InitializeCustom_Proxy
@ stub IWICPalette_InitializeFromBitmap_Proxy
@ stub IWICPalette_InitializeFromPalette_Proxy
@ stub IWICPalette_InitializePredefined_Proxy
@ stub IWICPixelFormatInfo_GetBitsPerPixel_Proxy
@ stub IWICPixelFormatInfo_GetChannelCount_Proxy
@ stub IWICPixelFormatInfo_GetChannelMask_Proxy
@ stub IWICStream_InitializeFromIStream_Proxy
@ stub IWICStream_InitializeFromMemory_Proxy
@ stdcall WICConvertBitmapSource(ptr ptr ptr)
@ stub WICCreateBitmapFromSection
@ stub WICCreateColorContext_Proxy
@ stub WICCreateImagingFactory_Proxy
@ stub WICGetMetadataContentSize
@ stub WICMapGuidToShortName
@ stub WICMapSchemaToName
@ stub WICMapShortNameToGuid
@ stub WICMatchMetadataContent
@ stub WICSerializeMetadataContent
@ stub WICSetEncoderFormat_Proxy

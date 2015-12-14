/* DDK definitions for storage media access */

#ifndef _NTDDSTOR_H_
#define _NTDDSTOR_H_

#ifdef __cplusplus
extern "C" {
#endif

#define IOCTL_STORAGE_BASE FILE_DEVICE_MASS_STORAGE

#define IOCTL_STORAGE_CHECK_VERIFY      CTL_CODE(IOCTL_STORAGE_BASE, 0x0200, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_STORAGE_MEDIA_REMOVAL     CTL_CODE(IOCTL_STORAGE_BASE, 0x0201, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_STORAGE_EJECT_MEDIA       CTL_CODE(IOCTL_STORAGE_BASE, 0x0202, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_STORAGE_LOAD_MEDIA        CTL_CODE(IOCTL_STORAGE_BASE, 0x0203, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_STORAGE_RESERVE           CTL_CODE(IOCTL_STORAGE_BASE, 0x0204, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_STORAGE_RELEASE           CTL_CODE(IOCTL_STORAGE_BASE, 0x0205, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_STORAGE_FIND_NEW_DEVICES  CTL_CODE(IOCTL_STORAGE_BASE, 0x0206, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_STORAGE_GET_MEDIA_TYPES   CTL_CODE(IOCTL_STORAGE_BASE, 0x0300, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_STORAGE_GET_MEDIA_TYPES_EX CTL_CODE(IOCTL_STORAGE_BASE, 0x0301, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_STORAGE_RESET_BUS         CTL_CODE(IOCTL_STORAGE_BASE, 0x0400, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_STORAGE_RESET_DEVICE      CTL_CODE(IOCTL_STORAGE_BASE, 0x0401, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_STORAGE_GET_DEVICE_NUMBER CTL_CODE(IOCTL_STORAGE_BASE, 0x0420, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_STORAGE_QUERY_PROPERTY    CTL_CODE(IOCTL_STORAGE_BASE, 0x0500, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _STORAGE_DEVICE_NUMBER {
    DEVICE_TYPE         DeviceType;
    ULONG               DeviceNumber;
    ULONG               PartitionNumber;
} STORAGE_DEVICE_NUMBER, *PSTORAGE_DEVICE_NUMBER;

typedef struct _STORAGE_BUS_RESET_REQUEST {
    UCHAR               PathId;
} STORAGE_BUS_RESET_REQUEST, *PSTORAGE_BUS_RESET_REQUEST;

typedef enum _STORAGE_BUS_TYPE {
   BusTypeUnknown = 0,
   BusTypeScsi,
   BusTypeAtapi,
   BusTypeAta,
   BusType1394,
   BusTypeSsa,
   BusTypeFibre,
   BusTypeUsb,
   BusTypeRAID,
   BusTypeiScsi,
   BusTypeSas,
   BusTypeSata,
   BusTypeSd,
   BusTypeMmc,
   BusTypeMax,
   BusTypeMaxReserved = 0x7F
} STORAGE_BUS_TYPE, *PSTORAGE_BUS_TYPE;

typedef struct _PREVENT_MEDIA_REMOVAL {
    BOOLEAN             PreventMediaRemoval;
} PREVENT_MEDIA_REMOVAL, *PPREVENT_MEDIA_REMOVAL;

typedef struct _TAPE_STATISTICS {
    ULONG               Version;
    ULONG               Flags;
    LARGE_INTEGER       RecoveredWrites;
    LARGE_INTEGER       UnrecoveredWrites;
    LARGE_INTEGER       RecoveredReads;
    LARGE_INTEGER       UnrecoveredReads;
    UCHAR               CompressionRatioReads;
    UCHAR               CompressionRatioWrites;
} TAPE_STATISTICS, *PTAPE_STATISTICS;

#define RECOVERED_WRITES_VALID          0x00000001
#define UNRECOVERED_WRITES_VALID        0x00000002
#define RECOVERED_READS_VALID           0x00000004
#define UNRECOVERED_READS_VALID         0x00000008
#define WRITE_COMPRESSION_INFO_VALID    0x00000010
#define READ_COMPRESSION_INFO_VALID     0x00000020

typedef struct _TAPE_GET_STATISTICS {
    ULONG               Operation;
} TAPE_GET_STATISTICS, *PTAPE_GET_STATISTICS;

#define TAPE_RETURN_STATISTICS          0L
#define TAPE_RETURN_ENV_INFO            1L
#define TAPE_RESET_STATISTICS           2L

typedef enum _STORAGE_MEDIA_TYPE {
    /* see also defines in ntdddisk.h */

    DDS_4mm = 0x20,
    MiniQic,
    Travan,
    QIC,
    MP_8mm,
    AME_8mm,
    AIT1_8mm,
    DLT,
    NCTP,
    IBM_3480,
    IBM_3490E,
    IBM_Magstar_3590,
    IBM_Magstar_MP,
    STK_DATA_D3,
    SONY_DTF,
    DV_6mm,
    DMI,
    SONY_D2,
    CLEANER_CARTRIDGE,
    CD_ROM,
    CD_R,
    CD_RW,
    DVD_ROM,
    DVD_R,
    DVD_RW,
    MO_3_RW,
    MO_5_WO,
    MO_5_RW,
    MO_5_LIMDOW,
    PC_5_WO,
    PC_5_RW,
    PD_5_RW,
    ABL_5_WO,
    PINNACLE_APEX_5_RW,
    SONY_12_WO,
    PHILIPS_12_WO,
    HITACHI_12_WO,
    CYGNET_12_WO,
    KODAK_14_WO,
    MO_NFR_525,
    NIKON_12_RW,
    IOMEGA_ZIP,
    IOMEGA_JAZ,
    SYQUEST_EZ135,
    SYQUEST_EZFLYER,
    SYQUEST_SYJET,
    AVATAR_F2,
    MP2_8mm
} STORAGE_MEDIA_TYPE, *PSTORAGE_MEDIA_TYPE;

#define MEDIA_ERASEABLE         0x00000001
#define MEDIA_WRITE_ONCE        0x00000002
#define MEDIA_READ_ONLY         0x00000004
#define MEDIA_READ_WRITE        0x00000008
#define MEDIA_WRITE_PROTECTED   0x00000100
#define MEDIA_CURRENTLY_MOUNTED 0x80000000

typedef struct _DEVICE_MEDIA_INFO {
    union {
        struct {
            LARGE_INTEGER       Cylinders;
            STORAGE_MEDIA_TYPE  MediaType;
            ULONG               TracksPerCylinder;
            ULONG               SectorsPerTrack;
            ULONG               BytesPerSector;
            ULONG               NumberMediaSides;
            ULONG               MediaCharacteristics;
        } DiskInfo;
        struct {
            LARGE_INTEGER       Cylinders;
            STORAGE_MEDIA_TYPE  MediaType;
            ULONG               TracksPerCylinder;
            ULONG               SectorsPerTrack;
            ULONG               BytesPerSector;
            ULONG               NumberMediaSides;
            ULONG               MediaCharacteristics;
        } RemovableDiskInfo;
        struct {
            STORAGE_MEDIA_TYPE  MediaType;
            ULONG               MediaCharacteristics;
            ULONG               CurrentBlockSize;
            STORAGE_BUS_TYPE    BusType;
            union {
               struct {
                  UCHAR MediumType;
                  UCHAR DensityCode;
               } ScsiInformation;
            } BusSpecificData;
        } TapeInfo;
    } DeviceSpecific;
} DEVICE_MEDIA_INFO, *PDEVICE_MEDIA_INFO;

typedef struct _GET_MEDIA_TYPES {
    ULONG               DeviceType;
    ULONG               MediaInfoCount;
    DEVICE_MEDIA_INFO   MediaInfo[1];
} GET_MEDIA_TYPES, *PGET_MEDIA_TYPES;

typedef enum _STORAGE_QUERY_TYPE {
   PropertyStandardQuery = 0, 
   PropertyIncludeSwIds,
   PropertyExistsQuery, 
   PropertyMaskQuery, 
   PropertyQueryMaxDefined 
} STORAGE_QUERY_TYPE, *PSTORAGE_QUERY_TYPE;

typedef enum _STORAGE_PROPERTY_ID {
   StorageDeviceProperty = 0,
   StorageAccessAlignmentProperty,
   StorageAdapterProperty,
   StorageDeviceIdProperty,
   StorageDeviceUniqueIdProperty,
   StorageDeviceWriteCacheProperty
} STORAGE_PROPERTY_ID, *PSTORAGE_PROPERTY_ID;

typedef struct _STORAGE_PROPERTY_QUERY {
    STORAGE_PROPERTY_ID         PropertyId;
    STORAGE_QUERY_TYPE          QueryType;
    UCHAR                       AdditionalParameters[1];
} STORAGE_PROPERTY_QUERY, *PSTORAGE_PROPERTY_QUERY;

typedef struct _STORAGE_DESCRIPTOR_HEADER {
    ULONG                       Version;
    ULONG                       Size;
} STORAGE_DESCRIPTOR_HEADER, *PSTORAGE_DESCRIPTOR_HEADER;

typedef struct _STORAGE_DEVICE_DESCRIPTOR {
    ULONG                       Version;
    ULONG                       Size;
    UCHAR                       DeviceType;
    UCHAR                       DeviceTypeModifier;
    BOOLEAN                     RemovableMedia;
    BOOLEAN                     CommandQueueing;
    ULONG                       VendorIdOffset;
    ULONG                       ProductIdOffset;
    ULONG                       ProductRevisionOffset;
    ULONG                       SerialNumberOffset;
    STORAGE_BUS_TYPE            BusType;
    ULONG                       RawPropertiesLength;
    UCHAR                       RawDeviceProperties[1];
} STORAGE_DEVICE_DESCRIPTOR, *PSTORAGE_DEVICE_DESCRIPTOR;

typedef struct _STORAGE_ADAPTER_DESCRIPTOR {
    ULONG                       Version;
    ULONG                       Size;
    ULONG                       MaximumTransferLength;
    ULONG                       MaximumPhysicalPages;
    ULONG                       AlignmentMask;
    BOOLEAN                     AdapterUsesPio;
    BOOLEAN                     AdapterScansDown;
    BOOLEAN                     CommandQueueing;
    BOOLEAN                     AcceleratedTransfer;
    STORAGE_BUS_TYPE            BusType;
    USHORT                      BusMajorVersion;
    USHORT                      BusMinorVersion;
} STORAGE_ADAPTER_DESCRIPTOR, *PSTORAGE_ADAPTER_DESCRIPTOR;

typedef struct _STORAGE_DEVICE_ID_DESCRIPTOR {
   ULONG  Version;
   ULONG  Size;
   ULONG  NumberOfIdentifiers;
   UCHAR  Identifiers[1];
} STORAGE_DEVICE_ID_DESCRIPTOR, *PSTORAGE_DEVICE_ID_DESCRIPTOR;

#ifdef __cplusplus
}
#endif

#endif /* _NTDDSTOR_H_ */

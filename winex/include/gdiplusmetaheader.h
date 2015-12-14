

#ifndef _GDIPLUSMETAHEADER_H
#define _GDIPLUSMETAHEADER_H

typedef struct
{
    DWORD   iType;
    DWORD   nSize;
    RECTL   rclBounds;
    RECTL   rclFrame;
    DWORD   dSignature;
    DWORD   nVersion;
    DWORD   nBytes;
    DWORD   nRecords;
    WORD    nHandles;
    WORD    sReserved;
    DWORD   nDescription;
    DWORD   offDescription;
    DWORD   nPalEntries;
    SIZEL   szlDevice;
    SIZEL   szlMillimeters;
} ENHMETAHEADER3;

#include <pshpack2.h>

typedef struct
{
    INT16  Left;
    INT16  Top;
    INT16  Right;
    INT16  Bottom;
} PWMFRect16;

typedef struct
{
    UINT32     Key;
    INT16      Hmf;
    PWMFRect16 BoundingBox;
    INT16      Inch;
    UINT32     Reserved;
    INT16      Checksum;
} WmfPlaceableFileHeader;

#include <poppack.h>

#define GDIP_EMFPLUSFLAGS_DISPLAY       0x00000001

#ifdef __cplusplus
class MetafileHeader
{
public:
    MetafileType        Type;
    UINT                Size;
    UINT                Version;
    UINT                EmfPlusFlags;
    REAL                DpiX;
    REAL                DpiY;
    INT                 X;
    INT                 Y;
    INT                 Width;
    INT                 Height;
    union
    {
        METAHEADER      WmfHeader;
        ENHMETAHEADER3  EmfHeader;
    };
    INT                 EmfPlusHeaderSize;
    INT                 LogicalDpiX;
    INT                 LogicalDpiY;

public:
    MetafileType GetType() const { return Type; }

    UINT GetMetafileSize() const { return Size; }

    UINT GetVersion() const { return Version; }

    UINT GetEmfPlusFlags() const { return EmfPlusFlags; }

    REAL GetDpiX() const { return DpiX; }

    REAL GetDpiY() const { return DpiY; }

    VOID GetBounds (OUT Rect *r) const
    {
        r->X = X;
        r->Y = Y;
        r->Width = Width;
        r->Height = Height;
    }

    BOOL IsWmf() const
    {
       return ((Type == MetafileTypeWmf) || (Type == MetafileTypeWmfPlaceable));
    }

    BOOL IsWmfPlaceable() const { return (Type == MetafileTypeWmfPlaceable); }

    BOOL IsEmf() const { return (Type == MetafileTypeEmf); }

    BOOL IsEmfOrEmfPlus() const { return (Type >= MetafileTypeEmf); }

    BOOL IsEmfPlus() const { return (Type >= MetafileTypeEmfPlusOnly); }

    BOOL IsEmfPlusDual() const { return (Type == MetafileTypeEmfPlusDual); }

    BOOL IsEmfPlusOnly() const { return (Type == MetafileTypeEmfPlusOnly); }

    BOOL IsDisplay() const
    {
        return IsEmfPlus() && ((EmfPlusFlags & GDIP_EMFPLUSFLAGS_DISPLAY) != 0);
    }

    const METAHEADER * GetWmfHeader() const
    {
        return IsWmf() ? &WmfHeader : NULL;
    }

    const ENHMETAHEADER3 * GetEmfHeader() const
    {
        return IsEmfOrEmfPlus() ? &EmfHeader : NULL;
    }
};
#else 

typedef struct MetafileHeader
{
    MetafileType        Type;
    UINT                Size;
    UINT                Version;
    UINT                EmfPlusFlags;
    REAL                DpiX;
    REAL                DpiY;
    INT                 X;
    INT                 Y;
    INT                 Width;
    INT                 Height;
    union
    {
        METAHEADER      WmfHeader;
        ENHMETAHEADER3  EmfHeader;
    } DUMMYUNIONNAME;
    INT                 EmfPlusHeaderSize;
    INT                 LogicalDpiX;
    INT                 LogicalDpiY;
} MetafileHeader;

#endif 

#endif 

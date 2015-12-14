

#ifndef _GDIPLUSTYPES_H
#define _GDIPLUSTYPES_H

typedef float REAL;

enum Status{
    Ok                          = 0,
    GenericError                = 1,
    InvalidParameter            = 2,
    OutOfMemory                 = 3,
    ObjectBusy                  = 4,
    InsufficientBuffer          = 5,
    NotImplemented              = 6,
    Win32Error                  = 7,
    WrongState                  = 8,
    Aborted                     = 9,
    FileNotFound                = 10,
    ValueOverflow               = 11,
    AccessDenied                = 12,
    UnknownImageFormat          = 13,
    FontFamilyNotFound          = 14,
    FontStyleNotFound           = 15,
    NotTrueTypeFont             = 16,
    UnsupportedGdiplusVersion   = 17,
    GdiplusNotInitialized       = 18,
    PropertyNotFound            = 19,
    PropertyNotSupported        = 20,
    ProfileNotFound             = 21
};


#ifdef __cplusplus
extern "C" {
#endif

typedef BOOL (CALLBACK * ImageAbort)(VOID *);
typedef ImageAbort DrawImageAbort;
typedef ImageAbort GetThumbnailImageAbort;

#ifdef __cplusplus
}
#endif


#ifdef __cplusplus

class Point
{
public:
   Point()
   {
       X = Y = 0;
   }

   Point(IN const Point &pt)
   {
       X = pt.X;
       Y = pt.Y;
   }

   

   Point(IN INT x, IN INT y)
   {
       X = x;
       Y = y;
   }

   Point operator+(IN const Point& pt) const
   {
       return Point(X + pt.X, Y + pt.Y);
   }

   Point operator-(IN const Point& pt) const
   {
       return Point(X - pt.X, Y - pt.Y);
   }

   BOOL Equals(IN const Point& pt)
   {
       return (X == pt.X) && (Y == pt.Y);
   }

public:
    INT X;
    INT Y;
};

class PointF
{
public:
   PointF()
   {
       X = Y = 0.0f;
   }

   PointF(IN const PointF &pt)
   {
       X = pt.X;
       Y = pt.Y;
   }

   

   PointF(IN REAL x, IN REAL y)
   {
       X = x;
       Y = y;
   }

   PointF operator+(IN const PointF& pt) const
   {
       return PointF(X + pt.X, Y + pt.Y);
   }

   PointF operator-(IN const PointF& pt) const
   {
       return PointF(X - pt.X, Y - pt.Y);
   }

   BOOL Equals(IN const PointF& pt)
   {
       return (X == pt.X) && (Y == pt.Y);
   }

public:
    REAL X;
    REAL Y;
};

class PathData
{
public:
    PathData()
    {
        Count = 0;
        Points = NULL;
        Types = NULL;
    }

    ~PathData()
    {
        if (Points != NULL)
        {
            delete Points;
        }

        if (Types != NULL)
        {
            delete Types;
        }
    }

private:
    PathData(const PathData &);
    PathData& operator=(const PathData &);

public:
    INT Count;
    PointF* Points;
    BYTE* Types;
};


class RectF
{
public:
    REAL X;
    REAL Y;
    REAL Width;
    REAL Height;
};


class Rect
{
public:
    INT X;
    INT Y;
    INT Width;
    INT Height;
};

#else 

typedef struct Point
{
    INT X;
    INT Y;
} Point;

typedef struct PointF
{
    REAL X;
    REAL Y;
} PointF;

typedef struct PathData
{
    INT Count;
    PointF* Points;
    BYTE* Types;
} PathData;

typedef struct RectF
{
    REAL X;
    REAL Y;
    REAL Width;
    REAL Height;
} RectF;

typedef struct Rect
{
    INT X;
    INT Y;
    INT Width;
    INT Height;
} Rect;

typedef struct CharacterRange
{
    INT First;
    INT Length;
} CharacterRange;

typedef enum Status Status;

#endif 

#endif

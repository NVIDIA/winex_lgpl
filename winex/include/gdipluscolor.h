

#ifndef _GDIPLUSCOLOR_H
#define _GDIPLUSCOLOR_H

enum ColorChannelFlags
{
    ColorChannelFlagsC,
    ColorChannelFlagsM,
    ColorChannelFlagsY,
    ColorChannelFlagsK,
    ColorChannelFlagsLast
};

#ifdef __cplusplus


class Color
{
protected:
    ARGB Argb;
};

#else 

typedef struct Color
{
    ARGB Argb;
} Color;

typedef enum ColorChannelFlags ColorChannelFlags;

#endif  

#endif  

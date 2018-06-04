/*
 * VARIANT
 *
 * Copyright 1998 Jean-Claude Cote
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 * Copyright (c) 2006-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * NOTES
 *   This implements the low-level and hi-level APIs for manipulating VARIANTs.
 *   The low-level APIs are used to do data coercion between different data types.
 *   The hi-level APIs are built on top of these low-level APIs and handle
 *   initialization, copying, destroying and changing the type of VARIANTs.
 *
 * TODO:
 *   - The Variant APIs do not support international languages, currency
 *     types, number formating and calendar.  They only support U.S. English format.
 *   - The Variant APIs do not the following types: IUknown, IDispatch, DECIMAL and SafeArray.
 *     The prototypes for these are commented out in the oleauto.h file.  They need
 *     to be implemented and cases need to be added to the switches of the  existing APIs.
 *   - The parsing of date for the VarDateFromStr is not complete.
 *   - The date manipulations do not support dates prior to 1900.
 *   - The parsing does not accept as many formats as the Windows implementation.
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#ifdef HAVE_FLOAT_H
# include <float.h>
#endif

#include "windef.h"
#include "objbase.h"
#include "oleauto.h"
#include "wine/heapstr.h"
#include "wine/debug.h"
#include "winerror.h"
#include "parsedt.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole);

#define SYSDUPSTRING(str) SysAllocStringLen((str), SysStringLen(str))

#ifndef FLT_MAX
# ifdef MAXFLOAT
#  define FLT_MAX MAXFLOAT
# else
#  error "Can't find #define for MAXFLOAT/FLT_MAX"
# endif
#endif

#undef CHAR_MAX
#undef CHAR_MIN
static const char CHAR_MAX = 127;
static const char CHAR_MIN = -128;
static const BYTE UI1_MAX = 255;
static const BYTE UI1_MIN = 0;
static const unsigned short UI2_MAX = 65535;
static const unsigned short UI2_MIN = 0;
static const short I2_MAX = 32767;
static const short I2_MIN =  -32768;
static const unsigned long UI4_MAX = 4294967295U;
static const unsigned long UI4_MIN = 0;
static const long I4_MAX = 2147483647;
static const long I4_MIN = -(2147483648U);
static const DATE DATE_MIN = -657434;
static const DATE DATE_MAX = 2958465;

/* the largest valid type
 */
#define VT_MAXVALIDTYPE VT_CLSID

/* This mask is used to set a flag in wReserved1 of
 * the VARIANTARG structure. The flag indicates if
 * the API function is using an inner variant or not.
 */
#define PROCESSING_INNER_VARIANT 0x0001

/* General use buffer.
 */
#define BUFFER_MAX 1024
static char pBuffer[BUFFER_MAX];

/*
 * Note a leap year is one that is a multiple of 4
 * but not of a 100.  Except if it is a multiple of
 * 400 then it is a leap year.
 */
/* According to postgreSQL date parsing functions there is
 * a leap year when this expression is true.
 * (((y % 4) == 0) && (((y % 100) != 0) || ((y % 400) == 0)))
 * So according to this there is 365.2515 days in one year.
 * One + every four years: 1/4 -> 365.25
 * One - every 100 years: 1/100 -> 365.01
 * One + every 400 years: 1/400 -> 365.0025
 */
/* static const double DAYS_IN_ONE_YEAR = 365.2515;
 *
 *  ^^  Might this be the key to an easy way to factor large prime numbers?
 *  Let's try using arithmetic.  <lawson_whitney@juno.com> 7 Mar 2000
 */
static const double DAYS_IN_ONE_YEAR = 365.2425;


/******************************************************************************
 *	   DateTimeStringToTm	[INTERNAL]
 *
 * Converts a string representation of a date and/or time to a tm structure.
 *
 * Note this function uses the postgresql date parsing functions found
 * in the parsedt.c file.
 *
 * Returns TRUE if successful.
 *
 * Note: This function does not parse the day of the week,
 * daylight savings time. It will only fill the followin fields in
 * the tm struct, tm_sec, tm_min, tm_hour, tm_year, tm_day, tm_mon.
 *
 ******************************************************************************/
static BOOL DateTimeStringToTm( OLECHAR* strIn, DWORD dwFlags, struct tm* pTm )
{
	BOOL res = FALSE;
	double		fsec;
	int 		tzp;
	int 		dtype;
	int 		nf;
	char	   *field[MAXDATEFIELDS];
	int 		ftype[MAXDATEFIELDS];
	char		lowstr[MAXDATELEN + 1];
	char* strDateTime = NULL;

	/* Convert the string to ASCII since this is the only format
	 * postgesql can handle.
	 */
	strDateTime = HEAP_strdupWtoA( GetProcessHeap(), 0, strIn );

	if( strDateTime != NULL )
	{
		/* Make sure we don't go over the maximum length
		 * accepted by postgesql.
		 */
		if( strlen( strDateTime ) <= MAXDATELEN )
		{
			if( ParseDateTime( strDateTime, lowstr, field, ftype, MAXDATEFIELDS, &nf) == 0 )
			{
				if( dwFlags & VAR_DATEVALUEONLY )
				{
					/* Get the date information.
					 * It returns 0 if date information was
					 * present and 1 if only time information was present.
					 * -1 if an error occures.
					 */
					if( DecodeDateTime(field, ftype, nf, &dtype, pTm, &fsec, &tzp) == 0 )
					{
						/* Eliminate the time information since we
						 * were asked to get date information only.
						 */
						pTm->tm_sec = 0;
						pTm->tm_min = 0;
						pTm->tm_hour = 0;
						res = TRUE;
					}
				}
				if( dwFlags & VAR_TIMEVALUEONLY )
				{
					/* Get time information only.
					 */
					if( DecodeTimeOnly(field, ftype, nf, &dtype, pTm, &fsec) == 0 )
					{
						res = TRUE;
					}
				}
				else
				{
					/* Get both date and time information.
					 * It returns 0 if date information was
					 * present and 1 if only time information was present.
					 * -1 if an error occures.
					 */
					if( DecodeDateTime(field, ftype, nf, &dtype, pTm, &fsec, &tzp) != -1 )
					{
						res = TRUE;
					}
				}
			}
		}
		HeapFree( GetProcessHeap(), 0, strDateTime );
	}

	return res;
}






/******************************************************************************
 *	   TmToDATE 	[INTERNAL]
 *
 * The date is implemented using an 8 byte floating-point number.
 * Days are represented by whole numbers increments starting with 0.00 has
 * being December 30 1899, midnight.
 * The hours are expressed as the fractional part of the number.
 * December 30 1899 at midnight = 0.00
 * January 1 1900 at midnight = 2.00
 * January 4 1900 at 6 AM = 5.25
 * January 4 1900 at noon = 5.50
 * December 29 1899 at midnight = -1.00
 * December 18 1899 at midnight = -12.00
 * December 18 1899 at 6AM = -12.25
 * December 18 1899 at 6PM = -12.75
 * December 19 1899 at midnight = -11.00
 * The tm structure is as follows:
 * struct tm {
 *		  int tm_sec;	   seconds after the minute - [0,59]
 *		  int tm_min;	   minutes after the hour - [0,59]
 *		  int tm_hour;	   hours since midnight - [0,23]
 *		  int tm_mday;	   day of the month - [1,31]
 *		  int tm_mon;	   months since January - [0,11]
 *		  int tm_year;	   years
 *		  int tm_wday;	   days since Sunday - [0,6]
 *		  int tm_yday;	   days since January 1 - [0,365]
 *		  int tm_isdst;    daylight savings time flag
 *		  };
 *
 * Note: This function does not use the tm_wday, tm_yday, tm_wday,
 * and tm_isdst fields of the tm structure. And only converts years
 * after 1900.
 *
 * Returns TRUE if successful.
 */
static BOOL TmToDATE( struct tm* pTm, DATE *pDateOut )
{
    int leapYear = 0;

    if( (pTm->tm_year - 1900) < 0 ) return FALSE;

    /* Start at 1. This is the way DATE is defined.
     * January 1, 1900 at Midnight is 1.00.
     * January 1, 1900 at 6AM is 1.25.
     * and so on.
     */
    *pDateOut = 1;

    /* Add the number of days corresponding to
     * tm_year.
     */
    *pDateOut += (pTm->tm_year - 1900) * 365;

    /* Add the leap days in the previous years between now and 1900.
     * Note a leap year is one that is a multiple of 4
     * but not of a 100.  Except if it is a multiple of
     * 400 then it is a leap year.
     */
    *pDateOut += ( (pTm->tm_year - 1) / 4 ) - ( 1900 / 4 );
    *pDateOut -= ( (pTm->tm_year - 1) / 100 ) - ( 1900 / 100 );
    *pDateOut += ( (pTm->tm_year - 1) / 400 ) - ( 1900 / 400 );

    /* Set the leap year flag if the
     * current year specified by tm_year is a
     * leap year. This will be used to add a day
     * to the day count.
     */
    if( isleap( pTm->tm_year ) )
        leapYear = 1;

    /* Add the number of days corresponding to
     * the month.
     */
    switch( pTm->tm_mon )
    {
    case 2:
        *pDateOut += 31;
        break;
    case 3:
        *pDateOut += ( 59 + leapYear );
        break;
    case 4:
        *pDateOut += ( 90 + leapYear );
        break;
    case 5:
        *pDateOut += ( 120 + leapYear );
        break;
    case 6:
        *pDateOut += ( 151 + leapYear );
        break;
    case 7:
        *pDateOut += ( 181 + leapYear );
        break;
    case 8:
        *pDateOut += ( 212 + leapYear );
        break;
    case 9:
        *pDateOut += ( 243 + leapYear );
        break;
    case 10:
        *pDateOut += ( 273 + leapYear );
        break;
    case 11:
        *pDateOut += ( 304 + leapYear );
        break;
    case 12:
        *pDateOut += ( 334 + leapYear );
        break;
    }
    /* Add the number of days in this month.
     */
    *pDateOut += pTm->tm_mday;

    /* Add the number of seconds, minutes, and hours
     * to the DATE. Note these are the fracionnal part
     * of the DATE so seconds / number of seconds in a day.
     */
    *pDateOut += pTm->tm_hour / 24.0;
    *pDateOut += pTm->tm_min / 1440.0;
    *pDateOut += pTm->tm_sec / 86400.0;
    return TRUE;
}

/******************************************************************************
 *	   DateToTm 	[INTERNAL]
 *
 * This function converts a windows DATE to a tm structure.
 *
 * It does not fill all the fields of the tm structure.
 * Here is a list of the fields that are filled:
 * tm_sec, tm_min, tm_hour, tm_year, tm_day, tm_mon.
 *
 * Note this function does not support dates before the January 1, 1900
 * or ( dateIn < 2.0 ).
 *
 * Returns TRUE if successful.
 */
static BOOL DateToTm( DATE dateIn, DWORD dwFlags, struct tm* pTm )
{
    double decimalPart = 0.0;
    double wholePart = 0.0;

    /* Do not process dates smaller than January 1, 1900.
     * Which corresponds to 2.0 in the windows DATE format.
     */
    if( dateIn < 2.0 ) return FALSE;

    memset(pTm,0,sizeof(*pTm));

    /* Because of the nature of DATE format which
     * associates 2.0 to January 1, 1900. We will
     * remove 1.0 from the whole part of the DATE
     * so that in the following code 1.0
     * will correspond to January 1, 1900.
     * This simplifies the processing of the DATE value.
     */
    dateIn -= 1.0;

    wholePart = (double) floor( dateIn );
    decimalPart = fmod( dateIn, wholePart );

    if( !(dwFlags & VAR_TIMEVALUEONLY) )
    {
        int nDay = 0;
        int leapYear = 0;
        double yearsSince1900 = 0;
        /* Start at 1900, this is where the DATE time 0.0 starts.
         */
        pTm->tm_year = 1900;
        /* find in what year the day in the "wholePart" falls into.
         * add the value to the year field.
         */
        yearsSince1900 = floor( (wholePart / DAYS_IN_ONE_YEAR) + 0.001 );
        pTm->tm_year += yearsSince1900;
        /* determine if this is a leap year.
         */
        if( isleap( pTm->tm_year ) )
        {
            leapYear = 1;
            wholePart++;
        }

        /* find what day of that year the "wholePart" corresponds to.
         * Note: nDay is in [1-366] format
         */
        nDay = (int) ( wholePart - floor( yearsSince1900 * DAYS_IN_ONE_YEAR ) );
        /* Set the tm_yday value.
         * Note: The day must be converted from [1-366] to [0-365]
         */
        /*pTm->tm_yday = nDay - 1;*/
        /* find which month this day corresponds to.
         */
        if( nDay <= 31 )
        {
            pTm->tm_mday = nDay;
            pTm->tm_mon = 0;
        }
        else if( nDay <= ( 59 + leapYear ) )
        {
            pTm->tm_mday = nDay - 31;
            pTm->tm_mon = 1;
        }
        else if( nDay <= ( 90 + leapYear ) )
        {
            pTm->tm_mday = nDay - ( 59 + leapYear );
            pTm->tm_mon = 2;
        }
        else if( nDay <= ( 120 + leapYear ) )
        {
            pTm->tm_mday = nDay - ( 90 + leapYear );
            pTm->tm_mon = 3;
        }
        else if( nDay <= ( 151 + leapYear ) )
        {
            pTm->tm_mday = nDay - ( 120 + leapYear );
            pTm->tm_mon = 4;
        }
        else if( nDay <= ( 181 + leapYear ) )
        {
            pTm->tm_mday = nDay - ( 151 + leapYear );
            pTm->tm_mon = 5;
        }
        else if( nDay <= ( 212 + leapYear ) )
        {
            pTm->tm_mday = nDay - ( 181 + leapYear );
            pTm->tm_mon = 6;
        }
        else if( nDay <= ( 243 + leapYear ) )
        {
            pTm->tm_mday = nDay - ( 212 + leapYear );
            pTm->tm_mon = 7;
        }
        else if( nDay <= ( 273 + leapYear ) )
        {
            pTm->tm_mday = nDay - ( 243 + leapYear );
            pTm->tm_mon = 8;
        }
        else if( nDay <= ( 304 + leapYear ) )
        {
            pTm->tm_mday = nDay - ( 273 + leapYear );
            pTm->tm_mon = 9;
        }
        else if( nDay <= ( 334 + leapYear ) )
        {
            pTm->tm_mday = nDay - ( 304 + leapYear );
            pTm->tm_mon = 10;
        }
        else if( nDay <= ( 365 + leapYear ) )
        {
            pTm->tm_mday = nDay - ( 334 + leapYear );
            pTm->tm_mon = 11;
        }
    }
    if( !(dwFlags & VAR_DATEVALUEONLY) )
    {
        /* find the number of seconds in this day.
         * fractional part times, hours, minutes, seconds.
         */
        pTm->tm_hour = (int) ( decimalPart * 24 );
        pTm->tm_min = (int) ( ( ( decimalPart * 24 ) - pTm->tm_hour ) * 60 );
        pTm->tm_sec = (int) ( ( ( decimalPart * 24 * 60 ) - ( pTm->tm_hour * 60 ) - pTm->tm_min ) * 60 );
    }
    return TRUE;
}



/******************************************************************************
 *	   SizeOfVariantData   	[INTERNAL]
 *
 * This function finds the size of the data referenced by a Variant based
 * the type "vt" of the Variant.
 */
static int SizeOfVariantData( VARIANT* parg )
{
    int size = 0;
    switch( V_VT(parg) & VT_TYPEMASK )
    {
    case( VT_I2 ):
        size = sizeof(short);
        break;
    case( VT_INT ):
        size = sizeof(int);
        break;
    case( VT_I4 ):
        size = sizeof(long);
        break;
    case( VT_UI1 ):
        size = sizeof(BYTE);
        break;
    case( VT_UI2 ):
        size = sizeof(unsigned short);
        break;
    case( VT_UINT ):
        size = sizeof(unsigned int);
        break;
    case( VT_UI4 ):
        size = sizeof(unsigned long);
        break;
    case( VT_R4 ):
        size = sizeof(float);
        break;
    case( VT_R8 ):
        size = sizeof(double);
        break;
    case( VT_DATE ):
        size = sizeof(DATE);
        break;
    case( VT_BOOL ):
        size = sizeof(VARIANT_BOOL);
        break;
    case( VT_BSTR ):
        size = sizeof(void*);
        break;
    case( VT_CY ):
    case( VT_DISPATCH ):
    case( VT_UNKNOWN ):
    case( VT_DECIMAL ):
    default:
        FIXME("Add size information for type vt=%d\n", V_VT(parg) & VT_TYPEMASK );
        break;
    }

    return size;
}
/******************************************************************************
 *	   StringDupAtoBstr		[INTERNAL]
 *
 */
static BSTR StringDupAtoBstr( char* strIn )
{
	BSTR bstr = NULL;
	OLECHAR* pNewString = NULL;
	pNewString = HEAP_strdupAtoW( GetProcessHeap(), 0, strIn );
	bstr = SysAllocString( pNewString );
	HeapFree( GetProcessHeap(), 0, pNewString );
	return bstr;
}

/******************************************************************************
 *		oleaut32_round		[INTERNAL]
 *
 * Round the double value to the nearest integer value.
 */
static double oleaut32_round( double d )
{
   double decimals = 0.0, integerValue = 0.0, roundedValue = 0.0;
    BOOL bEvenNumber = FALSE;
    int nSign = 0;

    /* Save the sign of the number
     */
   nSign = (d >= 0.0) ? 1 : -1;
    d = fabs( d );

	/* Remove the decimals.
	 */
   integerValue = floor( d );

    /* Set the Even flag.  This is used to round the number when
     * the decimals are exactly 1/2.  If the integer part is
     * odd the number is rounded up. If the integer part
     * is even the number is rounded down.  Using this method
     * numbers are rounded up|down half the time.
     */
   bEvenNumber = (((short)fmod(integerValue, 2)) == 0) ? TRUE : FALSE;

    /* Remove the integral part of the number.
     */
    decimals = d - integerValue;

	/* Note: Ceil returns the smallest integer that is greater that x.
	 * and floor returns the largest integer that is less than or equal to x.
	 */
    if( decimals > 0.5 )
    {
        /* If the decimal part is greater than 1/2
         */
        roundedValue = ceil( d );
    }
    else if( decimals < 0.5 )
    {
        /* If the decimal part is smaller than 1/2
         */
        roundedValue = floor( d );
    }
    else
    {
        /* the decimals are exactly 1/2 so round according to
         * the bEvenNumber flag.
         */
        if( bEvenNumber )
        {
            roundedValue = floor( d );
        }
        else
        {
            roundedValue = ceil( d );
        }
    }

	return roundedValue * nSign;
}

/******************************************************************************
 *		RemoveCharacterFromString		[INTERNAL]
 *
 * Removes any of the characters in "strOfCharToRemove" from the "str" argument.
 */
static void RemoveCharacterFromString( LPSTR str, LPSTR strOfCharToRemove )
{
	LPSTR pNewString = NULL;
	LPSTR strToken = NULL;

	/* Check if we have a valid argument
	 */
	if( str != NULL )
	{
		pNewString = strdup( str );
		str[0] = '\0';
		strToken = strtok( pNewString, strOfCharToRemove );
		while( strToken != NULL ) {
			strcat( str, strToken );
			strToken = strtok( NULL, strOfCharToRemove );
		}
		free( pNewString );
	}
	return;
}

/******************************************************************************
 *		GetValidRealString		[INTERNAL]
 *
 * Checks if the string is of proper format to be converted to a real value.
 */
static BOOL IsValidRealString( LPSTR strRealString )
{
	/* Real values that have a decimal point are required to either have
	 * digits before or after the decimal point.  We will assume that
	 * we do not have any digits at either position. If we do encounter
	 * some we will disable this flag.
	 */
	BOOL bDigitsRequired = TRUE;
	/* Processed fields in the string representation of the real number.
	 */
	BOOL bWhiteSpaceProcessed = FALSE;
	BOOL bFirstSignProcessed = FALSE;
	BOOL bFirstDigitsProcessed = FALSE;
	BOOL bDecimalPointProcessed = FALSE;
	BOOL bSecondDigitsProcessed = FALSE;
	BOOL bExponentProcessed = FALSE;
	BOOL bSecondSignProcessed = FALSE;
	BOOL bThirdDigitsProcessed = FALSE;
	/* Assume string parameter "strRealString" is valid and try to disprove it.
	 */
	BOOL bValidRealString = TRUE;

	/* Used to count the number of tokens in the "strRealString".
	 */
	LPSTR strToken = NULL;
	int nTokens = 0;
	LPSTR pChar = NULL;

	/* Check if we have a valid argument
	 */
	if( strRealString == NULL )
	{
		bValidRealString = FALSE;
	}

	if( bValidRealString == TRUE )
	{
		/* Make sure we only have ONE token in the string.
		 */
		strToken = strtok( strRealString, " " );
		while( strToken != NULL ) {
			nTokens++;
			strToken = strtok( NULL, " " );
		}

		if( nTokens != 1 )
		{
			bValidRealString = FALSE;
		}
	}


	/* Make sure this token contains only valid characters.
	 * The string argument to atof has the following form:
	 * [whitespace] [sign] [digits] [.digits] [ {d | D | e | E }[sign]digits]
	 * Whitespace consists of space and|or <TAB> characters, which are ignored.
     * Sign is either plus '+' or minus '-'.
     * Digits are one or more decimal digits.
     * Note: If no digits appear before the decimal point, at least one must
     * appear after the decimal point.
     * The decimal digits may be followed by an exponent.
     * An Exponent consists of an introductory letter ( D, d, E, or e) and
	 * an optionally signed decimal integer.
	 */
	pChar = strRealString;
	while( bValidRealString == TRUE && *pChar != '\0' )
	{
		switch( *pChar )
		{
		/* If whitespace...
		 */
		case ' ':
		case '\t':
			if( bWhiteSpaceProcessed ||
				bFirstSignProcessed ||
				bFirstDigitsProcessed ||
				bDecimalPointProcessed ||
				bSecondDigitsProcessed ||
				bExponentProcessed ||
				bSecondSignProcessed ||
				bThirdDigitsProcessed )
			{
				bValidRealString = FALSE;
			}
			break;
		/* If sign...
		 */
		case '+':
		case '-':
			if( bFirstSignProcessed == FALSE )
			{
				if( bFirstDigitsProcessed ||
					bDecimalPointProcessed ||
					bSecondDigitsProcessed ||
					bExponentProcessed ||
					bSecondSignProcessed ||
					bThirdDigitsProcessed )
				{
					bValidRealString = FALSE;
				}
				bWhiteSpaceProcessed = TRUE;
				bFirstSignProcessed = TRUE;
			}
			else if( bSecondSignProcessed == FALSE )
			{
                /* Note: The exponent must be present in
				 * order to accept the second sign...
				 */
				if( bExponentProcessed == FALSE ||
					bThirdDigitsProcessed ||
					bDigitsRequired )
				{
					bValidRealString = FALSE;
				}
				bFirstSignProcessed = TRUE;
				bWhiteSpaceProcessed = TRUE;
				bFirstDigitsProcessed = TRUE;
				bDecimalPointProcessed = TRUE;
				bSecondDigitsProcessed = TRUE;
				bSecondSignProcessed = TRUE;
			}
			break;

		/* If decimals...
		 */
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			if( bFirstDigitsProcessed == FALSE )
			{
				if( bDecimalPointProcessed ||
					bSecondDigitsProcessed ||
					bExponentProcessed ||
					bSecondSignProcessed ||
					bThirdDigitsProcessed )
				{
					bValidRealString = FALSE;
				}
				bFirstSignProcessed = TRUE;
				bWhiteSpaceProcessed = TRUE;
				/* We have found some digits before the decimal point
				 * so disable the "Digits required" flag.
				 */
				bDigitsRequired = FALSE;
			}
			else if( bSecondDigitsProcessed == FALSE )
			{
				if( bExponentProcessed ||
					bSecondSignProcessed ||
					bThirdDigitsProcessed )
				{
					bValidRealString = FALSE;
				}
				bFirstSignProcessed = TRUE;
				bWhiteSpaceProcessed = TRUE;
				bFirstDigitsProcessed = TRUE;
				bDecimalPointProcessed = TRUE;
				/* We have found some digits after the decimal point
				 * so disable the "Digits required" flag.
				 */
				bDigitsRequired = FALSE;
			}
			else if( bThirdDigitsProcessed == FALSE )
			{
				/* Getting here means everything else should be processed.
                 * If we get anything else than a decimal following this
                 * digit it will be flagged by the other cases, so
				 * we do not really need to do anything in here.
				 */
			}
			break;
		/* If DecimalPoint...
		 */
		case '.':
			if( bDecimalPointProcessed ||
				bSecondDigitsProcessed ||
				bExponentProcessed ||
				bSecondSignProcessed ||
				bThirdDigitsProcessed )
			{
				bValidRealString = FALSE;
			}
			bFirstSignProcessed = TRUE;
			bWhiteSpaceProcessed = TRUE;
			bFirstDigitsProcessed = TRUE;
			bDecimalPointProcessed = TRUE;
			break;
		/* If Exponent...
		 */
		case 'e':
		case 'E':
		case 'd':
		case 'D':
			if( bExponentProcessed ||
				bSecondSignProcessed ||
				bThirdDigitsProcessed ||
				bDigitsRequired )
			{
				bValidRealString = FALSE;
			}
			bFirstSignProcessed = TRUE;
			bWhiteSpaceProcessed = TRUE;
			bFirstDigitsProcessed = TRUE;
			bDecimalPointProcessed = TRUE;
			bSecondDigitsProcessed = TRUE;
			bExponentProcessed = TRUE;
			break;
		default:
			bValidRealString = FALSE;
			break;
		}
		/* Process next character.
		 */
		pChar++;
	}

	/* If the required digits were not present we have an invalid
	 * string representation of a real number.
	 */
	if( bDigitsRequired == TRUE )
	{
		bValidRealString = FALSE;
	}

	return bValidRealString;
}


/******************************************************************************
 *		Coerce	[INTERNAL]
 *
 * This function dispatches execution to the proper conversion API
 * to do the necessary coercion.
 *
 * FIXME: Passing down dwFlags to the conversion functions is wrong, this
 * 	  is a different flagmask. Check MSDN.
 */
static HRESULT Coerce( VARIANTARG* pd, LCID lcid, ULONG dwFlags, VARIANTARG* ps, VARTYPE vt )
{
	HRESULT res = S_OK;
	unsigned short vtFrom = 0;
	vtFrom = V_VT(ps) & VT_TYPEMASK;

	TRACE( "pd=%p, lcid=%lu, dwFlags=%lu, ps=%p (from: vt=%d to: vt=%d)\n", pd, lcid, dwFlags, ps, V_VT(ps), vt );


	/* Note: Since "long" and "int" values both have 4 bytes and are
	 * both signed integers "int" will be treated as "long" in the
	 * following code.
	 * The same goes for their unsigned versions.
	 */

	/* Trivial Case: If the coercion is from two types that are
	 * identical then we can blindly copy from one argument to another.*/
	if ((vt==vtFrom))
	{
	   return VariantCopy(pd,ps);
	}

	/* Cases requiring thought*/
	switch( vt )
	{

    case( VT_EMPTY ):
        res = VariantClear( pd );
        break;
    case( VT_NULL ):
        res = VariantClear( pd );
        if( res == S_OK )
        {
            V_VT(pd) = VT_NULL;
        }
        break;
	case( VT_I1 ):
		switch( vtFrom )
        {
        case( VT_I1 ):
            res = VariantCopy( pd, ps );
            break;
		case( VT_I2 ):
			res = VarI1FromI2( V_UNION(ps,iVal), &V_UNION(pd,cVal) );
			break;
		case( VT_INT ):
		case( VT_I4 ):
			res = VarI1FromI4( V_UNION(ps,lVal), &V_UNION(pd,cVal) );
			break;
		case( VT_UI1 ):
			res = VarI1FromUI1( V_UNION(ps,bVal), &V_UNION(pd,cVal) );
			break;
		case( VT_UI2 ):
			res = VarI1FromUI2( V_UNION(ps,uiVal), &V_UNION(pd,cVal) );
			break;
		case( VT_UINT ):
		case( VT_UI4 ):
			res = VarI1FromUI4( V_UNION(ps,ulVal), &V_UNION(pd,cVal) );
			break;
		case( VT_R4 ):
			res = VarI1FromR4( V_UNION(ps,fltVal), &V_UNION(pd,cVal) );
			break;
		case( VT_R8 ):
			res = VarI1FromR8( V_UNION(ps,dblVal), &V_UNION(pd,cVal) );
			break;
		case( VT_DATE ):
			res = VarI1FromDate( V_UNION(ps,date), &V_UNION(pd,cVal) );
			break;
		case( VT_BOOL ):
			res = VarI1FromBool( V_UNION(ps,boolVal), &V_UNION(pd,cVal) );
			break;
		case( VT_BSTR ):
			res = VarI1FromStr( V_UNION(ps,bstrVal), lcid, 0, &V_UNION(pd,cVal) );
			break;
		case( VT_CY ):
			res = VarI1FromCy( V_UNION(ps,cyVal), &V_UNION(pd,cVal) );
			break;
		case( VT_DISPATCH ):
			/*res = VarI1FromDisp( V_UNION(ps,pdispVal), lcid, &V_UNION(pd,cVal) );*/
		case( VT_DECIMAL ):
			/*res = VarI1FromDec( V_UNION(ps,decVal), &V_UNION(pd,cVal) );*/
		case( VT_UNKNOWN ):
		default:
			res = DISP_E_TYPEMISMATCH;
			FIXME("Coercion from %d to %d\n", vtFrom, vt );
			break;
		}
		break;

	case( VT_I2 ):
		switch( vtFrom )
		{
		case( VT_I1 ):
			res = VarI2FromI1( V_UNION(ps,cVal), &V_UNION(pd,iVal) );
			break;
        case( VT_I2 ):
            res = VariantCopy( pd, ps );
            break;
		case( VT_INT ):
		case( VT_I4 ):
			res = VarI2FromI4( V_UNION(ps,lVal), &V_UNION(pd,iVal) );
			break;
		case( VT_UI1 ):
			res = VarI2FromUI1( V_UNION(ps,bVal), &V_UNION(pd,iVal) );
			break;
		case( VT_UI2 ):
			res = VarI2FromUI2( V_UNION(ps,uiVal), &V_UNION(pd,iVal) );
			break;
		case( VT_UINT ):
		case( VT_UI4 ):
			res = VarI2FromUI4( V_UNION(ps,ulVal), &V_UNION(pd,iVal) );
			break;
		case( VT_R4 ):
			res = VarI2FromR4( V_UNION(ps,fltVal), &V_UNION(pd,iVal) );
			break;
		case( VT_R8 ):
			res = VarI2FromR8( V_UNION(ps,dblVal), &V_UNION(pd,iVal) );
			break;
		case( VT_DATE ):
			res = VarI2FromDate( V_UNION(ps,date), &V_UNION(pd,iVal) );
			break;
		case( VT_BOOL ):
			res = VarI2FromBool( V_UNION(ps,boolVal), &V_UNION(pd,iVal) );
			break;
		case( VT_BSTR ):
			res = VarI2FromStr( V_UNION(ps,bstrVal), lcid, 0, &V_UNION(pd,iVal) );
			break;
		case( VT_CY ):
			res = VarI2FromCy( V_UNION(ps,cyVal), &V_UNION(pd,iVal) );
			break;
		case( VT_DISPATCH ):
			/*res = VarI2FromDisp( V_UNION(ps,pdispVal), lcid, &V_UNION(pd,iVal) );*/
		case( VT_DECIMAL ):
			/*res = VarI2FromDec( V_UNION(ps,deiVal), &V_UNION(pd,iVal) );*/
		case( VT_UNKNOWN ):
		default:
			res = DISP_E_TYPEMISMATCH;
			FIXME("Coercion from %d to %d\n", vtFrom, vt );
			break;
		}
		break;

	case( VT_INT ):
	case( VT_I4 ):
		switch( vtFrom )
		{
		case( VT_EMPTY ):
			V_UNION(pd,lVal) = 0;
			res = S_OK;
			break;
		case( VT_I1 ):
			res = VarI4FromI1( V_UNION(ps,cVal), &V_UNION(pd,lVal) );
			break;
		case( VT_I2 ):
			res = VarI4FromI2( V_UNION(ps,iVal), &V_UNION(pd,lVal) );
            break;
        case( VT_INT ):
        case( VT_I4 ):
            res = VariantCopy( pd, ps );
            break;
		case( VT_UI1 ):
			res = VarI4FromUI1( V_UNION(ps,bVal), &V_UNION(pd,lVal) );
			break;
		case( VT_UI2 ):
			res = VarI4FromUI2( V_UNION(ps,uiVal), &V_UNION(pd,lVal) );
			break;
		case( VT_UINT ):
		case( VT_UI4 ):
			res = VarI4FromUI4( V_UNION(ps,ulVal), &V_UNION(pd,lVal) );
			break;
		case( VT_R4 ):
			res = VarI4FromR4( V_UNION(ps,fltVal), &V_UNION(pd,lVal) );
			break;
		case( VT_R8 ):
			res = VarI4FromR8( V_UNION(ps,dblVal), &V_UNION(pd,lVal) );
			break;
		case( VT_DATE ):
			res = VarI4FromDate( V_UNION(ps,date), &V_UNION(pd,lVal) );
			break;
		case( VT_BOOL ):
			res = VarI4FromBool( V_UNION(ps,boolVal), &V_UNION(pd,lVal) );
			break;
		case( VT_BSTR ):
			res = VarI4FromStr( V_UNION(ps,bstrVal), lcid, 0, &V_UNION(pd,lVal) );
			break;
		case( VT_CY ):
			res = VarI4FromCy( V_UNION(ps,cyVal), &V_UNION(pd,lVal) );
			break;
		case( VT_DISPATCH ):
			/*res = VarI4FromDisp( V_UNION(ps,pdispVal), lcid, &V_UNION(pd,lVal) );*/
		case( VT_DECIMAL ):
			/*res = VarI4FromDec( V_UNION(ps,deiVal), &V_UNION(pd,lVal) );*/
		case( VT_UNKNOWN ):
		default:
			res = DISP_E_TYPEMISMATCH;
			FIXME("Coercion from %d to %d\n", vtFrom, vt );
			break;
		}
		break;

	case( VT_UI1 ):
		switch( vtFrom )
		{
		case( VT_I1 ):
			res = VarUI1FromI1( V_UNION(ps,cVal), &V_UNION(pd,bVal) );
			break;
		case( VT_I2 ):
			res = VarUI1FromI2( V_UNION(ps,iVal), &V_UNION(pd,bVal) );
			break;
		case( VT_INT ):
		case( VT_I4 ):
			res = VarUI1FromI4( V_UNION(ps,lVal), &V_UNION(pd,bVal) );
			break;
        case( VT_UI1 ):
            res = VariantCopy( pd, ps );
            break;
		case( VT_UI2 ):
			res = VarUI1FromUI2( V_UNION(ps,uiVal), &V_UNION(pd,bVal) );
			break;
		case( VT_UINT ):
		case( VT_UI4 ):
			res = VarUI1FromUI4( V_UNION(ps,ulVal), &V_UNION(pd,bVal) );
			break;
		case( VT_R4 ):
			res = VarUI1FromR4( V_UNION(ps,fltVal), &V_UNION(pd,bVal) );
			break;
		case( VT_R8 ):
			res = VarUI1FromR8( V_UNION(ps,dblVal), &V_UNION(pd,bVal) );
			break;
		case( VT_DATE ):
			res = VarUI1FromDate( V_UNION(ps,date), &V_UNION(pd,bVal) );
			break;
		case( VT_BOOL ):
			res = VarUI1FromBool( V_UNION(ps,boolVal), &V_UNION(pd,bVal) );
			break;
		case( VT_BSTR ):
			res = VarUI1FromStr( V_UNION(ps,bstrVal), lcid, 0, &V_UNION(pd,bVal) );
			break;
		case( VT_CY ):
			res = VarUI1FromCy( V_UNION(ps,cyVal), &V_UNION(pd,bVal) );
			break;
		case( VT_DISPATCH ):
			/*res = VarUI1FromDisp( V_UNION(ps,pdispVal), lcid, &V_UNION(pd,bVal) );*/
		case( VT_DECIMAL ):
			/*res = VarUI1FromDec( V_UNION(ps,deiVal), &V_UNION(pd,bVal) );*/
		case( VT_UNKNOWN ):
		default:
			res = DISP_E_TYPEMISMATCH;
			FIXME("Coercion from %d to %d\n", vtFrom, vt );
			break;
		}
		break;

	case( VT_UI2 ):
		switch( vtFrom )
		{
		case( VT_I1 ):
			res = VarUI2FromI1( V_UNION(ps,cVal), &V_UNION(pd,uiVal) );
			break;
		case( VT_I2 ):
			res = VarUI2FromI2( V_UNION(ps,iVal), &V_UNION(pd,uiVal) );
			break;
		case( VT_INT ):
		case( VT_I4 ):
			res = VarUI2FromI4( V_UNION(ps,lVal), &V_UNION(pd,uiVal) );
			break;
		case( VT_UI1 ):
			res = VarUI2FromUI1( V_UNION(ps,bVal), &V_UNION(pd,uiVal) );
			break;
        case( VT_UI2 ):
            res = VariantCopy( pd, ps );
            break;
		case( VT_UINT ):
		case( VT_UI4 ):
			res = VarUI2FromUI4( V_UNION(ps,ulVal), &V_UNION(pd,uiVal) );
			break;
		case( VT_R4 ):
			res = VarUI2FromR4( V_UNION(ps,fltVal), &V_UNION(pd,uiVal) );
			break;
		case( VT_R8 ):
			res = VarUI2FromR8( V_UNION(ps,dblVal), &V_UNION(pd,uiVal) );
			break;
		case( VT_DATE ):
			res = VarUI2FromDate( V_UNION(ps,date), &V_UNION(pd,uiVal) );
			break;
		case( VT_BOOL ):
			res = VarUI2FromBool( V_UNION(ps,boolVal), &V_UNION(pd,uiVal) );
			break;
		case( VT_BSTR ):
			res = VarUI2FromStr( V_UNION(ps,bstrVal), lcid, 0, &V_UNION(pd,uiVal) );
			break;
		case( VT_CY ):
			res = VarUI2FromCy( V_UNION(ps,cyVal), &V_UNION(pd,uiVal) );
			break;
		case( VT_DISPATCH ):
			/*res = VarUI2FromDisp( V_UNION(ps,pdispVal), lcid, &V_UNION(pd,uiVal) );*/
		case( VT_DECIMAL ):
			/*res = VarUI2FromDec( V_UNION(ps,deiVal), &V_UNION(pd,uiVal) );*/
		case( VT_UNKNOWN ):
		default:
			res = DISP_E_TYPEMISMATCH;
			FIXME("Coercion from %d to %d\n", vtFrom, vt );
			break;
		}
		break;

	case( VT_UINT ):
	case( VT_UI4 ):
		switch( vtFrom )
		{
		case( VT_EMPTY ):
			V_UNION(pd,ulVal) = 0;
			res = S_OK;
			break;
		case( VT_I1 ):
			res = VarUI4FromI1( V_UNION(ps,cVal), &V_UNION(pd,ulVal) );
			break;
		case( VT_I2 ):
			res = VarUI4FromI2( V_UNION(ps,iVal), &V_UNION(pd,ulVal) );
			break;
		case( VT_INT ):
		case( VT_I4 ):
			res = VarUI4FromI4( V_UNION(ps,lVal), &V_UNION(pd,ulVal) );
			break;
		case( VT_UI1 ):
			res = VarUI4FromUI1( V_UNION(ps,bVal), &V_UNION(pd,ulVal) );
			break;
		case( VT_UI2 ):
			res = VarUI4FromUI2( V_UNION(ps,uiVal), &V_UNION(pd,ulVal) );
			break;
        case( VT_UI4 ):
            res = VariantCopy( pd, ps );
            break;
		case( VT_R4 ):
			res = VarUI4FromR4( V_UNION(ps,fltVal), &V_UNION(pd,ulVal) );
			break;
		case( VT_R8 ):
			res = VarUI4FromR8( V_UNION(ps,dblVal), &V_UNION(pd,ulVal) );
			break;
		case( VT_DATE ):
			res = VarUI4FromDate( V_UNION(ps,date), &V_UNION(pd,ulVal) );
			break;
		case( VT_BOOL ):
			res = VarUI4FromBool( V_UNION(ps,boolVal), &V_UNION(pd,ulVal) );
			break;
		case( VT_BSTR ):
			res = VarUI4FromStr( V_UNION(ps,bstrVal), lcid, 0, &V_UNION(pd,ulVal) );
			break;
		case( VT_CY ):
			res = VarUI4FromCy( V_UNION(ps,cyVal), &V_UNION(pd,ulVal) );
			break;
		case( VT_DISPATCH ):
			/*res = VarUI4FromDisp( V_UNION(ps,pdispVal), lcid, &V_UNION(pd,ulVal) );*/
		case( VT_DECIMAL ):
			/*res = VarUI4FromDec( V_UNION(ps,deiVal), &V_UNION(pd,ulVal) );*/
		case( VT_UNKNOWN ):
		default:
			res = DISP_E_TYPEMISMATCH;
			FIXME("Coercion from %d to %d\n", vtFrom, vt );
			break;
		}
		break;

	case( VT_R4 ):
		switch( vtFrom )
		{
		case( VT_I1 ):
			res = VarR4FromI1( V_UNION(ps,cVal), &V_UNION(pd,fltVal) );
			break;
		case( VT_I2 ):
			res = VarR4FromI2( V_UNION(ps,iVal), &V_UNION(pd,fltVal) );
			break;
		case( VT_INT ):
		case( VT_I4 ):
			res = VarR4FromI4( V_UNION(ps,lVal), &V_UNION(pd,fltVal) );
			break;
		case( VT_UI1 ):
			res = VarR4FromUI1( V_UNION(ps,bVal), &V_UNION(pd,fltVal) );
			break;
		case( VT_UI2 ):
			res = VarR4FromUI2( V_UNION(ps,uiVal), &V_UNION(pd,fltVal) );
			break;
		case( VT_UINT ):
		case( VT_UI4 ):
			res = VarR4FromUI4( V_UNION(ps,ulVal), &V_UNION(pd,fltVal) );
			break;
        case( VT_R4 ):
            res = VariantCopy( pd, ps );
            break;
		case( VT_R8 ):
			res = VarR4FromR8( V_UNION(ps,dblVal), &V_UNION(pd,fltVal) );
			break;
		case( VT_DATE ):
			res = VarR4FromDate( V_UNION(ps,date), &V_UNION(pd,fltVal) );
			break;
		case( VT_BOOL ):
			res = VarR4FromBool( V_UNION(ps,boolVal), &V_UNION(pd,fltVal) );
			break;
		case( VT_BSTR ):
			res = VarR4FromStr( V_UNION(ps,bstrVal), lcid, 0, &V_UNION(pd,fltVal) );
			break;
		case( VT_CY ):
			res = VarR4FromCy( V_UNION(ps,cyVal), &V_UNION(pd,fltVal) );
			break;
		case( VT_DISPATCH ):
			/*res = VarR4FromDisp( V_UNION(ps,pdispVal), lcid, &V_UNION(pd,fltVal) );*/
		case( VT_DECIMAL ):
			/*res = VarR4FromDec( V_UNION(ps,deiVal), &V_UNION(pd,fltVal) );*/
		case( VT_UNKNOWN ):
		default:
			res = DISP_E_TYPEMISMATCH;
			FIXME("Coercion from %d to %d\n", vtFrom, vt );
			break;
		}
		break;

	case( VT_R8 ):
		switch( vtFrom )
		{
		case( VT_I1 ):
			res = VarR8FromI1( V_UNION(ps,cVal), &V_UNION(pd,dblVal) );
			break;
		case( VT_I2 ):
			res = VarR8FromI2( V_UNION(ps,iVal), &V_UNION(pd,dblVal) );
			break;
		case( VT_INT ):
		case( VT_I4 ):
			res = VarR8FromI4( V_UNION(ps,lVal), &V_UNION(pd,dblVal) );
			break;
		case( VT_UI1 ):
			res = VarR8FromUI1( V_UNION(ps,bVal), &V_UNION(pd,dblVal) );
			break;
		case( VT_UI2 ):
			res = VarR8FromUI2( V_UNION(ps,uiVal), &V_UNION(pd,dblVal) );
			break;
		case( VT_UINT ):
		case( VT_UI4 ):
			res = VarR8FromUI4( V_UNION(ps,ulVal), &V_UNION(pd,dblVal) );
			break;
		case( VT_R4 ):
			res = VarR8FromR4( V_UNION(ps,fltVal), &V_UNION(pd,dblVal) );
			break;
        case( VT_R8 ):
            res = VariantCopy( pd, ps );
            break;
		case( VT_DATE ):
			res = VarR8FromDate( V_UNION(ps,date), &V_UNION(pd,dblVal) );
			break;
		case( VT_BOOL ):
			res = VarR8FromBool( V_UNION(ps,boolVal), &V_UNION(pd,dblVal) );
			break;
		case( VT_BSTR ):
			res = VarR8FromStr( V_UNION(ps,bstrVal), lcid, 0, &V_UNION(pd,dblVal) );
			break;
		case( VT_CY ):
			res = VarR8FromCy( V_UNION(ps,cyVal), &V_UNION(pd,dblVal) );
			break;
		case( VT_DISPATCH ):
			/*res = VarR8FromDisp( V_UNION(ps,pdispVal), lcid, &V_UNION(pd,dblVal) );*/
		case( VT_DECIMAL ):
			/*res = VarR8FromDec( V_UNION(ps,deiVal), &V_UNION(pd,dblVal) );*/
		case( VT_UNKNOWN ):
		default:
			res = DISP_E_TYPEMISMATCH;
			FIXME("Coercion from %d to %d\n", vtFrom, vt );
			break;
		}
		break;

	case( VT_DATE ):
		switch( vtFrom )
		{
		case( VT_I1 ):
			res = VarDateFromI1( V_UNION(ps,cVal), &V_UNION(pd,date) );
			break;
		case( VT_I2 ):
			res = VarDateFromI2( V_UNION(ps,iVal), &V_UNION(pd,date) );
			break;
		case( VT_INT ):
			res = VarDateFromInt( V_UNION(ps,intVal), &V_UNION(pd,date) );
			break;
		case( VT_I4 ):
			res = VarDateFromI4( V_UNION(ps,lVal), &V_UNION(pd,date) );
			break;
		case( VT_UI1 ):
			res = VarDateFromUI1( V_UNION(ps,bVal), &V_UNION(pd,date) );
			break;
		case( VT_UI2 ):
			res = VarDateFromUI2( V_UNION(ps,uiVal), &V_UNION(pd,date) );
			break;
		case( VT_UINT ):
			res = VarDateFromUint( V_UNION(ps,uintVal), &V_UNION(pd,date) );
			break;
		case( VT_UI4 ):
			res = VarDateFromUI4( V_UNION(ps,ulVal), &V_UNION(pd,date) );
			break;
		case( VT_R4 ):
			res = VarDateFromR4( V_UNION(ps,fltVal), &V_UNION(pd,date) );
			break;
		case( VT_R8 ):
			res = VarDateFromR8( V_UNION(ps,dblVal), &V_UNION(pd,date) );
			break;
        case( VT_DATE ):
            res = VariantCopy( pd, ps );
            break;
		case( VT_BOOL ):
			res = VarDateFromBool( V_UNION(ps,boolVal), &V_UNION(pd,date) );
			break;
		case( VT_BSTR ):
			res = VarDateFromStr( V_UNION(ps,bstrVal), lcid, 0, &V_UNION(pd,date) );
			break;
		case( VT_CY ):
			res = VarDateFromCy( V_UNION(ps,cyVal), &V_UNION(pd,date) );
			break;
		case( VT_DISPATCH ):
			/*res = VarDateFromDisp( V_UNION(ps,pdispVal), lcid, &V_UNION(pd,date) );*/
		case( VT_DECIMAL ):
			/*res = VarDateFromDec( V_UNION(ps,deiVal), &V_UNION(pd,date) );*/
		case( VT_UNKNOWN ):
		default:
			res = DISP_E_TYPEMISMATCH;
			FIXME("Coercion from %d to %d\n", vtFrom, vt );
			break;
		}
		break;

	case( VT_BOOL ):
		switch( vtFrom )
		{
		case( VT_EMPTY ):
			res = S_OK;
			V_UNION(pd,boolVal) = VARIANT_FALSE;
			break;
		case( VT_I1 ):
			res = VarBoolFromI1( V_UNION(ps,cVal), &V_UNION(pd,boolVal) );
			break;
		case( VT_I2 ):
			res = VarBoolFromI2( V_UNION(ps,iVal), &V_UNION(pd,boolVal) );
			break;
		case( VT_INT ):
			res = VarBoolFromInt( V_UNION(ps,intVal), &V_UNION(pd,boolVal) );
			break;
		case( VT_I4 ):
			res = VarBoolFromI4( V_UNION(ps,lVal), &V_UNION(pd,boolVal) );
			break;
		case( VT_UI1 ):
			res = VarBoolFromUI1( V_UNION(ps,bVal), &V_UNION(pd,boolVal) );
			break;
		case( VT_UI2 ):
			res = VarBoolFromUI2( V_UNION(ps,uiVal), &V_UNION(pd,boolVal) );
			break;
		case( VT_UINT ):
			res = VarBoolFromUint( V_UNION(ps,uintVal), &V_UNION(pd,boolVal) );
			break;
		case( VT_UI4 ):
			res = VarBoolFromUI4( V_UNION(ps,ulVal), &V_UNION(pd,boolVal) );
			break;
		case( VT_R4 ):
			res = VarBoolFromR4( V_UNION(ps,fltVal), &V_UNION(pd,boolVal) );
			break;
		case( VT_R8 ):
			res = VarBoolFromR8( V_UNION(ps,dblVal), &V_UNION(pd,boolVal) );
			break;
		case( VT_DATE ):
			res = VarBoolFromDate( V_UNION(ps,date), &V_UNION(pd,boolVal) );
			break;
        case( VT_BOOL ):
            res = VariantCopy( pd, ps );
            break;
		case( VT_BSTR ):
			res = VarBoolFromStr( V_UNION(ps,bstrVal), lcid, 0, &V_UNION(pd,boolVal) );
			break;
		case( VT_CY ):
			res = VarBoolFromCy( V_UNION(ps,cyVal), &V_UNION(pd,boolVal) );
			break;
		case( VT_DISPATCH ):
			/*res = VarBoolFromDisp( V_UNION(ps,pdispVal), lcid, &V_UNION(pd,boolVal) );*/
		case( VT_DECIMAL ):
			/*res = VarBoolFromDec( V_UNION(ps,deiVal), &V_UNION(pd,boolVal) );*/
		case( VT_UNKNOWN ):
		default:
			res = DISP_E_TYPEMISMATCH;
			FIXME("Coercion from %d to %d\n", vtFrom, vt );
			break;
		}
		break;

	case( VT_BSTR ):
		switch( vtFrom )
		{
		case( VT_EMPTY ):
			if ((V_UNION(pd,bstrVal) = SysAllocStringLen(NULL, 0)))
				res = S_OK;
			else
				res = E_OUTOFMEMORY;
			break;
		case( VT_I1 ):
			res = VarBstrFromI1( V_UNION(ps,cVal), lcid, 0, &V_UNION(pd,bstrVal) );
			break;
		case( VT_I2 ):
			res = VarBstrFromI2( V_UNION(ps,iVal), lcid, 0, &V_UNION(pd,bstrVal) );
			break;
		case( VT_INT ):
			res = VarBstrFromInt( V_UNION(ps,intVal), lcid, 0, &V_UNION(pd,bstrVal) );
			break;
		case( VT_I4 ):
			res = VarBstrFromI4( V_UNION(ps,lVal), lcid, 0, &V_UNION(pd,bstrVal) );
			break;
		case( VT_UI1 ):
			res = VarBstrFromUI1( V_UNION(ps,bVal), lcid, 0, &V_UNION(pd,bstrVal) );
			break;
		case( VT_UI2 ):
			res = VarBstrFromUI2( V_UNION(ps,uiVal), lcid, 0, &V_UNION(pd,bstrVal) );
			break;
		case( VT_UINT ):
			res = VarBstrFromUint( V_UNION(ps,uintVal), lcid, 0, &V_UNION(pd,bstrVal) );
			break;
		case( VT_UI4 ):
			res = VarBstrFromUI4( V_UNION(ps,ulVal), lcid, 0, &V_UNION(pd,bstrVal) );
			break;
		case( VT_UI8 ):
			res = VarBstrFromUI8( V_UNION(ps,ullVal), lcid, 0, &V_UNION(pd,bstrVal) );
			break;
		case( VT_R4 ):
			res = VarBstrFromR4( V_UNION(ps,fltVal), lcid, 0, &V_UNION(pd,bstrVal) );
			break;
		case( VT_R8 ):
			res = VarBstrFromR8( V_UNION(ps,dblVal), lcid, 0, &V_UNION(pd,bstrVal) );
			break;
		case( VT_DATE ):
			res = VarBstrFromDate( V_UNION(ps,date), lcid, 0, &V_UNION(pd,bstrVal) );
			break;
		case( VT_BOOL ):
			res = VarBstrFromBool( V_UNION(ps,boolVal), lcid, 0, &V_UNION(pd,bstrVal) );
			break;
		case( VT_BSTR ):
			res = VariantCopy( pd, ps );
			break;
		case( VT_CY ):
			res = VarBstrFromCy( V_UNION(ps,cyVal), lcid, 0, &V_UNION(pd,bstrVal) );
			break;
		case( VT_DISPATCH ):
			res = VarBstrFromDisp( V_UNION(ps,pdispVal), lcid, 0, &V_UNION(pd,bstrVal) );
			break;
		case( VT_DECIMAL ):
			/*res = VarBstrFromDec( V_UNION(ps,deiVal), lcid, 0, &(pd,bstrVal) );*/
		case( VT_UNKNOWN ):
		default:
			res = DISP_E_TYPEMISMATCH;
			FIXME("Coercion from %d to %d\n", vtFrom, vt );
			break;
		}
		break;

     case( VT_CY ):
	  switch( vtFrom )
	  {
	  case( VT_I1 ):
	     res = VarCyFromI1( V_UNION(ps,cVal), &V_UNION(pd,cyVal) );
	     break;
	  case( VT_I2 ):
	     res = VarCyFromI2( V_UNION(ps,iVal), &V_UNION(pd,cyVal) );
	     break;
	  case( VT_INT ):
	     res = VarCyFromInt( V_UNION(ps,intVal), &V_UNION(pd,cyVal) );
	     break;
	  case( VT_I4 ):
	     res = VarCyFromI4( V_UNION(ps,lVal), &V_UNION(pd,cyVal) );
	     break;
	  case( VT_UI1 ):
	     res = VarCyFromUI1( V_UNION(ps,bVal), &V_UNION(pd,cyVal) );
	     break;
	  case( VT_UI2 ):
	     res = VarCyFromUI2( V_UNION(ps,uiVal), &V_UNION(pd,cyVal) );
	     break;
	  case( VT_UINT ):
	     res = VarCyFromUint( V_UNION(ps,uintVal), &V_UNION(pd,cyVal) );
	     break;
	  case( VT_UI4 ):
	     res = VarCyFromUI4( V_UNION(ps,ulVal), &V_UNION(pd,cyVal) );
	     break;
	  case( VT_R4 ):
	     res = VarCyFromR4( V_UNION(ps,fltVal), &V_UNION(pd,cyVal) );
	     break;
	  case( VT_R8 ):
	     res = VarCyFromR8( V_UNION(ps,dblVal), &V_UNION(pd,cyVal) );
	     break;
	  case( VT_DATE ):
	     res = VarCyFromDate( V_UNION(ps,date), &V_UNION(pd,cyVal) );
	     break;
	  case( VT_BOOL ):
	     res = VarCyFromBool( V_UNION(ps,date), &V_UNION(pd,cyVal) );
	     break;
	  case( VT_CY ):
	     res = VariantCopy( pd, ps );
	     break;
	  case( VT_BSTR ):
	     res = VarCyFromStr( V_UNION(ps,bstrVal), lcid, 0, &V_UNION(pd,cyVal) );
	     break;
	  case( VT_DISPATCH ):
	     /*res = VarCyFromDisp( V_UNION(ps,pdispVal), lcid, &V_UNION(pd,cyVal) );*/
	  case( VT_DECIMAL ):
	     /*res = VarCyFromDec( V_UNION(ps,deiVal), &V_UNION(pd,cyVal) );*/
	     break;
	  case( VT_UNKNOWN ):
	  default:
	     res = DISP_E_TYPEMISMATCH;
	     FIXME("Coercion from %d to %d\n", vtFrom, vt );
	     break;
	  }
	  break;

	case( VT_UNKNOWN ):
	    if (vtFrom == VT_DISPATCH)
	    {
		res = IDispatch_QueryInterface(V_DISPATCH(ps), &IID_IUnknown, (LPVOID*)&V_UNKNOWN(pd));
	    }
	    else
	    {
		res = DISP_E_TYPEMISMATCH;
		FIXME("Coercion from %d to %d\n", vtFrom, vt );
	    }
	    break;
	default:
		res = DISP_E_TYPEMISMATCH;
		FIXME("Coercion from %d to %d\n", vtFrom, vt );
		break;
	}

	return res;
}

/******************************************************************************
 *		ValidateVtRange	[INTERNAL]
 *
 * Used internally by the hi-level Variant API to determine
 * if the vartypes are valid.
 */
static HRESULT WINAPI ValidateVtRange( VARTYPE vt )
{
    /* if by value we must make sure it is in the
     * range of the valid types.
     */
    if( ( vt & VT_TYPEMASK ) > VT_MAXVALIDTYPE )
    {
        WARN( "Bad vt (%d)->(%d)\n", vt, vt & VT_TYPEMASK );
        return DISP_E_BADVARTYPE;
    }
    return S_OK;
}


/******************************************************************************
 *		ValidateVartype	[INTERNAL]
 *
 * Used internally by the hi-level Variant API to determine
 * if the vartypes are valid.
 */
static HRESULT WINAPI ValidateVariantType( VARTYPE vt )
{
  HRESULT res = S_OK;

  /* check if we have a valid argument.
   */
  if( vt & VT_BYREF )
  {
    /* if by reference check that the type is in
     * the valid range and that it is not of empty or null type
     */
    if( ( vt & VT_TYPEMASK ) == VT_EMPTY ||
        ( vt & VT_TYPEMASK ) == VT_NULL ||
        ( vt & VT_TYPEMASK ) > VT_MAXVALIDTYPE )
    {
      res = E_INVALIDARG;
    }
  }
  else
  {
    res = ValidateVtRange( vt );
  }

  return res;
}

/******************************************************************************
 *		ValidateVt	[INTERNAL]
 *
 * Used internally by the hi-level Variant API to determine
 * if the vartypes are valid.
 */
static HRESULT WINAPI ValidateVt( VARTYPE vt )
{
	HRESULT res = S_OK;

	/* check if we have a valid argument.
	 */
	if( vt & VT_BYREF )
    {
        /* if by reference check that the type is in
         * the valid range and that it is not of empty or null type
         */
        if( ( vt & VT_TYPEMASK ) == VT_EMPTY ||
            ( vt & VT_TYPEMASK ) == VT_NULL ||
			( vt & VT_TYPEMASK ) > VT_MAXVALIDTYPE )
		{
			res = DISP_E_BADVARTYPE;
		}

    }
    else
    {
        res = ValidateVtRange( vt );
    }

	return res;
}





/******************************************************************************
 *		VariantInit	[OLEAUT32.8]
 *
 * Initializes the Variant.  Unlike VariantClear it does not interpret
 * the current contents of the Variant.
 */
void WINAPI VariantInit(VARIANTARG* pvarg)
{
  TRACE("(%p)\n",pvarg);

  memset(pvarg, 0, sizeof (VARIANTARG));
  V_VT(pvarg) = VT_EMPTY;

  return;
}

/******************************************************************************
 *		VariantClear	[OLEAUT32.9]
 *
 * This function clears the VARIANT by setting the vt field to VT_EMPTY. It also
 * sets the wReservedX field to 0.	The current contents of the VARIANT are
 * freed.  If the vt is VT_BSTR the string is freed. If VT_DISPATCH the object is
 * released. If VT_ARRAY the array is freed.
 */
HRESULT WINAPI VariantClear(VARIANTARG* pvarg)
{
  HRESULT res = S_OK;
  TRACE("(%p)\n",pvarg);

  res = ValidateVariantType( V_VT(pvarg) );
  if( res == S_OK )
  {
    if( !( V_VT(pvarg) & VT_BYREF ) )
    {
      /*
       * The VT_ARRAY flag is a special case of a safe array.
       */
      if ( (V_VT(pvarg) & VT_ARRAY) != 0)
      {
	SafeArrayDestroy(V_UNION(pvarg,parray));
      }
      else
      {
	switch( V_VT(pvarg) & VT_TYPEMASK )
	{
	  case( VT_BSTR ):
	    SysFreeString( V_UNION(pvarg,bstrVal) );
	    break;
	  case( VT_DISPATCH ):
	    if(V_UNION(pvarg,pdispVal)!=NULL)
	      IDispatch_Release(V_UNION(pvarg,pdispVal));
	    break;
	  case( VT_UNKNOWN ):
	    if(V_UNION(pvarg,punkVal)!=NULL)
	      IUnknown_Release(V_UNION(pvarg,punkVal));
	    break;
	  case( VT_SAFEARRAY ):
	    SafeArrayDestroy(V_UNION(pvarg,parray));
	    break;
	  default:
	    break;
	}
      }
    }

    /*
     * Empty all the fields and mark the type as empty.
     */
    memset(pvarg, 0, sizeof (VARIANTARG));
    V_VT(pvarg) = VT_EMPTY;
  }

  return res;
}

/******************************************************************************
 *		VariantCopy	[OLEAUT32.10]
 *
 * Frees up the designation variant and makes a copy of the source.
 */
HRESULT WINAPI VariantCopy(VARIANTARG* pvargDest, VARIANTARG* pvargSrc)
{
  HRESULT res = S_OK;

  TRACE("(%p, %p), vt=%d\n", pvargDest, pvargSrc, V_VT(pvargSrc));

  res = ValidateVariantType( V_VT(pvargSrc) );

  /* If the pointer are to the same variant we don't need
   * to do anything.
   */
  if( pvargDest != pvargSrc && res == S_OK )
  {
    res = VariantClear( pvargDest );

    if( res == S_OK )
    {
      if( V_VT(pvargSrc) & VT_BYREF )
      {
	/* In the case of byreference we only need
	 * to copy the pointer.
	 */
	pvargDest->n1.n2.n3 = pvargSrc->n1.n2.n3;
	V_VT(pvargDest) = V_VT(pvargSrc);
      }
      else
      {
	/*
	 * The VT_ARRAY flag is another way to designate a safe array.
	 */
	if (V_VT(pvargSrc) & VT_ARRAY)
	{
	  SafeArrayCopy(V_UNION(pvargSrc,parray), &V_UNION(pvargDest,parray));
	}
	else
	{
	  /* In the case of by value we need to
	   * copy the actual value. In the case of
	   * VT_BSTR a copy of the string is made,
	   * if VT_DISPATCH or VT_IUNKNOWN AddRef is
	   * called to increment the object's reference count.
	   */
	  switch( V_VT(pvargSrc) & VT_TYPEMASK )
	  {
	    case( VT_BSTR ):
	      V_UNION(pvargDest,bstrVal) = SYSDUPSTRING( V_UNION(pvargSrc,bstrVal) );
	      break;
	    case( VT_DISPATCH ):
	      V_UNION(pvargDest,pdispVal) = V_UNION(pvargSrc,pdispVal);
	      if (V_UNION(pvargDest,pdispVal)!=NULL)
		IDispatch_AddRef(V_UNION(pvargDest,pdispVal));
	      break;
	    case( VT_VARIANT ):
	      VariantCopy(V_UNION(pvargDest,pvarVal),V_UNION(pvargSrc,pvarVal));
	      break;
	    case( VT_UNKNOWN ):
	      V_UNION(pvargDest,punkVal) = V_UNION(pvargSrc,punkVal);
	      if (V_UNION(pvargDest,pdispVal)!=NULL)
		IUnknown_AddRef(V_UNION(pvargDest,punkVal));
	      break;
	    case( VT_SAFEARRAY ):
	      SafeArrayCopy(V_UNION(pvargSrc,parray), &V_UNION(pvargDest,parray));
	      break;
	    default:
	      pvargDest->n1.n2.n3 = pvargSrc->n1.n2.n3;
	      break;
	  }
	}

	V_VT(pvargDest) = V_VT(pvargSrc);
      }
    }
  }

  return res;
}


/******************************************************************************
 *		VariantCopyInd	[OLEAUT32.11]
 *
 * Frees up the destination variant and makes a copy of the source.  If
 * the source is of type VT_BYREF it performs the necessary indirections.
 */
HRESULT WINAPI VariantCopyInd(VARIANT* pvargDest, VARIANTARG* pvargSrc)
{
  HRESULT res = S_OK;

  TRACE("(%p, %p)\n", pvargDest, pvargSrc);

  res = ValidateVariantType( V_VT(pvargSrc) );

  if( FAILED(res) )
    return res;

  if( V_VT(pvargSrc) & VT_BYREF )
  {
    VARIANTARG varg;
    VariantInit( &varg );

    /* handle the in place copy.
     */
    if( pvargDest == pvargSrc )
    {
      /* we will use a copy of the source instead.
       */
      res = VariantCopy( &varg, pvargSrc );
      pvargSrc = &varg;
    }

    if( res == S_OK )
    {
      res = VariantClear( pvargDest );

      if( res == S_OK )
      {
	/*
	 * The VT_ARRAY flag is another way to designate a safearray variant.
	 */
	if ( V_VT(pvargSrc) & VT_ARRAY)
	{
	  SafeArrayCopy(*V_UNION(pvargSrc,pparray), &V_UNION(pvargDest,parray));
	}
	else
	{
	  /* In the case of by reference we need
	   * to copy the date pointed to by the variant.
	   */

	  /* Get the variant type.
	   */
	  switch( V_VT(pvargSrc) & VT_TYPEMASK )
	  {
	    case( VT_BSTR ):
	      V_UNION(pvargDest,bstrVal) = SYSDUPSTRING( *(V_UNION(pvargSrc,pbstrVal)) );
	      break;
	    case( VT_DISPATCH ):
	      break;
	    case( VT_VARIANT ):
	      {
		/* Prevent from cycling.  According to tests on
		 * VariantCopyInd in Windows and the documentation
		 * this API dereferences the inner Variants to only one depth.
		 * If the inner Variant itself contains an
		 * other inner variant the E_INVALIDARG error is
		 * returned.
		 */
		if( pvargSrc->n1.n2.wReserved1 & PROCESSING_INNER_VARIANT )
		{
		  /* If we get here we are attempting to deference
		   * an inner variant that that is itself contained
		   * in an inner variant so report E_INVALIDARG error.
		   */
		  res = E_INVALIDARG;
		}
		else
		{
		  /* Set the processing inner variant flag.
		   * We will set this flag in the inner variant
		   * that will be passed to the VariantCopyInd function.
		   */
		  (V_UNION(pvargSrc,pvarVal))->n1.n2.wReserved1 |= PROCESSING_INNER_VARIANT;

		  /* Dereference the inner variant.
		   */
		  res = VariantCopyInd( pvargDest, V_UNION(pvargSrc,pvarVal) );
		  /* We must also copy its type, I think.
		   */
		  V_VT(pvargSrc) = V_VT(V_UNION(pvargSrc,pvarVal));
		}
	      }
	      break;
	    case( VT_UNKNOWN ):
	      break;
	    case( VT_SAFEARRAY ):
	      SafeArrayCopy(*V_UNION(pvargSrc,pparray), &V_UNION(pvargDest,parray));
	      break;
	    default:
	      /* This is a by reference Variant which means that the union
	       * part of the Variant contains a pointer to some data of
	       * type "V_VT(pvargSrc) & VT_TYPEMASK".
	       * We will deference this data in a generic fashion using
	       * the void pointer "Variant.u.byref".
	       * We will copy this data into the union of the destination
	       * Variant.
	       */
	      memcpy( &pvargDest->n1.n2.n3, V_UNION(pvargSrc,byref), SizeOfVariantData( pvargSrc ) );
	      break;
	  }
	}

	V_VT(pvargDest) = V_VT(pvargSrc) & VT_TYPEMASK;
      }
    }

    /* this should not fail.
     */
    VariantClear( &varg );
  }
  else
  {
    res = VariantCopy( pvargDest, pvargSrc );
  }

  return res;
}

/******************************************************************************
 *		VariantChangeType	[OLEAUT32.12]
 */
HRESULT WINAPI VariantChangeType(VARIANTARG* pvargDest, VARIANTARG* pvargSrc,
							USHORT wFlags, VARTYPE vt)
{
	return VariantChangeTypeEx( pvargDest, pvargSrc, 0, wFlags, vt );
}

/******************************************************************************
 *		VariantChangeTypeEx	[OLEAUT32.147]
 */
HRESULT WINAPI VariantChangeTypeEx(VARIANTARG* pvargDest, VARIANTARG* pvargSrc,
							  LCID lcid, USHORT wFlags, VARTYPE vt)
{
	HRESULT res = S_OK;
	VARIANTARG varg;
	VariantInit( &varg );

	TRACE("(%p, %p, %ld, %u, %u) vtSrc=%d vtDest=%d\n", pvargDest, pvargSrc, lcid, wFlags, vt, V_VT(pvargSrc), V_VT(pvargDest));

	/* validate our source argument.
	 */
	res = ValidateVariantType( V_VT(pvargSrc) );

	/* validate the vartype.
	 */
	if( SUCCEEDED(res) )
	{
		res = ValidateVt( vt );
	}

	/* if we are doing an in-place conversion make a copy of the source.
	 */
	if( res == S_OK && pvargDest == pvargSrc )
	{
		res = VariantCopy( &varg, pvargSrc );
		pvargSrc = &varg;
	}

	if( SUCCEEDED(res) )
	{
		/* free up the destination variant.
		 */
		res = VariantClear( pvargDest );
	}

	if( SUCCEEDED(res) )
	{
		if( V_VT(pvargSrc) & VT_BYREF )
		{
			/* Convert the source variant to a "byvalue" variant.
			 */
			VARIANTARG Variant;
			VariantInit( &Variant );
			res = VariantCopyInd( &Variant, pvargSrc );
			if( SUCCEEDED(res) )
			{
				res = Coerce( pvargDest, lcid, wFlags, &Variant, vt );
				/* this should not fail.
				 */
				VariantClear( &Variant );
			}

		}
		else if (V_VT(pvargSrc) & VT_ARRAY)
                {
                    SafeArrayCopy(V_UNION(pvargSrc,parray), &V_UNION(pvargDest,parray));
                }
		else
		{
			/* Use the current "byvalue" source variant.
			 */
			res = Coerce( pvargDest, lcid, wFlags, pvargSrc, vt );
		}
	}
	/* this should not fail.
	 */
	VariantClear( &varg );

	/* set the type of the destination
	 */
	if ( SUCCEEDED(res) )
		V_VT(pvargDest) = vt;

	return res;
}




/******************************************************************************
 *		VarUI1FromI2		[OLEAUT32.130]
 */
HRESULT WINAPI VarUI1FromI2(short sIn, BYTE* pbOut)
{
	TRACE("( %d, %p ), stub\n", sIn, pbOut );

	/* Check range of value.
	 */
	if( sIn < UI1_MIN || sIn > UI1_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pbOut = (BYTE) sIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI1FromI4		[OLEAUT32.131]
 */
HRESULT WINAPI VarUI1FromI4(LONG lIn, BYTE* pbOut)
{
	TRACE("( %ld, %p ), stub\n", lIn, pbOut );

	/* Check range of value.
	 */
	if( lIn < UI1_MIN || lIn > UI1_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pbOut = (BYTE) lIn;

	return S_OK;
}


/******************************************************************************
 *		VarUI1FromR4		[OLEAUT32.132]
 */
HRESULT WINAPI VarUI1FromR4(FLOAT fltIn, BYTE* pbOut)
{
	TRACE("( %f, %p ), stub\n", fltIn, pbOut );

	/* Check range of value.
     */
    fltIn = oleaut32_round( fltIn );
	if( fltIn < UI1_MIN || fltIn > UI1_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pbOut = (BYTE) fltIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI1FromR8		[OLEAUT32.133]
 */
HRESULT WINAPI VarUI1FromR8(double dblIn, BYTE* pbOut)
{
	TRACE("( %f, %p ), stub\n", dblIn, pbOut );

	/* Check range of value.
     */
    dblIn = oleaut32_round( dblIn );
	if( dblIn < UI1_MIN || dblIn > UI1_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pbOut = (BYTE) dblIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI1FromDate		[OLEAUT32.135]
 */
HRESULT WINAPI VarUI1FromDate(DATE dateIn, BYTE* pbOut)
{
	TRACE("( %f, %p ), stub\n", dateIn, pbOut );

	/* Check range of value.
     */
    dateIn = oleaut32_round( dateIn );
	if( dateIn < UI1_MIN || dateIn > UI1_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pbOut = (BYTE) dateIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI1FromBool		[OLEAUT32.138]
 */
HRESULT WINAPI VarUI1FromBool(VARIANT_BOOL boolIn, BYTE* pbOut)
{
	TRACE("( %d, %p ), stub\n", boolIn, pbOut );

	*pbOut = (BYTE) boolIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI1FromI1		[OLEAUT32.237]
 */
HRESULT WINAPI VarUI1FromI1(CHAR cIn, BYTE* pbOut)
{
	TRACE("( %c, %p ), stub\n", cIn, pbOut );

	*pbOut = cIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI1FromUI2		[OLEAUT32.238]
 */
HRESULT WINAPI VarUI1FromUI2(USHORT uiIn, BYTE* pbOut)
{
	TRACE("( %d, %p ), stub\n", uiIn, pbOut );

	/* Check range of value.
	 */
	if( uiIn > UI1_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pbOut = (BYTE) uiIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI1FromUI4		[OLEAUT32.239]
 */
HRESULT WINAPI VarUI1FromUI4(ULONG ulIn, BYTE* pbOut)
{
	TRACE("( %ld, %p ), stub\n", ulIn, pbOut );

	/* Check range of value.
	 */
	if( ulIn > UI1_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pbOut = (BYTE) ulIn;

	return S_OK;
}


/******************************************************************************
 *		VarUI1FromStr		[OLEAUT32.136]
 */
HRESULT WINAPI VarUI1FromStr(OLECHAR* strIn, LCID lcid, ULONG dwFlags, BYTE* pbOut)
{
	double dValue = 0.0;
	LPSTR pNewString = NULL;

	TRACE("( %p, 0x%08lx, 0x%08lx, %p ), stub\n", strIn, lcid, dwFlags, pbOut );

	/* Check if we have a valid argument
	 */
	pNewString = HEAP_strdupWtoA( GetProcessHeap(), 0, strIn );
	RemoveCharacterFromString( pNewString, "," );
	if( IsValidRealString( pNewString ) == FALSE )
	{
		return DISP_E_TYPEMISMATCH;
	}

	/* Convert the valid string to a floating point number.
	 */
	dValue = atof( pNewString );

	/* We don't need the string anymore so free it.
	 */
	HeapFree( GetProcessHeap(), 0 , pNewString );

	/* Check range of value.
     */
    dValue = oleaut32_round( dValue );
	if( dValue < UI1_MIN || dValue > UI1_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pbOut = (BYTE) dValue;

	return S_OK;
}

/**********************************************************************
 *              VarUI1FromCy [OLEAUT32.134]
 * Convert currency to unsigned char
 */
HRESULT WINAPI VarUI1FromCy(CY cyIn, BYTE* pbOut) {
   double t = oleaut32_round((((double)cyIn.s.Hi * 4294967296.0) + (double)cyIn.s.Lo) / 10000);

   if (t > UI1_MAX || t < UI1_MIN) return DISP_E_OVERFLOW;

   *pbOut = (BYTE)t;
   return S_OK;
}

/******************************************************************************
 *		VarI2FromUI1		[OLEAUT32.48]
 */
HRESULT WINAPI VarI2FromUI1(BYTE bIn, short* psOut)
{
	TRACE("( 0x%08x, %p ), stub\n", bIn, psOut );

	*psOut = (short) bIn;

	return S_OK;
}

/******************************************************************************
 *		VarI2FromI4		[OLEAUT32.49]
 */
HRESULT WINAPI VarI2FromI4(LONG lIn, short* psOut)
{
	TRACE("( %lx, %p ), stub\n", lIn, psOut );

	/* Check range of value.
	 */
	if( lIn < I2_MIN || lIn > I2_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*psOut = (short) lIn;

	return S_OK;
}

/******************************************************************************
 *		VarI2FromR4		[OLEAUT32.50]
 */
HRESULT WINAPI VarI2FromR4(FLOAT fltIn, short* psOut)
{
	TRACE("( %f, %p ), stub\n", fltIn, psOut );

	/* Check range of value.
     */
    fltIn = oleaut32_round( fltIn );
	if( fltIn < I2_MIN || fltIn > I2_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*psOut = (short) fltIn;

	return S_OK;
}

/******************************************************************************
 *		VarI2FromR8		[OLEAUT32.51]
 */
HRESULT WINAPI VarI2FromR8(double dblIn, short* psOut)
{
	TRACE("( %f, %p ), stub\n", dblIn, psOut );

	/* Check range of value.
     */
    dblIn = oleaut32_round( dblIn );
	if( dblIn < I2_MIN || dblIn > I2_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*psOut = (short) dblIn;

	return S_OK;
}

/******************************************************************************
 *		VarI2FromDate		[OLEAUT32.53]
 */
HRESULT WINAPI VarI2FromDate(DATE dateIn, short* psOut)
{
	TRACE("( %f, %p ), stub\n", dateIn, psOut );

	/* Check range of value.
     */
    dateIn = oleaut32_round( dateIn );
	if( dateIn < I2_MIN || dateIn > I2_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*psOut = (short) dateIn;

	return S_OK;
}

/******************************************************************************
 *		VarI2FromBool		[OLEAUT32.56]
 */
HRESULT WINAPI VarI2FromBool(VARIANT_BOOL boolIn, short* psOut)
{
	TRACE("( %d, %p ), stub\n", boolIn, psOut );

	*psOut = (short) boolIn;

	return S_OK;
}

/******************************************************************************
 *		VarI2FromI1		[OLEAUT32.205]
 */
HRESULT WINAPI VarI2FromI1(CHAR cIn, short* psOut)
{
	TRACE("( %c, %p ), stub\n", cIn, psOut );

	*psOut = (short) cIn;

	return S_OK;
}

/******************************************************************************
 *		VarI2FromUI2		[OLEAUT32.206]
 */
HRESULT WINAPI VarI2FromUI2(USHORT uiIn, short* psOut)
{
	TRACE("( %d, %p ), stub\n", uiIn, psOut );

	/* Check range of value.
	 */
	if( uiIn > I2_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*psOut = (short) uiIn;

	return S_OK;
}

/******************************************************************************
 *		VarI2FromUI4		[OLEAUT32.207]
 */
HRESULT WINAPI VarI2FromUI4(ULONG ulIn, short* psOut)
{
	TRACE("( %lx, %p ), stub\n", ulIn, psOut );

	/* Check range of value.
	 */
	if( ulIn < I2_MIN || ulIn > I2_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*psOut = (short) ulIn;

	return S_OK;
}

/******************************************************************************
 *		VarI2FromStr		[OLEAUT32.54]
 */
HRESULT WINAPI VarI2FromStr(OLECHAR* strIn, LCID lcid, ULONG dwFlags, short* psOut)
{
	double dValue = 0.0;
	LPSTR pNewString = NULL;

	TRACE("( %p, 0x%08lx, 0x%08lx, %p ), stub\n", strIn, lcid, dwFlags, psOut );

	/* Check if we have a valid argument
	 */
	pNewString = HEAP_strdupWtoA( GetProcessHeap(), 0, strIn );
	RemoveCharacterFromString( pNewString, "," );
	if( IsValidRealString( pNewString ) == FALSE )
	{
		return DISP_E_TYPEMISMATCH;
	}

	/* Convert the valid string to a floating point number.
	 */
	dValue = atof( pNewString );

	/* We don't need the string anymore so free it.
	 */
	HeapFree( GetProcessHeap(), 0, pNewString );

	/* Check range of value.
     */
    dValue = oleaut32_round( dValue );
	if( dValue < I2_MIN || dValue > I2_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*psOut = (short)  dValue;

	return S_OK;
}

/**********************************************************************
 *              VarI2FromCy [OLEAUT32.52]
 * Convert currency to signed short
 */
HRESULT WINAPI VarI2FromCy(CY cyIn, short* psOut) {
   double t = oleaut32_round((((double)cyIn.s.Hi * 4294967296.0) + (double)cyIn.s.Lo) / 10000);

   if (t > I2_MAX || t < I2_MIN) return DISP_E_OVERFLOW;

   *psOut = (SHORT)t;
   return S_OK;
}

/******************************************************************************
 *		VarI4FromUI1		[OLEAUT32.58]
 */
HRESULT WINAPI VarI4FromUI1(BYTE bIn, LONG* plOut)
{
	TRACE("( %X, %p ), stub\n", bIn, plOut );

	*plOut = (LONG) bIn;

	return S_OK;
}


/******************************************************************************
 *		VarI4FromR4		[OLEAUT32.60]
 */
HRESULT WINAPI VarI4FromR4(FLOAT fltIn, LONG* plOut)
{
	TRACE("( %f, %p ), stub\n", fltIn, plOut );

	/* Check range of value.
     */
    fltIn = oleaut32_round( fltIn );
	if( fltIn < I4_MIN || fltIn > I4_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*plOut = (LONG) fltIn;

	return S_OK;
}

/******************************************************************************
 *		VarI4FromR8		[OLEAUT32.61]
 */
HRESULT WINAPI VarI4FromR8(double dblIn, LONG* plOut)
{
	TRACE("( %f, %p ), stub\n", dblIn, plOut );

	/* Check range of value.
     */
    dblIn = oleaut32_round( dblIn );
	if( dblIn < I4_MIN || dblIn > I4_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*plOut = (LONG) dblIn;

	return S_OK;
}

/******************************************************************************
 *		VarI4FromDate		[OLEAUT32.63]
 */
HRESULT WINAPI VarI4FromDate(DATE dateIn, LONG* plOut)
{
	TRACE("( %f, %p ), stub\n", dateIn, plOut );

	/* Check range of value.
     */
    dateIn = oleaut32_round( dateIn );
	if( dateIn < I4_MIN || dateIn > I4_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*plOut = (LONG) dateIn;

	return S_OK;
}

/******************************************************************************
 *		VarI4FromBool		[OLEAUT32.66]
 */
HRESULT WINAPI VarI4FromBool(VARIANT_BOOL boolIn, LONG* plOut)
{
	TRACE("( %d, %p ), stub\n", boolIn, plOut );

	*plOut = (LONG) boolIn;

	return S_OK;
}

/******************************************************************************
 *		VarI4FromI1		[OLEAUT32.209]
 */
HRESULT WINAPI VarI4FromI1(CHAR cIn, LONG* plOut)
{
	TRACE("( %c, %p ), stub\n", cIn, plOut );

	*plOut = (LONG) cIn;

	return S_OK;
}

/******************************************************************************
 *		VarI4FromUI2		[OLEAUT32.210]
 */
HRESULT WINAPI VarI4FromUI2(USHORT uiIn, LONG* plOut)
{
	TRACE("( %d, %p ), stub\n", uiIn, plOut );

	*plOut = (LONG) uiIn;

	return S_OK;
}

/******************************************************************************
 *		VarI4FromUI4		[OLEAUT32.211]
 */
HRESULT WINAPI VarI4FromUI4(ULONG ulIn, LONG* plOut)
{
	TRACE("( %lx, %p ), stub\n", ulIn, plOut );

	/* Check range of value.
	 */
	if( ulIn < I4_MIN || ulIn > I4_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*plOut = (LONG) ulIn;

	return S_OK;
}

/******************************************************************************
 *		VarI4FromI2		[OLEAUT32.59]
 */
HRESULT WINAPI VarI4FromI2(short sIn, LONG* plOut)
{
	TRACE("( %d, %p ), stub\n", sIn, plOut );

	*plOut = (LONG) sIn;

	return S_OK;
}

/******************************************************************************
 *		VarI4FromStr		[OLEAUT32.64]
 */
HRESULT WINAPI VarI4FromStr(OLECHAR* strIn, LCID lcid, ULONG dwFlags, LONG* plOut)
{
	double dValue = 0.0;
	LPSTR pNewString = NULL;

	TRACE("( %p, 0x%08lx, 0x%08lx, %p )\n", strIn, lcid, dwFlags, plOut );

	/* Check if we have a valid argument
	 */
	pNewString = HEAP_strdupWtoA( GetProcessHeap(), 0, strIn );
	RemoveCharacterFromString( pNewString, "," );
	if( IsValidRealString( pNewString ) == FALSE )
	{
		return DISP_E_TYPEMISMATCH;
	}

	/* Convert the valid string to a floating point number.
	 */
	dValue = atof( pNewString );

	/* We don't need the string anymore so free it.
	 */
	HeapFree( GetProcessHeap(), 0, pNewString );

	/* Check range of value.
     */
    dValue = oleaut32_round( dValue );
	if( dValue < I4_MIN || dValue > I4_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*plOut = (LONG) dValue;

	return S_OK;
}

/**********************************************************************
 *              VarI4FromCy [OLEAUT32.62]
 * Convert currency to signed long
 */
HRESULT WINAPI VarI4FromCy(CY cyIn, LONG* plOut) {
   double t = oleaut32_round((((double)cyIn.s.Hi * 4294967296.0) + (double)cyIn.s.Lo) / 10000);

   if (t > I4_MAX || t < I4_MIN) return DISP_E_OVERFLOW;

   *plOut = (LONG)t;
   return S_OK;
}

/******************************************************************************
 *		VarR4FromUI1		[OLEAUT32.68]
 */
HRESULT WINAPI VarR4FromUI1(BYTE bIn, FLOAT* pfltOut)
{
	TRACE("( %X, %p ), stub\n", bIn, pfltOut );

	*pfltOut = (FLOAT) bIn;

	return S_OK;
}

/******************************************************************************
 *		VarR4FromI2		[OLEAUT32.69]
 */
HRESULT WINAPI VarR4FromI2(short sIn, FLOAT* pfltOut)
{
	TRACE("( %d, %p ), stub\n", sIn, pfltOut );

	*pfltOut = (FLOAT) sIn;

	return S_OK;
}

/******************************************************************************
 *		VarR4FromI4		[OLEAUT32.70]
 */
HRESULT WINAPI VarR4FromI4(LONG lIn, FLOAT* pfltOut)
{
	TRACE("( %lx, %p ), stub\n", lIn, pfltOut );

	*pfltOut = (FLOAT) lIn;

	return S_OK;
}

/******************************************************************************
 *		VarR4FromR8		[OLEAUT32.71]
 */
HRESULT WINAPI VarR4FromR8(double dblIn, FLOAT* pfltOut)
{
	TRACE("( %f, %p ), stub\n", dblIn, pfltOut );

	/* Check range of value.
	 */
	if( dblIn < -(FLT_MAX) || dblIn > FLT_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pfltOut = (FLOAT) dblIn;

	return S_OK;
}

/******************************************************************************
 *		VarR4FromDate		[OLEAUT32.73]
 */
HRESULT WINAPI VarR4FromDate(DATE dateIn, FLOAT* pfltOut)
{
	TRACE("( %f, %p ), stub\n", dateIn, pfltOut );

	/* Check range of value.
	 */
	if( dateIn < -(FLT_MAX) || dateIn > FLT_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pfltOut = (FLOAT) dateIn;

	return S_OK;
}

/******************************************************************************
 *		VarR4FromBool		[OLEAUT32.76]
 */
HRESULT WINAPI VarR4FromBool(VARIANT_BOOL boolIn, FLOAT* pfltOut)
{
	TRACE("( %d, %p ), stub\n", boolIn, pfltOut );

	*pfltOut = (FLOAT) boolIn;

	return S_OK;
}

/******************************************************************************
 *		VarR4FromI1		[OLEAUT32.213]
 */
HRESULT WINAPI VarR4FromI1(CHAR cIn, FLOAT* pfltOut)
{
	TRACE("( %c, %p ), stub\n", cIn, pfltOut );

	*pfltOut = (FLOAT) cIn;

	return S_OK;
}

/******************************************************************************
 *		VarR4FromUI2		[OLEAUT32.214]
 */
HRESULT WINAPI VarR4FromUI2(USHORT uiIn, FLOAT* pfltOut)
{
	TRACE("( %d, %p ), stub\n", uiIn, pfltOut );

	*pfltOut = (FLOAT) uiIn;

	return S_OK;
}

/******************************************************************************
 *		VarR4FromUI4		[OLEAUT32.215]
 */
HRESULT WINAPI VarR4FromUI4(ULONG ulIn, FLOAT* pfltOut)
{
	TRACE("( %ld, %p ), stub\n", ulIn, pfltOut );

	*pfltOut = (FLOAT) ulIn;

	return S_OK;
}

/******************************************************************************
 *		VarR4FromStr		[OLEAUT32.74]
 */
HRESULT WINAPI VarR4FromStr(OLECHAR* strIn, LCID lcid, ULONG dwFlags, FLOAT* pfltOut)
{
	double dValue = 0.0;
	LPSTR pNewString = NULL;

	TRACE("( %p, %ld, %ld, %p ), stub\n", strIn, lcid, dwFlags, pfltOut );

	/* Check if we have a valid argument
	 */
	pNewString = HEAP_strdupWtoA( GetProcessHeap(), 0, strIn );
	RemoveCharacterFromString( pNewString, "," );
	if( IsValidRealString( pNewString ) == FALSE )
	{
		return DISP_E_TYPEMISMATCH;
	}

	/* Convert the valid string to a floating point number.
	 */
	dValue = atof( pNewString );

	/* We don't need the string anymore so free it.
	 */
	HeapFree( GetProcessHeap(), 0, pNewString );

	/* Check range of value.
	 */
	if( dValue < -(FLT_MAX) || dValue > FLT_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pfltOut = (FLOAT) dValue;

	return S_OK;
}

/**********************************************************************
 *              VarR4FromCy [OLEAUT32.72]
 * Convert currency to float
 */
HRESULT WINAPI VarR4FromCy(CY cyIn, FLOAT* pfltOut) {
   *pfltOut = (FLOAT)((((double)cyIn.s.Hi * 4294967296.0) + (double)cyIn.s.Lo) / 10000);

   return S_OK;
}

/******************************************************************************
 *		VarR8FromUI1		[OLEAUT32.78]
 */
HRESULT WINAPI VarR8FromUI1(BYTE bIn, double* pdblOut)
{
	TRACE("( %d, %p ), stub\n", bIn, pdblOut );

	*pdblOut = (double) bIn;

	return S_OK;
}

/******************************************************************************
 *		VarR8FromI2		[OLEAUT32.79]
 */
HRESULT WINAPI VarR8FromI2(short sIn, double* pdblOut)
{
	TRACE("( %d, %p ), stub\n", sIn, pdblOut );

	*pdblOut = (double) sIn;

	return S_OK;
}

/******************************************************************************
 *		VarR8FromI4		[OLEAUT32.80]
 */
HRESULT WINAPI VarR8FromI4(LONG lIn, double* pdblOut)
{
	TRACE("( %ld, %p ), stub\n", lIn, pdblOut );

	*pdblOut = (double) lIn;

	return S_OK;
}

/******************************************************************************
 *		VarR8FromR4		[OLEAUT32.81]
 */
HRESULT WINAPI VarR8FromR4(FLOAT fltIn, double* pdblOut)
{
	TRACE("( %f, %p ), stub\n", fltIn, pdblOut );

	*pdblOut = (double) fltIn;

	return S_OK;
}

/******************************************************************************
 *		VarR8FromDate		[OLEAUT32.83]
 */
HRESULT WINAPI VarR8FromDate(DATE dateIn, double* pdblOut)
{
	TRACE("( %f, %p ), stub\n", dateIn, pdblOut );

	*pdblOut = (double) dateIn;

	return S_OK;
}

/******************************************************************************
 *		VarR8FromBool		[OLEAUT32.86]
 */
HRESULT WINAPI VarR8FromBool(VARIANT_BOOL boolIn, double* pdblOut)
{
	TRACE("( %d, %p ), stub\n", boolIn, pdblOut );

	*pdblOut = (double) boolIn;

	return S_OK;
}

/******************************************************************************
 *		VarR8FromI1		[OLEAUT32.217]
 */
HRESULT WINAPI VarR8FromI1(CHAR cIn, double* pdblOut)
{
	TRACE("( %c, %p ), stub\n", cIn, pdblOut );

	*pdblOut = (double) cIn;

	return S_OK;
}

/******************************************************************************
 *		VarR8FromUI2		[OLEAUT32.218]
 */
HRESULT WINAPI VarR8FromUI2(USHORT uiIn, double* pdblOut)
{
	TRACE("( %d, %p ), stub\n", uiIn, pdblOut );

	*pdblOut = (double) uiIn;

	return S_OK;
}

/******************************************************************************
 *		VarR8FromUI4		[OLEAUT32.219]
 */
HRESULT WINAPI VarR8FromUI4(ULONG ulIn, double* pdblOut)
{
	TRACE("( %ld, %p ), stub\n", ulIn, pdblOut );

	*pdblOut = (double) ulIn;

	return S_OK;
}

/******************************************************************************
 *		VarR8FromStr		[OLEAUT32.84]
 */
HRESULT WINAPI VarR8FromStr(OLECHAR* strIn, LCID lcid, ULONG dwFlags, double* pdblOut)
{
	double dValue = 0.0;
	LPSTR pNewString = NULL;

	TRACE("( %p, %ld, %ld, %p ), stub\n", strIn, lcid, dwFlags, pdblOut );

	/* Check if we have a valid argument
	 */
	pNewString = HEAP_strdupWtoA( GetProcessHeap(), 0, strIn );
	RemoveCharacterFromString( pNewString, "," );
	if( IsValidRealString( pNewString ) == FALSE )
	{
		return DISP_E_TYPEMISMATCH;
	}

	/* Convert the valid string to a floating point number.
	 */
	dValue = atof( pNewString );

	/* We don't need the string anymore so free it.
	 */
	HeapFree( GetProcessHeap(), 0, pNewString );

	*pdblOut = dValue;

	return S_OK;
}

/**********************************************************************
 *              VarR8FromCy [OLEAUT32.82]
 * Convert currency to double
 */
HRESULT WINAPI VarR8FromCy(CY cyIn, double* pdblOut) {
   *pdblOut = (double)((((double)cyIn.s.Hi * 4294967296.0) + (double)cyIn.s.Lo) / 10000);

   return S_OK;
}

/******************************************************************************
 *		VarDateFromUI1		[OLEAUT32.88]
 */
HRESULT WINAPI VarDateFromUI1(BYTE bIn, DATE* pdateOut)
{
	TRACE("( %d, %p ), stub\n", bIn, pdateOut );

	*pdateOut = (DATE) bIn;

	return S_OK;
}

/******************************************************************************
 *		VarDateFromI2		[OLEAUT32.89]
 */
HRESULT WINAPI VarDateFromI2(short sIn, DATE* pdateOut)
{
	TRACE("( %d, %p ), stub\n", sIn, pdateOut );

	*pdateOut = (DATE) sIn;

	return S_OK;
}

/******************************************************************************
 *		VarDateFromI4		[OLEAUT32.90]
 */
HRESULT WINAPI VarDateFromI4(LONG lIn, DATE* pdateOut)
{
	TRACE("( %ld, %p ), stub\n", lIn, pdateOut );

	if( lIn < DATE_MIN || lIn > DATE_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pdateOut = (DATE) lIn;

	return S_OK;
}

/******************************************************************************
 *		VarDateFromR4		[OLEAUT32.91]
 */
HRESULT WINAPI VarDateFromR4(FLOAT fltIn, DATE* pdateOut)
{
    TRACE("( %f, %p ), stub\n", fltIn, pdateOut );

    if( ceil(fltIn) < DATE_MIN || floor(fltIn) > DATE_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pdateOut = (DATE) fltIn;

	return S_OK;
}

/******************************************************************************
 *		VarDateFromR8		[OLEAUT32.92]
 */
HRESULT WINAPI VarDateFromR8(double dblIn, DATE* pdateOut)
{
    TRACE("( %f, %p ), stub\n", dblIn, pdateOut );

	if( ceil(dblIn) < DATE_MIN || floor(dblIn) > DATE_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pdateOut = (DATE) dblIn;

	return S_OK;
}

/******************************************************************************
 *		VarDateFromStr		[OLEAUT32.94]
 * The string representing the date is composed of two parts, a date and time.
 *
 * The format of the time is has follows:
 * hh[:mm][:ss][AM|PM]
 * Whitespace can be inserted anywhere between these tokens.  A whitespace consists
 * of space and/or tab characters, which are ignored.
 *
 * The formats for the date part are has follows:
 * mm/[dd/][yy]yy
 * [dd/]mm/[yy]yy
 * [yy]yy/mm/dd
 * January dd[,] [yy]yy
 * dd January [yy]yy
 * [yy]yy January dd
 * Whitespace can be inserted anywhere between these tokens.
 *
 * The formats for the date and time string are has follows.
 * date[whitespace][time]
 * [time][whitespace]date
 *
 * These are the only characters allowed in a string representing a date and time:
 * [A-Z] [a-z] [0-9] ':' '-' '/' ',' ' ' '\t'
 */
HRESULT WINAPI VarDateFromStr(OLECHAR* strIn, LCID lcid, ULONG dwFlags, DATE* pdateOut)
{
    HRESULT ret = S_OK;
    struct tm TM;

    memset( &TM, 0, sizeof(TM) );

    TRACE("( %p, %lx, %lx, %p ), stub\n", strIn, lcid, dwFlags, pdateOut );

    if( DateTimeStringToTm( strIn, dwFlags, &TM ) )
    {
        if( TmToDATE( &TM, pdateOut ) == FALSE )
        {
            ret = E_INVALIDARG;
        }
    }
    else
    {
        ret = DISP_E_TYPEMISMATCH;
    }


	return ret;
}

/******************************************************************************
 *		VarDateFromI1		[OLEAUT32.221]
 */
HRESULT WINAPI VarDateFromI1(CHAR cIn, DATE* pdateOut)
{
	TRACE("( %c, %p ), stub\n", cIn, pdateOut );

	*pdateOut = (DATE) cIn;

	return S_OK;
}

/******************************************************************************
 *		VarDateFromUI2		[OLEAUT32.222]
 */
HRESULT WINAPI VarDateFromUI2(USHORT uiIn, DATE* pdateOut)
{
	TRACE("( %d, %p ), stub\n", uiIn, pdateOut );

	if( uiIn > DATE_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pdateOut = (DATE) uiIn;

	return S_OK;
}

/******************************************************************************
 *		VarDateFromUI4		[OLEAUT32.223]
 */
HRESULT WINAPI VarDateFromUI4(ULONG ulIn, DATE* pdateOut)
{
	TRACE("( %ld, %p ), stub\n", ulIn, pdateOut );

	if( ulIn < DATE_MIN || ulIn > DATE_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pdateOut = (DATE) ulIn;

	return S_OK;
}

/******************************************************************************
 *		VarDateFromBool		[OLEAUT32.96]
 */
HRESULT WINAPI VarDateFromBool(VARIANT_BOOL boolIn, DATE* pdateOut)
{
	TRACE("( %d, %p ), stub\n", boolIn, pdateOut );

	*pdateOut = (DATE) boolIn;

	return S_OK;
}

/**********************************************************************
 *              VarDateFromCy [OLEAUT32.93]
 * Convert currency to date
 */
HRESULT WINAPI VarDateFromCy(CY cyIn, DATE* pdateOut) {
   *pdateOut = (DATE)((((double)cyIn.s.Hi * 4294967296.0) + (double)cyIn.s.Lo) / 10000);

   if (*pdateOut > DATE_MAX || *pdateOut < DATE_MIN) return DISP_E_TYPEMISMATCH;
   return S_OK;
}

/******************************************************************************
 *		VarBstrFromUI1		[OLEAUT32.108]
 */
HRESULT WINAPI VarBstrFromUI1(BYTE bVal, LCID lcid, ULONG dwFlags, BSTR* pbstrOut)
{
	TRACE("( %d, %ld, %ld, %p ), stub\n", bVal, lcid, dwFlags, pbstrOut );
	sprintf( pBuffer, "%d", bVal );

	*pbstrOut =  StringDupAtoBstr( pBuffer );

	return S_OK;
}

/******************************************************************************
 *		VarBstrFromI2		[OLEAUT32.109]
 */
HRESULT WINAPI VarBstrFromI2(short iVal, LCID lcid, ULONG dwFlags, BSTR* pbstrOut)
{
	TRACE("( %d, %ld, %ld, %p ), stub\n", iVal, lcid, dwFlags, pbstrOut );
	sprintf( pBuffer, "%d", iVal );
	*pbstrOut = StringDupAtoBstr( pBuffer );

	return S_OK;
}

/******************************************************************************
 *		VarBstrFromI4		[OLEAUT32.110]
 */
HRESULT WINAPI VarBstrFromI4(LONG lIn, LCID lcid, ULONG dwFlags, BSTR* pbstrOut)
{
	TRACE("( %ld, %ld, %ld, %p ), stub\n", lIn, lcid, dwFlags, pbstrOut );

	sprintf( pBuffer, "%ld", lIn );
	*pbstrOut = StringDupAtoBstr( pBuffer );

	return S_OK;
}

/******************************************************************************
 *		VarBstrFromR4		[OLEAUT32.111]
 */
HRESULT WINAPI VarBstrFromR4(FLOAT fltIn, LCID lcid, ULONG dwFlags, BSTR* pbstrOut)
{
	TRACE("( %f, %ld, %ld, %p ), stub\n", fltIn, lcid, dwFlags, pbstrOut );

	sprintf( pBuffer, "%.7g", fltIn );
	*pbstrOut = StringDupAtoBstr( pBuffer );

	return S_OK;
}

/******************************************************************************
 *		VarBstrFromR8		[OLEAUT32.112]
 */
HRESULT WINAPI VarBstrFromR8(double dblIn, LCID lcid, ULONG dwFlags, BSTR* pbstrOut)
{
	TRACE("( %f, %ld, %ld, %p ), stub\n", dblIn, lcid, dwFlags, pbstrOut );

	sprintf( pBuffer, "%.15g", dblIn );
	*pbstrOut = StringDupAtoBstr( pBuffer );

	return S_OK;
}

/******************************************************************************
 *    VarBstrFromCy   [OLEAUT32.113]
 */
HRESULT WINAPI VarBstrFromCy(CY cyIn, LCID lcid, ULONG dwFlags, BSTR *pbstrOut) {
	FIXME("([cyIn], %08lx, %08lx, %p), stub.\n", lcid, dwFlags, pbstrOut);
	return E_NOTIMPL;
}


/******************************************************************************
 *		VarBstrFromDate		[OLEAUT32.114]
 *
 * The date is implemented using an 8 byte floating-point number.
 * Days are represented by whole numbers increments starting with 0.00 as
 * being December 30 1899, midnight.
 * The hours are expressed as the fractional part of the number.
 * December 30 1899 at midnight = 0.00
 * January 1 1900 at midnight = 2.00
 * January 4 1900 at 6 AM = 5.25
 * January 4 1900 at noon = 5.50
 * December 29 1899 at midnight = -1.00
 * December 18 1899 at midnight = -12.00
 * December 18 1899 at 6AM = -12.25
 * December 18 1899 at 6PM = -12.75
 * December 19 1899 at midnight = -11.00
 * The tm structure is as follows:
 * struct tm {
 *		  int tm_sec;	   seconds after the minute - [0,59]
 *		  int tm_min;	   minutes after the hour - [0,59]
 *		  int tm_hour;	   hours since midnight - [0,23]
 *		  int tm_mday;	   day of the month - [1,31]
 *		  int tm_mon;	   months since January - [0,11]
 *		  int tm_year;	   years
 *		  int tm_wday;	   days since Sunday - [0,6]
 *		  int tm_yday;	   days since January 1 - [0,365]
 *		  int tm_isdst;    daylight savings time flag
 *		  };
 */
HRESULT WINAPI VarBstrFromDate(DATE dateIn, LCID lcid, ULONG dwFlags, BSTR* pbstrOut)
{
    struct tm TM;
    memset( &TM, 0, sizeof(TM) );

    TRACE("( %f, %ld, %ld, %p ), stub\n", dateIn, lcid, dwFlags, pbstrOut );

    if( DateToTm( dateIn, dwFlags, &TM ) == FALSE )
			{
        return E_INVALIDARG;
		}

    if( dwFlags & VAR_DATEVALUEONLY )
			strftime( pBuffer, BUFFER_MAX, "%x", &TM );
    else if( dwFlags & VAR_TIMEVALUEONLY )
			strftime( pBuffer, BUFFER_MAX, "%X", &TM );
		else
        strftime( pBuffer, BUFFER_MAX, "%x %X", &TM );

		*pbstrOut = StringDupAtoBstr( pBuffer );

	return S_OK;
}

/******************************************************************************
 *		VarBstrFromBool		[OLEAUT32.116]
 */
HRESULT WINAPI VarBstrFromBool(VARIANT_BOOL boolIn, LCID lcid, ULONG dwFlags, BSTR* pbstrOut)
{
	TRACE("( %d, %ld, %ld, %p ), stub\n", boolIn, lcid, dwFlags, pbstrOut );

	sprintf( pBuffer, (boolIn == VARIANT_FALSE) ? "False" : "True" );

	*pbstrOut = StringDupAtoBstr( pBuffer );

	return S_OK;
}

/******************************************************************************
 *		VarBstrFromI1		[OLEAUT32.229]
 */
HRESULT WINAPI VarBstrFromI1(CHAR cIn, LCID lcid, ULONG dwFlags, BSTR* pbstrOut)
{
	TRACE("( %c, %ld, %ld, %p ), stub\n", cIn, lcid, dwFlags, pbstrOut );
	sprintf( pBuffer, "%d", cIn );
	*pbstrOut = StringDupAtoBstr( pBuffer );

	return S_OK;
}

/******************************************************************************
 *		VarBstrFromUI2		[OLEAUT32.230]
 */
HRESULT WINAPI VarBstrFromUI2(USHORT uiIn, LCID lcid, ULONG dwFlags, BSTR* pbstrOut)
{
	TRACE("( %d, %ld, %ld, %p ), stub\n", uiIn, lcid, dwFlags, pbstrOut );
	sprintf( pBuffer, "%d", uiIn );
	*pbstrOut = StringDupAtoBstr( pBuffer );

	return S_OK;
}

/******************************************************************************
 *		VarBstrFromUI4		[OLEAUT32.231]
 */
HRESULT WINAPI VarBstrFromUI4(ULONG ulIn, LCID lcid, ULONG dwFlags, BSTR* pbstrOut)
{
	TRACE("( %ld, %ld, %ld, %p ), stub\n", ulIn, lcid, dwFlags, pbstrOut );
	sprintf( pBuffer, "%ld", ulIn );
	*pbstrOut = StringDupAtoBstr( pBuffer );

	return S_OK;
}

/******************************************************************************
 *		VarBstrFromUI8		[OLEAUT32.369]
 */
HRESULT WINAPI VarBstrFromUI8(ULONG64 ullIn, LCID lcid, ULONG dwFlags, BSTR* pbstrOut)
{
	TRACE("( %llu, %u, %u, %p ), stub\n", ullIn, lcid, dwFlags, pbstrOut );
	sprintf( pBuffer, "%llu", ullIn );
	*pbstrOut = StringDupAtoBstr( pBuffer );

	return S_OK;
}

/******************************************************************************
 *		VarBoolFromUI1		[OLEAUT32.118]
 */
HRESULT WINAPI VarBoolFromUI1(BYTE bIn, VARIANT_BOOL* pboolOut)
{
	TRACE("( %d, %p ), stub\n", bIn, pboolOut );

	if( bIn == 0 )
	{
		*pboolOut = VARIANT_FALSE;
	}
	else
	{
		*pboolOut = VARIANT_TRUE;
	}

	return S_OK;
}

/******************************************************************************
 *		VarBoolFromI2		[OLEAUT32.119]
 */
HRESULT WINAPI VarBoolFromI2(short sIn, VARIANT_BOOL* pboolOut)
{
	TRACE("( %d, %p ), stub\n", sIn, pboolOut );

	*pboolOut = (sIn) ? VARIANT_TRUE : VARIANT_FALSE;

	return S_OK;
}

/******************************************************************************
 *		VarBoolFromI4		[OLEAUT32.120]
 */
HRESULT WINAPI VarBoolFromI4(LONG lIn, VARIANT_BOOL* pboolOut)
{
	TRACE("( %ld, %p ), stub\n", lIn, pboolOut );

	*pboolOut = (lIn) ? VARIANT_TRUE : VARIANT_FALSE;

	return S_OK;
}

/******************************************************************************
 *		VarBoolFromR4		[OLEAUT32.121]
 */
HRESULT WINAPI VarBoolFromR4(FLOAT fltIn, VARIANT_BOOL* pboolOut)
{
	TRACE("( %f, %p ), stub\n", fltIn, pboolOut );

	*pboolOut = (fltIn == 0.0) ? VARIANT_FALSE : VARIANT_TRUE;

	return S_OK;
}

/******************************************************************************
 *		VarBoolFromR8		[OLEAUT32.122]
 */
HRESULT WINAPI VarBoolFromR8(double dblIn, VARIANT_BOOL* pboolOut)
{
	TRACE("( %f, %p ), stub\n", dblIn, pboolOut );

	*pboolOut = (dblIn == 0.0) ? VARIANT_FALSE : VARIANT_TRUE;

	return S_OK;
}

/******************************************************************************
 *		VarBoolFromDate		[OLEAUT32.123]
 */
HRESULT WINAPI VarBoolFromDate(DATE dateIn, VARIANT_BOOL* pboolOut)
{
	TRACE("( %f, %p ), stub\n", dateIn, pboolOut );

	*pboolOut = (dateIn == 0.0) ? VARIANT_FALSE : VARIANT_TRUE;

	return S_OK;
}

/******************************************************************************
 *		VarBoolFromStr		[OLEAUT32.125]
 */
HRESULT WINAPI VarBoolFromStr(OLECHAR* strIn, LCID lcid, ULONG dwFlags, VARIANT_BOOL* pboolOut)
{
	HRESULT ret = S_OK;
	char* pNewString = NULL;

	TRACE("( %p, %ld, %ld, %p ), stub\n", strIn, lcid, dwFlags, pboolOut );

    pNewString = HEAP_strdupWtoA( GetProcessHeap(), 0, strIn );

	if( pNewString == NULL || strlen( pNewString ) == 0 )
	{
		ret = DISP_E_TYPEMISMATCH;
	}

	if( ret == S_OK )
	{
		if( strncasecmp( pNewString, "True", strlen( pNewString ) ) == 0 )
		{
			*pboolOut = VARIANT_TRUE;
		}
		else if( strncasecmp( pNewString, "False", strlen( pNewString ) ) == 0 )
		{
			*pboolOut = VARIANT_FALSE;
		}
		else
		{
			/* Try converting the string to a floating point number.
			 */
			double dValue = 0.0;
			HRESULT res = VarR8FromStr( strIn, lcid, dwFlags, &dValue );
			if( res != S_OK )
			{
				ret = DISP_E_TYPEMISMATCH;
			}
			else
				*pboolOut = (dValue == 0.0) ?
						VARIANT_FALSE : VARIANT_TRUE;
		}
	}

	HeapFree( GetProcessHeap(), 0, pNewString );

	return ret;
}

/******************************************************************************
 *		VarBoolFromI1		[OLEAUT32.233]
 */
HRESULT WINAPI VarBoolFromI1(CHAR cIn, VARIANT_BOOL* pboolOut)
{
	TRACE("( %c, %p ), stub\n", cIn, pboolOut );

	*pboolOut = (cIn == 0) ? VARIANT_FALSE : VARIANT_TRUE;

	return S_OK;
}

/******************************************************************************
 *		VarBoolFromUI2		[OLEAUT32.234]
 */
HRESULT WINAPI VarBoolFromUI2(USHORT uiIn, VARIANT_BOOL* pboolOut)
{
	TRACE("( %d, %p ), stub\n", uiIn, pboolOut );

	*pboolOut = (uiIn == 0) ? VARIANT_FALSE : VARIANT_TRUE;

	return S_OK;
}

/******************************************************************************
 *		VarBoolFromUI4		[OLEAUT32.235]
 */
HRESULT WINAPI VarBoolFromUI4(ULONG ulIn, VARIANT_BOOL* pboolOut)
{
	TRACE("( %ld, %p ), stub\n", ulIn, pboolOut );

	*pboolOut = (ulIn == 0) ? VARIANT_FALSE : VARIANT_TRUE;

	return S_OK;
}

/**********************************************************************
 *              VarBoolFromCy [OLEAUT32.124]
 * Convert currency to boolean
 */
HRESULT WINAPI VarBoolFromCy(CY cyIn, VARIANT_BOOL* pboolOut) {
      if (cyIn.s.Hi || cyIn.s.Lo) *pboolOut = -1;
      else *pboolOut = 0;

      return S_OK;
}

/******************************************************************************
 *		VarI1FromUI1		[OLEAUT32.244]
 */
HRESULT WINAPI VarI1FromUI1(BYTE bIn, CHAR* pcOut)
{
	TRACE("( %d, %p ), stub\n", bIn, pcOut );

	/* Check range of value.
	 */
	if( bIn > CHAR_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pcOut = (CHAR) bIn;

	return S_OK;
}

/******************************************************************************
 *		VarI1FromI2		[OLEAUT32.245]
 */
HRESULT WINAPI VarI1FromI2(short uiIn, CHAR* pcOut)
{
	TRACE("( %d, %p ), stub\n", uiIn, pcOut );

	if( uiIn > CHAR_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pcOut = (CHAR) uiIn;

	return S_OK;
}

/******************************************************************************
 *		VarI1FromI4		[OLEAUT32.246]
 */
HRESULT WINAPI VarI1FromI4(LONG lIn, CHAR* pcOut)
{
	TRACE("( %ld, %p ), stub\n", lIn, pcOut );

	if( lIn < CHAR_MIN || lIn > CHAR_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pcOut = (CHAR) lIn;

	return S_OK;
}

/******************************************************************************
 *		VarI1FromR4		[OLEAUT32.247]
 */
HRESULT WINAPI VarI1FromR4(FLOAT fltIn, CHAR* pcOut)
{
	TRACE("( %f, %p ), stub\n", fltIn, pcOut );

    fltIn = oleaut32_round( fltIn );
	if( fltIn < CHAR_MIN || fltIn > CHAR_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pcOut = (CHAR) fltIn;

	return S_OK;
}

/******************************************************************************
 *		VarI1FromR8		[OLEAUT32.248]
 */
HRESULT WINAPI VarI1FromR8(double dblIn, CHAR* pcOut)
{
	TRACE("( %f, %p ), stub\n", dblIn, pcOut );

    dblIn = oleaut32_round( dblIn );
    if( dblIn < CHAR_MIN || dblIn > CHAR_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pcOut = (CHAR) dblIn;

	return S_OK;
}

/******************************************************************************
 *		VarI1FromDate		[OLEAUT32.249]
 */
HRESULT WINAPI VarI1FromDate(DATE dateIn, CHAR* pcOut)
{
	TRACE("( %f, %p ), stub\n", dateIn, pcOut );

    dateIn = oleaut32_round( dateIn );
	if( dateIn < CHAR_MIN || dateIn > CHAR_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pcOut = (CHAR) dateIn;

	return S_OK;
}

/******************************************************************************
 *		VarI1FromStr		[OLEAUT32.251]
 */
HRESULT WINAPI VarI1FromStr(OLECHAR* strIn, LCID lcid, ULONG dwFlags, CHAR* pcOut)
{
	double dValue = 0.0;
	LPSTR pNewString = NULL;

	TRACE("( %p, %ld, %ld, %p ), stub\n", strIn, lcid, dwFlags, pcOut );

	/* Check if we have a valid argument
	 */
	pNewString = HEAP_strdupWtoA( GetProcessHeap(), 0, strIn );
	RemoveCharacterFromString( pNewString, "," );
	if( IsValidRealString( pNewString ) == FALSE )
	{
		return DISP_E_TYPEMISMATCH;
	}

	/* Convert the valid string to a floating point number.
	 */
	dValue = atof( pNewString );

	/* We don't need the string anymore so free it.
	 */
	HeapFree( GetProcessHeap(), 0, pNewString );

	/* Check range of value.
     */
    dValue = oleaut32_round( dValue );
	if( dValue < CHAR_MIN || dValue > CHAR_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pcOut = (CHAR) dValue;

	return S_OK;
}

/******************************************************************************
 *		VarI1FromBool		[OLEAUT32.253]
 */
HRESULT WINAPI VarI1FromBool(VARIANT_BOOL boolIn, CHAR* pcOut)
{
	TRACE("( %d, %p ), stub\n", boolIn, pcOut );

	*pcOut = (CHAR) boolIn;

	return S_OK;
}

/******************************************************************************
 *		VarI1FromUI2		[OLEAUT32.254]
 */
HRESULT WINAPI VarI1FromUI2(USHORT uiIn, CHAR* pcOut)
{
	TRACE("( %d, %p ), stub\n", uiIn, pcOut );

	if( uiIn > CHAR_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pcOut = (CHAR) uiIn;

	return S_OK;
}

/******************************************************************************
 *		VarI1FromUI4		[OLEAUT32.255]
 */
HRESULT WINAPI VarI1FromUI4(ULONG ulIn, CHAR* pcOut)
{
	TRACE("( %ld, %p ), stub\n", ulIn, pcOut );

	if( ulIn > CHAR_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pcOut = (CHAR) ulIn;

	return S_OK;
}

/**********************************************************************
 *              VarI1FromCy [OLEAUT32.250]
 * Convert currency to signed char
 */
HRESULT WINAPI VarI1FromCy(CY cyIn, CHAR* pcOut) {
   double t = oleaut32_round((((double)cyIn.s.Hi * 4294967296.0) + (double)cyIn.s.Lo) / 10000);

   if (t > CHAR_MAX || t < CHAR_MIN) return DISP_E_OVERFLOW;

   *pcOut = (CHAR)t;
   return S_OK;
}

/******************************************************************************
 *		VarUI2FromUI1		[OLEAUT32.257]
 */
HRESULT WINAPI VarUI2FromUI1(BYTE bIn, USHORT* puiOut)
{
	TRACE("( %d, %p ), stub\n", bIn, puiOut );

	*puiOut = (USHORT) bIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI2FromI2		[OLEAUT32.258]
 */
HRESULT WINAPI VarUI2FromI2(short uiIn, USHORT* puiOut)
{
	TRACE("( %d, %p ), stub\n", uiIn, puiOut );

	if( uiIn < UI2_MIN )
	{
		return DISP_E_OVERFLOW;
	}

	*puiOut = (USHORT) uiIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI2FromI4		[OLEAUT32.259]
 */
HRESULT WINAPI VarUI2FromI4(LONG lIn, USHORT* puiOut)
{
	TRACE("( %ld, %p ), stub\n", lIn, puiOut );

	if( lIn < UI2_MIN || lIn > UI2_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*puiOut = (USHORT) lIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI2FromR4		[OLEAUT32.260]
 */
HRESULT WINAPI VarUI2FromR4(FLOAT fltIn, USHORT* puiOut)
{
	TRACE("( %f, %p ), stub\n", fltIn, puiOut );

    fltIn = oleaut32_round( fltIn );
	if( fltIn < UI2_MIN || fltIn > UI2_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*puiOut = (USHORT) fltIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI2FromR8		[OLEAUT32.261]
 */
HRESULT WINAPI VarUI2FromR8(double dblIn, USHORT* puiOut)
{
	TRACE("( %f, %p ), stub\n", dblIn, puiOut );

    dblIn = oleaut32_round( dblIn );
    if( dblIn < UI2_MIN || dblIn > UI2_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*puiOut = (USHORT) dblIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI2FromDate		[OLEAUT32.262]
 */
HRESULT WINAPI VarUI2FromDate(DATE dateIn, USHORT* puiOut)
{
	TRACE("( %f, %p ), stub\n", dateIn, puiOut );

    dateIn = oleaut32_round( dateIn );
	if( dateIn < UI2_MIN || dateIn > UI2_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*puiOut = (USHORT) dateIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI2FromStr		[OLEAUT32.264]
 */
HRESULT WINAPI VarUI2FromStr(OLECHAR* strIn, LCID lcid, ULONG dwFlags, USHORT* puiOut)
{
	double dValue = 0.0;
	LPSTR pNewString = NULL;

	TRACE("( %p, %ld, %ld, %p ), stub\n", strIn, lcid, dwFlags, puiOut );

	/* Check if we have a valid argument
	 */
	pNewString = HEAP_strdupWtoA( GetProcessHeap(), 0, strIn );
	RemoveCharacterFromString( pNewString, "," );
	if( IsValidRealString( pNewString ) == FALSE )
	{
		return DISP_E_TYPEMISMATCH;
	}

	/* Convert the valid string to a floating point number.
	 */
	dValue = atof( pNewString );

	/* We don't need the string anymore so free it.
	 */
	HeapFree( GetProcessHeap(), 0, pNewString );

	/* Check range of value.
     */
    dValue = oleaut32_round( dValue );
	if( dValue < UI2_MIN || dValue > UI2_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*puiOut = (USHORT) dValue;

	return S_OK;
}

/******************************************************************************
 *		VarUI2FromBool		[OLEAUT32.266]
 */
HRESULT WINAPI VarUI2FromBool(VARIANT_BOOL boolIn, USHORT* puiOut)
{
	TRACE("( %d, %p ), stub\n", boolIn, puiOut );

	*puiOut = (USHORT) boolIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI2FromI1		[OLEAUT32.267]
 */
HRESULT WINAPI VarUI2FromI1(CHAR cIn, USHORT* puiOut)
{
	TRACE("( %c, %p ), stub\n", cIn, puiOut );

	*puiOut = (USHORT) cIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI2FromUI4		[OLEAUT32.268]
 */
HRESULT WINAPI VarUI2FromUI4(ULONG ulIn, USHORT* puiOut)
{
	TRACE("( %ld, %p ), stub\n", ulIn, puiOut );

	if( ulIn > UI2_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*puiOut = (USHORT) ulIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI4FromStr		[OLEAUT32.277]
 */
HRESULT WINAPI VarUI4FromStr(OLECHAR* strIn, LCID lcid, ULONG dwFlags, ULONG* pulOut)
{
	double dValue = 0.0;
	LPSTR pNewString = NULL;

	TRACE("( %p, %ld, %ld, %p ), stub\n", strIn, lcid, dwFlags, pulOut );

	/* Check if we have a valid argument
	 */
	pNewString = HEAP_strdupWtoA( GetProcessHeap(), 0, strIn );
	RemoveCharacterFromString( pNewString, "," );
	if( IsValidRealString( pNewString ) == FALSE )
	{
		return DISP_E_TYPEMISMATCH;
	}

	/* Convert the valid string to a floating point number.
	 */
	dValue = atof( pNewString );

	/* We don't need the string anymore so free it.
	 */
	HeapFree( GetProcessHeap(), 0, pNewString );

	/* Check range of value.
     */
    dValue = oleaut32_round( dValue );
	if( dValue < UI4_MIN || dValue > UI4_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pulOut = (ULONG) dValue;

	return S_OK;
}

/**********************************************************************
 *              VarUI2FromCy [OLEAUT32.263]
 * Convert currency to unsigned short
 */
HRESULT WINAPI VarUI2FromCy(CY cyIn, USHORT* pusOut) {
   double t = oleaut32_round((((double)cyIn.s.Hi * 4294967296.0) + (double)cyIn.s.Lo) / 10000);

   if (t > UI2_MAX || t < UI2_MIN) return DISP_E_OVERFLOW;

   *pusOut = (USHORT)t;

   return S_OK;
}

/******************************************************************************
 *		VarUI4FromUI1		[OLEAUT32.270]
 */
HRESULT WINAPI VarUI4FromUI1(BYTE bIn, ULONG* pulOut)
{
	TRACE("( %d, %p ), stub\n", bIn, pulOut );

	*pulOut = (USHORT) bIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI4FromI2		[OLEAUT32.271]
 */
HRESULT WINAPI VarUI4FromI2(short uiIn, ULONG* pulOut)
{
	TRACE("( %d, %p ), stub\n", uiIn, pulOut );

	if( uiIn < UI4_MIN )
	{
		return DISP_E_OVERFLOW;
	}

	*pulOut = (ULONG) uiIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI4FromI4		[OLEAUT32.272]
 */
HRESULT WINAPI VarUI4FromI4(LONG lIn, ULONG* pulOut)
{
	TRACE("( %ld, %p ), stub\n", lIn, pulOut );

	if( lIn < 0 )
	{
		return DISP_E_OVERFLOW;
	}

	*pulOut = (ULONG) lIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI4FromR4		[OLEAUT32.273]
 */
HRESULT WINAPI VarUI4FromR4(FLOAT fltIn, ULONG* pulOut)
{
    fltIn = oleaut32_round( fltIn );
    if( fltIn < UI4_MIN || fltIn > UI4_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pulOut = (ULONG) fltIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI4FromR8		[OLEAUT32.274]
 */
HRESULT WINAPI VarUI4FromR8(double dblIn, ULONG* pulOut)
{
	TRACE("( %f, %p ), stub\n", dblIn, pulOut );

	dblIn = oleaut32_round( dblIn );
	if( dblIn < UI4_MIN || dblIn > UI4_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pulOut = (ULONG) dblIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI4FromDate		[OLEAUT32.275]
 */
HRESULT WINAPI VarUI4FromDate(DATE dateIn, ULONG* pulOut)
{
	TRACE("( %f, %p ), stub\n", dateIn, pulOut );

	dateIn = oleaut32_round( dateIn );
	if( dateIn < UI4_MIN || dateIn > UI4_MAX )
	{
		return DISP_E_OVERFLOW;
	}

	*pulOut = (ULONG) dateIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI4FromBool		[OLEAUT32.279]
 */
HRESULT WINAPI VarUI4FromBool(VARIANT_BOOL boolIn, ULONG* pulOut)
{
	TRACE("( %d, %p ), stub\n", boolIn, pulOut );

	*pulOut = (ULONG) boolIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI4FromI1		[OLEAUT32.280]
 */
HRESULT WINAPI VarUI4FromI1(CHAR cIn, ULONG* pulOut)
{
	TRACE("( %c, %p ), stub\n", cIn, pulOut );

	*pulOut = (ULONG) cIn;

	return S_OK;
}

/******************************************************************************
 *		VarUI4FromUI2		[OLEAUT32.281]
 */
HRESULT WINAPI VarUI4FromUI2(USHORT uiIn, ULONG* pulOut)
{
	TRACE("( %d, %p ), stub\n", uiIn, pulOut );

	*pulOut = (ULONG) uiIn;

	return S_OK;
}

/**********************************************************************
 *              VarUI4FromCy [OLEAUT32.276]
 * Convert currency to unsigned long
 */
HRESULT WINAPI VarUI4FromCy(CY cyIn, ULONG* pulOut) {
   double t = oleaut32_round((((double)cyIn.s.Hi * 4294967296.0) + (double)cyIn.s.Lo) / 10000);

   if (t > UI4_MAX || t < UI4_MIN) return DISP_E_OVERFLOW;

   *pulOut = (ULONG)t;

   return S_OK;
}

/**********************************************************************
 *              VarCyFromUI1 [OLEAUT32.98]
 * Convert unsigned char to currency
 */
HRESULT WINAPI VarCyFromUI1(BYTE bIn, CY* pcyOut) {
    pcyOut->s.Hi = 0;
    pcyOut->s.Lo = ((ULONG)bIn) * 10000;

    return S_OK;
}

/**********************************************************************
 *              VarCyFromI2 [OLEAUT32.99]
 * Convert signed short to currency
 */
HRESULT WINAPI VarCyFromI2(short sIn, CY* pcyOut) {
    if (sIn < 0) pcyOut->s.Hi = -1;
    else pcyOut->s.Hi = 0;
    pcyOut->s.Lo = ((ULONG)sIn) * 10000;

    return S_OK;
}

/**********************************************************************
 *              VarCyFromI4 [OLEAUT32.100]
 * Convert signed long to currency
 */
HRESULT WINAPI VarCyFromI4(LONG lIn, CY* pcyOut) {
      double t = (double)lIn * (double)10000;
      pcyOut->s.Hi = (LONG)(t / (double)4294967296.0);
      pcyOut->s.Lo = (ULONG)fmod(t, (double)4294967296.0);
      if (lIn < 0) pcyOut->s.Hi--;

      return S_OK;
}

/**********************************************************************
 *              VarCyFromR4 [OLEAUT32.101]
 * Convert float to currency
 */
HRESULT WINAPI VarCyFromR4(FLOAT fltIn, CY* pcyOut) {
   double t = oleaut32_round((double)fltIn * (double)10000);
   pcyOut->s.Hi = (LONG)(t / (double)4294967296.0);
   pcyOut->s.Lo = (ULONG)fmod(t, (double)4294967296.0);
   if (fltIn < 0) pcyOut->s.Hi--;

   return S_OK;
}

/**********************************************************************
 *              VarCyFromR8 [OLEAUT32.102]
 * Convert double to currency
 */
HRESULT WINAPI VarCyFromR8(double dblIn, CY* pcyOut) {
   double t = oleaut32_round(dblIn * (double)10000);
   pcyOut->s.Hi = (LONG)(t / (double)4294967296.0);
   pcyOut->s.Lo = (ULONG)fmod(t, (double)4294967296.0);
   if (dblIn < 0) pcyOut->s.Hi--;

   return S_OK;
}

/**********************************************************************
 *              VarCyFromDate [OLEAUT32.103]
 * Convert date to currency
 */
HRESULT WINAPI VarCyFromDate(DATE dateIn, CY* pcyOut) {
   double t = oleaut32_round((double)dateIn * (double)10000);
   pcyOut->s.Hi = (LONG)(t / (double)4294967296.0);
   pcyOut->s.Lo = (ULONG)fmod(t, (double)4294967296.0);
   if (dateIn < 0) pcyOut->s.Hi--;

   return S_OK;
}

/**********************************************************************
 *              VarCyFromStr [OLEAUT32.104]
 */
HRESULT WINAPI VarCyFromStr(OLECHAR *strIn, LCID lcid, ULONG dwFlags, CY *pcyOut) {
	FIXME("(%p, %08lx, %08lx, %p), stub.\n", strIn, lcid, dwFlags, pcyOut);
	return E_NOTIMPL;
}


/**********************************************************************
 *              VarCyFromBool [OLEAUT32.106]
 * Convert boolean to currency
 */
HRESULT WINAPI VarCyFromBool(VARIANT_BOOL boolIn, CY* pcyOut) {
   if (boolIn < 0) pcyOut->s.Hi = -1;
   else pcyOut->s.Hi = 0;
   pcyOut->s.Lo = (ULONG)boolIn * (ULONG)10000;

   return S_OK;
}

/**********************************************************************
 *              VarCyFromI1 [OLEAUT32.225]
 * Convert signed char to currency
 */
HRESULT WINAPI VarCyFromI1(signed char cIn, CY* pcyOut) {
   if (cIn < 0) pcyOut->s.Hi = -1;
   else pcyOut->s.Hi = 0;
   pcyOut->s.Lo = (ULONG)cIn * (ULONG)10000;

   return S_OK;
}

/**********************************************************************
 *              VarCyFromUI2 [OLEAUT32.226]
 * Convert unsigned short to currency
 */
HRESULT WINAPI VarCyFromUI2(USHORT usIn, CY* pcyOut) {
   pcyOut->s.Hi = 0;
   pcyOut->s.Lo = (ULONG)usIn * (ULONG)10000;

   return S_OK;
}

/**********************************************************************
 *              VarCyFromUI4 [OLEAUT32.227]
 * Convert unsigned long to currency
 */
HRESULT WINAPI VarCyFromUI4(ULONG ulIn, CY* pcyOut) {
   double t = (double)ulIn * (double)10000;
   pcyOut->s.Hi = (LONG)(t / (double)4294967296.0);
   pcyOut->s.Lo = (ULONG)fmod(t, (double)4294967296.0);

   return S_OK;
}


/**********************************************************************
 *              DosDateTimeToVariantTime [OLEAUT32.14]
 * Convert dos representation of time to the date and time representation
 * stored in a variant.
 */
INT WINAPI DosDateTimeToVariantTime(USHORT wDosDate, USHORT wDosTime,
                                    DATE *pvtime)
{
    struct tm t;

    TRACE("( 0x%x, 0x%x, %p ), stub\n", wDosDate, wDosTime, pvtime );

    t.tm_sec = (wDosTime & 0x001f) * 2;
    t.tm_min = (wDosTime & 0x07e0) >> 5;
    t.tm_hour = (wDosTime & 0xf800) >> 11;

    t.tm_mday = (wDosDate & 0x001f);
    t.tm_mon = (wDosDate & 0x01e0) >> 5;
    t.tm_year = ((wDosDate & 0xfe00) >> 9) + 1980;

    return TmToDATE( &t, pvtime );
}


/**********************************************************************
 *              VarParseNumFromStr [OLEAUT32.46]
 */
HRESULT WINAPI VarParseNumFromStr(OLECHAR * strIn, LCID lcid, ULONG dwFlags,
                                  NUMPARSE * pnumprs, BYTE * rgbDig)
{
    int i;
    int cDig;

    TRACE("('%s',flags=%lx,....), partial stub!\n",debugstr_w(strIn),
          pnumprs->dwInFlags);

    if (pnumprs->dwInFlags & (NUMPRS_HEX_OCT | NUMPRS_PARENS |
                              NUMPRS_THOUSANDS | NUMPRS_CURRENCY |
                              NUMPRS_EXPONENT))
        WARN ("dwInFlags contains some unimplemented flags: %lx Str: '%s'\n",
              pnumprs->dwInFlags, debugstr_w (strIn));

    cDig = 0;
    i = 0;

    pnumprs->dwOutFlags = 0;
    pnumprs->nPwr10	= 0;
    pnumprs->nBaseShift	= 0;
    pnumprs->cchUsed    = 0;

    /* Strip leading whitespace if NUMPRS_LEADING_WHITE */
    while (strIn[i] && (pnumprs->dwInFlags & NUMPRS_LEADING_WHITE) &&
           isspaceW (strIn[i]))
    {
        pnumprs->dwOutFlags |= NUMPRS_LEADING_WHITE;
        i++;
    }

    /* Handle leading + or - */
    if (strIn[i] && (pnumprs->dwInFlags & NUMPRS_LEADING_PLUS) &&
        (strIn[i] == '+'))
    {
        pnumprs->dwOutFlags |= NUMPRS_LEADING_PLUS;
        i++;
    }
    else if (strIn[i] && (pnumprs->dwInFlags & NUMPRS_LEADING_MINUS) &&
             (strIn[i] == '-'))
    {
        pnumprs->dwOutFlags |= (NUMPRS_NEG | NUMPRS_LEADING_MINUS);
        i++;
    }

    /* Strip leading zeros */
    while ((strIn[i] == '0') && (strIn[i+1] >= '0') && (strIn[i+1] <= '9'))
        i++;

    /* Get digits */
    for (; strIn[i] ;i++)
    {
        if ((strIn[i] >= '0') && (strIn[i] <= '9'))
        {
            /* Check if we're overflowing the dest buffer */
            if (cDig >= pnumprs->cDig)
            {
                pnumprs->cDig = cDig;
                pnumprs->cchUsed = i;
                pnumprs->dwOutFlags |= NUMPRS_INEXACT;
                return DISP_E_OVERFLOW;
            }

            rgbDig[cDig] = strIn[i] - '0';
            cDig++;
            if (pnumprs->dwOutFlags & NUMPRS_DECIMAL)
                pnumprs->nPwr10--;
        }
        else if (strIn[i] == '.' && (pnumprs->dwInFlags & NUMPRS_DECIMAL))
        {
            if (pnumprs->dwOutFlags & NUMPRS_DECIMAL)
                break;
            pnumprs->dwOutFlags |= NUMPRS_DECIMAL;
        }
        else
            break;
    }

    /* Handle trailing 0's, making sure to handle there being nothing but */
    while ((cDig >= 2) && (rgbDig[cDig - 1] == 0))
    {
        pnumprs->nPwr10++;
        cDig--;
    }

    /* Handle trailing + or - if no sign seen so far */
    if (!(pnumprs->dwOutFlags & (NUMPRS_LEADING_PLUS | NUMPRS_LEADING_MINUS)))
    {
        if (strIn[i] && (pnumprs->dwInFlags & NUMPRS_TRAILING_PLUS) &&
            (strIn[i] == '+'))
        {
            pnumprs->dwOutFlags |= NUMPRS_TRAILING_PLUS;
            i++;
        }
        else if (strIn[i] && (pnumprs->dwInFlags & NUMPRS_TRAILING_MINUS) &&
                 (strIn[i] == '-'))
        {
            pnumprs->dwOutFlags |= (NUMPRS_NEG | NUMPRS_TRAILING_MINUS);
            i++;
        }
    }

    /* Handle trailing whitespace */
    while (strIn[i] && (pnumprs->dwInFlags & NUMPRS_TRAILING_WHITE) &&
           isspaceW (strIn[i]))
    {
        pnumprs->dwOutFlags |= NUMPRS_TRAILING_WHITE;
        i++;
    }

    pnumprs->cDig = cDig;

    /* Check for nothing being parsed */
    if (cDig == 0)
        return DISP_E_TYPEMISMATCH;

    pnumprs->cchUsed = i;
    return S_OK;
}


/**********************************************************************
 *              VarNumFromParseNum [OLEAUT32.47]
 */
HRESULT WINAPI VarNumFromParseNum(NUMPARSE * pnumprs, BYTE * rgbDig,
                                  ULONG dwVtBits, VARIANT * pvar)
{
    long long int xint = 0;
    int i;
    TRACE("(dwVtBits=0x%lx,....)\n",dwVtBits);

    if (pnumprs->nBaseShift != 0)
        ERR ("Unsupported nBaseShift: %d\n", pnumprs->nBaseShift);

    if (pnumprs->nPwr10 < 0)
    {
        long double f = 0.0;
        int Count = pnumprs->cDig + pnumprs->nPwr10;
        int CurChar = 0;

        for (CurChar = 0; CurChar < Count; CurChar++)
            f = (f * 10.0) + rgbDig[CurChar];

        Count = -(pnumprs->nPwr10);
        for (i = 0; i < Count; i++,CurChar++)
            f = f + (rgbDig[CurChar] / powl (10.0, (i + 1)));

        if (pnumprs->dwOutFlags & NUMPRS_NEG)
            f = -f;

        TRACE ("Value: %Lf\n", f);

        /* Need to chose "best fit" option; the order is a guess in the
           case of >1 option being specified - really should test on
           Windows, but common case is that only one is specified */
        if (dwVtBits & VTBIT_R8) {
            V_VT(pvar) = VT_R8;
            V_UNION(pvar,dblVal) = f;
            return S_OK;
        }
        else if (dwVtBits & VTBIT_R4) {
            V_VT(pvar) = VT_R4;
            V_UNION(pvar,fltVal) = f;
            return S_OK;
        }
        else if (pnumprs->dwOutFlags & NUMPRS_NEG)
        {
            /* Prefer signed types if negative number */
            if (dwVtBits & VTBIT_I4) {
                V_VT(pvar) = VT_I4;
                V_UNION(pvar,intVal) = f;
                return S_OK;
            }
            else if (dwVtBits & VTBIT_I2) {
                V_VT(pvar) = VT_I2;
                V_UNION(pvar,iVal) = f;
                return S_OK;
            }
            else if (dwVtBits & VTBIT_I1) {
                V_VT(pvar) = VT_I1;
                V_UNION(pvar,cVal) = f;
                return S_OK;
            }
        }
        else if (dwVtBits & VTBIT_UI4) {
            V_VT(pvar) = VT_UI4;
            V_UNION(pvar,uintVal) = f;
            return S_OK;
        }
        else if (dwVtBits & VTBIT_I4) {
            V_VT(pvar) = VT_I4;
            V_UNION(pvar,intVal) = f;
            return S_OK;
        }
        else if (dwVtBits & VTBIT_UI2) {
            V_VT(pvar) = VT_UI2;
            V_UNION(pvar,uiVal) = f;
            return S_OK;
        }
        else if (dwVtBits & VTBIT_I2) {
            V_VT(pvar) = VT_I2;
            V_UNION(pvar,iVal) = f;
            return S_OK;
        }
        else if (dwVtBits & VTBIT_UI1) {
            V_VT(pvar) = VT_UI1;
            V_UNION(pvar,bVal) = f;
            return S_OK;
        }
        else if (dwVtBits & VTBIT_I1) {
            V_VT(pvar) = VT_I1;
            V_UNION(pvar,cVal) = f;
            return S_OK;
        }

        ERR ("vtbitmask is unsupported for nPwr10 < 0 %lx\n", dwVtBits);
        return E_FAIL;
    }

    for (i=0;i<pnumprs->cDig;i++)
	xint = xint*10 + rgbDig[i];

    for (i = 0; i < pnumprs->nPwr10; i++)
        xint *= 10;

    if (pnumprs->dwOutFlags & NUMPRS_NEG)
        xint = -xint;

    TRACE ("Value: %lld\n", xint);

    /* Need to chose "best fit" option; the order is a guess in the
       case of >1 option being specified - really should test on
       Windows, but common case is that only one is specified */
    if (pnumprs->dwOutFlags & NUMPRS_NEG)
    {
        /* Prefer signed types if negative number */
        if (dwVtBits & VTBIT_I4) {
            V_VT(pvar) = VT_I4;
            V_UNION(pvar,intVal) = xint;
            return S_OK;
        }
        else if (dwVtBits & VTBIT_I2) {
            V_VT(pvar) = VT_I2;
            V_UNION(pvar,iVal) = xint;
            return S_OK;
        }
        else if (dwVtBits & VTBIT_I1) {
            V_VT(pvar) = VT_I1;
            V_UNION(pvar,cVal) = xint;
            return S_OK;
        }
    }
    if (dwVtBits & VTBIT_UI4) {
	V_VT(pvar) = VT_UI4;
	V_UNION(pvar,uintVal) = xint;
	return S_OK;
    }
    else if (dwVtBits & VTBIT_I4) {
	V_VT(pvar) = VT_I4;
	V_UNION(pvar,intVal) = xint;
	return S_OK;
    }
    else if (dwVtBits & VTBIT_UI2) {
	V_VT(pvar) = VT_UI2;
	V_UNION(pvar,uiVal) = xint;
	return S_OK;
    }
    else if (dwVtBits & VTBIT_I2) {
	V_VT(pvar) = VT_I2;
	V_UNION(pvar,iVal) = xint;
	return S_OK;
    }
    else if (dwVtBits & VTBIT_UI1) {
	V_VT(pvar) = VT_UI1;
	V_UNION(pvar,bVal) = xint;
	return S_OK;
    }
    else if (dwVtBits & VTBIT_I1) {
	V_VT(pvar) = VT_I1;
	V_UNION(pvar,cVal) = xint;
	return S_OK;
    }
    else if (dwVtBits & VTBIT_R8) {
	V_VT(pvar) = VT_R8;
	V_UNION(pvar,dblVal) = xint;
	return S_OK;
    }
    else if (dwVtBits & VTBIT_R4) {
	V_VT(pvar) = VT_R4;
	V_UNION(pvar,fltVal) = xint;
	return S_OK;
    }

    ERR("vtbitmask is unsupported %lx\n",dwVtBits);
    return E_FAIL;
}


/**********************************************************************
 *              VariantTimeToDosDateTime [OLEAUT32.13]
 * Convert variant representation of time to the date and time representation
 * stored in dos.
 */
INT WINAPI VariantTimeToDosDateTime(DATE pvtime, USHORT *wDosDate, USHORT *wDosTime)
{
    struct tm t;
    *wDosTime = 0;
    *wDosDate = 0;

    TRACE("( 0x%x, 0x%x, %p ), stub\n", *wDosDate, *wDosTime, &pvtime );

    if (DateToTm(pvtime, 0, &t) < 0) return 0;

    *wDosTime = *wDosTime | (t.tm_sec / 2);
    *wDosTime = *wDosTime | (t.tm_min << 5);
    *wDosTime = *wDosTime | (t.tm_hour << 11);

    *wDosDate = *wDosDate | t.tm_mday ;
    *wDosDate = *wDosDate | t.tm_mon << 5;
    *wDosDate = *wDosDate | ((t.tm_year - 1980) << 9) ;

    return 1;
}


/***********************************************************************
 *              SystemTimeToVariantTime [OLEAUT32.184]
 */
HRESULT WINAPI SystemTimeToVariantTime( LPSYSTEMTIME  lpSystemTime, double *pvtime )
{
    static const BYTE Days_Per_Month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    static const BYTE Days_Per_Month_LY[] = {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    struct tm t;

    TRACE(" %d/%d/%d %d:%d:%d\n",
          lpSystemTime->wMonth, lpSystemTime->wDay,
          lpSystemTime->wYear, lpSystemTime->wHour,
          lpSystemTime->wMinute, lpSystemTime->wSecond);

    if (lpSystemTime->wYear >= 1900)
    {
        t.tm_sec = lpSystemTime->wSecond;
        t.tm_min = lpSystemTime->wMinute;
        t.tm_hour = lpSystemTime->wHour;

        t.tm_mday = lpSystemTime->wDay;
        t.tm_mon = lpSystemTime->wMonth;
        t.tm_year = lpSystemTime->wYear;

        return TmToDATE( &t, pvtime );
    }
    else
    {
        t.tm_sec = lpSystemTime->wSecond;
        t.tm_min = lpSystemTime->wMinute;
        t.tm_hour = lpSystemTime->wHour;

        if (isleap(lpSystemTime->wYear) )
            t.tm_mday = Days_Per_Month_LY[13 - lpSystemTime->wMonth] - lpSystemTime->wDay;
        else
            t.tm_mday = Days_Per_Month[13 - lpSystemTime->wMonth] - lpSystemTime->wDay;

        t.tm_mon = 13 - lpSystemTime->wMonth;
        t.tm_year = 1900 + 1899 - lpSystemTime->wYear;

        TmToDATE( &t, pvtime );

        *pvtime *= -1;

        return 1;
    }

    return 0;
}

/***********************************************************************
 *              VariantTimeToSystemTime [OLEAUT32.185]
 */
HRESULT WINAPI VariantTimeToSystemTime( double vtime, LPSYSTEMTIME  lpSystemTime )
{
    double t = 0, timeofday = 0;

    static const BYTE Days_Per_Month[] =    {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    static const BYTE Days_Per_Month_LY[] = {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    /* The Month_Code is used to find the Day of the Week (LY = LeapYear)*/
    static const BYTE Month_Code[] =    {0, 1, 4, 4, 0, 2, 5, 0, 3, 6, 1, 4, 6};
    static const BYTE Month_Code_LY[] = {0, 0, 3, 4, 0, 2, 5, 0, 3, 6, 1, 4, 6};

    /* The Century_Code is used to find the Day of the Week */
    static const BYTE Century_Code[]  = {0, 6, 4, 2};

    struct tm r;

    TRACE(" Variant = %f SYSTEMTIME ptr %p", vtime, lpSystemTime);

    if (vtime >= 0)
    {

        if (DateToTm(vtime, 0, &r ) <= 0) return 0;

        lpSystemTime->wSecond = r.tm_sec;
        lpSystemTime->wMinute = r.tm_min;
        lpSystemTime->wHour = r.tm_hour;
        lpSystemTime->wDay = r.tm_mday;
        lpSystemTime->wMonth = r.tm_mon;

        if (lpSystemTime->wMonth == 12)
            lpSystemTime->wMonth = 1;
        else
            lpSystemTime->wMonth++;

        lpSystemTime->wYear = r.tm_year;
    }
    else
    {
        vtime = -1*vtime;

        if (DateToTm(vtime, 0, &r ) <= 0) return 0;

        lpSystemTime->wSecond = r.tm_sec;
        lpSystemTime->wMinute = r.tm_min;
        lpSystemTime->wHour = r.tm_hour;

        lpSystemTime->wMonth = 13 - r.tm_mon;

        if (lpSystemTime->wMonth == 1)
            lpSystemTime->wMonth = 12;
        else
            lpSystemTime->wMonth--;

        lpSystemTime->wYear = 1899 - (r.tm_year - 1900);

        if (!isleap(lpSystemTime->wYear) )
            lpSystemTime->wDay = Days_Per_Month[13 - lpSystemTime->wMonth] - r.tm_mday;
        else
            lpSystemTime->wDay = Days_Per_Month_LY[13 - lpSystemTime->wMonth] - r.tm_mday;


    }

    if (!isleap(lpSystemTime->wYear))
    {
        /*
          (Century_Code+Month_Code+Year_Code+Day) % 7

          The century code repeats every 400 years , so the array
          works out like this,

          Century_Code[0] is for 16th/20th Centry
          Century_Code[1] is for 17th/21th Centry
          Century_Code[2] is for 18th/22th Centry
          Century_Code[3] is for 19th/23th Centry

          The year code is found with the formula (year + (year / 4))
          the "year" must be between 0 and 99 .

          The Month Code (Month_Code[1]) starts with January and
          ends with December.
        */

        lpSystemTime->wDayOfWeek = (
            Century_Code[(( (lpSystemTime->wYear+100) - lpSystemTime->wYear%100) /100) %4]+
            ((lpSystemTime->wYear%100)+(lpSystemTime->wYear%100)/4)+
            Month_Code[lpSystemTime->wMonth]+
            lpSystemTime->wDay) % 7;

        if (lpSystemTime->wDayOfWeek == 0) lpSystemTime->wDayOfWeek = 7;
        else lpSystemTime->wDayOfWeek -= 1;
    }
    else
    {
        lpSystemTime->wDayOfWeek = (
            Century_Code[(((lpSystemTime->wYear+100) - lpSystemTime->wYear%100)/100)%4]+
            ((lpSystemTime->wYear%100)+(lpSystemTime->wYear%100)/4)+
            Month_Code_LY[lpSystemTime->wMonth]+
            lpSystemTime->wDay) % 7;

        if (lpSystemTime->wDayOfWeek == 0) lpSystemTime->wDayOfWeek = 7;
        else lpSystemTime->wDayOfWeek -= 1;
    }

    t = floor(vtime);
    timeofday = vtime - t;

    lpSystemTime->wMilliseconds = (timeofday
                                   - lpSystemTime->wHour*(1/24)
                                   - lpSystemTime->wMinute*(1/1440)
                                   - lpSystemTime->wSecond*(1/86400) )*(1/5184000);

    return 1;
}

/***********************************************************************
 *              VarUdateFromDate [OLEAUT32.331]
 */
HRESULT WINAPI VarUdateFromDate( DATE datein, ULONG dwFlags, UDATE *pudateout)
{
    HRESULT i = 0;
    static const BYTE Days_Per_Month[] =    {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    static const BYTE Days_Per_Month_LY[] = {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    TRACE("DATE = %f\n", (double)datein);
    i = VariantTimeToSystemTime(datein, &(pudateout->st) );

    if (i)
    {
        pudateout->wDayOfYear = 0;

        if (isleap(pudateout->st.wYear))
        {
            for (i =1; i<pudateout->st.wMonth; i++)
                pudateout->wDayOfYear += Days_Per_Month[i];
        }
        else
        {
            for (i =1; i<pudateout->st.wMonth; i++)
                pudateout->wDayOfYear += Days_Per_Month_LY[i];
        }

        pudateout->wDayOfYear += pudateout->st.wDay;
        dwFlags = 0; /*VAR_VALIDDATE*/
    }
    else dwFlags = 0;

    return i;
}

/***********************************************************************
 *              VarDateFromUdate [OLEAUT32.330]
 */
HRESULT WINAPI VarDateFromUdate(UDATE *pudateout,
                                ULONG dwFlags, DATE *datein)
{
    HRESULT i;
    double t = 0;
    TRACE(" %d/%d/%d %d:%d:%d\n",
          pudateout->st.wMonth, pudateout->st.wDay,
          pudateout->st.wYear, pudateout->st.wHour,
          pudateout->st.wMinute, pudateout->st.wSecond);


    i = SystemTimeToVariantTime(&(pudateout->st), &t);
    *datein = t;

    if (i) dwFlags = 0; /*VAR_VALIDDATE*/
    else dwFlags = 0;

    return i;
}


/**********************************************************************
 *              VarBstrCmp [OLEAUT32.440]
 *
 * flags can be:
 *   NORM_IGNORECASE, NORM_IGNORENONSPACE, NORM_IGNORESYMBOLS
 *   NORM_IGNORESTRINGWIDTH, NORM_IGNOREKANATYPE, NORM_IGNOREKASHIDA
 *
 */
HRESULT WINAPI VarBstrCmp(BSTR left, BSTR right, LCID lcid, DWORD flags)
{
    INT r;

    FIXME("( %s %s %ld %lx ) partial stub\n", debugstr_w(left), debugstr_w(right), lcid, flags);

    if((!left) || (!right))
        return VARCMP_NULL;

    if(flags&NORM_IGNORECASE)
        r = lstrcmpiW(left,right);
    else
        r = lstrcmpW(left,right);

    if(r<0)
        return VARCMP_LT;
    if(r>0)
        return VARCMP_GT;

    return VARCMP_EQ;
}

/**********************************************************************
 *              VarBstrCat [OLEAUT32.439]
 */
HRESULT WINAPI VarBstrCat(BSTR left, BSTR right, BSTR *out)
{
    BSTR result;

    TRACE("( %s %s %p )\n", debugstr_w(left), debugstr_w(right), out);

    if( (!left) || (!right) || (!out) )
        return 0;

    result = SysAllocStringLen(left, lstrlenW(left)+lstrlenW(right));
    lstrcatW(result,right);

    *out = result;

    return 1;
}

/**********************************************************************
 *              VarCat [OLEAUT32.441]
 */
HRESULT WINAPI VarCat(LPVARIANT left, LPVARIANT right, LPVARIANT out)
{
    /* Should we VariantClear out? */
    /* Can we handle array, vector, by ref etc. */
    if ((V_VT(left)&VT_TYPEMASK) == VT_NULL &&
        (V_VT(right)&VT_TYPEMASK) == VT_NULL)
    {
        V_VT(out) = VT_NULL;
        return S_OK;
    }
    else if (V_VT(left) == VT_BSTR && V_VT(right) == VT_BSTR)
    {
        V_VT(out) = VT_BSTR;
        VarBstrCat (V_BSTR(left), V_BSTR(right), &V_BSTR(out));
        return S_OK;
    }
    else
        FIXME ("types not supported\n");
    return S_OK;
}


/**********************************************************************
 *              VarBstrFromDisp [OLEAUT32.115]
 */
HRESULT WINAPI VarBstrFromDisp (IDispatch* pdispIn, LCID lcid, ULONG dwFlags,
                                BSTR* pbstrOut)
{
    HRESULT Res;
    DISPPARAMS Params;
    VARIANT VarArg,VarStr;

    TRACE ("pdispIn: %p, lcid: 0x%lx, dwFlags: 0x%lx, pbstrOut: %p\n",
           pdispIn, lcid, dwFlags, pbstrOut);

    if (dwFlags)
        WARN ("non-zero dwFlags currently ignored\n");

    memset (&Params, 0, sizeof (Params));
    VariantInit (&VarArg);
    
    if (!pdispIn)
        return E_INVALIDARG;

    Res = IDispatch_Invoke (pdispIn, DISPID_VALUE, &IID_NULL, lcid,
                            DISPATCH_PROPERTYGET, &Params,
                            &VarArg, NULL, NULL);
    if (!SUCCEEDED (Res))
        return DISP_E_TYPEMISMATCH;

    /* Convert returned variant to bstr */
    VariantInit (&VarStr);
    Res = VariantChangeType (&VarStr, &VarArg, 0, VT_BSTR);
    VariantClear (&VarArg);
    if (!SUCCEEDED (Res))
        return DISP_E_TYPEMISMATCH;

    *pbstrOut = V_UNION(&VarStr,bstrVal);
    TRACE ("returning: %s\n", debugstr_w (*pbstrOut));
    return S_OK;
}

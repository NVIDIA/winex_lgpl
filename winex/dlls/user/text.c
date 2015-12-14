/*
 * USER text functions
 *
 * Copyright 1993, 1994 Alexandre Julliard
 * Copyright 2002 Bill Medland
 *
 * Contains
 *   1.  DrawText functions
 *   2.  GrayString functions
 *   3.  TabbedText functions
 */

#include <string.h>
#include <assert.h>

#include "windef.h"
#include "wingdi.h"
#include "wine/winuser16.h"
#include "wine/unicode.h"
#include "winbase.h"
#include "winerror.h"
#include "winnls.h"
#include "user.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(text);

/*********************************************************************
 *
 *            DrawText functions
 *
 * Design issues
 *   How many buffers to use
 *     While processing in DrawText there are potentially three different forms
 *     of the text that need to be held.  How are they best held?
 *     1. The original text is needed, of course, to see what to display.
 *     2. The text that will be returned to the user if the DT_MODIFYSTRING is
 *        in effect.
 *     3. The buffered text that is about to be displayed e.g. the current line.
 *        Typically this will exclude the ampersands used for prefixing etc.
 *
 *     Complications.
 *     a. If the buffered text to be displayed includes the ampersands then
 *        we will need special measurement and draw functions that will ignore
 *        the ampersands (e.g. by copying to a buffer without the prefix and
 *        then using the normal forms).  This may involve less space but may
 *        require more processing.  e.g. since a line containing tabs may
 *        contain several underlined characters either we need to carry around
 *        a list of prefix locations or we may need to locate them several
 *        times.
 *     b. If we actually directly modify the "original text" as we go then we
 *        will need some special "caching" to handle the fact that when we
 *        ellipsify the text the ellipsis may modify the next line of text,
 *        which we have not yet processed.  (e.g. ellipsification of a W at the
 *        end of a line will overwrite the W, the \n and the first character of
 *        the next line, and a \0 will overwrite the second.  Try it!!)
 *
 *     Option 1.  Three separate storages. (To be implemented)
 *       If DT_MODIFYSTRING is in effect then allocate an extra buffer to hold
 *       the edited string in some form, either as the string itself or as some
 *       sort of "edit list" to be applied just before returning.
 *       Use a buffer that holds the ellipsified current line sans ampersands
 *       and accept the need occasionally to recalculate the prefixes (if
 *       DT_EXPANDTABS and not DT_NOPREFIX and not DT_HIDEPREFIX)
 */

#define TAB     9
#define LF     10
#define CR     13
#define SPACE  32
#define PREFIX 38

#define FORWARD_SLASH '/'
#define BACK_SLASH '\\'

static const WCHAR ELLIPSISW[] = {'.','.','.', 0};

typedef struct tag_ellipsis_data
{
    int before;
    int len;
    int under;
    int after;
} ellipsis_data;

/*********************************************************************
 *                      TEXT_Ellipsify (static)
 *
 *  Add an ellipsis to the end of the given string whilst ensuring it fits.
 *
 * If the ellipsis alone doesn't fit then it will be returned anyway.
 *
 * See Also TEXT_PathEllipsify
 *
 * Arguments
 *   hdc        [in] The handle to the DC that defines the font.
 *   str        [in/out] The string that needs to be modified.
 *   max_str    [in] The dimension of str (number of WCHAR).
 *   len_str    [in/out] The number of characters in str
 *   width      [in] The maximum width permitted (in logical coordinates)
 *   size       [out] The dimensions of the text
 *   modstr     [out] The modified form of the string, to be returned to the
 *                    calling program.  It is assumed that the caller has
 *                    made sufficient space available so we don't need to
 *                    know the size of the space.  This pointer may be NULL if
 *                    the modified string is not required.
 *   len_before [out] The number of characters before the ellipsis.
 *   len_ellip  [out] The number of characters in the ellipsis.
 *
 * See for example Microsoft article Q249678.
 *
 * For now we will simply use three dots rather than worrying about whether
 * the font contains an explicit ellipsis character.
 */
static void TEXT_Ellipsify (HDC hdc, WCHAR *str, unsigned int max_len,
                            unsigned int *len_str, int width, SIZE *size,
                            WCHAR *modstr,
                            int *len_before, int *len_ellip)
{
    unsigned int len_ellipsis;
    unsigned int lo, mid, hi;

    len_ellipsis = strlenW (ELLIPSISW);
    if (len_ellipsis > max_len) len_ellipsis = max_len;
    if (*len_str > max_len - len_ellipsis)
        *len_str = max_len - len_ellipsis;

    /* First do a quick binary search to get an upper bound for *len_str. */
    if (*len_str > 0 &&
        GetTextExtentExPointW(hdc, str, *len_str, width, NULL, NULL, size) &&
        size->cx > width)
    {
        for (lo = 0, hi = *len_str; lo < hi; )
        {
            mid = (lo + hi) / 2;
            if (!GetTextExtentExPointW(hdc, str, mid, width, NULL, NULL, size))
                break;
            if (size->cx > width)
                hi = mid;
            else
                lo = mid + 1;
        }
        *len_str = hi;
    }
    /* Now this should take only a couple iterations at most. */
    for ( ; ; )
    {
        lstrcpynW (str + *len_str, ELLIPSISW, len_ellipsis);

        if (!GetTextExtentExPointW (hdc, str, *len_str + len_ellipsis, width,
                                    NULL, NULL, size)) break;

        if (!*len_str || size->cx <= width) break;

        (*len_str)--;
    }
    *len_ellip = len_ellipsis;
    *len_before = *len_str;
    *len_str += len_ellipsis;

    if (modstr)
        lstrcpynW (modstr, str, *len_str);
}

/*********************************************************************
 *                      TEXT_PathEllipsify (static)
 *
 * Add an ellipsis to the provided string in order to make it fit within
 * the width.  The ellipsis is added as specified for the DT_PATH_ELLIPSIS
 * flag.
 *
 * See Also TEXT_Ellipsify
 *
 * Arguments
 *   hdc        [in] The handle to the DC that defines the font.
 *   str        [in/out] The string that needs to be modified
 *   max_str    [in] The dimension of str (number of WCHAR).
 *   len_str    [in/out] The number of characters in str
 *   width      [in] The maximum width permitted (in logical coordinates)
 *   size       [out] The dimensions of the text
 *   modstr     [out] The modified form of the string, to be returned to the
 *                    calling program.  It is assumed that the caller has
 *                    made sufficient space available so we don't need to
 *                    know the size of the space.  This pointer may be NULL if
 *                    the modified string is not required.
 *   pellip     [out] The ellipsification results
 *
 * For now we will simply use three dots rather than worrying about whether
 * the font contains an explicit ellipsis character.
 *
 * The following applies, I think to Win95.  We will need to extend it for
 * Win98 which can have both path and end ellipsis at the same time (e.g.
 *  C:\MyLongFileName.Txt becomes ...\MyLongFileN...)
 *
 * The resulting string consists of as much as possible of the following:
 * 1. The ellipsis itself
 * 2. The last \ or / of the string (if any)
 * 3. Everything after the last \ or / of the string (if any) or the whole
 *    string if there is no / or \.  I believe that under Win95 this would
 *    include everything even though some might be clipped off the end whereas
 *    under Win98 that might be ellipsified too.
 *    Yet to be investigated is whether this would include wordbreaking if the
 *    filename is more than 1 word and splitting if DT_EDITCONTROL was in
 *    effect.  (If DT_EDITCONTROL is in effect then on occasions text will be
 *    broken within words).
 * 4. All the stuff before the / or \, which is placed before the ellipsis.
 */
static void TEXT_PathEllipsify (HDC hdc, WCHAR *str, unsigned int max_len,
                                unsigned int *len_str, int width, SIZE *size,
                                WCHAR *modstr, ellipsis_data *pellip)
{
    int len_ellipsis;
    int len_trailing;
    int len_under;
    WCHAR *lastBkSlash, *lastFwdSlash, *lastSlash;

    len_ellipsis = strlenW (ELLIPSISW);
    if (!max_len) return;
    if (len_ellipsis >= max_len) len_ellipsis = max_len - 1;
    if (*len_str + len_ellipsis >= max_len)
        *len_str = max_len - len_ellipsis-1;
        /* Hopefully this will never happen, otherwise it would probably lose
         * the wrong character
         */
    str[*len_str] = '\0'; /* to simplify things */

    lastBkSlash  = strrchrW (str, BACK_SLASH);
    lastFwdSlash = strrchrW (str, FORWARD_SLASH);
    lastSlash = lastBkSlash > lastFwdSlash ? lastBkSlash : lastFwdSlash;
    if (!lastSlash) lastSlash = str;
    len_trailing = *len_str - (lastSlash - str);

    /* overlap-safe movement to the right */
    memmove (lastSlash+len_ellipsis, lastSlash, len_trailing * sizeof(WCHAR));
    lstrcpynW (lastSlash, ELLIPSISW, len_ellipsis);
    len_trailing += len_ellipsis;
    /* From this point on lastSlash actually points to the ellipsis in front
     * of the last slash and len_trailing includes the ellipsis
     */

    len_under = 0;
    for ( ; ; )
    {
        if (!GetTextExtentExPointW (hdc, str, *len_str + len_ellipsis, width,
                                    NULL, NULL, size)) break;

        if (lastSlash == str || size->cx <= width) break;

        /* overlap-safe movement to the left */
        memmove (lastSlash-1, lastSlash, len_trailing * sizeof(WCHAR));
        lastSlash--;
        len_under++;

        assert (*len_str);
        (*len_str)--;
    }
    pellip->before = lastSlash-str;
    pellip->len = len_ellipsis;
    pellip->under = len_under;
    pellip->after = len_trailing - len_ellipsis;
    *len_str += len_ellipsis;

    if (modstr)
        lstrcpynW (modstr, str, *len_str);
}

/*********************************************************************
 *                      TEXT_WordBreak (static)
 *
 *  Perform wordbreak processing on the given string
 *
 * Assumes that DT_WORDBREAK has been specified and not all the characters
 * fit.  Note that this function should even be called when the first character
 * that doesn't fit is known to be a space or tab, so that it can swallow them.
 *
 * Note that the Windows processing has some strange properties.
 * 1. If the text is left-justified and there is room for some of the spaces
 *    that follow the last word on the line then those that fit are included on
 *    the line.
 * 2. If the text is centred or right-justified and there is room for some of
 *    the spaces that follow the last word on the line then all but one of those
 *    that fit are included on the line.
 * 3. (Reasonable behaviour) If the word breaking causes a space to be the first
 *    character of a new line it will be skipped.
 *
 * Arguments
 *   hdc        [in] The handle to the DC that defines the font.
 *   str        [in/out] The string that needs to be broken.
 *   max_str    [in] The dimension of str (number of WCHAR).
 *   len_str    [in/out] The number of characters in str
 *   width      [in] The maximum width permitted
 *   format     [in] The format flags in effect
 *   chars_fit  [in] The maximum number of characters of str that are already
 *              known to fit; chars_fit+1 is known not to fit.
 *   chars_used [out] The number of characters of str that have been "used" and
 *              do not need to be included in later text.  For example this will
 *              include any spaces that have been discarded from the start of
 *              the next line.
 *   size       [out] The size of the returned text in logical coordinates
 *
 * Pedantic assumption - Assumes that the text length is monotonically
 * increasing with number of characters (i.e. no weird kernings)
 *
 * Algorithm
 *
 * Work back from the last character that did fit to either a space or the last
 * character of a word, whichever is met first.
 * If there was one or the first character didn't fit then
 *     If the text is centred or right justified and that one character was a
 *     space then break the line before that character
 *     Otherwise break the line after that character
 *     and if the next character is a space then discard it.
 * Suppose there was none (and the first character did fit).
 *     If Break Within Word is permitted
 *         break the word after the last character that fits (there must be
 *         at least one; none is caught earlier).
 *     Otherwise
 *         discard any trailing space.
 *         include the whole word; it may be ellipsified later
 *
 * Break Within Word is permitted under a set of circumstances that are not
 * totally clear yet.  Currently our best guess is:
 *     If DT_EDITCONTROL is in effect and neither DT_WORD_ELLIPSIS nor
 *     DT_PATH_ELLIPSIS is
 */

static void TEXT_WordBreak (HDC hdc, WCHAR *str, unsigned int max_str,
                            unsigned int *len_str,
                            int width, int format, unsigned int chars_fit,
                            unsigned int *chars_used, SIZE *size)
{
    WCHAR *p;
    int word_fits;
    assert (format & DT_WORDBREAK);
    assert (chars_fit < *len_str);

    /* Work back from the last character that did fit to either a space or the
     * last character of a word, whichever is met first.
     */
    p = str + chars_fit; /* The character that doesn't fit */
    word_fits = TRUE;
    if (!chars_fit)
        ; /* we pretend that it fits anyway */
    else if (*p == SPACE) /* chars_fit < *len_str so this is valid */
        p--; /* the word just fitted */
    else
    {
        while (p > str && *(--p) != SPACE)
            ;
        word_fits = (p != str || *p == SPACE);
    }
    /* If there was one or the first character didn't fit then */
    if (word_fits)
    {
        /* break the line before/after that character */
        if (!(format & (DT_RIGHT | DT_CENTER)) || *p != SPACE)
            p++;
        *len_str = p - str;
        /* and if the next character is a space then discard it. */
        *chars_used = *len_str;
        if (*p == SPACE)
            (*chars_used)++;
    }
    /* Suppose there was none. */
    else
    {
        if ((format & (DT_EDITCONTROL | DT_WORD_ELLIPSIS | DT_PATH_ELLIPSIS)) ==
            DT_EDITCONTROL)
        {
            /* break the word after the last character that fits (there must be
             * at least one; none is caught earlier).
             */
            *len_str = chars_fit;
            *chars_used = chars_fit;

            /* FIXME - possible error.  Since the next character is now removed
             * this could make the text longer so that it no longer fits, and
             * so we need a loop to test and shrink.
             */
        }
        /* Otherwise */
        else
        {
            /* discard any trailing space. */
            const WCHAR *e = str + *len_str;
            p = str + chars_fit;
            while (p < e && *p != SPACE)
                p++;
            *chars_used = p - str;
            if (p < e) /* i.e. loop failed because *p == SPACE */
                (*chars_used)++;

            /* include the whole word; it may be ellipsified later */
            *len_str = p - str;
            /* Possible optimisation; if DT_WORD_ELLIPSIS only use chars_fit+1
             * so that it will be too long
             */
        }
    }
    /* Remeasure the string */
    GetTextExtentExPointW (hdc, str, *len_str, 0, NULL, NULL, size);
}

/*********************************************************************
 *                      TEXT_SkipChars
 *
 *  Skip over the given number of characters, bearing in mind prefix
 *  substitution and the fact that a character may take more than one
 *  WCHAR (Unicode surrogates are two words long) (and there may have been
 *  a trailing &)
 *
 * Parameters
 *   new_count  [out] The updated count
 *   new_str    [out] The updated pointer
 *   start_count [in] The count of remaining characters corresponding to the
 *                    start of the string
 *   start_str  [in] The starting point of the string
 *   max        [in] The number of characters actually in this segment of the
 *                   string (the & counts)
 *   n          [in] The number of characters to skip (if prefix then
 *                   &c counts as one)
 *   prefix     [in] Apply prefix substitution
 *
 * Return Values
 *   none
 *
 * Remarks
 *   There must be at least n characters in the string
 *   We need max because the "line" may have ended with a & followed by a tab
 *   or newline etc. which we don't want to swallow
 */

static void TEXT_SkipChars (int *new_count, const WCHAR **new_str,
                            int start_count, const WCHAR *start_str,
                            int max, int n, int prefix)
{
    /* This is specific to wide characters, MSDN doesn't say anything much
     * about Unicode surrogates yet and it isn't clear if _wcsinc will
     * correctly handle them so we'll just do this the easy way for now
     */

    if (prefix)
    {
        const WCHAR *str_on_entry = start_str;
        assert (max >= n);
        max -= n;
        while (n--)
            if (*start_str++ == PREFIX && max--)
                start_str++;
            else;
        start_count -= (start_str - str_on_entry);
    }
    else
    {
        start_str += n;
        start_count -= n;
    }
    *new_str = start_str;
    *new_count = start_count;
}

/*********************************************************************
 *                      TEXT_Reprefix
 *
 *  Reanalyse the text to find the prefixed character.  This is called when
 *  wordbreaking or ellipsification has shortened the string such that the
 *  previously noted prefixed character is no longer visible.
 *
 * Parameters
 *   str        [in] The original string segment (including all characters)
 *   ns         [in] The number of characters in str (including prefixes)
 *   pe         [in] The ellipsification data
 *
 * Return Values
 *   The prefix offset within the new string segment (the one that contains the
 *   ellipses and does not contain the prefix characters) (-1 if none)
 */

static int TEXT_Reprefix (const WCHAR *str, unsigned int ns,
                          const ellipsis_data *pe)
{
    int result = -1;
    unsigned int i = 0;
    unsigned int n = pe->before + pe->under + pe->after;
    assert (n <= ns);
    while (i < n)
    {
        if (i == pe->before)
        {
            /* Reached the path ellipsis; jump over it */
            if (ns < pe->under) break;
            str += pe->under;
            ns -= pe->under;
            i += pe->under;
            if (!pe->after) break; /* Nothing after the path ellipsis */
        }
        if (!ns) break;
        ns--;
        if (*str++ == PREFIX)
        {
            if (!ns) break;
            if (*str != PREFIX)
                result = (i < pe->before || pe->under == 0) ? i : i - pe->under + pe->len;
                /* pe->len may be non-zero while pe_under is zero */
            str++;
            ns--;
        }
        else;
        i++;
    }
    return result;
}

/*********************************************************************
 *  Returns true if and only if the remainder of the line is a single
 *  newline representation or nothing
 */

static int remainder_is_none_or_newline (int num_chars, const WCHAR *str)
{
    if (!num_chars) return TRUE;
    if (*str != LF && *str != CR) return FALSE;
    if (!--num_chars) return TRUE;
    if (*str == *(str+1)) return FALSE;
    str++;
    if (*str != CR && *str != LF) return FALSE;
    if (--num_chars) return FALSE;
    return TRUE;
}

/*********************************************************************
 *  Return next line of text from a string.
 *
 * hdc - handle to DC.
 * str - string to parse into lines.
 * count - length of str.
 * dest - destination in which to return line.
 * len - dest buffer size in chars on input, copied length into dest on output.
 * width - maximum width of line in pixels.
 * format - format type passed to DrawText.
 * retsize - returned size of the line in pixels.
 * last_line - TRUE if is the last line that will be processed
 * p_retstr - If DT_MODIFYSTRING this points to a cursor in the buffer in which
 *            the return string is built.
 * tabwidth - The width of a tab in logical coordinates
 * pprefix_offset - Here is where we return the offset within dest of the first
 *                  prefixed (underlined) character.  -1 is returned if there
 *                  are none.  Note that there may be more; the calling code
 *                  will need to use TEXT_Reprefix to find any later ones.
 * pellip - Here is where we return the information about any ellipsification
 *          that was carried out.  Note that if tabs are being expanded then
 *          this data will correspond to the last text segment actually
 *          returned in dest; by definition there would not have been any
 *          ellipsification in earlier text segments of the line.
 *
 * Returns pointer to next char in str after end of the line
 * or NULL if end of str reached.
 */
static const WCHAR *TEXT_NextLineW( HDC hdc, const WCHAR *str, int *count,
                                 WCHAR *dest, int *len, int width, DWORD format,
                                 SIZE *retsize, int last_line, WCHAR **p_retstr,
                                 int tabwidth, int *pprefix_offset,
                                 ellipsis_data *pellip)
{
    int i = 0, j = 0;
    int plen = 0;
    SIZE size;
    int maxl = *len;
    int seg_i, seg_count, seg_j;
    int max_seg_width;
    int num_fit;
    int word_broken;
    int line_fits;
    int j_in_seg;
    int ellipsified;
    *pprefix_offset = -1;

    /* For each text segment in the line */

    retsize->cy = 0;
    while (*count)
    {

        /* Skip any leading tabs */

        if (str[i] == TAB && (format & DT_EXPANDTABS))
        {
            plen = ((plen/tabwidth)+1)*tabwidth;
            (*count)--; if (j < maxl) dest[j++] = str[i++]; else i++;
            while (*count && str[i] == TAB)
            {
                plen += tabwidth;
                (*count)--; if (j < maxl) dest[j++] = str[i++]; else i++;
            }
        }


        /* Now copy as far as the next tab or cr/lf or eos */

        seg_i = i;
        seg_count = *count;
        seg_j = j;

        while (*count &&
               (str[i] != TAB || !(format & DT_EXPANDTABS)) &&
               ((str[i] != CR && str[i] != LF) || (format & DT_SINGLELINE)))
        {
	    if (str[i] == PREFIX && !(format & DT_NOPREFIX) && *count > 1)
            {
                (*count)--, i++; /* Throw away the prefix itself */
                if (str[i] == PREFIX)
                {
                    /* Swallow it before we see it again */
		    (*count)--; if (j < maxl) dest[j++] = str[i++]; else i++;
                }
		else if (*pprefix_offset == -1 || *pprefix_offset >= seg_j)
                {
                    *pprefix_offset = j;
                }
                /* else the previous prefix was in an earlier segment of the
                 * line; we will leave it to the drawing code to catch this
                 * one.
                 */
	    }
            else
            {
                (*count)--; if (j < maxl) dest[j++] = str[i++]; else i++;
            }
        }


        /* Measure the whole text segment and possibly WordBreak and
         * ellipsify it
         */

        j_in_seg = j - seg_j;
        max_seg_width = width - plen;
        GetTextExtentExPointW (hdc, dest + seg_j, j_in_seg, max_seg_width, &num_fit, NULL, &size);

        /* The Microsoft handling of various combinations of formats is weird.
         * The following may very easily be incorrect if several formats are
         * combined, and may differ between versions (to say nothing of the
         * several bugs in the Microsoft versions).
         */
        word_broken = 0;
        line_fits = (num_fit >= j_in_seg);
        if (!line_fits && (format & DT_WORDBREAK))
        {
            const WCHAR *s;
            int chars_used;
            TEXT_WordBreak (hdc, dest+seg_j, maxl-seg_j, &j_in_seg,
                            max_seg_width, format, num_fit, &chars_used, &size);
            line_fits = (size.cx <= max_seg_width);
            /* and correct the counts */
            TEXT_SkipChars (count, &s, seg_count, str+seg_i, i-seg_i,
                            chars_used, !(format & DT_NOPREFIX));
            i = s - str;
            word_broken = 1;
        }
        pellip->before = j_in_seg;
        pellip->under = 0;
        pellip->after = 0;
        pellip->len = 0;
        ellipsified = 0;
        if (!line_fits && (format & DT_PATH_ELLIPSIS))
        {
            TEXT_PathEllipsify (hdc, dest + seg_j, maxl-seg_j, &j_in_seg,
                                max_seg_width, &size, *p_retstr, pellip);
            line_fits = (size.cx <= max_seg_width);
            ellipsified = 1;
        }
        /* NB we may end up ellipsifying a word-broken or path_ellipsified
         * string */
        if ((!line_fits && (format & DT_WORD_ELLIPSIS)) ||
            ((format & DT_END_ELLIPSIS) &&
              ((last_line && *count) ||
               (remainder_is_none_or_newline (*count, &str[i]) && !line_fits))))
        {
            int before, len_ellipsis;
            TEXT_Ellipsify (hdc, dest + seg_j, maxl-seg_j, &j_in_seg,
                            max_seg_width, &size, *p_retstr, &before, &len_ellipsis);
            if (before > pellip->before)
            {
                /* We must have done a path ellipsis too */
                pellip->after = before - pellip->before - pellip->len;
                /* Leave the len as the length of the first ellipsis */
            }
            else
            {
                /* If we are here after a path ellipsification it must be
                 * because even the ellipsis itself didn't fit.
                 */
                assert (pellip->under == 0 && pellip->after == 0);
                pellip->before = before;
                pellip->len = len_ellipsis;
                /* pellip->after remains as zero as does
                 * pellip->under
                 */
            }
            line_fits = (size.cx <= max_seg_width);
            ellipsified = 1;
        }
        /* As an optimisation if we have ellipsified and we are expanding
         * tabs and we haven't reached the end of the line we can skip to it
         * now rather than going around the loop again.
         */
        if ((format & DT_EXPANDTABS) && ellipsified)
        {
            if (format & DT_SINGLELINE)
                *count = 0;
            else
            {
                while ((*count) && str[i] != CR && str[i] != LF)
                {
                    (*count)--, i++;
                }
            }
        }

        j = seg_j + j_in_seg;
        if (*pprefix_offset >= seg_j + pellip->before)
        {
            *pprefix_offset = TEXT_Reprefix (str + seg_i, i - seg_i, pellip);
            if (*pprefix_offset != -1)
                *pprefix_offset += seg_j;
        }

        plen += size.cx;
        if (size.cy > retsize->cy)
            retsize->cy = size.cy;

        if (word_broken)
            break;
        else if (!*count)
            break;
        else if (str[i] == CR || str[i] == LF)
        {
            (*count)--, i++;
            if (*count && (str[i] == CR || str[i] == LF) && str[i] != str[i-1])
            {
                (*count)--, i++;
            }
            break;
        }
        /* else it was a Tab and we go around again */
    }

    retsize->cx = plen;
    *len = j;
    if (*count)
        return (&str[i]);
    else
        return NULL;
}


/***********************************************************************
 *                      TEXT_DrawUnderscore
 *
 *  Draw the underline under the prefixed character
 *
 * Parameters
 *   hdc        [in] The handle of the DC for drawing
 *   x          [in] The x location of the line segment (logical coordinates)
 *   y          [in] The y location of where the underscore should appear
 *                   (logical coordinates)
 *   str        [in] The text of the line segment
 *   offset     [in] The offset of the underscored character within str
 */

static void TEXT_DrawUnderscore (HDC hdc, int x, int y, const WCHAR *str, int offset)
{
    int prefix_x;
    int prefix_end;
    SIZE size;
    HPEN hpen;
    HPEN oldPen;

    GetTextExtentPointW (hdc, str, offset, &size);
    prefix_x = x + size.cx;
    GetTextExtentPointW (hdc, str, offset+1, &size);
    prefix_end = x + size.cx - 1;
    /* The above method may eventually be slightly wrong due to kerning etc. */

    hpen = CreatePen (PS_SOLID, 1, GetTextColor (hdc));
    oldPen = SelectObject (hdc, hpen);
    MoveToEx (hdc, prefix_x, y, NULL);
    LineTo (hdc, prefix_end, y);
    SelectObject (hdc, oldPen);
    DeleteObject (hpen);
}

/***********************************************************************
 *           DrawTextExW    (USER32.@)
 *
 * The documentation on the extra space required for DT_MODIFYSTRING at MSDN
 * is not quite complete, especially with regard to \0.  We will assume that
 * the returned string could have a length of up to i_count+3 and also have
 * a trailing \0 (which would be 4 more than a not-null-terminated string but
 * 3 more than a null-terminated string).  If this is not so then increase
 * the allowance in DrawTextExA.
 */
#define MAX_STATIC_BUFFER 1024
INT WINAPI DrawTextExW( HDC hdc, LPWSTR str, INT i_count,
                        LPRECT rect, UINT flags, LPDRAWTEXTPARAMS dtp )
{
    SIZE size;
    const WCHAR *strPtr;
    WCHAR *retstr, *p_retstr;
    size_t size_retstr;
    static WCHAR line[MAX_STATIC_BUFFER];
    int len, lh, count=i_count;
    TEXTMETRICW tm;
    int lmargin = 0, rmargin = 0;
    int x = rect->left, y = rect->top;
    int width = rect->right - rect->left;
    int max_width = 0;
    int last_line;
    int tabwidth /* to keep gcc happy */ = 0;
    int prefix_offset;
    ellipsis_data ellip;

    TRACE("%s, %d , [(%d,%d),(%d,%d)]\n", debugstr_wn (str, count), count,
	  rect->left, rect->top, rect->right, rect->bottom);

   if (dtp) TRACE("Params: iTabLength=%d, iLeftMargin=%d, iRightMargin=%d\n",
          dtp->iTabLength, dtp->iLeftMargin, dtp->iRightMargin);

    if (!str) return 0;
    if (count == -1) count = strlenW(str);
    if (count == 0) return 0;
    strPtr = str;

    if (flags & DT_SINGLELINE)
        flags &= ~DT_WORDBREAK;

    GetTextMetricsW(hdc, &tm);
    if (flags & DT_EXTERNALLEADING)
	lh = tm.tmHeight + tm.tmExternalLeading;
    else
	lh = tm.tmHeight;

    if (dtp)
    {
        lmargin = dtp->iLeftMargin * tm.tmAveCharWidth;
        rmargin = dtp->iRightMargin * tm.tmAveCharWidth;
        if (!(flags & (DT_CENTER | DT_RIGHT)))
            x += lmargin;
        dtp->uiLengthDrawn = 0;     /* This param RECEIVES number of chars processed */
    }

    if (flags & DT_EXPANDTABS)
    {
        int tabstop = ((flags & DT_TABSTOP) && dtp) ? dtp->iTabLength : 8;
	tabwidth = tm.tmAveCharWidth * tabstop;
    }

    if (flags & DT_CALCRECT) flags |= DT_NOCLIP;

    if (flags & DT_MODIFYSTRING)
    {
        size_retstr = (count + 4) * sizeof (WCHAR);
        retstr = HeapAlloc(GetProcessHeap(), 0, size_retstr);
        if (!retstr) return 0;
        memcpy (retstr, str, size_retstr);
    }
    else
    {
        size_retstr = 0;
        retstr = NULL;
    }
    p_retstr = retstr;

    do
    {
	len = MAX_STATIC_BUFFER;
        last_line = !(flags & DT_NOCLIP) && y + ((flags & DT_EDITCONTROL) ? 2*lh-1 : lh) > rect->bottom;
	strPtr = TEXT_NextLineW(hdc, strPtr, &count, line, &len, width, flags, &size, last_line, &p_retstr, tabwidth, &prefix_offset, &ellip);

	if (flags & DT_CENTER) x = (rect->left + rect->right -
				    size.cx) / 2;
	else if (flags & DT_RIGHT) x = rect->right - size.cx;

	if (flags & DT_SINGLELINE)
	{
	    if (flags & DT_VCENTER) y = rect->top +
	    	(rect->bottom - rect->top) / 2 - size.cy / 2;
	    else if (flags & DT_BOTTOM) y = rect->bottom - size.cy;
        }

	if (!(flags & DT_CALCRECT))
	{
            const WCHAR *str = line;
            int xseg = x;
            while (len)
            {
                int len_seg;
                SIZE size;
                if ((flags & DT_EXPANDTABS))
                {
                    const WCHAR *p;
                    p = str; while (p < str+len && *p != TAB) p++;
                    len_seg = p - str;
                    if (len_seg != len && !GetTextExtentPointW(hdc, str, len_seg, &size))
                        return 0;
                }
                else
                    len_seg = len;

                if (!ExtTextOutW( hdc, xseg, y,
                                 ((flags & DT_NOCLIP) ? 0 : ETO_CLIPPED) |
                                 ((flags & DT_RTLREADING) ? ETO_RTLREADING : 0),
                                 rect, str, len_seg, NULL ))  return 0;
                if (prefix_offset != -1 && prefix_offset < len_seg)
                {
                    TEXT_DrawUnderscore (hdc, xseg, y + tm.tmAscent + 1, str, prefix_offset);
                }
                len -= len_seg;
                str += len_seg;
                if (len)
                {
                    assert ((flags & DT_EXPANDTABS) && *str == TAB);
                    len--; str++;
                    xseg += ((size.cx/tabwidth)+1)*tabwidth;
                    if (prefix_offset != -1)
                    {
                        if (prefix_offset < len_seg)
                        {
                            /* We have just drawn an underscore; we ought to
                             * figure out where the next one is.  I am going
                             * to leave it for now until I have a better model
                             * for the line, which will make reprefixing easier.
                             * This is where ellip would be used.
                             */
                            prefix_offset = -1;
                        }
                        else
                            prefix_offset -= len_seg;
                    }
                }
            }
	}
	else if (size.cx > max_width)
	    max_width = size.cx;

	y += lh;
        if (dtp)
            dtp->uiLengthDrawn += len;
    }
    while (strPtr && !last_line);

    if (flags & DT_CALCRECT)
    {
	rect->right = rect->left + max_width;
	rect->bottom = y;
        if (dtp)
            rect->right += lmargin + rmargin;
    }
    if (retstr)
    {
        memcpy (str, retstr, size_retstr);
        HeapFree (GetProcessHeap(), 0, retstr);
    }
    return y - rect->top;
}

/***********************************************************************
 *           DrawTextExA    (USER32.@)
 *
 * If DT_MODIFYSTRING is specified then there must be room for up to
 * 4 extra characters.  We take great care about just how much modified
 * string we return.
 */
INT WINAPI DrawTextExA( HDC hdc, LPSTR str, INT count,
                        LPRECT rect, UINT flags, LPDRAWTEXTPARAMS dtp )
{
   WCHAR *wstr;
   WCHAR *p;
   INT ret = 0;
   int i;
   DWORD wcount;
   DWORD wmax;
   DWORD amax;

   if (!str) return 0;
   if (count == -1) count = strlen(str);
   if (!count) return 0;
   wcount = MultiByteToWideChar( CP_ACP, 0, str, count, NULL, 0 );
   wmax = wcount;
   amax = count;
   if (flags & DT_MODIFYSTRING)
   {
        wmax += 4;
        amax += 4;
   }
   wstr = HeapAlloc(GetProcessHeap(), 0, wmax * sizeof(WCHAR));
   if (wstr)
   {
       MultiByteToWideChar( CP_ACP, 0, str, count, wstr, wcount );
       if (flags & DT_MODIFYSTRING)
           for (i=4, p=wstr+wcount; i--; p++) *p=0xFFFE;
           /* Initialise the extra characters so that we can see which ones
            * change.  U+FFFE is guaranteed to be not a unicode character and
            * so will not be generated by DrawTextEx itself.
            */
       ret = DrawTextExW( hdc, wstr, wcount, rect, flags, NULL );
       if (flags & DT_MODIFYSTRING)
       {
            /* Unfortunately the returned string may contain multiple \0s
             * and so we need to measure it ourselves.
             */
            for (i=4, p=wstr+wcount; i-- && *p != 0xFFFE; p++) wcount++;
            WideCharToMultiByte( CP_ACP, 0, wstr, wcount, str, amax, NULL, NULL );
       }
       HeapFree(GetProcessHeap(), 0, wstr);
   }
   return ret;
}

/***********************************************************************
 *           DrawTextW    (USER32.@)
 */
INT WINAPI DrawTextW( HDC hdc, LPCWSTR str, INT count, LPRECT rect, UINT flags )
{
    DRAWTEXTPARAMS dtp;

    memset (&dtp, 0, sizeof(dtp));
    if (flags & DT_TABSTOP)
    {
        dtp.iTabLength = (flags >> 8) && 0xff;
        flags &= 0xffff00ff;
    }
    return DrawTextExW(hdc, (LPWSTR)str, count, rect, flags, &dtp);
}

/***********************************************************************
 *           DrawTextA    (USER32.@)
 */
INT WINAPI DrawTextA( HDC hdc, LPCSTR str, INT count, LPRECT rect, UINT flags )
{
    DRAWTEXTPARAMS dtp;

    memset (&dtp, 0, sizeof(dtp));
    if (flags & DT_TABSTOP)
    {
        dtp.iTabLength = (flags >> 8) && 0xff;
        flags &= 0xffff00ff;
    }
    return DrawTextExA( hdc, (LPSTR)str, count, rect, flags, &dtp );
}

/***********************************************************************
 *           DrawText    (USER.85)
 */
INT16 WINAPI DrawText16( HDC16 hdc, LPCSTR str, INT16 count, LPRECT16 rect, UINT16 flags )
{
    INT16 ret;

    if (rect)
    {
        RECT rect32;
        CONV_RECT16TO32( rect, &rect32 );
        ret = DrawTextA( hdc, str, count, &rect32, flags );
        CONV_RECT32TO16( &rect32, rect );
    }
    else ret = DrawTextA( hdc, str, count, NULL, flags);
    return ret;
}


/***********************************************************************
 *
 *           GrayString functions
 */

/* ### start build ### */
extern WORD CALLBACK TEXT_CallTo16_word_wlw(GRAYSTRINGPROC16,WORD,LONG,WORD);
/* ### stop build ### */

struct gray_string_info
{
    GRAYSTRINGPROC16 proc;
    LPARAM           param;
};

/* callback for 16-bit gray string proc */
static BOOL CALLBACK gray_string_callback( HDC hdc, LPARAM param, INT len )
{
    const struct gray_string_info *info = (struct gray_string_info *)param;
    return TEXT_CallTo16_word_wlw( info->proc, hdc, info->param, len );
}

/***********************************************************************
 *           TEXT_GrayString
 *
 * FIXME: The call to 16-bit code only works because the wine GDI is a 16-bit
 * heap and we can guarantee that the handles fit in an INT16. We have to
 * rethink the strategy once the migration to NT handles is complete.
 * We are going to get a lot of code-duplication once this migration is
 * completed...
 *
 */
static BOOL TEXT_GrayString(HDC hdc, HBRUSH hb, GRAYSTRINGPROC fn, LPARAM lp, INT len,
                            INT x, INT y, INT cx, INT cy, BOOL unicode, BOOL _32bit)
{
    HBITMAP hbm, hbmsave;
    HBRUSH hbsave;
    HFONT hfsave;
    HDC memdc;
    int slen = len;
    BOOL retval = TRUE;
    COLORREF fg, bg;

    if(!hdc) return FALSE;
    if (!(memdc = CreateCompatibleDC(hdc))) return FALSE;

    if(len == 0)
    {
        if(unicode)
    	    slen = lstrlenW((LPCWSTR)lp);
    	else if(_32bit)
    	    slen = strlen((LPCSTR)lp);
    	else
    	    slen = strlen(MapSL(lp));
    }

    if((cx == 0 || cy == 0) && slen != -1)
    {
        SIZE s;
        if(unicode)
            GetTextExtentPoint32W(hdc, (LPCWSTR)lp, slen, &s);
        else if(_32bit)
            GetTextExtentPoint32A(hdc, (LPCSTR)lp, slen, &s);
        else
            GetTextExtentPoint32A(hdc, MapSL(lp), slen, &s);
        if(cx == 0) cx = s.cx;
        if(cy == 0) cy = s.cy;
    }

    hbm = CreateBitmap(cx, cy, 1, 1, NULL);
    hbmsave = (HBITMAP)SelectObject(memdc, hbm);
    hbsave = SelectObject( memdc, GetStockObject(BLACK_BRUSH) );
    PatBlt( memdc, 0, 0, cx, cy, PATCOPY );
    SelectObject( memdc, hbsave );
    SetTextColor(memdc, RGB(255, 255, 255));
    SetBkColor(memdc, RGB(0, 0, 0));
    hfsave = (HFONT)SelectObject(memdc, GetCurrentObject(hdc, OBJ_FONT));

    if(fn)
    {
        if(_32bit)
            retval = fn(memdc, lp, slen);
        else
            retval = (BOOL)((BOOL16)((GRAYSTRINGPROC16)fn)((HDC16)memdc, lp, (INT16)slen));
    }
    else
    {
        if(unicode)
            TextOutW(memdc, 0, 0, (LPCWSTR)lp, slen);
        else if(_32bit)
            TextOutA(memdc, 0, 0, (LPCSTR)lp, slen);
        else
            TextOutA(memdc, 0, 0, MapSL(lp), slen);
    }

    SelectObject(memdc, hfsave);

/*
 * Windows doc says that the bitmap isn't grayed when len == -1 and
 * the callback function returns FALSE. However, testing this on
 * win95 showed otherwise...
*/
#ifdef GRAYSTRING_USING_DOCUMENTED_BEHAVIOUR
    if(retval || len != -1)
#endif
    {
        hbsave = (HBRUSH)SelectObject(memdc, CACHE_GetPattern55AABrush());
        PatBlt(memdc, 0, 0, cx, cy, 0x000A0329);
        SelectObject(memdc, hbsave);
    }

    if(hb) hbsave = (HBRUSH)SelectObject(hdc, hb);
    fg = SetTextColor(hdc, RGB(0, 0, 0));
    bg = SetBkColor(hdc, RGB(255, 255, 255));
    BitBlt(hdc, x, y, cx, cy, memdc, 0, 0, 0x00E20746);
    SetTextColor(hdc, fg);
    SetBkColor(hdc, bg);
    if(hb) SelectObject(hdc, hbsave);

    SelectObject(memdc, hbmsave);
    DeleteObject(hbm);
    DeleteDC(memdc);
    return retval;
}


/***********************************************************************
 *           GrayString   (USER.185)
 */
BOOL16 WINAPI GrayString16( HDC16 hdc, HBRUSH16 hbr, GRAYSTRINGPROC16 gsprc,
                            LPARAM lParam, INT16 cch, INT16 x, INT16 y,
                            INT16 cx, INT16 cy )
{
    struct gray_string_info info;

    if (!gsprc) return TEXT_GrayString(hdc, hbr, NULL, lParam, cch, x, y, cx, cy, FALSE, FALSE);
    info.proc  = gsprc;
    info.param = lParam;
    return TEXT_GrayString( hdc, hbr, gray_string_callback, (LPARAM)&info,
                            cch, x, y, cx, cy, FALSE, FALSE);
}


/***********************************************************************
 *           GrayStringA   (USER32.@)
 */
BOOL WINAPI GrayStringA( HDC hdc, HBRUSH hbr, GRAYSTRINGPROC gsprc,
                         LPARAM lParam, INT cch, INT x, INT y,
                         INT cx, INT cy )
{
    return TEXT_GrayString(hdc, hbr, gsprc, lParam, cch, x, y, cx, cy,
                           FALSE, TRUE);
}


/***********************************************************************
 *           GrayStringW   (USER32.@)
 */
BOOL WINAPI GrayStringW( HDC hdc, HBRUSH hbr, GRAYSTRINGPROC gsprc,
                         LPARAM lParam, INT cch, INT x, INT y,
                         INT cx, INT cy )
{
    return TEXT_GrayString(hdc, hbr, gsprc, lParam, cch, x, y, cx, cy,
                           TRUE, TRUE);
}

/***********************************************************************
 *
 *           TabbedText functions
 */

/***********************************************************************
 *           TEXT_TabbedTextOut
 *
 * Helper function for TabbedTextOut() and GetTabbedTextExtent().
 * Note: this doesn't work too well for text-alignment modes other
 *       than TA_LEFT|TA_TOP. But we want bug-for-bug compatibility :-)
 */
static LONG TEXT_TabbedTextOut( HDC hdc, INT x, INT y, LPCSTR lpstr,
                                INT count, INT cTabStops, const INT16 *lpTabPos16,
                                const INT *lpTabPos32, INT nTabOrg,
                                BOOL fDisplayText )
{
    INT defWidth;
    SIZE extent;
    int i, tabPos = x;
    int start = x;

    extent.cx = 0;
    extent.cy = 0;

    if (cTabStops == 1)
    {
        defWidth = lpTabPos32 ? *lpTabPos32 : *lpTabPos16;
        cTabStops = 0;
    }
    else
    {
        TEXTMETRICA tm;
        GetTextMetricsA( hdc, &tm );
        defWidth = 8 * tm.tmAveCharWidth;
    }

    while (count > 0)
    {
        for (i = 0; i < count; i++)
            if (lpstr[i] == '\t') break;
        GetTextExtentPointA( hdc, lpstr, i, &extent );
        if (lpTabPos32)
        {
            while ((cTabStops > 0) &&
                   (nTabOrg + *lpTabPos32 <= x + extent.cx))
            {
                lpTabPos32++;
                cTabStops--;
            }
        }
        else
        {
            while ((cTabStops > 0) &&
                   (nTabOrg + *lpTabPos16 <= x + extent.cx))
            {
                lpTabPos16++;
                cTabStops--;
            }
        }
        if (i == count)
            tabPos = x + extent.cx;
        else if (cTabStops > 0)
            tabPos = nTabOrg + (lpTabPos32 ? *lpTabPos32 : *lpTabPos16);
        else
            tabPos = nTabOrg + ((x + extent.cx - nTabOrg) / defWidth + 1) * defWidth;
        if (fDisplayText)
        {
            RECT r;
            r.left   = x;
            r.top    = y;
            r.right  = tabPos;
            r.bottom = y + extent.cy;
            ExtTextOutA( hdc, x, y,
                           GetBkMode(hdc) == OPAQUE ? ETO_OPAQUE : 0,
                           &r, lpstr, i, NULL );
        }
        x = tabPos;
        count -= i+1;
        lpstr += i+1;
    }
    return MAKELONG(tabPos - start, extent.cy);
}


/***********************************************************************
 *           TabbedTextOut    (USER.196)
 */
LONG WINAPI TabbedTextOut16( HDC16 hdc, INT16 x, INT16 y, LPCSTR lpstr,
                             INT16 count, INT16 cTabStops,
                             const INT16 *lpTabPos, INT16 nTabOrg )
{
    TRACE("%04x %d,%d %s %d\n", hdc, x, y, debugstr_an(lpstr,count), count );
    return TEXT_TabbedTextOut( hdc, x, y, lpstr, count, cTabStops,
                               lpTabPos, NULL, nTabOrg, TRUE );
}


/***********************************************************************
 *           TabbedTextOutA    (USER32.@)
 */
LONG WINAPI TabbedTextOutA( HDC hdc, INT x, INT y, LPCSTR lpstr, INT count,
                            INT cTabStops, const INT *lpTabPos, INT nTabOrg )
{
    TRACE("%04x %d,%d %s %d\n", hdc, x, y, debugstr_an(lpstr,count), count );
    return TEXT_TabbedTextOut( hdc, x, y, lpstr, count, cTabStops,
                               NULL, lpTabPos, nTabOrg, TRUE );
}


/***********************************************************************
 *           TabbedTextOutW    (USER32.@)
 */
LONG WINAPI TabbedTextOutW( HDC hdc, INT x, INT y, LPCWSTR str, INT count,
                            INT cTabStops, const INT *lpTabPos, INT nTabOrg )
{
    LONG ret;
    LPSTR p;
    INT acount;
    UINT codepage = CP_ACP; /* FIXME: get codepage of font charset */

    acount = WideCharToMultiByte(codepage,0,str,count,NULL,0,NULL,NULL);
    p = HeapAlloc( GetProcessHeap(), 0, acount );
    if(p == NULL) return 0; /* FIXME: is this the correct return on failure */
    acount = WideCharToMultiByte(codepage,0,str,count,p,acount,NULL,NULL);
    ret = TabbedTextOutA( hdc, x, y, p, acount, cTabStops, lpTabPos, nTabOrg );
    HeapFree( GetProcessHeap(), 0, p );
    return ret;
}


/***********************************************************************
 *           GetTabbedTextExtent    (USER.197)
 */
DWORD WINAPI GetTabbedTextExtent16( HDC16 hdc, LPCSTR lpstr, INT16 count,
                                    INT16 cTabStops, const INT16 *lpTabPos )
{
    TRACE("%04x %s %d\n", hdc, debugstr_an(lpstr,count), count );
    return TEXT_TabbedTextOut( hdc, 0, 0, lpstr, count, cTabStops,
                               lpTabPos, NULL, 0, FALSE );
}


/***********************************************************************
 *           GetTabbedTextExtentA    (USER32.@)
 */
DWORD WINAPI GetTabbedTextExtentA( HDC hdc, LPCSTR lpstr, INT count,
                                   INT cTabStops, const INT *lpTabPos )
{
    TRACE("%04x %s %d\n", hdc, debugstr_an(lpstr,count), count );
    return TEXT_TabbedTextOut( hdc, 0, 0, lpstr, count, cTabStops,
                               NULL, lpTabPos, 0, FALSE );
}


/***********************************************************************
 *           GetTabbedTextExtentW    (USER32.@)
 */
DWORD WINAPI GetTabbedTextExtentW( HDC hdc, LPCWSTR lpstr, INT count,
                                   INT cTabStops, const INT *lpTabPos )
{
    LONG ret;
    LPSTR p;
    INT acount;
    UINT codepage = CP_ACP; /* FIXME: get codepage of font charset */

    acount = WideCharToMultiByte(codepage,0,lpstr,count,NULL,0,NULL,NULL);
    p = HeapAlloc( GetProcessHeap(), 0, acount );
    if(p == NULL) return 0; /* FIXME: is this the correct failure value? */
    acount = WideCharToMultiByte(codepage,0,lpstr,count,p,acount,NULL,NULL);
    ret = GetTabbedTextExtentA( hdc, p, acount, cTabStops, lpTabPos );
    HeapFree( GetProcessHeap(), 0, p );
    return ret;
}

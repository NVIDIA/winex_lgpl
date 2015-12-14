/*
 * line edition function for Win32 console
 *
 * Copyright 2001 Eric Pouech
 * Copyright (c) 2008-2015 NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2002-2005 the ReWind project authors (see LICENSE.ReWind)
 */

#include "config.h"
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "wincon.h"
#include "wine/unicode.h"
#include "winnls.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(console);

/* console.c */
extern int CONSOLE_GetHistory(int idx, WCHAR* buf, int buf_len);
extern BOOL CONSOLE_AppendHistory(const WCHAR *p);
extern unsigned int CONSOLE_GetNumHistoryEntries(void);
extern void CONSOLE_FillLineUniform(HANDLE hConsoleOutput, int i, int j, int len, LPCHAR_INFO lpFill);

struct WCEL_Context;

typedef struct
{
    WCHAR			val;		/* vk or unicode char */
    void			(*func)(struct WCEL_Context* ctx);
} KeyEntry;

typedef struct
{
    DWORD			keyState;	/* keyState (from INPUT_RECORD) to match */
    BOOL			chkChar;	/* check vk or char */
    KeyEntry*			entries;	/* array of entries */
} KeyMap;

typedef struct WCEL_Context {
    WCHAR*			line;		/* the line being edited */
    size_t			alloc;		/* number of WCHAR in line */
    unsigned    		len;		/* number of chars in line */
    unsigned			ofs;		/* offset for cursor in current line */
    WCHAR*			yanked;		/* yanked line */
    unsigned			mark;		/* marked point (emacs mode only) */
    CONSOLE_SCREEN_BUFFER_INFO	csbi;		/* current state (initial cursor, window size, attribute) */
    HANDLE			hConIn;
    HANDLE			hConOut;
    unsigned			done : 1,	/* to 1 when we're done with editing */
	                        error : 1,	/* to 1 when an error occurred in the editing */
                                can_wrap : 1;   /* to 1 when multi-line edition can take place */
    unsigned			histSize;
    unsigned			histPos;
    WCHAR*			histCurr;
} WCEL_Context;

#if 0
static void WCEL_Dump(WCEL_Context* ctx, const char* pfx)
{
    MESSAGE("%s: [line=%s[alloc=%u] ofs=%u len=%u start=(%d,%d) mask=%c%c%c]\n"
	    "\t\thist=(size=%u pos=%u curr=%s)\n"
            "\t\tyanked=%s\n",
	    pfx, debugstr_w(ctx->line), ctx->alloc, ctx->ofs, ctx->len,
	    ctx->csbi.dwCursorPosition.X, ctx->csbi.dwCursorPosition.Y,
	    ctx->done ? 'D' : 'd', ctx->error ? 'E' : 'e', ctx->can_wrap ? 'W' : 'w',
	    ctx->histSize, ctx->histPos, debugstr_w(ctx->histCurr),
            debugstr_w(ctx->yanked));
}
#endif

/* ====================================================================
 *
 * Console helper functions
 *
 * ====================================================================*/

static BOOL WCEL_Get(WCEL_Context* ctx, INPUT_RECORD* ir)
{
    DWORD		retv;

    for (;;)
    {
	/* data available ? */
	if (ReadConsoleInputW(ctx->hConIn, ir, 1, &retv) && retv == 1)
	    return TRUE;
	/* then wait... */
	switch (WaitForSingleObject(ctx->hConIn, INFINITE))
	{
	case WAIT_OBJECT_0:
	    break;
	default:
	    /* we have checked that hConIn was a console handle (could be sb) */
	    ERR("Shouldn't happen\n");
	    /* fall thru */
	case WAIT_ABANDONED:
	case WAIT_TIMEOUT:
	    ctx->error = 1;
	    ERR("hmm bad situation\n");
	    return FALSE;
	}
    }
}

static inline void WCEL_Beep(WCEL_Context* ctx)
{
    Beep(400, 300);
}

static inline BOOL WCEL_IsSingleLine(WCEL_Context* ctx, size_t len)
{
    return ctx->csbi.dwCursorPosition.X + ctx->len + len <= ctx->csbi.dwSize.X;
}

static inline COORD WCEL_GetCoord(WCEL_Context* ctx, int ofs)
{
    COORD       c;
    unsigned    len = ctx->csbi.dwSize.X - ctx->csbi.dwCursorPosition.X;

    c.Y = ctx->csbi.dwCursorPosition.Y;
    if (ofs >= len)
    {
        ofs -= len;
        c.X = ofs % ctx->csbi.dwSize.X;
        c.Y += 1 + ofs / ctx->csbi.dwSize.X;
    }
    else c.X = ctx->csbi.dwCursorPosition.X + ofs;
    return c;
}

static inline void WCEL_Update(WCEL_Context* ctx, int beg, int len)
{
    WriteConsoleOutputCharacterW(ctx->hConOut, &ctx->line[beg], len,
                                 WCEL_GetCoord(ctx, beg), NULL);
}

/* ====================================================================
 *
 * context manipulation functions
 *
 * ====================================================================*/

static BOOL WCEL_Grow(WCEL_Context* ctx, size_t len)
{
    if (!WCEL_IsSingleLine(ctx, len) && !ctx->can_wrap)
    {
        FIXME("Mode doesn't allow to wrap. However, we should allow to overwrite current string\n");
        return FALSE;
    }

    if (ctx->len + len >= ctx->alloc)
    {
	WCHAR*	newline;
        size_t  newsize;

        /* round up size to 32 byte-WCHAR boundary */
        newsize = (ctx->len + len + 1 + 31) & ~31;
	newline = HeapReAlloc(GetProcessHeap(), 0, ctx->line, sizeof(WCHAR) * newsize);
	if (!newline) return FALSE;
	ctx->line = newline;
	ctx->alloc = newsize;
    }
    return TRUE;
}

static void WCEL_DeleteString(WCEL_Context* ctx, int beg, int end)
{
    unsigned    str_len = end - beg;
    COORD       cbeg = WCEL_GetCoord(ctx, ctx->len - str_len);
    COORD       cend = WCEL_GetCoord(ctx, ctx->len);
    CHAR_INFO   ci;

    if (end < ctx->len)
	memmove(&ctx->line[beg], &ctx->line[end], (ctx->len - end) * sizeof(WCHAR));
    /* we need to clean from ctx->len - str_len to ctx->len */

    ci.Char.UnicodeChar = ' ';
    ci.Attributes = ctx->csbi.wAttributes;

    if (cbeg.Y == cend.Y)
    {
        /* partial erase of sole line */
        CONSOLE_FillLineUniform(ctx->hConOut, cbeg.X, cbeg.Y,
                                cend.X - cbeg.X, &ci);
    }
    else
    {
        int         i;
        /* erase til eol on first line */
        CONSOLE_FillLineUniform(ctx->hConOut, cbeg.X, cbeg.Y,
                                ctx->csbi.dwSize.X - cbeg.X, &ci);
        /* completly erase all the others (full lines) */
        for (i = cbeg.Y + 1; i < cend.Y; i++)
            CONSOLE_FillLineUniform(ctx->hConOut, 0, i, ctx->csbi.dwSize.X, &ci);
        /* erase from beg of line until last pos on last line */
        CONSOLE_FillLineUniform(ctx->hConOut, 0, cend.Y, cend.X, &ci);
    }
    ctx->len -= str_len;
    WCEL_Update(ctx, 0, ctx->len);
    ctx->line[ctx->len] = 0;
}

static void WCEL_InsertString(WCEL_Context* ctx, const WCHAR* str)
{
    size_t	len = lstrlenW(str);

    if (!len || !WCEL_Grow(ctx, len)) return;
    if (ctx->len > ctx->ofs)
	memmove(&ctx->line[ctx->ofs + len], &ctx->line[ctx->ofs], (ctx->len - ctx->ofs) * sizeof(WCHAR));
    memcpy(&ctx->line[ctx->ofs], str, len * sizeof(WCHAR));
    ctx->len += len;
    ctx->line[ctx->len] = 0;
    WCEL_Update(ctx, ctx->ofs, ctx->len - ctx->ofs);

    ctx->ofs += len;
}

static void WCEL_InsertChar(WCEL_Context* ctx, WCHAR c)
{
    WCHAR	buffer[2];

    /* do not insert 0..31 control characters */
    if (c < ' ' && c != '\t') return;

    buffer[0] = c;
    buffer[1] = 0;
    WCEL_InsertString(ctx, buffer);
}

static void WCEL_FreeYank(WCEL_Context* ctx)
{
    if (ctx->yanked)
    {
        HeapFree(GetProcessHeap(), 0, ctx->yanked);
        ctx->yanked = NULL;
    }
}

static void WCEL_SaveYank(WCEL_Context* ctx, int beg, int end)
{
    int len = end - beg;
    if (len <= 0) return;

    WCEL_FreeYank(ctx);
    ctx->yanked = HeapReAlloc(GetProcessHeap(), 0, ctx->yanked, (len + 1) * sizeof(WCHAR));
    if (!ctx->yanked) return;
    memcpy(ctx->yanked, &ctx->line[beg], len * sizeof(WCHAR));
    ctx->yanked[len] = 0;
}

/* FIXME NTDLL doesn't export iswalnum, and I don't want to link in msvcrt when most
 * of the data lay in unicode lib
 */
static inline BOOL WCEL_iswalnum(WCHAR wc)
{
    return get_char_typeW(wc) & (C1_ALPHA|C1_DIGIT|C1_LOWER|C1_UPPER);
}

static int WCEL_GetLeftWordTransition(WCEL_Context* ctx, int ofs)
{
    ofs--;
    while (ofs >= 0 && !WCEL_iswalnum(ctx->line[ofs])) ofs--;
    while (ofs >= 0 && WCEL_iswalnum(ctx->line[ofs])) ofs--;
    if (ofs >= 0) ofs++;
    return max(ofs, 0);
}

static int WCEL_GetRightWordTransition(WCEL_Context* ctx, int ofs)
{
    ofs++;
    while (ofs <= ctx->len && WCEL_iswalnum(ctx->line[ofs])) ofs++;
    while (ofs <= ctx->len && !WCEL_iswalnum(ctx->line[ofs])) ofs++;
    return min(ofs, ctx->len);
}

static WCHAR* WCEL_GetHistory(WCEL_Context* ctx, int idx)
{
    WCHAR*	ptr;

    if (idx == ctx->histSize - 1)
    {
	ptr = HeapAlloc(GetProcessHeap(), 0, (lstrlenW(ctx->histCurr) + 1) * sizeof(WCHAR));
	lstrcpyW(ptr, ctx->histCurr);
    }
    else
    {
	int	len = CONSOLE_GetHistory(idx, NULL, 0);

	if ((ptr = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR))))
	{
	    CONSOLE_GetHistory(idx, ptr, len);
	}
    }
    return ptr;
}

static void	WCEL_HistoryInit(WCEL_Context* ctx)
{
    ctx->histPos  = CONSOLE_GetNumHistoryEntries();
    ctx->histSize = ctx->histPos + 1;
    ctx->histCurr = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WCHAR));
}

static void    WCEL_MoveToHist(WCEL_Context* ctx, int idx)
{
    WCHAR*	data = WCEL_GetHistory(ctx, idx);
    int		len = lstrlenW(data) + 1;

    /* save current line edition for recall when needed (FIXME seems broken to me) */
    if (ctx->histPos == ctx->histSize - 1)
    {
	if (ctx->histCurr) HeapFree(GetProcessHeap(), 0, ctx->histCurr);
	ctx->histCurr = HeapAlloc(GetProcessHeap(), 0, (ctx->len + 1) * sizeof(WCHAR));
	memcpy(ctx->histCurr, ctx->line, (ctx->len + 1) * sizeof(WCHAR));
    }
    /* need to clean also the screen if new string is shorter than old one */
    WCEL_DeleteString(ctx, 0, ctx->len);
    ctx->ofs = 0;
    /* insert new string */
    if (WCEL_Grow(ctx, len))
    {
	WCEL_InsertString(ctx, data);
	HeapFree(GetProcessHeap(), 0, data);
	ctx->histPos = idx;
    }
}

static void    WCEL_FindPrevInHist(WCEL_Context* ctx)
{
    int startPos = ctx->histPos;
    WCHAR*	data;
    int		len, oldofs;

    if (ctx->histPos && ctx->histPos == ctx->histSize) {
        startPos--;
        ctx->histPos--;
    }

    do {
       data = WCEL_GetHistory(ctx, ctx->histPos);

       if (ctx->histPos) ctx->histPos--;
       else ctx->histPos = (ctx->histSize-1);

       len = lstrlenW(data) + 1;
       if ((len >= ctx->ofs) && 
           (memcmp(ctx->line, data, ctx->ofs * sizeof(WCHAR)) == 0)) {

           /* need to clean also the screen if new string is shorter than old one */
           WCEL_DeleteString(ctx, 0, ctx->len);

           if (WCEL_Grow(ctx, len))
           {
              oldofs = ctx->ofs;
              ctx->ofs = 0;
              WCEL_InsertString(ctx, data);
              ctx->ofs = oldofs;
              SetConsoleCursorPosition(ctx->hConOut, WCEL_GetCoord(ctx, ctx->ofs));
              HeapFree(GetProcessHeap(), 0, data);
              return;
           }
       }
    } while (ctx->histPos != startPos);
    
    return;
}

/* ====================================================================
 *
 * basic edition functions
 *
 * ====================================================================*/

static void WCEL_Done(WCEL_Context* ctx)
{
    WCHAR       nl = '\n';
    if (!WCEL_Grow(ctx, 1)) return;
    ctx->line[ctx->len++] = '\n';
    ctx->line[ctx->len] = 0;
    WriteConsoleW(ctx->hConOut, &nl, 1, NULL, NULL);
    ctx->done = 1;
}

static void WCEL_MoveLeft(WCEL_Context* ctx)
{
    if (ctx->ofs > 0) ctx->ofs--;
}

static void WCEL_MoveRight(WCEL_Context* ctx)
{
    if (ctx->ofs < ctx->len) ctx->ofs++;
}

static void WCEL_MoveToLeftWord(WCEL_Context* ctx)
{
    int	new_ofs = WCEL_GetLeftWordTransition(ctx, ctx->ofs);
    if (new_ofs != ctx->ofs) ctx->ofs = new_ofs;
}

static void WCEL_MoveToRightWord(WCEL_Context* ctx)
{
    int	new_ofs = WCEL_GetRightWordTransition(ctx, ctx->ofs);
    if (new_ofs != ctx->ofs) ctx->ofs = new_ofs;
}

static void WCEL_MoveToBeg(WCEL_Context* ctx)
{
    ctx->ofs = 0;
}

static void WCEL_MoveToEnd(WCEL_Context* ctx)
{
    ctx->ofs = ctx->len;
}

static void WCEL_SetMark(WCEL_Context* ctx)
{
    ctx->mark = ctx->ofs;
}

static void WCEL_ExchangeMark(WCEL_Context* ctx)
{
    unsigned tmp;

    if (ctx->mark > ctx->len) return;
    tmp = ctx->ofs;
    ctx->ofs = ctx->mark;
    ctx->mark = tmp;
}

static void WCEL_CopyMarkedZone(WCEL_Context* ctx)
{
    unsigned beg, end;

    if (ctx->mark > ctx->len || ctx->mark == ctx->ofs) return;
    if (ctx->mark > ctx->ofs)
    {
	beg = ctx->ofs;		end = ctx->mark;
    }
    else
    {
	beg = ctx->mark;	end = ctx->ofs;
    }
    WCEL_SaveYank(ctx, beg, end);
}

static void WCEL_TransposeChar(WCEL_Context* ctx)
{
    WCHAR	c;

    if (!ctx->ofs || ctx->ofs == ctx->len) return;

    c = ctx->line[ctx->ofs];
    ctx->line[ctx->ofs] = ctx->line[ctx->ofs - 1];
    ctx->line[ctx->ofs - 1] = c;

    WCEL_Update(ctx, ctx->ofs - 1, 2);
    ctx->ofs++;
}

static void WCEL_TransposeWords(WCEL_Context* ctx)
{
    int	left_ofs = WCEL_GetLeftWordTransition(ctx, ctx->ofs),
        right_ofs = WCEL_GetRightWordTransition(ctx, ctx->ofs);
    if (left_ofs < ctx->ofs && right_ofs > ctx->ofs)
    {
        unsigned len_r = right_ofs - ctx->ofs;
        unsigned len_l = ctx->ofs - left_ofs;

        char*   tmp = HeapAlloc(GetProcessHeap(), 0, len_r * sizeof(WCHAR));
        if (!tmp) return;

        memcpy(tmp, &ctx->line[ctx->ofs], len_r * sizeof(WCHAR));
        memmove(&ctx->line[left_ofs + len_r], &ctx->line[left_ofs], len_l * sizeof(WCHAR));
        memcpy(&ctx->line[left_ofs], tmp, len_r * sizeof(WCHAR));

        HeapFree(GetProcessHeap(), 0, tmp);
        WCEL_Update(ctx, left_ofs, len_l + len_r);
        ctx->ofs = right_ofs;
    }
}

static void WCEL_LowerCaseWord(WCEL_Context* ctx)
{
    int	new_ofs = WCEL_GetRightWordTransition(ctx, ctx->ofs);
    if (new_ofs != ctx->ofs)
    {
	int	i;
	for (i = ctx->ofs; i <= new_ofs; i++)
	    ctx->line[i] = tolowerW(ctx->line[i]);
        WCEL_Update(ctx, ctx->ofs, new_ofs - ctx->ofs + 1);
	ctx->ofs = new_ofs;
    }
}

static void WCEL_UpperCaseWord(WCEL_Context* ctx)
{
    int	new_ofs = WCEL_GetRightWordTransition(ctx, ctx->ofs);
    if (new_ofs != ctx->ofs)
    {
	int	i;
	for (i = ctx->ofs; i <= new_ofs; i++)
	    ctx->line[i] = toupperW(ctx->line[i]);
	WCEL_Update(ctx, ctx->ofs, new_ofs - ctx->ofs + 1);
	ctx->ofs = new_ofs;
    }
}

static void WCEL_CapitalizeWord(WCEL_Context* ctx)
{
    int	new_ofs = WCEL_GetRightWordTransition(ctx, ctx->ofs);
    if (new_ofs != ctx->ofs)
    {
	int	i;

	ctx->line[ctx->ofs] = toupperW(ctx->line[ctx->ofs]);
	for (i = ctx->ofs + 1; i <= new_ofs; i++)
	    ctx->line[i] = tolowerW(ctx->line[i]);
	WCEL_Update(ctx, ctx->ofs, new_ofs - ctx->ofs + 1);
	ctx->ofs = new_ofs;
    }
}

static void WCEL_Yank(WCEL_Context* ctx)
{
    WCEL_InsertString(ctx, ctx->yanked);
}

static void WCEL_KillToEndOfLine(WCEL_Context* ctx)
{
    WCEL_SaveYank(ctx, ctx->ofs, ctx->len);
    WCEL_DeleteString(ctx, ctx->ofs, ctx->len);
}

static void WCEL_KillMarkedZone(WCEL_Context* ctx)
{
    unsigned beg, end;

    if (ctx->mark > ctx->len || ctx->mark == ctx->ofs) return;
    if (ctx->mark > ctx->ofs)
    {
	beg = ctx->ofs;		end = ctx->mark;
    }
    else
    {
	beg = ctx->mark;	end = ctx->ofs;
    }
    WCEL_SaveYank(ctx, beg, end);
    WCEL_DeleteString(ctx, beg, end);
    ctx->ofs = beg;
}

static void WCEL_DeletePrevChar(WCEL_Context* ctx)
{
    if (ctx->ofs)
    {
	WCEL_DeleteString(ctx, ctx->ofs - 1, ctx->ofs);
	ctx->ofs--;
    }
}

static void WCEL_DeleteCurrChar(WCEL_Context* ctx)
{
    if (ctx->ofs < ctx->len)
	WCEL_DeleteString(ctx, ctx->ofs, ctx->ofs + 1);
}

static void WCEL_DeleteLeftWord(WCEL_Context* ctx)
{
    int	new_ofs = WCEL_GetLeftWordTransition(ctx, ctx->ofs);
    if (new_ofs != ctx->ofs)
    {
	WCEL_DeleteString(ctx, new_ofs, ctx->ofs);
	ctx->ofs = new_ofs;
    }
}

static void WCEL_DeleteRightWord(WCEL_Context* ctx)
{
    int	new_ofs = WCEL_GetRightWordTransition(ctx, ctx->ofs);
    if (new_ofs != ctx->ofs)
    {
	WCEL_DeleteString(ctx, ctx->ofs, new_ofs);
    }
}

static void WCEL_MoveToPrevHist(WCEL_Context* ctx)
{
    if (ctx->histPos) WCEL_MoveToHist(ctx, ctx->histPos - 1);
}

static void WCEL_MoveToNextHist(WCEL_Context* ctx)
{
    if (ctx->histPos < ctx->histSize - 1) WCEL_MoveToHist(ctx, ctx->histPos + 1);
}

static void WCEL_MoveToFirstHist(WCEL_Context* ctx)
{
    if (ctx->histPos != 0) WCEL_MoveToHist(ctx, 0);
}

static void WCEL_MoveToLastHist(WCEL_Context* ctx)
{
    if (ctx->histPos != ctx->histSize - 1) WCEL_MoveToHist(ctx, ctx->histSize - 1);
}

static void WCEL_Redraw(WCEL_Context* ctx)
{
    COORD       c = WCEL_GetCoord(ctx, ctx->len);
    CHAR_INFO   ci;

    WCEL_Update(ctx, 0, ctx->len);

    ci.Char.UnicodeChar = ' ';
    ci.Attributes = ctx->csbi.wAttributes;

    CONSOLE_FillLineUniform(ctx->hConOut, c.X, c.Y, ctx->csbi.dwSize.X - c.X, &ci);
}

static void WCEL_RepeatCount(WCEL_Context* ctx)
{
#if 0
/* FIXME: wait untill all console code is in kernel32 */
    INPUT_RECORD        ir;
    unsigned            repeat = 0;

    while (WCEL_Get(ctx, &ir, FALSE))
    {
        if (ir.EventType != KEY_EVENT) break;
        if (ir.Event.KeyEvent.bKeyDown)
        {
            if ((ir.Event.KeyEvent.dwControlKeyState & ~(NUMLOCK_ON|SCROLLLOCK_ON|CAPSLOCK_ON)) != 0)
                break;
            if (ir.Event.KeyEvent.uChar.UnicodeChar < '0' ||
                ir.Event.KeyEvent.uChar.UnicodeChar > '9')
                break;
            repeat = repeat * 10 + ir.Event.KeyEvent.uChar.UnicodeChar - '0';
        }
        WCEL_Get(ctx, &ir, TRUE);
    }
    FIXME("=> %u\n", repeat);
#endif
}

/* ====================================================================
 *
 * 		Key Maps
 *
 * ====================================================================*/

#define CTRL(x)	((x) - '@')
static KeyEntry StdKeyMap[] =
{
    {/*BACK*/0x08,	WCEL_DeletePrevChar 	},
    {/*RETURN*/0x0d,	WCEL_Done		},
    {/*DEL*/127,	WCEL_DeleteCurrChar 	},
    {	0,		NULL			}
};

static KeyEntry Win32ExtraStdKeyMap[] = 
{
    {/*VK_F8*/   0x77,	WCEL_FindPrevInHist	},
    {	0,		NULL			}
};


static	KeyEntry EmacsKeyMapCtrl[] =
{
    {	CTRL('@'),	WCEL_SetMark		},
    {	CTRL('A'),	WCEL_MoveToBeg		},
    {	CTRL('B'),	WCEL_MoveLeft		},
    /* C: done in server */
    {	CTRL('D'),	WCEL_DeleteCurrChar	},
    {	CTRL('E'),	WCEL_MoveToEnd		},
    {	CTRL('F'),	WCEL_MoveRight		},
    {	CTRL('G'),	WCEL_Beep		},
    {	CTRL('H'),	WCEL_DeletePrevChar	},
    /* I: meaningless (or tab ???) */
    {	CTRL('J'),	WCEL_Done		},
    {	CTRL('K'),	WCEL_KillToEndOfLine	},
    {   CTRL('L'),      WCEL_Redraw             },
    {	CTRL('M'),	WCEL_Done		},
    {	CTRL('N'),	WCEL_MoveToNextHist	},
    /* O; insert line... meaningless */
    {	CTRL('P'),	WCEL_MoveToPrevHist	},
    /* Q: [NIY] quoting... */
    /* R: [NIY] search backwards... */
    /* S: [NIY] search forwards... */
    {	CTRL('T'),	WCEL_TransposeChar	},
    {   CTRL('U'),      WCEL_RepeatCount        },
    /* V: paragraph down... meaningless */
    {	CTRL('W'),	WCEL_KillMarkedZone	},
    {	CTRL('X'),	WCEL_ExchangeMark	},
    {	CTRL('Y'),	WCEL_Yank		},
    /* Z: meaningless */
    {	0,		NULL			}
};

static KeyEntry EmacsKeyMapAlt[] =
{
    {/*DEL*/127,	WCEL_DeleteLeftWord	},
    {	'<',		WCEL_MoveToFirstHist	},
    {	'>',		WCEL_MoveToLastHist	},
    {	'?',		WCEL_Beep		},
    {	'b',		WCEL_MoveToLeftWord	},
    {   'c',		WCEL_CapitalizeWord	},
    {	'd',		WCEL_DeleteRightWord	},
    {	'f',		WCEL_MoveToRightWord	},
    {	'l',		WCEL_LowerCaseWord	},
    {   't',		WCEL_TransposeWords	},
    {	'u',		WCEL_UpperCaseWord	},
    {	'w', 		WCEL_CopyMarkedZone	},
    {	0,		NULL			}
};

static KeyEntry EmacsKeyMapExtended[] =
{
    {/*RETURN*/  0x0d,	WCEL_Done },
    {/*VK_PRIOR*/0x21, 	WCEL_MoveToPrevHist	},
    {/*VK_NEXT*/ 0x22,	WCEL_MoveToNextHist 	},
    {/*VK_END*/  0x23,	WCEL_MoveToEnd		},
    {/*VK_HOME*/ 0x24,	WCEL_MoveToBeg		},
    {/*VK_RIGHT*/0x27,	WCEL_MoveRight 		},
    {/*VK_LEFT*/ 0x25,	WCEL_MoveLeft 		},
    {/*VK_DEL*/  0x2e,  WCEL_DeleteCurrChar     },
    {	0,		NULL 			}
};

static KeyMap	EmacsKeyMap[] =
{
    {0x00000000, 1, StdKeyMap},
    {0x00000001, 1, EmacsKeyMapAlt},	/* left  alt  */
    {0x00000002, 1, EmacsKeyMapAlt},	/* right alt  */
    {0x00000004, 1, EmacsKeyMapCtrl},	/* left  ctrl */
    {0x00000008, 1, EmacsKeyMapCtrl},	/* right ctrl */
    {0x00000100, 0, EmacsKeyMapExtended},
    {0,		 0, 0}
};

static	KeyEntry Win32KeyMapExtended[] =
{
    {/*VK_LEFT*/ 0x25, 	WCEL_MoveLeft 		},
    {/*VK_RIGHT*/0x27,	WCEL_MoveRight		},
    {/*VK_HOME*/ 0x24,	WCEL_MoveToBeg 		},
    {/*VK_END*/  0x23,	WCEL_MoveToEnd 		},
    {/*VK_UP*/   0x26, 	WCEL_MoveToPrevHist 	},
    {/*VK_DOWN*/ 0x28,	WCEL_MoveToNextHist	},
    {/*VK_DEL*/  0x2e,	WCEL_DeleteCurrChar	},
    {	0,		NULL 			}
};

static	KeyEntry Win32KeyMapCtrlExtended[] =
{
    {/*VK_LEFT*/ 0x25, 	WCEL_MoveToLeftWord 	},
    {/*VK_RIGHT*/0x27,	WCEL_MoveToRightWord	},
    {/*VK_END*/  0x23,	WCEL_KillToEndOfLine	},
    {	0,		NULL 			}
};

KeyMap	Win32KeyMap[] =
{
    {0x00000000, 1, StdKeyMap},
    {0x00000000, 0, Win32ExtraStdKeyMap},
    {0x00000100, 0, Win32KeyMapExtended},
    {0x00000104, 0, Win32KeyMapCtrlExtended},
    {0x00000108, 0, Win32KeyMapCtrlExtended},
    {0,		 0, 0}
};
#undef CTRL

/* ====================================================================
 *
 * 		Read line master function
 *
 * ====================================================================*/

WCHAR* CONSOLE_Readline(HANDLE hConsoleIn, int use_emacs)
{
    WCEL_Context	ctx;
    INPUT_RECORD	ir;
    KeyMap*		km;
    KeyEntry*		ke;
    unsigned		ofs;
    void		(*func)(struct WCEL_Context* ctx);
    DWORD               ks;

    memset(&ctx, 0, sizeof(ctx));
    ctx.hConIn = hConsoleIn;
    WCEL_HistoryInit(&ctx);
    if ((ctx.hConOut = CreateFileA("CONOUT$", GENERIC_READ|GENERIC_WRITE, 0, NULL,
				    OPEN_EXISTING, 0, 0 )) == INVALID_HANDLE_VALUE ||
	!GetConsoleScreenBufferInfo(ctx.hConOut, &ctx.csbi))
	return NULL;
    ctx.can_wrap = (GetConsoleMode(ctx.hConOut, &ks) && (ks & ENABLE_WRAP_AT_EOL_OUTPUT)) ? 1 : 0;

    if (!WCEL_Grow(&ctx, 1))
    {
	CloseHandle(ctx.hConOut);
	return NULL;
    }
    ctx.line[0] = 0;

/* EPP     WCEL_Dump(&ctx, "init"); */

    while (!ctx.done && !ctx.error && WCEL_Get(&ctx, &ir))
    {
	if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown) continue;
	TRACE("key%s repeatCount=%u, keyCode=%02x scanCode=%02x char=%02x keyState=%08lx\n",
	      ir.Event.KeyEvent.bKeyDown ? "Down" : "Up  ", ir.Event.KeyEvent.wRepeatCount,
	      ir.Event.KeyEvent.wVirtualKeyCode, ir.Event.KeyEvent.wVirtualScanCode,
	      ir.Event.KeyEvent.uChar.UnicodeChar, ir.Event.KeyEvent.dwControlKeyState);

/* EPP  	WCEL_Dump(&ctx, "before func"); */
	ofs = ctx.ofs;
        /* mask out some bits which don't interest us */
        ks = ir.Event.KeyEvent.dwControlKeyState & ~(NUMLOCK_ON|SCROLLLOCK_ON|CAPSLOCK_ON);

	func = NULL;
	for (km = (use_emacs) ? EmacsKeyMap : Win32KeyMap; km->entries != NULL; km++)
	{
	    if (km->keyState != ks)
		continue;
	    if (km->chkChar)
	    {
		for (ke = &km->entries[0]; ke->func != 0; ke++)
		    if (ke->val == ir.Event.KeyEvent.uChar.UnicodeChar) break;
	    }
	    else
	    {
		for (ke = &km->entries[0]; ke->func != 0; ke++)
		    if (ke->val == ir.Event.KeyEvent.wVirtualKeyCode) break;

	    }
	    if (ke->func)
	    {
		func = ke->func;
		break;
	    }
	}

	if (func)
	    (func)(&ctx);
	else if (!(ir.Event.KeyEvent.dwControlKeyState & (ENHANCED_KEY|LEFT_ALT_PRESSED)))
	    WCEL_InsertChar(&ctx, ir.Event.KeyEvent.uChar.UnicodeChar);
	else TRACE("Dropped event\n");

/* EPP         WCEL_Dump(&ctx, "after func"); */
	if (ctx.ofs != ofs)
	    SetConsoleCursorPosition(ctx.hConOut, WCEL_GetCoord(&ctx, ctx.ofs));
    }
    if (ctx.error)
    {
	HeapFree(GetProcessHeap(), 0, ctx.line);
	ctx.line = NULL;
    }
    WCEL_FreeYank(&ctx);
    if (ctx.line)
	CONSOLE_AppendHistory(ctx.line);

    CloseHandle(ctx.hConOut);
    if (ctx.histCurr) HeapFree(GetProcessHeap(), 0, ctx.histCurr);
    return ctx.line;
}


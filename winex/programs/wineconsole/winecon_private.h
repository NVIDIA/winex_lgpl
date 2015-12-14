/*
 * an application for displaying Win32 console
 *
 * Copyright 2001 Eric Pouech
 */

#include <winbase.h>
#include <wincon.h>

#include "wineconsole_res.h"

/* this is the configuration stored & loaded into the registry */
struct config_data {
    unsigned	cell_width;	/* width in pixels of a character */
    unsigned	cell_height;	/* height in pixels of a character */
    int		cursor_size;	/* in % of cell height */
    int		cursor_visible;
    DWORD       def_attr;
    WCHAR       face_name[32];  /* name of font (size is LF_FACESIZE) */
    DWORD       font_weight;
    DWORD       history_size;
    DWORD       menu_mask;      /* MK_CONTROL MK_SHIFT mask to drive submenu opening */
    DWORD       quick_edit;     /* whether mouse ops are sent to app (false) or used for content selection (true) */
    unsigned	sb_width;	/* active screen buffer width */
    unsigned	sb_height;	/* active screen buffer height */
    unsigned	win_width;	/* size (in cells) of visible part of window (width & height) */
    unsigned	win_height;
    COORD	win_pos;	/* position (in cells) of visible part of screen buffer in window */
    BOOL        exit_on_die;    /* whether the wineconsole should quit if server destroys the console */
};

struct inner_data {
    struct config_data  curcfg;
    struct config_data  defcfg;

    CHAR_INFO*		cells;		/* local copy of cells (sb_width * sb_height) */

    COORD		cursor;		/* position in cells of cursor */

    HANDLE		hConIn;		/* console input handle */
    HANDLE		hConOut;	/* screen buffer handle: has to be changed when active sb changes */
    HANDLE		hSynchro;	/* waitable handle signalled by server when something in server has been modified */

    int			(*fnMainLoop)(struct inner_data* data);
    void		(*fnPosCursor)(const struct inner_data* data);
    void		(*fnShapeCursor)(struct inner_data* data, int size, int vis, BOOL force);
    void		(*fnComputePositions)(struct inner_data* data);
    void		(*fnRefresh)(const struct inner_data* data, int tp, int bm);
    void		(*fnResizeScreenBuffer)(struct inner_data* data);
    void		(*fnSetTitle)(const struct inner_data* data);
    void		(*fnScroll)(struct inner_data* data, int pos, BOOL horz);
    void		(*fnDeleteBackend)(struct inner_data* data);

    void*               private;        /* data part belonging to the choosen backed */
};

/* from wineconsole.c */
extern void WINECON_NotifyWindowChange(struct inner_data* data);
extern int  WINECON_GetHistorySize(HANDLE hConIn);
extern BOOL WINECON_SetHistorySize(HANDLE hConIn, int size);
extern int  WINECON_GetHistoryMode(HANDLE hConIn);
extern BOOL WINECON_SetHistoryMode(HANDLE hConIn, int mode);
extern BOOL WINECON_GetConsoleTitle(HANDLE hConIn, WCHAR* buffer, size_t len);
extern void WINECON_FetchCells(struct inner_data* data, int upd_tp, int upd_bm);
extern int  WINECON_GrabChanges(struct inner_data* data);

/* from registry.c */
extern BOOL WINECON_RegLoad(struct config_data* cfg);
extern BOOL WINECON_RegSave(const struct config_data* cfg);

/* backends... */
extern BOOL WCUSER_InitBackend(struct inner_data* data);

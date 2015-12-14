/* console.h */
/* Copyright 1998 - Joseph Pranevich */

/* Include file for definitions pertaining to Wine's text-console
   interface.
*/

#ifndef __WINE_CONSOLE_H
#define __WINE_CONSOLE_H

#ifndef __WINE_CONFIG_H
# error You must include config.h to use this header
#endif

#include <stdio.h>

/* Can we compile with curses/ncurses? */
#if (	(defined(HAVE_LIBNCURSES) || defined(HAVE_LIBCURSES)) &&	\
	(defined(HAVE_CURSES_H) || defined(HAVE_NCURSES_H))		\
)
# define WINE_NCURSES
#else
# undef WINE_NCURSES
#endif

#define CONSOLE_DEFAULT_DRIVER "tty"

typedef struct CONSOLE_DRIVER
{
   void (*init)(void);
   void (*close)(void);
   void (*write)(char, int, int, int);
   void (*moveCursor)(char, char);
   void (*getCursorPosition)(char *, char *);
   void (*getCharacterAtCursor)(char *, int *, int *, int *);
   void (*clearScreen)(void);

   /* Color-control functions */
   int  (*allocColor)(int color);
   void (*setBackgroundColor)(int fg, int bg);
   void (*getBackgroundColor)(int *fg, int *bg);

   /* Keyboard Functions */
   int  (*checkForKeystroke)(char *, char *);
   void (*getKeystroke)(char *, char *);

   /* Windowing Functions */
   void (*resizeScreen)(int, int);
   void (*notifyResizeScreen)(int, int); /* May be rethought later... */

   /* Accelerator Functions (Screen) */
   void (*clearWindow)(char, char, char, char, int, int);
   void (*scrollUpWindow)(char, char, char, char, char, int, int);
   void (*scrollDownWindow)(char, char, char, char, char, int, int);

   /* Accelerator Functions (Keyboard) */
   char (*getCharacter)(void);

   /* Other functions */
   void (*refresh)(void);

   /* Other data */
   int norefresh;
   FILE *console_out;
   FILE *console_in;
   int x_res;
   int y_res;

} CONSOLE_device;

extern CONSOLE_device driver; /* Global driver struct */

/* Generic defines */
void CONSOLE_Close(void);
void CONSOLE_Write(char out, int fg_color, int bg_color, int attribute);
void CONSOLE_MoveCursor(char row, char col);
void CONSOLE_ClearWindow(char, char, char, char, int, int);
void CONSOLE_ScrollUpWindow(char, char, char, char, char, int, int);
void CONSOLE_ScrollDownWindow(char, char, char, char, char, int, int);
int  CONSOLE_CheckForKeystroke(char *, char*);
void CONSOLE_GetKeystroke(char *, char *);
void CONSOLE_GetCursorPosition(char *, char *);
void CONSOLE_GetCharacterAtCursor(char *, int *, int *, int *);
void CONSOLE_Refresh(void);
void CONSOLE_SetRefresh(int);
int  CONSOLE_GetRefresh(void);
void CONSOLE_ClearScreen(void);
char CONSOLE_GetCharacter(void);
void CONSOLE_ResizeScreen(int, int);
void CONSOLE_NotifyResizeScreen(int, int);
void CONSOLE_WriteRawString(char *);
int  CONSOLE_AllocColor(int);
void CONSOLE_SetBackgroundColor(int fg, int bg);
void CONSOLE_GetBackgroundColor(int *fg, int *bg);

/* Generic Defines */
void GENERIC_Start(void);
void GENERIC_ClearWindow(char, char, char, char, int, int);
void GENERIC_ScrollUpWindow(char, char, char, char, char, int, int);
void GENERIC_ScrollDownWindow(char, char, char, char, char, int, int);
char GENERIC_GetCharacter(void);

/* TTY specific defines */
void TTY_Write(char out, int fg_color, int bg_color, int attribute);
void TTY_Start(void);
void TTY_GetKeystroke(char *, char *);

#ifdef WINE_NCURSES

/* ncurses defines */
void NCURSES_Write(char out, int fg_color, int bg_color, int attribute);
void NCURSES_Start(void);
void NCURSES_Init(void);
void NCURSES_Close(void);
int  NCURSES_CheckForKeystroke(char *, char *);
void NCURSES_GetKeystroke(char *, char *);
void NCURSES_MoveCursor(char ,char);
void NCURSES_GetCursorPosition(char *, char *);
void NCURSES_GetCharacterAtCursor(char *, int *, int *, int *);
void NCURSES_Refresh(void);
void NCURSES_ClearScreen(void);
void NCURSES_NotifyResizeScreen(int x, int y);
int  NCURSES_AllocColor(int);
void NCURSES_SetBackgroundColor(int fg, int bg);
void NCURSES_GetBackgroundColor(int *fg, int *bg);

#endif /* WINE_NCURSES */

/* Xterm specific defines */
void XTERM_Start(void);
void XTERM_Close(void);
void XTERM_Init(void);
void XTERM_ResizeScreen(int x, int y);

/* Color defines */
/* These will eventually be hex triples for dynamic allocation */
/* Triplets added by A.C. and commented out until the support  */
/* code can be written into the console routines.              */
#define WINE_BLACK		1     /*    0x000000      */
#define WINE_BLUE		2     /*    0x0000ff      */
#define WINE_GREEN		3     /*    0x008000      */
#define WINE_CYAN		4     /*    0x00eeee      */
#define WINE_MAGENTA		5     /*    0xcd00cd      */
#define WINE_BROWN		6     /*    0xcd3333      */
#define WINE_RED		7     /*    0xff0000      */
#define WINE_LIGHT_GRAY		8     /*    0xc0c0c0      */
#define WINE_DARK_GRAY		9     /*    0x808080      */
#define WINE_LIGHT_BLUE		10    /*    0x98f5ff      */
#define WINE_LIGHT_GREEN	11    /*    0x00ff00      */
#define WINE_LIGHT_RED		12    /*    0xee6363      */
#define WINE_LIGHT_MAGENTA	13    /*    0xff00ff      */
#define WINE_LIGHT_CYAN		14    /*    0x00ffff      */
#define WINE_YELLOW		15    /*    0xffff00      */
#define WINE_WHITE		16    /*    0xffffff      */

#endif /* CONSOLE_H */

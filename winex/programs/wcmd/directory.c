/*
 * WCMD - Wine-compatible command line interface - Directory functions.
 *
 * (C) 1999 D A Pickles
 *
 * On entry, global variables quals, param1, param2 contain
 * the qualifiers (uppercased and concatenated) and parameters entered, with
 * environment-variable and batch parameter substitution already done.
 */

#include "wcmd.h"

int WCMD_dir_sort (const void *a, const void *b);
void WCMD_list_directory (char *path, int level);
char * WCMD_filesize64 (__int64 free);
char * WCMD_strrev (char *buff);


extern char nyi[];
extern char newline[];
extern char version_string[];
extern char anykey[];
extern int echo_mode;
extern char quals[MAX_PATH], param1[MAX_PATH], param2[MAX_PATH];
extern DWORD errorlevel;

int file_total, dir_total, line_count, page_mode, recurse, wide, bare, 
    max_width;
__int64 byte_total;

/*****************************************************************************
 * WCMD_directory
 *
 * List a file directory.
 *
 */

void WCMD_directory () {

char path[MAX_PATH], drive[8];
int status;
ULARGE_INTEGER avail, total, free;
CONSOLE_SCREEN_BUFFER_INFO consoleInfo;

  line_count = 5;
  byte_total = 0;
  file_total = dir_total = 0;

  /* Handle args */
  page_mode = (strstr(quals, "/P") != NULL);
  recurse = (strstr(quals, "/S") != NULL);
  wide    = (strstr(quals, "/W") != NULL);
  bare    = (strstr(quals, "/B") != NULL);

  /* Handle conflicting args and initialization */
  if (bare) wide = FALSE;

  if (wide) {
     GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &consoleInfo);
     max_width = consoleInfo.dwSize.X;
  }

  if (param1[0] == '\0') strcpy (param1, ".");
  status = GetFullPathName (param1, sizeof(path), path, NULL);
  if (!status) {
    WCMD_print_error();
    return;
  }
  lstrcpyn (drive, path, 3);

  if (!bare) {
     status = WCMD_volume (0, drive);
     if (!status) {
       return;
     }
  }

  WCMD_list_directory (path, 0);
  lstrcpyn (drive, path, 4);
  GetDiskFreeSpaceEx (drive, &avail, &total, &free);

  if (!bare) {
     if (recurse) {
       WCMD_output ("\n\n     Total files listed:\n%8d files%25s bytes\n",
            file_total, WCMD_filesize64 (byte_total));
       WCMD_output ("%8d directories %18s bytes free\n\n", 
            dir_total, WCMD_filesize64 (free.QuadPart));
     } else {
       WCMD_output (" %18s bytes free\n\n", WCMD_filesize64 (free.QuadPart));
     }
  }
}

/*****************************************************************************
 * WCMD_list_directory
 *
 * List a single file directory. This function (and those below it) can be called
 * recursively when the /S switch is used.
 *
 * FIXME: Entries sorted by name only. Should we support DIRCMD??
 * FIXME: Assumes 24-line display for the /P qualifier.
 * FIXME: Other command qualifiers not supported.
 * FIXME: DIR /S FILENAME fails if at least one matching file is not found in the top level.
 */

void WCMD_list_directory (char *search_path, int level) {

char string[1024], datestring[32], timestring[32];
char mem_err[] = "Memory Allocation Error";
char *p;
char real_path[MAX_PATH];
DWORD count;
WIN32_FIND_DATA *fd;
FILETIME ft;
SYSTEMTIME st;
HANDLE hff;
int status, dir_count, file_count, entry_count, i, widest, linesout, cur_width, tmp_width;
ULARGE_INTEGER byte_count, file_size;

  dir_count = 0;
  file_count = 0;
  entry_count = 0;
  byte_count.QuadPart = 0;
  widest = 0;
  linesout = 0;
  cur_width = 0;

/*
 *  If the path supplied does not include a wildcard, and the endpoint of the
 *  path references a directory, we need to list the *contents* of that
 *  directory not the directory file itself.
 */

  if ((strchr(search_path, '*') == NULL) && (strchr(search_path, '%') == NULL)) {
    status = GetFileAttributes (search_path);
    if ((status != -1) && (status & FILE_ATTRIBUTE_DIRECTORY)) {
      if (search_path[strlen(search_path)-1] == '\\') {
        strcat (search_path, "*");
      }
      else {
        strcat (search_path, "\\*");
      }
    }
  }

  /* Work out the actual current directory name */
  p = strrchr (search_path, '\\');
  memset(real_path, 0x00, sizeof(real_path));
  lstrcpyn (real_path, search_path, (p-search_path+2));

  /* Load all files into an in memory structure */
  fd = malloc (sizeof(WIN32_FIND_DATA));
  hff = FindFirstFile (search_path, fd);
  if (hff == INVALID_HANDLE_VALUE) {
    SetLastError (ERROR_FILE_NOT_FOUND);
    WCMD_print_error ();
    free (fd);
    return;
  }
  do {
    entry_count++;

    /* Keep running track of longest filename for wide output */
    if (wide) {
       int tmpLen = strlen((fd+(entry_count-1))->cFileName) + 3;
       if ((fd+(entry_count-1))->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) tmpLen = tmpLen + 2;
       if (tmpLen > widest) widest = tmpLen;
    }

    fd = realloc (fd, (entry_count+1)*sizeof(WIN32_FIND_DATA));
    if (fd == NULL) {
      FindClose (hff);
      WCMD_output (mem_err);
       return;
    }
  } while (FindNextFile(hff, (fd+entry_count)) != 0);
  FindClose (hff);

  /* Sort the list of files */
  qsort (fd, entry_count, sizeof(WIN32_FIND_DATA), WCMD_dir_sort);

  /* Output the results */
  if (!bare) {
     if (level != 0) WCMD_output ("\n\n");
     WCMD_output ("Directory of %s\n\n", real_path);
     if (page_mode) {
       line_count += 2;
       if (line_count > 23) {
         line_count = 0;
         WCMD_output (anykey);
         ReadFile (GetStdHandle(STD_INPUT_HANDLE), string, sizeof(string), &count, NULL);
       }
     }
  }

  for (i=0; i<entry_count; i++) {
    FileTimeToLocalFileTime (&(fd+i)->ftLastWriteTime, &ft);
    FileTimeToSystemTime (&ft, &st);
    GetDateFormat (0, DATE_SHORTDATE, &st, NULL, datestring,
      		sizeof(datestring));
    GetTimeFormat (0, TIME_NOSECONDS, &st,
      		NULL, timestring, sizeof(timestring));

    if (wide) {

      tmp_width = cur_width;
      if ((fd+i)->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
          WCMD_output ("[");
          WCMD_output ("%s", (fd+i)->cFileName);
          WCMD_output ("]");
          dir_count++;
          tmp_width = tmp_width + strlen((fd+i)->cFileName) + 2;
      } else {
          WCMD_output ("%s", (fd+i)->cFileName);
          tmp_width = tmp_width + strlen((fd+i)->cFileName) ;
          file_count++;
#ifndef NONAMELESSSTRUCT
          file_size.LowPart = (fd+i)->nFileSizeLow;
          file_size.HighPart = (fd+i)->nFileSizeHigh;
#else
          file_size.s.LowPart = (fd+i)->nFileSizeLow;
          file_size.s.HighPart = (fd+i)->nFileSizeHigh;
#endif
      byte_count.QuadPart += file_size.QuadPart;
      }
      cur_width = cur_width + widest;

      if ((cur_width + widest) > max_width) {
          WCMD_output ("\n");
          cur_width = 0;
          linesout++;
      } else {
          WCMD_output ("%*.s", (tmp_width - cur_width) ,"");
      }

    } else if ((fd+i)->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      dir_count++;

      if (!bare) {
         WCMD_output ("%8s  %8s   <DIR>        %s\n",
      	     datestring, timestring, (fd+i)->cFileName);
         linesout++;
      } else {
         if (!((strcmp((fd+i)->cFileName, ".") == 0) || 
               (strcmp((fd+i)->cFileName, "..") == 0))) {
            WCMD_output ("%s%s\n", recurse?real_path:"", (fd+i)->cFileName);
            linesout++;
         }
      }
    }
    else {
      file_count++;
#ifndef NONAMELESSSTRUCT
      file_size.LowPart = (fd+i)->nFileSizeLow;
      file_size.HighPart = (fd+i)->nFileSizeHigh;
#else
      file_size.s.LowPart = (fd+i)->nFileSizeLow;
      file_size.s.HighPart = (fd+i)->nFileSizeHigh;
#endif
      byte_count.QuadPart += file_size.QuadPart;
	  if (!bare) {
         WCMD_output ("%8s  %8s    %10s  %s\n",
     	     datestring, timestring,
	         WCMD_filesize64(file_size.QuadPart), (fd+i)->cFileName);
         linesout++;
      } else {
         WCMD_output ("%s%s\n", recurse?real_path:"", (fd+i)->cFileName);
         linesout++;
      }
    }
    if (page_mode) {
      line_count = line_count + linesout;
      linesout = 0;
      if (line_count > 23) {
        line_count = 0;
        WCMD_output (anykey);
        ReadFile (GetStdHandle(STD_INPUT_HANDLE), string, sizeof(string), &count, NULL);
      }
    }
  }

  if (wide && cur_width>0) {
      WCMD_output ("\n");
      if (page_mode) {
        if (++line_count > 23) {
          line_count = 0;
          WCMD_output (anykey);
          ReadFile (GetStdHandle(STD_INPUT_HANDLE), string, sizeof(string), &count, NULL);
        }
      }
  }

  if (!bare) {
     if (file_count == 1) {
       WCMD_output ("       1 file %25s bytes\n", WCMD_filesize64 (byte_count.QuadPart));
     }
     else {
       WCMD_output ("%8d files %24s bytes\n", file_count, WCMD_filesize64 (byte_count.QuadPart));
     }
     if (page_mode) {
       if (++line_count > 23) {
         line_count = 0;
         WCMD_output (anykey);
         ReadFile (GetStdHandle(STD_INPUT_HANDLE), string, sizeof(string), &count, NULL);
       }
     }
  }
  byte_total = byte_total + byte_count.QuadPart;
  file_total = file_total + file_count;
  dir_total = dir_total + dir_count;

  if (!bare) {
     if (dir_count == 1) WCMD_output ("1 directory         ");
     else WCMD_output ("%8d directories", dir_count);
     if (page_mode) {
       if (++line_count > 23) {
         line_count = 0;
         WCMD_output (anykey);
         ReadFile (GetStdHandle(STD_INPUT_HANDLE), string, sizeof(string), &count, NULL);
       }
     }
  }
  for (i=0; i<entry_count; i++) {
    if ((recurse) &&
          ((fd+i)->cFileName[0] != '.') &&
      	  ((fd+i)->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
#if 0
      GetFullPathName ((fd+i)->cFileName, sizeof(string), string, NULL);
#endif
      p = strrchr (search_path, '\\');
      lstrcpyn (string, search_path, (p-search_path+2));
      lstrcat (string, (fd+i)->cFileName);
      lstrcat (string, p);
      WCMD_list_directory (string, 1);
    }
  }
  free (fd);
  return;
}

/*****************************************************************************
 * WCMD_filesize64
 *
 * Convert a 64-bit number into a character string, with commas every three digits.
 * Result is returned in a static string overwritten with each call.
 * FIXME: There must be a better algorithm!
 */

char * WCMD_filesize64 (__int64 n) {

__int64 q;
int r, i;
char *p;
static char buff[32];

  p = buff;
  i = -3;
  do {
    if ((++i)%3 == 1) *p++ = ',';
    q = n / 10;
    r = n - (q * 10);
    *p++ = r + '0';
    *p = '\0';
    n = q;
  } while (n != 0);
  WCMD_strrev (buff);
  return buff;
}

/*****************************************************************************
 * WCMD_strrev
 *
 * Reverse a character string in-place (strrev() is not available under unixen :-( ).
 */

char * WCMD_strrev (char *buff) {

int r, i;
char b;

  r = lstrlen (buff);
  for (i=0; i<r/2; i++) {
    b = buff[i];
    buff[i] = buff[r-i-1];
    buff[r-i-1] = b;
  }
  return (buff);
}


int WCMD_dir_sort (const void *a, const void *b) {

  return (lstrcmpi(((WIN32_FIND_DATA *)a)->cFileName,
  	((WIN32_FIND_DATA *)b)->cFileName));
}


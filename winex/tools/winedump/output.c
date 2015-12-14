/*
 *  Code generation functions
 *
 *  Copyright 2000 Jon Griffiths
 */
#include "winedump.h"

/* Output files */
static FILE *specfile = NULL;
static FILE *hfile    = NULL;
static FILE *cfile    = NULL;

static void  output_spec_postamble (void);
static void  output_header_postamble (void);
static void  output_c_postamble (void);
static void  output_c_banner (const parsed_symbol *sym);
static const char *get_format_str (int type);
static const char *get_in_or_out (const parsed_symbol *sym, size_t arg);


/*******************************************************************
 *         output_spec_preamble
 *
 * Write the first part of the .spec file
 */
void  output_spec_preamble (void)
{
  specfile = open_file (OUTPUT_DLL_NAME, ".spec", "w");

  atexit (output_spec_postamble);

  if (VERBOSE)
    puts ("Creating .spec preamble");

  fprintf (specfile,
           "# Generated from %s by winedump\nname    %s\n"
           "type    win32\ninit    %s_Init\n\nimport kernel32.dll\n"
           "import ntdll.dll\n", globals.input_name, OUTPUT_DLL_NAME,
           OUTPUT_UC_DLL_NAME);

  if (globals.forward_dll)
    fprintf (specfile,"#import %s.dll\n", globals.forward_dll);

  fprintf (specfile, "\n\ndebug_channels (%s)\n\n", OUTPUT_DLL_NAME);
}


/*******************************************************************
 *         output_spec_symbol
 *
 * Write a symbol to the .spec file
 */
void  output_spec_symbol (const parsed_symbol *sym)
{
  char ord_spec[16];

  assert (specfile);
  assert (sym && sym->symbol);

  if (sym->ordinal >= 0)
    snprintf(ord_spec, 8, "%d", sym->ordinal);
  else
  {
    ord_spec[0] = '@';
    ord_spec[1] = '\0';
  }
  if (sym->flags & SYM_THISCALL)
    strcat (ord_spec, " -i386"); /* For binary compatibility only */

  if (!globals.do_code || !sym->function_name)
  {
    if (sym->flags & SYM_DATA)
    {
      if (globals.forward_dll)
        fprintf (specfile, "%s forward %s %s.%s #", ord_spec, sym->symbol,
                 globals.forward_dll, sym->symbol);

      fprintf (specfile, "%s extern %s %s\n", ord_spec, sym->symbol,
               sym->arg_name[0]);
      return;
    }

    if (globals.forward_dll)
      fprintf (specfile, "%s forward %s %s.%s\n", ord_spec, sym->symbol,
               globals.forward_dll, sym->symbol);
    else
      fprintf (specfile, "%s stub %s\n", ord_spec, sym->symbol);
  }
  else
  {
    unsigned int i = sym->flags & SYM_THISCALL ? 1 : 0;

    fprintf (specfile, "%s %s %s(", ord_spec, sym->varargs ? "varargs" :
             symbol_get_call_convention(sym), sym->symbol);

    for (; i < sym->argc; i++)
      fprintf (specfile, " %s", symbol_get_spec_type(sym, i));

    if (sym->argc)
      fputc (' ', specfile);
    fprintf (specfile, ") %s_%s", OUTPUT_UC_DLL_NAME, sym->function_name);

    if (sym->flags & SYM_THISCALL)
      fputs (" # __thiscall", specfile);

    fputc ('\n',specfile);
  }
}


/*******************************************************************
 *         output_spec_postamble
 *
 * Write the last part of the .spec file
 */
static void output_spec_postamble (void)
{
  if (specfile)
    fclose (specfile);
  specfile = NULL;
}


/*******************************************************************
 *         output_header_preamble
 *
 * Write the first part of the .h file
 */
void  output_header_preamble (void)
{
  hfile = open_file (OUTPUT_DLL_NAME, "_dll.h", "w");

  atexit (output_header_postamble);

  fprintf (hfile,
           "/*\n * %s.dll\n *\n * Generated from %s.dll by winedump.\n *\n"
           " * DO NOT SEND GENERATED DLLS FOR INCLUSION INTO WINE !\n * \n */"
           "\n#ifndef __WINE_%s_DLL_H\n#define __WINE_%s_DLL_H\n\n#include "
           "\"config.h\"\n#include \"windef.h\"\n#include \"wine/debug.h\"\n"
           "#include \"winbase.h\"\n#include \"winnt.h\"\n\n\n",
           OUTPUT_DLL_NAME, OUTPUT_DLL_NAME, OUTPUT_UC_DLL_NAME,
           OUTPUT_UC_DLL_NAME);
}


/*******************************************************************
 *         output_header_symbol
 *
 * Write a symbol to the .h file
 */
void  output_header_symbol (const parsed_symbol *sym)
{
  assert (hfile);
  assert (sym && sym->symbol);

  if (!globals.do_code)
    return;

  if (sym->flags & SYM_DATA)
    return;

  if (!sym->function_name)
    fprintf (hfile, "/* __%s %s_%s(); */\n", symbol_get_call_convention(sym),
             OUTPUT_UC_DLL_NAME, sym->symbol);
  else
  {
    output_prototype (hfile, sym);
    fputs (";\n", hfile);
  }
}


/*******************************************************************
 *         output_header_postamble
 *
 * Write the last part of the .h file
 */
static void output_header_postamble (void)
{
  if (hfile)
  {
    fprintf (hfile, "\n\n\n#endif\t/* __WINE_%s_DLL_H */\n",
             OUTPUT_UC_DLL_NAME);
    fclose (hfile);
    hfile = NULL;
  }
}


/*******************************************************************
 *         output_c_preamble
 *
 * Write the first part of the .c file
 */
void  output_c_preamble (void)
{
  cfile = open_file (OUTPUT_DLL_NAME, "_main.c", "w");

  atexit (output_c_postamble);

  fprintf (cfile,
           "/*\n * %s.dll\n *\n * Generated from %s by winedump.\n *\n"
           " * DO NOT SUBMIT GENERATED DLLS FOR INCLUSION INTO WINE!\n * \n */"
           "\n\n#include \"%s_dll.h\"\n\nWINE_DEFAULT_DEBUG_CHANNEL(%s);\n\n",
           OUTPUT_DLL_NAME, globals.input_name, OUTPUT_DLL_NAME,
           OUTPUT_DLL_NAME);

  if (globals.forward_dll)
  {
    if (VERBOSE)
      puts ("Creating a forwarding DLL");

    fputs ("\nHMODULE hDLL=0;\t/* DLL to call */\n\n", cfile);
  }

  fputs ("#ifdef __i386__\n#define GET_THIS(t,p) t p;\\\n__asm__ __volatile__"
         " (\"movl %%ecx, %0\" : \"=m\" (p))\n#endif\n\n\n", cfile);

  fprintf (cfile,
           "BOOL WINAPI %s_Init(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID "
           "lpvReserved)\n{\n\tTRACE(\"(0x%%08x, %%ld, %%p)\\n\",hinstDLL,"
           "fdwReason,lpvReserved);\n\n\t"
           "if (fdwReason == DLL_PROCESS_ATTACH)\n\t{\n\t\t",
           OUTPUT_UC_DLL_NAME);

  if (globals.forward_dll)
  {
    fprintf (cfile,
             "hDLL = LoadLibraryA( \"%s\" );\n\t\t"
             "TRACE(\":Forwarding DLL (%s) loaded (%%ld)\\n\",(LONG)hDLL);",
             globals.forward_dll, globals.forward_dll);
  }
  else
    fputs ("/* FIXME: Initialisation */", cfile);

  fputs ("\n\t}\n\telse if (fdwReason == DLL_PROCESS_DETACH)\n\t{\n\t\t",
         cfile);

  if (globals.forward_dll)
  {
    fprintf (cfile,
             "FreeLibrary( hDLL );\n\t\tTRACE(\":Forwarding DLL (%s)"
             " freed\\n\");", globals.forward_dll);
  }
  else
    fputs ("/* FIXME: Cleanup */", cfile);

  fputs ("\n\t}\n\n\treturn TRUE;\n}\n\n\n", cfile);
}


#define CPP_END  if (sym->flags & SYM_THISCALL) \
  fputs ("#endif\n", cfile); fputs ("\n\n", cfile)
#define GET_THIS if (sym->flags & SYM_THISCALL) \
  fprintf (cfile, "\tGET_THIS(%s,%s);\n", sym->arg_text[0],sym->arg_name[0])

/*******************************************************************
 *         output_c_symbol
 *
 * Write a symbol to the .c file
 */
void  output_c_symbol (const parsed_symbol *sym)
{
  unsigned int i, start = sym->flags & SYM_THISCALL ? 1 : 0;
  int is_void;

  assert (cfile);
  assert (sym && sym->symbol);

  if (!globals.do_code)
    return;

  if (sym->flags & SYM_DATA)
  {
    fprintf (cfile, "/* FIXME: Move to top of file */\n%s;\n\n",
             sym->arg_text[0]);
    return;
  }

  if (sym->flags & SYM_THISCALL)
    fputs ("#ifdef __i386__\n", cfile);

  output_c_banner(sym);

  if (!sym->function_name)
  {
    /* #ifdef'd dummy */
    fprintf (cfile, "#if 0\n__%s %s_%s()\n{\n\t/* %s in .spec */\n}\n#endif\n",
             symbol_get_call_convention(sym), OUTPUT_UC_DLL_NAME, sym->symbol,
             globals.forward_dll ? "@forward" : "@stub");
    CPP_END;
    return;
  }

  is_void = !strcmp (sym->return_text, "void");

  output_prototype (cfile, sym);
  fputs ("\n{\n", cfile);

  if (!globals.do_trace)
  {
    GET_THIS;
    fputs ("\tFIXME(\":stub\\n\");\n", cfile);
    if (!is_void)
        fprintf (cfile, "\treturn (%s) 0;\n", sym->return_text);
    fputs ("}\n", cfile);
    CPP_END;
    return;
  }

  /* Tracing, maybe forwarding as well */
  if (globals.forward_dll)
  {
    /* Write variables for calling */
    if (sym->varargs)
      fputs("\tva_list valist;\n", cfile);

    fprintf (cfile, "\t%s (__%s *pFunc)(", sym->return_text,
             symbol_get_call_convention(sym));

    for (i = start; i < sym->argc; i++)
      fprintf (cfile, "%s%s", i > start ? ", " : "", sym->arg_text [i]);

    fprintf (cfile, "%s);\n", sym->varargs ? ",..." : sym->argc == 1 &&
             sym->flags & SYM_THISCALL ? "" : sym->argc ? "" : "void");

    if (!is_void)
      fprintf (cfile, "\t%s retVal;\n", sym->return_text);

    GET_THIS;

    fprintf (cfile, "\tpFunc=(void*)GetProcAddress(hDLL,\"%s\");\n",
             sym->symbol);
  }

  /* TRACE input arguments */
  fprintf (cfile, "\tTRACE(\"(%s", !sym->argc ? "void" : "");

  for (i = 0; i < sym->argc; i++)
    fprintf (cfile, "%s(%s)%s", i ? "," : "", sym->arg_text [i],
             get_format_str (sym->arg_type [i]));

  fprintf (cfile, "%s): %s\\n\"", sym->varargs ? ",..." : "",
           globals.forward_dll ? "forward" : "stub");

  for (i = 0; i < sym->argc; i++)
    if (sym->arg_type[i] != ARG_STRUCT)
      fprintf(cfile, ",%s%s%s%s", sym->arg_type[i] == ARG_LONG ? "(LONG)" : "",
              sym->arg_type[i] == ARG_WIDE_STRING ? "debugstr_w(" : "",
              sym->arg_name[i],
              sym->arg_type[i] == ARG_WIDE_STRING ? ")" : "");

  fputs (");\n", cfile);

  /* native system library forwarding */
  if (globals.forward_native)
  {
    if (!is_void)
      fprintf (cfile, "\treturn ");
    else 
      fprintf (cfile, "\t");
      
    fprintf (cfile, "%s(", sym->symbol);
    

    for (i = 0; i < sym->argc; i++)
      fprintf (cfile, "%s%s", i ? "," : "", sym->arg_name [i]);
      
    fprintf (cfile, ");\n");

    fputs ("}\n", cfile);
    CPP_END;
    return;
  }

  if (!globals.forward_dll)
  {
    if (!is_void)
      fprintf (cfile, "\treturn (%s) 0;\n", sym->return_text);
    fputs ("}\n", cfile);
    CPP_END;
    return;
  }

  /* Call the DLL */
  if (sym->varargs)
    fprintf (cfile, "\tva_start(valist,%s);\n", sym->arg_name[sym->argc-1]);

  fprintf (cfile, "\t%spFunc(", !is_void ? "retVal = " : "");

  for (i = 0; i < sym->argc; i++)
    fprintf (cfile, "%s%s", i ? "," : "", sym->arg_name [i]);

  fputs (sym->varargs ? ",valist);\n\tva_end(valist);" : ");", cfile);

  /* TRACE return value */
  fprintf (cfile, "\n\tTRACE(\"Returned (%s)\\n\"",
           get_format_str (sym->return_type));

  if (!is_void)
  {
    if (sym->return_type == ARG_WIDE_STRING)
      fputs (",debugstr_w(retVal)", cfile);
    else
      fprintf (cfile, ",%s%s", sym->return_type == ARG_LONG ? "(LONG)" : "",
               sym->return_type == ARG_STRUCT ? "" : "retVal");
    fputs (");\n\treturn retVal;\n", cfile);
  }
  else
    fputs (");\n", cfile);

  fputs ("}\n", cfile);
  CPP_END;
}


/*******************************************************************
 *         output_c_postamble
 *
 * Write the last part of the .c file
 */
static void output_c_postamble (void)
{
  if (cfile)
    fclose (cfile);
  cfile = NULL;
}


/*******************************************************************
 *         output_makefile
 *
 * Write a Wine compatible makefile.in
 */
void  output_makefile (void)
{
  FILE *makefile = open_file ("Makefile", ".in", "w");

  if (VERBOSE)
    puts ("Creating makefile");

  fprintf (makefile,
           "# Generated from %s by winedump.\nTOPSRCDIR = @top_srcdir@\n"
           "TOPOBJDIR = ../..\nSRCDIR    = @srcdir@\nVPATH     = @srcdir@\n"
           "MODULE    = %s\nEXTRALIBS = $(LIBUNICODE)\n\n"
           "LDDLLFLAGS = @LDDLLFLAGS@\nSYMBOLFILE = $(MODULE).tmp.o\n\n"
           "C_SRCS = \\\n\t%s_main.c\n\n@MAKE_DLL_RULES@\n\n### Dependencies:",
           globals.input_name, OUTPUT_DLL_NAME, OUTPUT_DLL_NAME);

  fclose (makefile);
}


/*******************************************************************
 *         output_install_script
 *
 * Write a script to insert the DLL into Wine
 *
 * Rather than using diff/patch, several sed calls are generated
 * so the script can be re-run at any time without breaking.
 */
void  output_install_script (void)
{
  char cmd[128];
  FILE *install_file = open_file (OUTPUT_DLL_NAME, "_install", "w");

  if (VERBOSE)
    puts ("Creating install script");

  fprintf (install_file,
           "#!/bin/bash\n# Generated from %s.dll by winedump.\n\n"
           "if [ $# -ne 1 ] || [ ! -d $1 ] || [ ! -f"
           " $1/AUTHORS ]; then\n\t[ $# -eq 1 ] && echo \"Invalid path\"\n"
           "\techo \"Usage: $0 wine-base-dir\"\n\texit 1\nfi\n\n"
           "if [ -d $1/dlls/%s ]; then\n\techo \"DLL is already present\"\n"
           "\texit 1\nfi\n\necho Adding DLL %s to Wine build tree...\n"
           "echo\n\nmkdir $1/dlls/%s\ncp %s.spec $1/dlls/%s\n"
           "cp %s_main.c $1/dlls/%s\ncp %s_dll.h $1/dlls/%s\n"
           "cp Makefile.in $1/dlls/%s/Makefile.in\necho Copied DLL files\n\n"
           "cd $1\n\nsed '/dlls\\/"
           "x11drv\\/Makefile/{G;s/$/dlls\\/%s\\/Makefile/;}' configure.in"
           " >t.tmp\nmv -f t.tmp configure.in\necho Patched configure.in\n\n"
           "sed '/all:/{G;s/$/\\^lib%s.so \\\\/;}'"
           " dlls/Makefile.in| tr ^ \\\\t >t.tmp\n"
           "sed '/SUBDIRS =/{G;s/$/\\^%s \\\\/;}' t.tmp | tr ^ \\\\t >t.tmp2"
           "\nsed '/Map library name /{G;s/$/^\\$(RM) \\$\\@ \\&\\& \\$\\"
           "(LN_S\\) %s\\/lib%s.\\$(LIBEXT) \\$\\@/;}' t.tmp2 | tr ^ \\\\t"
           " > t.tmp\nsed '/Map library name /{G;s/$/lib%s.\\$(LIBEXT): "
           "%s\\/lib%s.\\$(LIBEXT)/;}' t.tmp > t.tmp2\nsed '/dll "
           "dependencies/{G;s/$/^\\@cd %s \\&\\& \\$(MAKE) lib%s.\\$(LIBEXT)"
           "/;}' t.tmp2 | tr ^ \\\\t > t.tmp\nsed '/dll "
           "dependencies/{G;s/$/%s\\/lib%s.\\$(LIBEXT)\\: libkernel32."
           "\\$(LIBEXT) libntdll.\\$(LIBEXT)/;}' t.tmp > t.tmp2\n"
	   "mv -f t.tmp2 dlls/Makefile.in\nrm -f t.tmp\necho Patched dlls/"
           "Makefile.in\n\necho\necho ...done.\necho Run \\'autoconf\\', "
           "\\'./configure\\' then \\'make\\' to rebuild Wine\n\n",
           OUTPUT_DLL_NAME, OUTPUT_DLL_NAME, OUTPUT_DLL_NAME, OUTPUT_DLL_NAME,
           OUTPUT_DLL_NAME, OUTPUT_DLL_NAME, OUTPUT_DLL_NAME, OUTPUT_DLL_NAME,
           OUTPUT_DLL_NAME, OUTPUT_DLL_NAME, OUTPUT_DLL_NAME, OUTPUT_DLL_NAME,
           OUTPUT_DLL_NAME, OUTPUT_DLL_NAME, OUTPUT_DLL_NAME, OUTPUT_DLL_NAME,
           OUTPUT_DLL_NAME, OUTPUT_DLL_NAME, OUTPUT_DLL_NAME, OUTPUT_DLL_NAME,
           OUTPUT_DLL_NAME, OUTPUT_DLL_NAME, OUTPUT_DLL_NAME);

  fclose (install_file);
  snprintf (cmd, sizeof (cmd), "chmod a+x %s_install", OUTPUT_DLL_NAME);
  system (cmd);
}


/*******************************************************************
 *         output_prototype
 *
 * Write a C prototype for a parsed symbol
 */
void  output_prototype (FILE *file, const parsed_symbol *sym)
{
  unsigned int i, start = sym->flags & SYM_THISCALL ? 1 : 0;

  fprintf (file, "%s __%s %s_%s(", sym->return_text, symbol_get_call_convention(sym),
           OUTPUT_UC_DLL_NAME, sym->function_name);

  if (!sym->argc || (sym->argc == 1 && sym->flags & SYM_THISCALL))
    fputs ("void", file);
  else
    for (i = start; i < sym->argc; i++)
      fprintf (file, "%s%s %s", i > start ? ", " : "", sym->arg_text [i],
               sym->arg_name [i]);
  if (sym->varargs)
    fputs (", ...", file);
  fputc (')', file);
}


/*******************************************************************
 *         output_c_banner
 *
 * Write a function banner to the .c file
 */
void  output_c_banner (const parsed_symbol *sym)
{
  char ord_spec[16];
  size_t i;

  if (sym->ordinal >= 0)
    snprintf(ord_spec, sizeof (ord_spec), "%d", sym->ordinal);
  else
  {
    ord_spec[0] = '@';
    ord_spec[1] = '\0';
  }

  fprintf (cfile, "/*********************************************************"
           "*********\n *\t\t%s (%s.%s)\n *\n", sym->symbol,
           OUTPUT_UC_DLL_NAME, ord_spec);

  if (globals.do_documentation && sym->function_name)
  {
    fputs (" *\n * PARAMS\n *\n", cfile);

    if (!sym->argc)
      fputs (" *  None.\n *\n", cfile);
    else
    {
      for (i = 0; i < sym->argc; i++)
        fprintf (cfile, " *  %s [%s]%s\n", sym->arg_name [i],
                 get_in_or_out(sym, i),
                 strcmp (sym->arg_name [i], "_this") ? "" :
                 "     Pointer to the class object (in ECX)");

      if (sym->varargs)
        fputs (" *  ...[I]\n", cfile);
      fputs (" *\n", cfile);
    }

    fputs (" * RETURNS\n *\n", cfile);

    if (sym->return_text && !strcmp (sym->return_text, "void"))
      fputs (" *  Nothing.\n", cfile);
    else
      fprintf (cfile, " *  %s\n", sym->return_text);
  }
  fputs (" *\n */\n", cfile);
}


/*******************************************************************
 *         get_format_str
 *
 * Get a string containing the correct format string for a type
 */
static const char *get_format_str (int type)
{
  switch (type)
  {
  case ARG_VOID:        return "void";
  case ARG_FLOAT:       return "%f";
  case ARG_DOUBLE:      return "%g";
  case ARG_POINTER:     return "%p";
  case ARG_WIDE_STRING:
  case ARG_STRING:      return "%s";
  case ARG_LONG:        return "%ld";
  case ARG_STRUCT:      return "struct";
  }
  assert (0);
  return "";
}


/*******************************************************************
 *         get_in_or_out
 *
 * Determine if a parameter is In or In/Out
 */
static const char *get_in_or_out (const parsed_symbol *sym, size_t arg)
{
  assert (sym && arg < sym->argc);
  assert (globals.do_documentation);

  if (sym->arg_flag [arg] & CT_CONST)
    return "In";

  switch (sym->arg_type [arg])
  {
  case ARG_FLOAT:
  case ARG_DOUBLE:
  case ARG_LONG:
  case ARG_STRUCT:      return "In";
  case ARG_POINTER:
  case ARG_WIDE_STRING:
  case ARG_STRING:      return "In/Out";
  }
  assert (0);
  return "";
}

/*
 *  Option processing and main()
 *
 *  Copyright 2000 Jon Griffiths
 */
#include "winedump.h"


_globals globals; /* All global variables */


static void do_include (const char *arg)
{
  globals.directory = arg;
  globals.do_code = 1;
}


static inline const char* strip_ext (const char *str)
{
  char *ext = strstr(str, ".dll");
  if (ext)
    return str_substring (str, ext);
  else
    return strdup (str);
}


static void do_name (const char *arg)
{
  globals.dll_name = strip_ext (arg);
}


static void do_spec (const char *arg)
{
    if (globals.mode != NONE) fatal("Only one mode can be specified\n");
    globals.mode = SPEC;
}


static void do_demangle (const char *arg)
{
    if (globals.mode != NONE) fatal("Only one mode can be specified\n");
    globals.mode = DMGL;
    globals.do_code = 1;
}


static void do_dump (const char *arg)
{
    if (globals.mode != NONE) fatal("Only one mode can be specified\n");
    globals.mode = DUMP;
    globals.do_code = 1;
}


static void do_code (void)
{
  globals.do_code = 1;
}


static void do_trace (void)
{
  globals.do_trace = 1;
  globals.do_code = 1;
}


static void do_forward (const char *arg)
{
  globals.forward_dll = arg;
  globals.do_trace = 1;
  globals.do_code = 1;
}

static void do_native (void)
{
  globals.forward_native = 1;
  globals.do_trace = 1;
  globals.do_code = 1;
}


static void do_document (void)
{
  globals.do_documentation = 1;
}

static void do_cdecl (void)
{
  globals.do_cdecl = 1;
}


static void do_quiet (void)
{
  globals.do_quiet = 1;
}


static void do_start (const char *arg)
{
  globals.start_ordinal = atoi (arg);
  if (!globals.start_ordinal)
    fatal ("Invalid -s option (must be numeric)");
}


static void do_end (const char *arg)
{
  globals.end_ordinal = atoi (arg);
  if (!globals.end_ordinal)
    fatal ("Invalid -e option (must be numeric)");
}


static void do_verbose (void)
{
  globals.do_verbose = 1;
}


static void do_symdmngl (void)
{
    globals.do_demangle = 1;
}

static void do_dumphead (void)
{
    globals.do_dumpheader = 1;
}

static void do_dumpsect (const char* arg)
{
    globals.dumpsect = arg;
}

static void do_dumpall(void)
{
    globals.do_dumpheader = 1;
    globals.dumpsect = "ALL";
}

struct option
{
  const char *name;
  Mode mode;
  int   has_arg;
  void  (*func) ();
  const char *usage;
};

static const struct option option_table[] = {
  {"-h",    NONE, 0, do_usage,    "-h           Display this help message"},
  {"sym",   DMGL, 0, do_demangle, "sym <sym>    Demangle C++ symbol <sym> and exit"},
  {"spec",  SPEC, 0, do_spec,     "spec <dll>   Use dll for input file and generate implementation code"},
  {"-I",    SPEC, 1, do_include,  "-I dir       Look for prototypes in 'dir' (implies -c)"},
  {"-c",    SPEC, 0, do_code,     "-c           Generate skeleton code (requires -I)"},
  {"-t",    SPEC, 0, do_trace,    "-t           TRACE arguments (implies -c)"},
  {"-f",    SPEC, 1, do_forward,  "-f dll       Forward calls to 'dll' (implies -t)"},
  {"-n",    SPEC, 0, do_native,   "-n           Forward calls to native system library (pass through!) (implies -t)"},
  {"-D",    SPEC, 0, do_document, "-D           Generate documentation"},
  {"-o",    SPEC, 1, do_name,     "-o name      Set the output dll name (default: dll)"},
  {"-C",    SPEC, 0, do_cdecl,    "-C           Assume __cdecl calls (default: __stdcall)"},
  {"-s",    SPEC, 1, do_start,    "-s num       Start prototype search after symbol 'num'"},
  {"-e",    SPEC, 1, do_end,      "-e num       End prototype search after symbol 'num'"},
  {"-q",    SPEC, 0, do_quiet,    "-q           Don't show progress (quiet)."},
  {"-v",    SPEC, 0, do_verbose,  "-v           Show lots of detail while working (verbose)."},
  {"dump",  DUMP, 0, do_dump,     "dump <mod>   Dumps the content of the module (dll, exe...) named <mod>"},
  {"-C",    DUMP, 0, do_symdmngl, "-C           Turns on symbol demangling"},
  {"-f",    DUMP, 0, do_dumphead, "-f           Dumps file header information"},
  {"-j",    DUMP, 1, do_dumpsect, "-j sect_name Dumps only the content of section sect_name (import, export, debug)"},
  {"-x",    DUMP, 0, do_dumpall,  "-x           Dumps everything"},
  {NULL,    NONE, 0, NULL,        NULL}
};

void do_usage (void)
{
    const struct option *opt;
    printf ("Usage: winedump [-h | sym <sym> | spec <dll> | dump <dll>]\n");
    printf ("Mode options (can be put as the mode (sym/spec/dump...) is declared):\n");
    printf ("\tWhen used in -h mode\n");
    for (opt = option_table; opt->name; opt++)
	if (opt->mode == NONE)
	    printf ("\t   %s\n", opt->usage);
    printf ("\tWhen used in sym mode\n");
    for (opt = option_table; opt->name; opt++)
	if (opt->mode == DMGL)
	    printf ("\t   %s\n", opt->usage);
    printf ("\tWhen used in spec mode\n");
    for (opt = option_table; opt->name; opt++)
	if (opt->mode == SPEC)
	    printf ("\t   %s\n", opt->usage);
    printf ("\tWhen used in dump mode\n");
    for (opt = option_table; opt->name; opt++)
	if (opt->mode == DUMP)
	    printf ("\t   %s\n", opt->usage);

    puts ("\n");
    exit (1);
}


/*******************************************************************
 *          parse_options
 *
 * Parse options from the argv array
 */
static void parse_options (char *argv[])
{
  const struct option *opt;
  char *const *ptr;
  const char *arg = NULL;

  ptr = argv + 1;

  while (*ptr != NULL)
  {
    for (opt = option_table; opt->name; opt++)
    {
     if (globals.mode != NONE && opt->mode != NONE && globals.mode != opt->mode)
       continue;
     if (((opt->has_arg == 1) && !strncmp (*ptr, opt->name, strlen (opt->name))) ||
	 ((opt->has_arg == 2) && !strcmp (*ptr, opt->name)))
      {
        arg = *ptr + strlen (opt->name);
        if (*arg == '\0') arg = *++ptr;
        break;
      }
      if (!strcmp (*ptr, opt->name))
      {
        arg = NULL;
        break;
      }
    }

    if (!opt->name)
    {
        if ((*ptr)[0] == '-')
            fatal ("Unrecognized option");
        if (globals.input_name != NULL)
            fatal ("Only one file can be treated at once");
        globals.input_name = *ptr;
    }
    else if (opt->has_arg && arg != NULL)
	opt->func (arg);
    else
	opt->func ("");

    ptr++;
  }

  if (globals.mode == SPEC && globals.do_code && !globals.directory)
    fatal ("-I must be used if generating code");

  if (VERBOSE && QUIET)
    fatal ("Options -v and -q are mutually exclusive");
}

static void set_module_name(unsigned setUC)
{
    const char*	ptr;
    char*	buf;
    int		len;

    /* FIXME: we shouldn't assume all module extensions are .dll in winedump
     * in some cases, we could have some .drv for example
     */
    /* get module name from name */
    if ((ptr = strrchr (globals.input_name, '/')))
	ptr++;
    else
	ptr = globals.input_name;
    len = strlen(ptr);
    if (len > 4 && strcmp(ptr + len - 4, ".dll") == 0)
	len -= 4;
    buf = malloc(len + 1);
    memcpy(buf, (void*)ptr, len);
    buf[len] = 0;
    globals.input_module = buf;
    OUTPUT_UC_DLL_NAME = (setUC) ? str_toupper( strdup (OUTPUT_DLL_NAME)) : "";
}

/*******************************************************************
 *         main
 */
#ifdef __GNUC__
int   main (int argc __attribute__((unused)), char *argv[])
#else
int   main (int argc, char *argv[])
#endif
{
    parsed_symbol symbol;
    int count = 0;

    globals.mode = NONE;
    globals.forward_dll = NULL;
    globals.forward_native = NULL;
    globals.input_name = NULL;

    parse_options (argv);

    memset (&symbol, 0, sizeof (parsed_symbol));

    switch (globals.mode)
    {
    case DMGL:
	globals.uc_dll_name = "";
	VERBOSE = 1;

	symbol_init (&symbol, globals.input_name);
	globals.input_module = "";
	if (symbol_demangle (&symbol) == -1)
	    fatal( "Symbol hasn't got a mangled name\n");
	if (symbol.flags & SYM_DATA)
	    printf (symbol.arg_text[0]);
	else
	    output_prototype (stdout, &symbol);
	fputc ('\n', stdout);
	symbol_clear(&symbol);
	break;

    case SPEC:
        if (globals.input_name == NULL)
            fatal("No file name has been given\n");
	set_module_name(1);
	dll_open (globals.input_name);

	output_spec_preamble ();
	output_header_preamble ();
	output_c_preamble ();

	while (!dll_next_symbol (&symbol))
	{
	    count++;

	    if (NORMAL)
		printf ("Export %3d - '%s' ...%c", count, symbol.symbol,
			VERBOSE ? '\n' : ' ');

	    if (globals.do_code && count >= globals.start_ordinal
		&& (!globals.end_ordinal || count <= globals.end_ordinal))
	    {
		/* Attempt to get information about the symbol */
		int result = symbol_demangle (&symbol);

		if (result)
		    result = symbol_search (&symbol);

		if (!result && symbol.function_name)
		    /* Clean up the prototype */
		    symbol_clean_string (symbol.function_name);

		if (NORMAL)
		    puts (result ? "[Not Found]" : "[OK]");
	    }
	    else if (NORMAL)
		puts ("[Ignoring]");

	    output_spec_symbol (&symbol);
	    output_header_symbol (&symbol);
	    output_c_symbol (&symbol);

	    symbol_clear (&symbol);
	}

	output_makefile ();
	output_install_script ();

	if (VERBOSE)
	    puts ("Finished, Cleaning up...");
	break;
    case NONE:
	do_usage();
	break;
    case DUMP:
        if (globals.input_name == NULL)
            fatal("No file name has been given\n");
	set_module_name(0);
	dump_file(globals.input_name);
	break;
    }

    return 0;
}

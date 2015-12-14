/*
 * What processor?
 *
 * Copyright 1995,1997 Morten Welinder
 * Copyright 1997-1998 Marcus Meissner
 */

#include "config.h"
#include "wine/port.h"

#include <ctype.h>
#include <string.h>
#include <stdio.h>

#ifdef __APPLE__
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include "winbase.h"
#include "winreg.h"
#include "winnt.h"
#include "winerror.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(reg);

static BYTE PF[64] = {0,};
static BOOL cache = FALSE;
static SYSTEM_INFO cachedsi;

void cache_system_info();


/***********************************************************************
 * 			GetSystemInfo            	[KERNEL32.@]
 *
 * Gets the current system information.
 *
 * On the first call it creates cached values, so it doesn't have to determine
 * them repeatedly. On Linux, the /proc/cpuinfo special file is used.
 *
 * It creates a registry subhierarchy, looking like:
 * \HARDWARE\DESCRIPTION\System\CentralProcessor\<processornumber>\
 *							Identifier (CPU x86)
 * Note that there is a hierarchy for every processor installed, so this
 * supports multiprocessor systems. This is done like Win95 does it, I think.
 *
 * It also creates a cached flag array for IsProcessorFeaturePresent().
 *
 * No NULL ptr check for LPSYSTEM_INFO in Win9x.
 *
 * RETURNS
 *	nothing, really
 */

VOID WINAPI GetSystemInfo(
	LPSYSTEM_INFO si	/* [out] system information */
) {
	if (!cache)
      cache_system_info();
    
    memcpy(si,&cachedsi,sizeof(*si));
    return;
}

void cache_system_info()
{
	HKEY	xhkey=0,hkey;

	memset(PF,0,sizeof(PF));

	/* choose sensible defaults ...
	 * FIXME: perhaps overrideable with precompiler flags?
	 */
	cachedsi.u.s.wProcessorArchitecture     = PROCESSOR_ARCHITECTURE_INTEL;
	cachedsi.dwPageSize 			= getpagesize();

	/* FIXME: the two entries below should be computed somehow... */
	cachedsi.lpMinimumApplicationAddress	= (void *)0x00010000;
	cachedsi.lpMaximumApplicationAddress	= (void *)0x7FFFFFFF;
	cachedsi.dwActiveProcessorMask		= 1;
	cachedsi.dwNumberOfProcessors		= 1;
	cachedsi.dwProcessorType		= PROCESSOR_INTEL_PENTIUM;
	cachedsi.dwAllocationGranularity	= 0x10000;
	cachedsi.wProcessorLevel		= 5; /* 586 */
	cachedsi.wProcessorRevision		= 0;

	cache = TRUE; /* even if there is no more info, we now have a cacheentry */

	/* Hmm, reasonable processor feature defaults? */

        /* Create these registry keys for all systems
	 * FPU entry is often empty on Windows, so we don't care either */
	if ( (RegCreateKeyA(HKEY_LOCAL_MACHINE,"HARDWARE\\DESCRIPTION\\System\\FloatingPointProcessor",&hkey)!=ERROR_SUCCESS)
	  || (RegCreateKeyA(HKEY_LOCAL_MACHINE,"HARDWARE\\DESCRIPTION\\System\\CentralProcessor",&hkey)!=ERROR_SUCCESS) )
	{
            WARN("Unable to write FPU/CPU info to registry\n");
        }

#ifdef linux
	{
	char buf[20];
	char line[200];
	FILE *f = fopen ("/proc/cpuinfo", "r");

	if (!f)
		return;
        xhkey = 0;
	while (fgets(line,200,f)!=NULL) {
		char	*s,*value;

		/* NOTE: the ':' is the only character we can rely on */
		if (!(value = strchr(line,':')))
			continue;
		/* terminate the valuename */
		*value++ = '\0';
		/* skip any leading spaces */
		while (*value==' ') value++;
		if ((s=strchr(value,'\n')))
			*s='\0';

                if (!strncasecmp (line, "cpu MHz", strlen ("cpu MHz")))
                {
                   float f;

                   if (sscanf (value, "%f", &f))
                   {
                      DWORD iVal = (DWORD)f;
                      RegSetValueExA (xhkey, "~MHz", 0, REG_DWORD,
                                      (BYTE *)&iVal, 4);
                   }
                   continue;
                }

		/* 2.1 method */
		if (!strncasecmp(line, "cpu family",strlen("cpu family"))) {
			if (isdigit (value[0])) {
				switch (value[0] - '0') {
				case 3: cachedsi.dwProcessorType = PROCESSOR_INTEL_386;
					cachedsi.wProcessorLevel= 3;
					break;
				case 4: cachedsi.dwProcessorType = PROCESSOR_INTEL_486;
					cachedsi.wProcessorLevel= 4;
					break;
				case 5:
				case 6: /* PPro/2/3 has same info as P1 */
					cachedsi.dwProcessorType = PROCESSOR_INTEL_PENTIUM;
					cachedsi.wProcessorLevel= 5;
					break;
				case 1: /* two-figure levels */
                                    if (value[1] == '5')
                                    {
                                        cachedsi.dwProcessorType = PROCESSOR_INTEL_PENTIUM;
                                        cachedsi.wProcessorLevel= 5;
                                        break;
                                    }
                                    /* fall through */
				default:
					FIXME("unknown cpu family '%s', please report ! (-> setting to 386)\n", value);
					break;
				}
			}
			/* set the CPU type of the current processor */
			sprintf(buf,"CPU %ld",cachedsi.dwProcessorType);
			if (xhkey)
				RegSetValueExA(xhkey,"Identifier",0,REG_SZ,(BYTE *)buf,
					strlen(buf));
			continue;
		}
		/* old 2.0 method */
		if (!strncasecmp(line, "cpu",strlen("cpu"))) {
			if (	isdigit (value[0]) && value[1] == '8' &&
				value[2] == '6' && value[3] == 0
			) {
				switch (value[0] - '0') {
				case 3: cachedsi.dwProcessorType = PROCESSOR_INTEL_386;
					cachedsi.wProcessorLevel= 3;
					break;
				case 4: cachedsi.dwProcessorType = PROCESSOR_INTEL_486;
					cachedsi.wProcessorLevel= 4;
					break;
				case 5:
				case 6: /* PPro/2/3 has same info as P1 */
					cachedsi.dwProcessorType = PROCESSOR_INTEL_PENTIUM;
					cachedsi.wProcessorLevel= 5;
					break;
				default:
					FIXME("unknown Linux 2.0 cpu family '%s', please report ! (-> setting to 386)\n", value);
					break;
				}
			}
			/* set the CPU type of the current processor
			 * FIXME: someone reported P4 as being set to
			 * "              Intel(R) Pentium(R) 4 CPU 1500MHz"
			 * Do we need to do the same ?
			 * */
			sprintf(buf,"CPU %ld",cachedsi.dwProcessorType);
			if (xhkey)
				RegSetValueExA(xhkey,"Identifier",0,REG_SZ,(BYTE *)buf,
					strlen(buf));
			continue;
		}
		if (!strncasecmp(line,"fdiv_bug",strlen("fdiv_bug"))) {
			if (!strncasecmp(value,"yes",3))
				PF[PF_FLOATING_POINT_PRECISION_ERRATA] = TRUE;

			continue;
		}
		if (!strncasecmp(line,"fpu",strlen("fpu"))) {
			if (!strncasecmp(value,"no",2))
				PF[PF_FLOATING_POINT_EMULATED] = TRUE;

			continue;
		}
		if (!strncasecmp(line,"processor",strlen("processor"))) {
			/* processor number counts up... */
			unsigned int x;

			if (sscanf(value,"%d",&x))
				if (x+1>cachedsi.dwNumberOfProcessors)
					cachedsi.dwNumberOfProcessors=x+1;

			/* Create a new processor subkey on a multiprocessor
			 * system
			 */
			sprintf(buf,"%d",x);
			if (xhkey)
				RegCloseKey(xhkey);
			RegCreateKeyA(hkey,buf,&xhkey);
		}
		if (!strncasecmp(line,"stepping",strlen("stepping"))) {
			int	x;

			if (sscanf(value,"%d",&x))
				cachedsi.wProcessorRevision = x;
		}
		if (	!strncasecmp(line,"flags",strlen("flags"))	||
			!strncasecmp(line,"features",strlen("features"))
		) {
			if (strstr(value,"cx8"))
				PF[PF_COMPARE_EXCHANGE_DOUBLE] = TRUE;
			if (strstr(value,"mmx"))
				PF[PF_MMX_INSTRUCTIONS_AVAILABLE] = TRUE;
			if (strstr(value,"tsc"))
				PF[PF_RDTSC_INSTRUCTION_AVAILABLE] = TRUE;
			if (strstr(value,"3dnow"))
				PF[PF_AMD3D_INSTRUCTIONS_AVAILABLE] = TRUE;
			if (strstr(value,"sse"))
				PF[PF_XMMI_INSTRUCTIONS_AVAILABLE] = TRUE;
			if (strstr(value,"sse2"))
				PF[PF_XMMI64_INSTRUCTIONS_AVAILABLE] = TRUE;
			if (strstr(value,"sse3"))
				PF[PF_SSE3_INSTRUCTIONS_AVAILABLE] = TRUE;

		}
	}
	fclose (f);
	}
#elif defined(__APPLE__)
   {
      int iVal, i;
      size_t ValSize;
      char sVal[256];
      long long lVal;

      ValSize = sizeof (int);
      if (sysctlbyname ("hw.optional.mmx", &iVal, &ValSize, NULL, 0) == 0)
         PF[PF_MMX_INSTRUCTIONS_AVAILABLE] = iVal;

      ValSize = sizeof (int);
      if (sysctlbyname ("hw.optional.sse", &iVal, &ValSize, NULL, 0) == 0)
         PF[PF_XMMI_INSTRUCTIONS_AVAILABLE] = iVal;

      ValSize = sizeof (int);
      if (sysctlbyname ("hw.optional.sse2", &iVal, &ValSize, NULL, 0) == 0)
         PF[PF_XMMI64_INSTRUCTIONS_AVAILABLE] = iVal;

      ValSize = sizeof (int);
      if (sysctlbyname ("hw.optional.sse3", &iVal, &ValSize, NULL, 0) == 0)
         PF[PF_SSE3_INSTRUCTIONS_AVAILABLE] = iVal;

      ValSize = sizeof (int);
      if (sysctlbyname ("hw.optional.floatingpoint", &iVal, &ValSize, NULL,
                        0) == 0)
         PF[PF_FLOATING_POINT_EMULATED] = !iVal;

      ValSize = sizeof (sVal);
      if (sysctlbyname ("machdep.cpu.features", &sVal, &ValSize, NULL, 0) == 0)
      {
         if (strstr (sVal, "TSC") != NULL)
            PF[PF_RDTSC_INSTRUCTION_AVAILABLE] = TRUE;

         if (strstr (sVal, "PAE") != NULL)
            PF[PF_PAE_ENABLED] = TRUE;

         if (strstr (sVal, "CX8") != NULL)
            PF[PF_COMPARE_EXCHANGE_DOUBLE] = TRUE;

         if (strstr (sVal, "CX16") != NULL)
            PF[PF_COMPARE_EXCHANGE128] = TRUE;
      }

      ValSize = sizeof (int);
      if (sysctlbyname ("hw.ncpu", &iVal, &ValSize, NULL, 0) == 0)
         cachedsi.dwNumberOfProcessors = iVal;

      for (i = 0; i < cachedsi.dwNumberOfProcessors; i++)
      {
         HKEY ProcKey;
         int Family = 0, Model = 0, Stepping = 0;

         sprintf (sVal, "%d", i);
         RegCreateKeyA (hkey, sVal, &ProcKey);

         ValSize = sizeof (sVal);
         if (sysctlbyname ("machdep.cpu.vendor", &sVal, &ValSize, NULL, 0) == 0)
            RegSetValueExA (ProcKey, "VendorIdentifier", 0, REG_SZ, (BYTE*)sVal,
                            strlen (sVal));

         ValSize = sizeof (sVal);
         if (sysctlbyname ("machdep.cpu.brand_string", &sVal, &ValSize, NULL,
                           0) == 0)
            RegSetValueExA (ProcKey, "ProcessorNameString", 0, REG_SZ,
                            (BYTE*)sVal, strlen (sVal));

         ValSize = sizeof (int);
         sysctlbyname ("machdep.cpu.family", &Family, &ValSize, NULL, 0);
         ValSize = sizeof (int);
         sysctlbyname ("machdep.cpu.model", &Model, &ValSize, NULL, 0);
         ValSize = sizeof (int);
         sysctlbyname ("machdep.cpu.stepping", &Stepping, &ValSize, NULL, 0);
         sprintf (sVal, "x86 Family %d Model %d Stepping %d",
                  Family, Model, Stepping);
         RegSetValueExA (ProcKey, "Identifier", 0, REG_SZ, (BYTE*)sVal,
                         strlen (sVal));

         cachedsi.wProcessorRevision = Model << 8 | Stepping;

         ValSize = sizeof (lVal);
         if (sysctlbyname ("hw.cpufrequency", &lVal, &ValSize, NULL, 0) == 0)
         {
            iVal = (int)(lVal / 1000000);
            RegSetValueExA (ProcKey, "~MHz", 0, REG_DWORD, (BYTE *)&iVal, 4);
         }

         RegCloseKey (ProcKey);

         cachedsi.dwActiveProcessorMask |= 1 << i;
      }

      cachedsi.dwProcessorType = PROCESSOR_INTEL_PENTIUM;
      cachedsi.wProcessorLevel = 5;
   }
#else
	FIXME("not yet supported on this system !!\n");

        /* Fill in some stuff by default on an x86 */
#ifdef __i386__
	PF[PF_FLOATING_POINT_EMULATED] = FALSE;
        PF[PF_COMPARE_EXCHANGE_DOUBLE] = TRUE;
        PF[PF_MMX_INSTRUCTIONS_AVAILABLE] = TRUE;
        PF[PF_RDTSC_INSTRUCTION_AVAILABLE] = TRUE;
#endif

	RegCreateKeyA(hkey,"0",&xhkey);
	RegSetValueExA(xhkey,"Identifier",0,REG_SZ,"CPU 386",strlen("CPU 386"));
#endif  /* !linux */
	if (xhkey)
		RegCloseKey(xhkey);
	if (hkey)
		RegCloseKey(hkey);
	TRACE("<- CPU arch %d, res'd %d, pagesize %ld, minappaddr %p, maxappaddr %p, act.cpumask %08lx, "
          "numcpus %ld, CPU type %ld, allocgran. %ld, CPU level %d, CPU rev 0x%hx\n", 
          cachedsi.u.s.wProcessorArchitecture, cachedsi.u.s.wReserved, cachedsi.dwPageSize, 
          cachedsi.lpMinimumApplicationAddress, cachedsi.lpMaximumApplicationAddress, 
          cachedsi.dwActiveProcessorMask, cachedsi.dwNumberOfProcessors, cachedsi.dwProcessorType, 
          cachedsi.dwAllocationGranularity, cachedsi.wProcessorLevel, cachedsi.wProcessorRevision);
}

/***********************************************************************
 * 			GetNativeSystemInfo            	[KERNEL32.@]
 *
 * Gets information about the current system to an application running 
 * under WOW64. If the function is called from a 64-bit application, 
 * it is equivalent to the GetSystemInfo function.
 *
 * RETURNS
 *	none
 */
VOID WINAPI GetNativeSystemInfo(
	LPSYSTEM_INFO si	/* [out] system information */
) {
	FIXME("(%p) Stub! forwarding to GetSystemInfo\n", si);
	GetSystemInfo( si );
}


/***********************************************************************
 *                      IsWow64Process                  [KERNEL32.@]
 *
 *  Determines if the specified process is running under WOW64.
 *
 *  RETURNS
 *     TRUE on success
 *     FALSE on error (and sets LastError)
 */

BOOL WINAPI IsWow64Process(
  HANDLE hProcess,             /* [in] process handle */
  PBOOL Wow64Process           /* [out] pointer to return result */
) {
  	FIXME("(0x%04x, %p) stub!\n", hProcess, Wow64Process);
  	/* we don't support WOW64, so this is trivial ... */
	*Wow64Process = FALSE;
	return TRUE;
}

/***********************************************************************
 * 			IsProcessorFeaturePresent	[KERNEL32.@]
 * RETURNS:
 *	TRUE if processor feature present
 *	FALSE otherwise
 */
BOOL WINAPI IsProcessorFeaturePresent (
	DWORD feature	/* [in] feature number, see PF_ defines */
) {

  /* Ensure the information is loaded and cached */
  if (!cache)
    cache_system_info();

  if (feature < 64)
    return PF[feature];
  else
    return FALSE;
}

#!/usr/bin/perl
##Wine Quick Debug Report Maker Thingy (WQDRMK)
##By Adam Sacarny
##(c) 1998-1999
##Do not say this is yours without my express permisson, or I will
##hunt you down and kill you like the savage animal I am.
##
## Improvements by Gerald Pfeifer <pfeifer@dbai.tuwien.ac.at>
## (c) 2000
##
## Released under the WINE licence.
##
##Changelog: 
##August 29, 1999 - Work around for debugger exit (or lack thereof)
##                - Should now put debugging output in correct place
##April 19, 1999 - Much nicer way to select wine's location
##               - Option to disable creation of a debugging output
##               - Now places debugging output where it was started
##April 4, 1999 - Sanity check for file locations/wine strippedness
##              - Various code cleanups/fixes
##March 21, 1999 - Bash 2.0 STDERR workaround (Thanks Ryan Cumming!)
##March 1, 1999 - Check for stripped build
##February 3, 1999 - Fix to chdir to the program's directory
##February 1, 1999 - Cleaned up code
##January 26, 1999 - Fixed various bugs...
##                 - Made newbie mode easier
##January 25, 1999 - Initial Release
## -------------------------------------------
##| IRCNET/UNDERNET: jazzfan AOL: Jazzrock12  |
##| E-MAIL: magicbox@bestweb.net ICQ: 19617831|
##|   Utah Jazz Page @ http://www.gojazz.net  |
##|  Wine Builds @ http://www.gojazz.net/wine |
## -------------------------------------------
sub do_var {
	$var=$_[0];
	$var =~ s/\t//g;
	return $var;
}
open STDERR, ">&SAVEERR"; open STDERR, ">&STDOUT"; 
$ENV{'SHELL'}="/bin/bash";
$var0 = qq{
	What is your level of WINE expertise? 1-newbie 2-intermediate 3-advanced
	
	1 - Makes a debug report as defined in the WINE documentation. Best
	    for new WINE users. If you're not sure what -debugmsg is, then
	    use this mode.
	2 - Makes a debug report that is more customizable (Example: you can
	    choose what -debugmsg 's to use). You are asked more questions in
	    this mode. May intimidate newbies.
	3 - Just like 2, but not corner cutting. Assumes you know what you're
	    doing so it leaves out the long descriptions.
};
print do_var($var0)."\n";
until ($debuglevel >= 1 and $debuglevel <= 3) {
	print "Enter your level of WINE expertise (1-3): ";
	$debuglevel=<STDIN>;
	chomp $debuglevel;
} 

if ($debuglevel < 3) {
	$var1 = qq{
	This program will make a debug report for WINE developers. It generates
	two files. The first one has everything asked for by the bugreports guide;
	the second has *all* of the debug output, which can go to thousands of
	lines.
	To (hopefully) get the bug fixed, attach the first file to a messsage
	sent to the comp.emulators.ms-windows.wine newsgroup. The developers
	might ask you for "the last X lines from the report". If so, just
	provide the output of the following command:
	    gzip -d (output file) | tail -n (X) > outfile
	If you do not want to create one of the files, just specify "no file".
	};
	print do_var($var1);
} elsif ($debuglevel =~ 3) {
	$var2 = qq{
	This program will output to two files:
	1. Formatted debug report you might want to post to the newsgroup
	2. File with ALL the debug output (It will later be compressed with
	gzip, so leave off the trailing .gz)
	If you do not want to create one of the files, just type in "no file"
	and I'll skip it.
	};
	print do_var($var2);
}

print "\nFilename for the formatted debug report: ";
$outfile=<STDIN>;
chomp $outfile;
$var23 = qq{
I don't think you typed in the right filename. Let's try again.
};
while ($outfile =~ /^(\s)*$/) {
	print do_var($var23);
	$outfile=<STDIN>;
	chomp $outfile;
}

print "Filename for full debug output: ";
$dbgoutfile=<STDIN>;
chomp $dbgoutfile;
while ($dbgoutfile =~ /^(\s)*$/) {
	print do_var($var23);
	$dbgoutfile=<STDIN>;
	chomp $dbgoutfile;
}

$var31 = qq{
Since you will only be creating the formatted report, I will need a
temporary place to put the full output.
You may not enter "no file" for this.
Enter the filename for the temporary file:
};
if ($outfile ne "no file" and $dbgoutfile eq "no file") {
	print do_var($var31);
	$tmpoutfile=<STDIN>;
	chomp $tmpoutfile;
	while (($tmpoutfile =~ /^(\s)*$/) or ($tmpoutfile eq "no file")) {
		print do_var($var23);
		$tmpoutfile=<STDIN>;
		chomp $tmpoutfile;
	}
}

$whereis=`whereis wine`;
chomp $whereis;
print "\nWhere is your copy of Wine located?\n\n";
$whereis =~ s/^wine\: //;
@locations = split(/\s/,$whereis);
print "1 - Unlisted (I'll prompt you for a new location\n";
print "2 - Unsure (I'll use #3, that's probably it)\n";
$i=2;
foreach $location (@locations) {
	$i++;
	print "$i - $location\n";
}
print "\n";
sub select_wineloc {
	do
		{
		print "Enter the number that corresponds to Wine's location: ";
		$wineloc=<STDIN>;
		chomp $wineloc;
		}
	while ( ! ( $wineloc >=1 and $wineloc <= 2+@locations ) );
	if ($wineloc == 1) {
		$var25 = qq{
		Enter the full path to wine (Example: /usr/bin/wine):
		};
		$var26 = qq{
		Please enter the full path to wine. A full path is the
		directories leading up to a program's location, and then the
		program. For example, if you had the program "wine" in the
		directory "/usr/bin", you would type in "/usr/bin/wine". Now
		try:
		};
		print do_var($var25) if $debuglevel == 3;
		print do_var($var26) if $debuglevel < 3;
		$wineloc=<STDIN>;
		chomp $wineloc;
		while ($wineloc =~ /^(\s)*$/) {
			print do_var($var23);
			$wineloc=<STDIN>;
			chomp $wineloc;
		}
	}
	elsif ($wineloc == 2) {
		$wineloc=$locations[0];
	}
	else {
		$wineloc=$locations[$wineloc-3];
	}
}
&select_wineloc;
print "Checking if $wineloc is stripped...\n";
$ifstrip = `nm $wineloc 2>&1`;
while ($ifstrip =~ /no symbols/) {
	$var24 = qq{
	Your wine is stripped! You probably downloaded it off of the internet.
	If you have another location of wine that may be used, enter it now.
	Otherwise, hit control-c and download an unstripped version, then re-run
	this script. Note: stripped versions make useless debug reports
	};
	print do_var($var24);
	&select_wineloc;
	$ifstrip = `nm $wineloc 2>&1`;
}
while ($ifstrip =~ /not recognized/) {
	$var26 = qq{
	Looks like you gave me something that isn't a wine binary (It could be a
	text file). Try again.
	};
	print do_var($var26);
	&select_wineloc;
	print "Checking if $wineloc is stripped...\n";
	$ifstrip = `nm $wineloc 2>&1`;
}

print "\nWhat version of windows are you using with wine?\n\n".
      "0 - None\n".
      "1 - Windows 3.x\n".
      "2 - Windows 95\n".
      "3 - Windows 98\n".
      "4 - Windows NT 3.5x\n".
      "5 - Windows NT4.x\n".
      "6 - Windows 2000\n".
      "7 - Other\n\n";
do
	{
	print "Enter the number that corresponds to your windows version: ";
	$winver=<STDIN>; 
	chomp $winver; 
	}
until ($winver >= 0 and $winver <= 7); 
if ($winver =~ 0) { 
	$winver="None Installed"; 
} elsif ($winver =~ 1) { 
	$winver="Windows 3.x"; 
} elsif ($winver =~ 2) { 
	$winver="Windows 95"; 
} elsif ($winver =~ 3) { 
	$winver="Windows 98"; 
} elsif ($winver =~ 4) { 
	$winver="Windows NT 3.5x"; 
} elsif ($winver =~ 5) { 
	$winver="Windows NT 4.x"; 
} elsif ($winver =~ 6) { 
	$winver="Windows NT 5.x"; 
} elsif ($winver =~ 7) { 
	print "What version of Windows are you using? ";
	$winver=<STDIN>; 
	chomp $winver; 
}
if ($debuglevel < 3) {
	$var7 = qq{
	Enter the full path to the program you want to run. Remember what you
	were told before - a full path is the directories leading up to the
	program and then the program's name, like /dos/windows/sol.exe, not
	sol.exe:
	};
	print do_var($var7);
}
if ($debuglevel =~ 3) {
	$var8 = qq{
	Enter the full path to the program you want to run (Example: 
	/dos/windows/sol.exe, NOT sol.exe): 
	};
	print do_var($var8);
} 
$program=<STDIN>;
chomp $program;
while ($program =~ /^(\s)*$/) {
	print do_var($var23);
	$program=<STDIN>;
	chomp $program;
}
$program =~ s/\"//g;
$var9 = qq{
Enter the name, version, and manufacturer of the program (Example:
Netscape Navigator 4.5):
};
print do_var($var9);
$progname=<STDIN>;
chomp $progname;
$var10 = qq{
Enter 0 if your program is 16 bit (Windows 3.x), 1 if your program is 32
bit (Windows 9x, NT3.x and up), or 2 if you are unsure:
};
print do_var($var10);
$progbits=<STDIN>;
chomp $progbits;
until ($progbits == 0 or $progbits == 1 or $progbits == 2) {
	print "You must enter 0, 1 or 2!\n";
	$progbits=<STDIN>;
	chomp $progbits 
}
if ($progbits =~ 0) { 
	$progbits=Win16
} elsif ($progbits =~ 1) { 
	$progbits=Win32 
} else { 
	$progbits = "Unsure" 
} 
if ($debuglevel > 1) {
	if ($debuglevel =~ 2) {
		$var11 = qq{
		Enter any extra debug options. Default is +relay - If you don't
		know what options to use, just hit enter, and I'll use those (Example, the
		developer tells you to re-run with -debugmsg +dosfs,+module you would type
		in +dosfs,+module). Hit enter if you're not sure what to do:
		};
		print do_var($var11);
	} elsif ($debuglevel =~ 3) {
		$var12 = qq{
		Enter any debug options you would like to use. Just enter parts after
		-debugmsg. Default is +relay:
		};
		print do_var($var12);
	}
	$debugopts=<STDIN>;
	chomp $debugopts;
	if ($debugopts =~ /-debugmsg /) {
		($crap, $debugopts) = split / /,$debugopts; 
	}
	if ($debugopts =~ /^\s*$/) { 
		$debugopts="+relay"; 
	} 
} elsif ($debuglevel =~ 1) {
	$debugopts = "+relay"; 
} 
if ($debuglevel > 1) {
	if ($debuglevel =~ 2) {
		$var13 = qq{
		How many trailing lines of debugging info do you want to include in the report
		you're going to submit (First file)? If a developer asks you to include
		the last 1000 lines, enter 1000 here. Default is 200, which is reached by
		pressing enter. (If you're not sure, just hit enter):
		};
		print do_var($var13);
	} elsif ($debuglevel =~ 3) {
		$var14 = qq{
		Enter how many lines of trailing debugging output you want in your nice
		formatted report. Default is 200:
		};
		print do_var($var14);
	}
	$lastnlines=<STDIN>;
	chomp $lastnlines;
	if ($lastnlines =~ /^\s*$/) { 
	$lastnlines=200; 
	} 
} elsif ($debuglevel =~ 1) {
	$lastnlines=200; 
}
if ($debuglevel > 1) { 
	$var15 = qq{
	Enter any extra options you want to pass to WINE. Strongly recommended you
	include -managed:
	};
	print do_var($var15);
	$extraops=<STDIN>;
	chomp $extraops;
} elsif ($debuglevel =~ 1) {
	$extraops="-managed";
}

print "\nEnter the name of your distribution (Example: Redhat 6.1): ";
$dist=<STDIN>;
chomp $dist;

if ($debuglevel > 1) {
	if ($debuglevel =~ 2) {
		$var16 = qq{
		When you ran ./configure to build wine, were there any special options
		you used to do so (Example: --enable-dll)? If you didn't use any special
		options or didn't compile WINE on your own, just hit enter:
		};
		print do_var($var16);
	} elsif ($debuglevel =~ 3) {
		$var17 = qq{
		Enter any special options you used when running ./configure for WINE
		(Default is none, use if you didn't compile wine yourself):
		};
		print do_var($var17);
	}
	$configopts=<STDIN>;
	chomp $configopts;
	if ($configopts =~ /^\s*$/) { 
	$configopts="None"; 
	} 
} elsif ($debuglevel =~ 1) {
	$configopts="None"; 
}
if ($debuglevel > 1) {
	if ($debuglevel =~ 2) {
		$var18 = qq{
		Is your wine version CVS or from a .tar.gz file? As in... did you download it
		off a website/ftpsite or did you/have you run cvs on it to update it?
		For CVS: YYMMDD, where YY is the year (99), MM is the month (01), and DD
		is the day (14), that you last updated it (Example: 990114). 
		For tar.gz: Just hit enter and I'll figure out the version for you:
		};
		print do_var($var18);
	} elsif ($debuglevel =~ 3) {
		$var19 = qq{
		Is your wine from CVS? Enter the last CVS update date for it here, in
		YYMMDD form (If it's from a tarball, just hit enter):
		};
		print do_var($var19);
	}
	$winever=<STDIN>;
	chomp $winever;
	if ($winever =~ /[0-9]+/) {  
		$winever .= " CVS";
	}
	else {
		$winever = `$wineloc -v 2>&1`;
		chomp $winever;
	} 
} elsif ($debuglevel =~ 1) {
	$winever=`$wineloc -v 2>&1`;
	chomp $winever;
}
$gccver=`gcc -v 2>&1`;
($leftover,$gccver) = split /\n/,$gccver;
chomp $gccver;
$cpu=`uname -m`;
chomp $cpu;
$kernelver=`uname -r`;
chomp $kernelver;
$ostype=`uname -s`;
chomp $ostype;
$wineneeds=`ldd $wineloc`;
if ($debuglevel < 3) {
	$var20 = qq{
	OK, now I'm going to run WINE. I will close it for you once the wine
	debugger comes up. NOTE: You won't see ANY debug messages. Don't
	worry, they are being output to a file. Since there are so many, it's
	not a good idea to have them all output to a terminal (Speed slowdown 
	mainly).
	WINE will still run much slower than normal, because there will be so
	many debug messages being output to file. 
	};
	print do_var($var20);
} elsif ($debuglevel =~ 3) {
	$var21 = qq{
	OK, now it's time to run WINE. I will close down WINE for you after
	the debugger is finished doing its thing.
	};
	print do_var($var21);
}
$bashver=qw("/bin/bash -version");
if ($bashver =~ /2\./) { $outflags = "2>" }
else { $outflags = ">\&" }
print "Hit enter to start wine!\n";
$blank=<STDIN>;
$dir=$program;
$dir=~m#(.*)/#;
$dir=$1;
use Cwd;
$nowdir=getcwd;
chdir($dir);
if (!($outfile =~ /\//) and $outfile ne "no file") {
	$outfile = "$nowdir/$outfile";
}
if (!($dbgoutfile =~ /\//) and $dbgoutfile ne "no file") {
	$dbgoutfile = "$nowdir/$dbgoutfile";
}
if (!($tmpoutfile =~ /\//)) {
	$tmpoutfile = "$nowdir/$tmpoutfile";
}
$SIG{CHLD}=$SIG{CLD}=sub { wait };
if ($dbgoutfile ne "no file") {
	unlink("$dbgoutfile");
	if ($pid=fork()) {
	}
	elsif (defined $pid) {
		close(0);close(1);close(2);
		exec "echo quit | $wineloc -debugmsg $debugopts $extraops \"$program\" > $dbgoutfile 2>&1";
	}
	else {
		die "couldn't fork";
	}
	while (kill(0, $pid)) {
		sleep(5);
		$last = `tail -n 5 $dbgoutfile | grep Wine-dbg`;
		if ($last =~ /Wine-dbg/) {
			kill "TERM", $pid;
			break;
		}
	}
	if ($outfile ne "no file") {
		$lastlines=`tail -n $lastnlines $dbgoutfile`;
		system("gzip $dbgoutfile");
		&generate_outfile;
	}
	else {
		system("gzip $dbgoutfile");
	}
}
elsif ($outfile ne "no file" and $dbgoutfile eq "no file") {
	if ($pid=fork()) {
	}
	elsif (defined $pid) {
		close(0);close(1);close(2);
		exec "echo quit | $wineloc -debugmsg $debugopts $extraops \"$program\" 2>&1| tee $tmpoutfile | tail -n $lastnlines > $outfile";
	}
	else {
		die "couldn't fork";
	}
	print "$outfile $tmpoutfile";
	while (kill(0, $pid)) {
		sleep(5);
		$last = `tail -n 5 $tmpoutfile | grep Wine-dbg`;
		if ($last =~ /Wine-dbg/) {
			kill "TERM", $pid;
			break;
		}
	}
	unlink($tmpoutfile);
	open(OUTFILE, "$outfile");
	while (<OUTFILE>) {
		$lastlines .= $_;
	}
	close(OUTFILE);
	unlink($outfile);
	&generate_outfile;
}
else {
	$var27 = qq{
	I guess you don't want me to make any debugging output. I'll send
	it to your terminal. This will be a *lot* of output -- hit enter to
	continue, control-c to quit.
	Repeat: this will be a lot of output!
	};
	print do_var($var27);
	$blah=<STDIN>;
	system("$wineloc -debugmsg $debugmsg $extraops \"$program\"");
}
sub generate_outfile {
open(OUTFILE,">$outfile");
print OUTFILE <<EOM;
Auto-generated debug report by Wine Quick Debug Report Maker Thingy:
WINE Version:                $winever
Windows Version:             $winver
Distribution:                $dist
Kernel Version:              $kernelver
OS Type:                     $ostype
CPU:                         $cpu
GCC Version:                 $gccver
Program:                     $progname
Program Type:                $progbits
Debug Options:               -debugmsg $debugopts
Other Extra Commands Passed: $extraops
Extra ./configure Commands:  $configopts
Wine Dependencies: 
$wineneeds
Last $lastnlines lines of debug output follows:
$lastlines
I have a copy of the full debug report, if it is needed.
Thank you!
EOM
}
$var22 = qq{
Great! We're finished making the debug report. Do whatever with it. 
};
$var28 = qq{
The filename for the formatted report is:
$outfile
};
$var29 = qq{
The filename for the compressed full debug is:
$dbgoutfile.gz
Note that it is $dbgoutfile.gz, since I compressed it with gzip for you.
};
$var30 = qq{
Having problems with the script? Tell the wine newsgroup
(comp.emulators.ms-windows.wine).
};
print do_var($var22);
print do_var($var28) if $outfile ne "no file";
print do_var($var29) if $dbgoutfile ne "no file";
print do_var($var30);


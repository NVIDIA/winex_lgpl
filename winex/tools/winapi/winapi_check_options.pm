package winapi_check_options;
use base qw(options);

use strict;

use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);
require Exporter;

@ISA = qw(Exporter);
@EXPORT = qw();
@EXPORT_OK = qw($options);

use config qw($current_dir $wine_dir);
use options qw($options &parse_comma_list);

my %options_long = (
    "debug" => { default => 0, description => "debug mode" },
    "help" => { default => 0, description => "help mode" },
    "verbose" => { default => 0, description => "verbose mode" },

    "progress" => { default => 1, description => "show progress" },

    "win16" => { default => 1, description => "Win16 checking" },
    "win32" => { default => 1, description => "Win32 checking" },

    "shared" =>  { default => 0, description => "show shared functions between Win16 and Win32" },
    "shared-segmented" =>  { default => 0, description => "segmented shared functions between Win16 and Win32 checking" },

    "config" => { default => 1, parent => "local", description => "check configuration include consistancy" },
    "config-unnessary" => { default => 0, parent => "config", description => "check for unnessary #include \"config.h\"" },

    "spec-mismatch" => { default => 0, description => "spec file mismatch checking" },

    "local" =>  { default => 1, description => "local checking" },
    "module" => { 
	default => { active => 1, filter => 0, hash => {} },
	parent => "local",
	parser => \&parser_comma_list,
	description => "module filter"
    },

    "argument" => { default => 1, parent => "local", description => "argument checking" },
    "argument-count" => { default => 1, parent => "argument", description => "argument count checking" },
    "argument-forbidden" => {
	default => { active => 1, filter => 0, hash => {} },
	parent => "argument",
	parser => \&parser_comma_list,
	description => "argument forbidden checking"
    },
    "argument-kind" => {
	default => { active => 1, filter => 1, hash => { double => 1 } },
	parent => "argument",
	parser => \&parser_comma_list,
	description => "argument kind checking"
    },
    "calling-convention" => { default => 1, parent => "local", description => "calling convention checking" },
    "calling-convention-win16" => { default => 0, parent => "calling-convention", description => "calling convention checking (Win16)" },
    "calling-convention-win32" => { default => 1, parent => "calling-convention", description => "calling convention checking (Win32)" },
    "misplaced" => { default => 1, parent => "local", description => "check for misplaced functions" },
    "statements"  => { default => 0, parent => "local", description => "check for statements inconsistances" },
    "cross-call" => { default => 0, parent => "statements",  description => "check for cross calling functions" },
    "cross-call-win32-win16" => { 
	default => 0, parent => "cross-call", description => "check for cross calls between win32 and win16"
     },
    "cross-call-unicode-ascii" => { 
	default => 0, parent => "cross-call", description => "check for cross calls between Unicode and ASCII" 
    },
    "debug-messages" => { default => 0, parent => "statements", description => "check for debug messages inconsistances" },

    "comments" => {
	default => 1,
	parent => "local",
	description => "comments checking"
	},
    "comments-cplusplus" => {
	default => 1,
	parent => "comments",
	description => "C++ comments checking"
	},

    "documentation" => {
	default => 1,
	parent => "local", 
	description => "check for documentation inconsistances"
	},
    "documentation-pedantic" => { 
	default => 0, 
	parent => "documentation", 
	description => "be pendantic when checking for documentation inconsistances"
	},

    "documentation-arguments" => {
	default => 1,
	parent => "documentation",
	description => "check for arguments documentation inconsistances\n"
	},
    "documentation-comment-indent" => {
	default => 0, 
	parent => "documentation", description => "check for documentation comment indent inconsistances"
	},
    "documentation-comment-width" => {
	default => 0, 
	parent => "documentation", description => "check for documentation comment width inconsistances"
	},
    "documentation-name" => {
	default => 1,
	parent => "documentation",
	description => "check for documentation name inconsistances\n"
	},
    "documentation-ordinal" => {
	default => 1,
	parent => "documentation",
	description => "check for documentation ordinal inconsistances\n"
	},
    "documentation-wrong" => {
	default => 1,
	parent => "documentation",
	description => "check for wrong documentation\n"
	},

    "prototype" => {default => 0, parent => ["local", "headers"], description => "prototype checking" },
    "global" => { default => 1, description => "global checking" },
    "declared" => { default => 1, parent => "global", description => "declared checking" },
    "implemented" => { default => 0, parent => "local", description => "implemented checking" },
    "implemented-win32" => { default => 0, parent => "implemented", description => "implemented as win32 checking" },
    "include" => { default => 1, parent => "global", description => "include checking" },

    "headers" => { default => 0, description => "headers checking" },
    "headers-duplicated" => { default => 0, parent => "headers", description => "duplicated function declarations checking" },
    "headers-misplaced" => { default => 0, parent => "headers", description => "misplaced function declarations checking" },
    "headers-needed" => { default => 1, parent => "headers", description => "headers needed checking" },
    "headers-unused" => { default => 0, parent => "headers", description => "headers unused checking" },
);

my %options_short = (
    "d" => "debug",
    "?" => "help",
    "v" => "verbose"
);

my $options_usage = "usage: winapi_check [--help] [<files>]\n";

$options = '_winapi_check_options'->new(\%options_long, \%options_short, $options_usage);

package _winapi_check_options;
use base qw(_options);

use strict;

sub report_module {
    my $self = shift;
    my $refvalue = $self->{MODULE};
    
    my $name = shift;

    if(defined($name)) {
	return $$refvalue->{active} && (!$$refvalue->{filter} || $$refvalue->{hash}->{$name}); 
    } else {
	return 0;
    } 
}

sub report_argument_forbidden {
    my $self = shift;   
    my $refargument_forbidden = $self->{ARGUMENT_FORBIDDEN};

    my $type = shift;

    return $$refargument_forbidden->{active} && (!$$refargument_forbidden->{filter} || $$refargument_forbidden->{hash}->{$type}); 
}

sub report_argument_kind {
    my $self = shift;
    my $refargument_kind = $self->{ARGUMENT_KIND};

    my $kind = shift;

    return $$refargument_kind->{active} && (!$$refargument_kind->{filter} || $$refargument_kind->{hash}->{$kind}); 

}

1;

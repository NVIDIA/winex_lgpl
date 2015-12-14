package winapi_extract_options;
use base qw(options);

use strict;

use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);
require Exporter;

@ISA = qw(Exporter);
@EXPORT = qw();
@EXPORT_OK = qw($options);

use options qw($options &parse_comma_list);

my %options_long = (
    "debug" => { default => 0, description => "debug mode" },
    "help" => { default => 0, description => "help mode" },
    "verbose" => { default => 0, description => "verbose mode" },

    "progress" => { default => 1, description => "show progress" },

    "win16" => { default => 1, description => "Win16 extraction" },
    "win32" => { default => 1, description => "Win32 extraction" },

    "local" =>  { default => 1, description => "local extraction" },
    "global" => { default => 1, description => "global extraction" },

    "spec-files" => { default => 0, parent => "global", description => "spec files extraction" },
    "stub-statistics" => { default => 1, parent => "global", description => "stub statistics" },
    "winetest" => { default => 1, parent => "global", description => "winetest extraction" },
);

my %options_short = (
    "d" => "debug",
    "?" => "help",
    "v" => "verbose"
);

my $options_usage = "usage: winapi_extract [--help] [<files>]\n";

$options = '_options'->new(\%options_long, \%options_short, $options_usage);

1;

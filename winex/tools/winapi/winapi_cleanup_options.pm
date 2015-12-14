#
# Copyright 2002 Patrik Stridvall
#

package winapi_cleanup_options;
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

    "cpp-comments" => { default => 1, description => "converts C++ comments to C comments" },
    "trailing-whitespace" => { default => 0, description => "remove trailing whitespace" },
);

my %options_short = (
    "d" => "debug",
    "?" => "help",
    "v" => "verbose"
);

my $options_usage = "usage: winapi_cleanup [--help] [<files>]\n";

$options = '_options'->new(\%options_long, \%options_short, $options_usage);

1;

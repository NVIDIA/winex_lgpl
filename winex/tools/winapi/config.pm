package config;

use strict;

use setup qw($current_dir $wine_dir $winapi_dir $winapi_check_dir);

use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);
require Exporter;

@ISA = qw(Exporter);
@EXPORT = qw(
    &file_absolutize &file_normalize
    &file_directory
    &file_type &files_filter
    &file_skip &files_skip
    &get_api_files &get_c_files &get_h_files &get_spec_files
);
@EXPORT_OK = qw(
    $current_dir $wine_dir $winapi_dir $winapi_check_dir
);

use vars qw($current_dir $wine_dir $winapi_dir $winapi_check_dir);

use output qw($output);

use File::Find;

sub file_type {
    local $_ = shift;

    $_ = file_absolutize($_);

    m%^(?:libtest|rc|server|tests|tools)/% && return "";
    m%^(?:programs|debugger|miscemu)/% && return "wineapp";
    m%^(?:library|tsx11|unicode)/% && return "library";
    m%^windows/x11drv/wineclipsrv\.c$% && return "application";

    return "winelib";
}

sub files_filter {
    my $type = shift;

    my @files;
    foreach my $file (@_) {
	if(file_type($file) eq $type) {
	    push @files, $file;
	}
    }

    return @files;
}

sub file_skip {
    local $_ = shift;

    $_ = file_absolutize($_);

    m%^(?:libtest|programs|rc|server|tests|tools)/% && return 1;
    m%^(?:debugger|miscemu|tsx11|server|unicode)/% && return 1;
    m%^dlls/wineps/data/% && return 1;
    m%^windows/x11drv/wineclipsrv\.c$% && return 1;
    m%^dlls/winmm/wineoss/midipatch\.c$% && return 1;
    m%(?:glue|spec)\.c$% && return 1;

    return 0;
}

sub files_skip {
    my @files;
    foreach my $file (@_) {
	if(!file_skip($file)) {
	    push @files, $file;
	}
    }

    return @files;
}

sub file_absolutize {
    local $_ = shift;

    $_ = file_normalize($_);
    if(!s%^$wine_dir/%%) {
	$_ = "$current_dir/$_";
    }
    s%^\./%%;

    return $_;
}

sub file_normalize {
    local $_ = shift;

    foreach my $dir (split(m%/%, $current_dir)) {
	if(s%^(\.\./)*\.\./$dir/%%) {
	    if(defined($1)) {
		$_ = "$1$_";
	    }
	}
    }

    while(m%^(.*?)([^/\.]+)/\.\./(.*?)$%) {
	if($2 ne "." && $2 ne "..") {
	    $_ = "$1$3";
	}
    }

    return $_;
}

sub file_directory {
    local $_ = shift;

    s%/?[^/]*$%%;
    if(!$_) {
	$_ = ".";
    }

    s%^(?:\./)?(.*?)(?:/\.)?%$1%;

    return $_;
}

sub _get_files {
    my $extension = shift;
    my $type = shift;
    my $dir = shift;

    $output->progress("$wine_dir: searching for *.$extension");

    if(!defined($dir)) {
	$dir = $wine_dir;
    }

    my @files;

    my @dirs = ($dir);
    while(defined(my $dir = shift @dirs)) {
	opendir(DIR, $dir);
	my @entries= readdir(DIR);
	closedir(DIR);
	foreach (@entries) {
	    $_ = "$dir/$_";    
	    if(/\.\.?$/) {
		# Nothing
	    } elsif(-d $_) {
		push @dirs, $_;
	    } elsif(/\.$extension$/ && (!defined($type) || file_type($_) eq $type)) {
		s%^$wine_dir/%%;
		push @files, $_;
	    }
	}
    }

    return @files;
}

sub get_api_files { 
    my $name = shift; 
    return _get_files("api", undef, "$winapi_check_dir/$name");
}
sub get_c_files { return _get_files("c", @_); }
sub get_h_files { return _get_files("h", @_); }
sub get_spec_files { return _get_files("spec", @_); }

1;

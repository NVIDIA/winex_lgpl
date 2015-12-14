package winapi_global;

use strict;

use nativeapi qw($nativeapi);
use options qw($options);
use output qw($output);
use winapi qw($win16api $win32api @winapis);

sub check {
    my $type_found = shift;

    if($options->win16) {
	_check($win16api, $type_found);
    }

    if($options->win32) {
	_check($win32api, $type_found);
    }
}

sub _check {
    my $winapi = shift;
    my $type_found = shift;

    my $winver = $winapi->name;

    if($options->argument) {
	foreach my $type ($winapi->all_declared_types) {
	    if(!$$type_found{$type} && !$winapi->is_limited_type($type) && $type ne "CONTEXT86 *") {
		$output->write("*.c: $winver: ");
		$output->write("type ($type) not used\n");
	    }
	}
    }

    if($options->argument && $options->argument_forbidden) {
	my $types_used = $winapi->types_unlimited_used_in_modules;

	foreach my $type (sort(keys(%$types_used))) {
	    $output->write("*.c: type ($type) only used in module[s] (");
	    my $count = 0;
	    foreach my $module (sort(keys(%{$$types_used{$type}}))) {
		if($count++) { $output->write(", "); }
		$output->write("$module");
	    }
	    $output->write(")\n");
	}
    }
}

1;


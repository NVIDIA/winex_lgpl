package util;

use strict;

use vars qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
require Exporter;

@ISA = qw(Exporter);
@EXPORT = qw(
    &append_file &edit_file &read_file &replace_file
    &normalize_set &is_subset
);
@EXPORT_OK = qw();
%EXPORT_TAGS = ();

########################################################################
# append_file

sub append_file {
    my $filename = shift;
    my $function = shift;

    open(OUT, ">> $filename") || die "Can't open file '$filename'";
    my $result = &$function(\*OUT, @_);
    close(OUT);

    return $result;
}

########################################################################
# edit_file

sub edit_file {
    my $filename = shift;
    my $function = shift;

    open(IN, "< $filename") || die "Can't open file '$filename'";
    open(OUT, "> $filename.tmp") || die "Can't open file '$filename.tmp'";

    my $result = &$function(\*IN, \*OUT, @_);

    close(IN);
    close(OUT);

    if($result) {
        unlink("$filename");
        rename("$filename.tmp", "$filename");
    } else {
        unlink("$filename.tmp");
    }

    return $result;
}

########################################################################
# read_file

sub read_file {
    my $filename = shift;
    my $function = shift;

    open(IN, "< $filename") || die "Can't open file '$filename'";
    my $result = &$function(\*IN, @_);
    close(IN);

    return $result;
}

########################################################################
# replace_file

sub replace_file {
    my $filename = shift;
    my $function = shift;

    open(OUT, "> $filename.tmp") || die "Can't open file '$filename.tmp'";

    my $result = &$function(\*OUT, @_);

    close(OUT);

    if($result) {
        unlink("$filename");
        rename("$filename.tmp", "$filename");
    } else {
        unlink("$filename.tmp");
    }

    return $result;
}		   

########################################################################
# normalize_set

sub normalize_set {
    local $_ = shift;

    if(!defined($_)) {
	return undef;
    }
    
    my %hash = ();
    foreach my $key (split(/\s*&\s*/)) {
	$hash{$key}++;
    }

    return join(" & ", sort(keys(%hash)));
}

########################################################################
# is_subset

sub is_subset {
    my $subset = shift;
    my $set = shift;

    foreach my $subitem (split(/ & /, $subset)) {
	my $match = 0;
	foreach my $item (split(/ & /, $set)) {
	    if($subitem eq $item) {
		$match = 1;
		last;
	    }
	}
	if(!$match) {
	    return 0;
	}
    }
    return 1;
}

1;

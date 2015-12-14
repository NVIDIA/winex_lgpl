package winapi_options;

use strict;

use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);
require Exporter;

@ISA = qw(Exporter);
@EXPORT = qw(&parse_comma_list);
@EXPORT_OK = qw($options);

use vars qw($options);

use config qw($current_dir $wine_dir);
use output qw($output);

sub parser_comma_list {
    my $prefix = shift;
    my $value = shift;
    if(defined($prefix) && $prefix eq "no") {
	return { active => 0, filter => 0, hash => {} };
    } elsif(defined($value)) {
	my %names;
	for my $name (split /,/, $value) {
	    $names{$name} = 1;
	}
	return { active => 1, filter => 1, hash => \%names };
    } else {
	return { active => 1, filter => 0, hash => {} };
    }
}

my %options = (
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

my %short_options = (
    "d" => "debug",
    "?" => "help",
    "v" => "verbose"
);

sub new {
    my $proto = shift;
    my $class = ref($proto) || $proto;
    my $self  = {};
    bless ($self, $class);

    $self->options_set("default");

    my $c_files = \@{$self->{C_FILES}};
    my $h_files = \@{$self->{H_FILES}};
    my $module = \${$self->{MODULE}};
    my $global = \${$self->{GLOBAL}};
    my $headers = \${$self->{HEADERS}};

    my @files;

    if($wine_dir eq ".") {
	$$global = 1;
    } else {
	$$global = 0;
    }

    while(defined($_ = shift @ARGV)) {
	if(/^--(all|none)$/) {
	    $self->options_set("$1");
	    next;
	} elsif(/^-([^=]*)(=(.*))?$/) {
	    my $name;
	    my $value;
	    if(defined($2)) {
		$name = $1;
		$value = $3;
            } else {
		$name = $1;
	    }
	    
	    if($name =~ /^([^-].*)$/) {
		$name = $short_options{$1};
	    } else {
		$name =~ s/^-(.*)$/$1/;
	    }
	    	   
	    my $prefix;
	    if(defined($name) && $name =~ /^no-(.*)$/) {
		$name = $1;
		$prefix = "no";
		if(defined($value)) {
		    $output->write("options with prefix 'no' can't take parameters\n");

		    return undef;
		}
	    }

	    my $option;
	    if(defined($name)) {
		$option = $options{$name};
	    }

	    if(defined($option)) {
		my $key = $$option{key};
		my $parser = $$option{parser};
		my $refvalue = \${$self->{$key}};
		my @parents = ();
		
		if(defined($$option{parent})) {
		    if(ref($$option{parent}) eq "ARRAY") {
			@parents = @{$$option{parent}};
		    } else {
			@parents = $$option{parent};
		    }
		}

		if(defined($parser)) { 
		    $$refvalue = &$parser($prefix,$value);
		} else {
		    if(defined($value)) {
			$$refvalue = $value;
		    } elsif(!defined($prefix)) {
			$$refvalue = 1;
		    } else {
			$$refvalue = 0;
		    }
		}

		if((ref($$refvalue) eq "HASH" && $$refvalue->{active}) || $$refvalue) {
		    while($#parents >= 0) {
			my @old_parents = @parents;
			@parents = ();
			foreach my $parent (@old_parents) {
			    my $parentkey = $options{$parent}{key};
			    my $refparentvalue = \${$self->{$parentkey}};
			    
			    $$refparentvalue = 1;

			    if(defined($options{$parent}{parent})) {
				if(ref($options{$parent}{parent}) eq "ARRAY") {
				    push @parents, @{$options{$parent}{parent}};
				} else {
				    push @parents, $options{$parent}{parent};
				}
			    }
			}
		    }
		}
		next;
	    }	 
	}
	
	if(/^--module-dlls$/) {
	    my @dirs = `cd dlls && find . -type d ! -name CVS`;
	    my %names;
	    for my $dir (@dirs) {
		chomp $dir;
		$dir =~ s/^\.\/(.*)$/$1/;
		next if $dir eq "";
		$names{$dir} = 1;
	    }
	    $$module = { active => 1, filter => 1, hash => \%names };
	}	
	elsif(/^-(.*)$/) {
	    $output->write("unknown option: $_\n"); 

	    return undef;
	} else {
	    if(!-e $_) {
		$output->write("$_: no such file or directory\n");

		return undef;
	    }

	    push @files, $_;
	}
    }

    if($self->help) {
	return $self;
    }

    my @paths = ();
    my @c_files = ();
    my @h_files = ();
    foreach my $file (@files) {
	if($file =~ /\.c$/) {
	    push @c_files, $file;
	} elsif($file =~ /\.h$/) {
	    push @h_files, $file;
	} else {
	    push @paths, $file;
	}
    }

    if($#h_files >= 0) {
	$$headers = 1;
    }

    if($#c_files == -1 && $#h_files == -1 &&
       ($#paths == -1 || ($#paths == 0 && $paths[0] eq $wine_dir)))
    {
	@paths = ".";
    } else {
	$$global = 0;
    }

    if($#paths != -1 || $#c_files != -1) {
	my $c_command = "find " . join(" ", @paths, @c_files) . " -name \\*.c";
	my %found;
	@$c_files = sort(map {
	    s/^\.\/(.*)$/$1/;
	    if(defined($found{$_}) || /glue\.c|spec\.c$/) {
		();
	    } else {
		$found{$_}++;
		$_;
	    }
	} split(/\n/, `$c_command`));
    }

    if($#paths != -1 || $#h_files != -1) {
	my $h_command = "find " . join(" ", @paths, @h_files) . " -name \\*.h";
	my %found;

	@$h_files = sort(map {
	    s/^\.\/(.*)$/$1/;
	    if(defined($found{$_})) {
		();
	    } else {
		$found{$_}++;
		$_;
	    }
	} split(/\n/, `$h_command`));
    }

    $options = $self;

    return $self;
}

sub DESTROY {
}

sub options_set {
    my $self = shift;

    local $_ = shift;
    for my $name (sort(keys(%options))) {
        my $option = $options{$name};
	my $key = uc($name);
	$key =~ tr/-/_/;
	$$option{key} = $key;
	my $refvalue = \${$self->{$key}};

	if(/^default$/) {
	    $$refvalue = $$option{default};
	} elsif(/^all$/) {
	    if($name !~ /^help|debug|verbose|module$/) {
		if(ref($$refvalue) ne "HASH") {
		    $$refvalue = 1;
		} else {
		    $$refvalue = { active => 1, filter => 0, hash => {} };
		}
	    }
	} elsif(/^none$/) {
	    if($name !~ /^help|debug|verbose|module$/) {
		if(ref($$refvalue) ne "HASH") {
		    $$refvalue = 0;
		} else {
		    $$refvalue = { active => 0, filter => 0, hash => {} };
		}
	    }
	}
    }
}

sub show_help {
    my $self = shift;

    my $maxname = 0;
    for my $name (sort(keys(%options))) {
	if(length($name) > $maxname) {
	    $maxname = length($name);
	}
    }

    print "usage: winapi-check [--help] [<files>]\n";
    print "\n";
    for my $name (sort(keys(%options))) {
	my $option = $options{$name};
        my $description = $$option{description};
	my $default = $$option{default};
	my $current = ${$self->{$$option{key}}};

	my $value = $current;
	
	my $output;
	if(ref($value) ne "HASH") {
	    if($value) {
		$output = "--no-$name";
	    } else {
		$output = "--$name";
	    }
	} else {
	    if($value->{active}) {
		$output = "--[no-]$name\[=<value>]";
	    } else {
		$output = "--$name\[=<value>]";
	    }
	}

	print "$output";
	for (0..(($maxname - length($name) + 17) - (length($output) - length($name) + 1))) { print " "; }
	if(ref($value) ne "HASH") {
	    if($value) {
		print "Disable ";
	    } else {
		print "Enable ";
	    }    
	} else {
	    if($value->{active}) {
		print "(Disable) ";
	    } else {
		print "Enable ";
	    }
	}
        if($default == $current) {
	    print "$description (default)\n";
	} else {
	    print "$description\n";
	}    
    }
}

sub AUTOLOAD {
    my $self = shift;

    my $name = $winapi_options::AUTOLOAD;
    $name =~ s/^.*::(.[^:]*)$/\U$1/;

    my $refvalue = $self->{$name};
    if(!defined($refvalue)) {
	die "<internal>: winapi_options.pm: member $name does not exists\n"; 
    }

    if(ref($$refvalue) ne "HASH") {
	return $$refvalue;
    } else {
	return $$refvalue->{active};
    }
}

sub c_files { my $self = shift; return @{$self->{C_FILES}}; }

sub h_files { my $self = shift; return @{$self->{H_FILES}}; }

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


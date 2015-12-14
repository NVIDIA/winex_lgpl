package c_parser;

use strict;

use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);
require Exporter;

@ISA = qw(Exporter);
@EXPORT = qw();
@EXPORT_OK = qw();

use options qw($options);
use output qw($output);

use c_function;

########################################################################
# new
#
sub new {
    my $proto = shift;
    my $class = ref($proto) || $proto;
    my $self  = {};
    bless ($self, $class);

    my $file = \${$self->{FILE}};
    my $found_comment = \${$self->{FOUND_COMMENT}};
    my $found_declaration = \${$self->{FOUND_DECLARATION}};
    my $create_function = \${$self->{CREATE_FUNCTION}};
    my $found_function = \${$self->{FOUND_FUNCTION}};
    my $found_function_call = \${$self->{FOUND_FUNCTION_CALL}};
    my $found_line = \${$self->{FOUND_LINE}};
    my $found_preprocessor = \${$self->{FOUND_PREPROCESSOR}};
    my $found_statement = \${$self->{FOUND_STATEMENT}};
    my $found_variable = \${$self->{FOUND_VARIABLE}};

    $$file = shift;

    $$found_comment = sub { return 1; };
    $$found_declaration = sub { return 1; };
    $$create_function = sub { return new c_function; };
    $$found_function = sub { return 1; };
    $$found_function_call = sub { return 1; };
    $$found_line = sub { return 1; };
    $$found_preprocessor = sub { return 1; };
    $$found_statement = sub { return 1; };
    $$found_variable = sub { return 1; };

    return $self;
}

########################################################################
# set_found_comment_callback
#
sub set_found_comment_callback {
    my $self = shift;

    my $found_comment = \${$self->{FOUND_COMMENT}};

    $$found_comment = shift;
}

########################################################################
# set_found_declaration_callback
#
sub set_found_declaration_callback {
    my $self = shift;

    my $found_declaration = \${$self->{FOUND_DECLARATION}};

    $$found_declaration = shift;
}

########################################################################
# set_found_function_callback
#
sub set_found_function_callback {
    my $self = shift;

    my $found_function = \${$self->{FOUND_FUNCTION}};

    $$found_function = shift;
}

########################################################################
# set_found_function_call_callback
#
sub set_found_function_call_callback {
    my $self = shift;

    my $found_function_call = \${$self->{FOUND_FUNCTION_CALL}};

    $$found_function_call = shift;
}

########################################################################
# set_found_line_callback
#
sub set_found_line_callback {
    my $self = shift;

    my $found_line = \${$self->{FOUND_LINE}};

    $$found_line = shift;
}

########################################################################
# set_found_preprocessor_callback
#
sub set_found_preprocessor_callback {
    my $self = shift;

    my $found_preprocessor = \${$self->{FOUND_PREPROCESSOR}};

    $$found_preprocessor = shift;
}

########################################################################
# set_found_statement_callback
#
sub set_found_statement_callback {
    my $self = shift;

    my $found_statement = \${$self->{FOUND_STATEMENT}};

    $$found_statement = shift;
}

########################################################################
# set_found_variable_callback
#
sub set_found_variable_callback {
    my $self = shift;

    my $found_variable = \${$self->{FOUND_VARIABLE}};

    $$found_variable = shift;
}

########################################################################
# _parse_c

sub _parse_c {
    my $self = shift;

    my $pattern = shift;
    my $refcurrent = shift;
    my $refline = shift;
    my $refcolumn = shift;

    my $refmatch = shift;

    local $_ = $$refcurrent;
    my $line = $$refline;
    my $column = $$refcolumn;

    my $match;
    if(s/^(?:$pattern)//s) {
	$self->_update_c_position($&, \$line, \$column);
	$match = $&;
    } else {
	return 0;
    }

    $self->_parse_c_until_one_of("\\S", \$_, \$line, \$column);

    $$refcurrent = $_;
    $$refline = $line;
    $$refcolumn = $column;

    $$refmatch = $match;

    return 1;
}

########################################################################
# _parse_c_error
#
# FIXME: Use caller (See man perlfunc)

sub _parse_c_error {
    my $self = shift;

    my $file = \${$self->{FILE}};

    local $_ = shift;
    my $line = shift;
    my $column = shift;
    my $context = shift;
    my $message = shift;

    $message = "parse error" if !$message;

    my $current = "";
    if($_) {
	my @lines = split(/\n/, $_);

	$current .= $lines[0] . "\n" if $lines[0];
        $current .= $lines[1] . "\n" if $lines[1];
    }

    if($output->prefix) {
	$output->write("\n");
	$output->prefix("");
    }
    
    if($current) {
	$output->write("$$file:$line." . ($column + 1) . ": $context: $message: \\\n$current");
    } else {
	$output->write("$$file:$line." . ($column + 1) . ": $context: $message\n");
    }

    exit 1;
}

########################################################################
# _parse_c_warning

sub _parse_c_warning {
    my $self = shift;

    my $line = shift;
    my $column = shift;
    my $message = shift;

    $output->write("$line." . ($column + 1) . ": $message\n");
}

########################################################################
# _parse_c_until_one_of

sub _parse_c_until_one_of {
    my $self = shift;

    my $characters = shift;
    my $refcurrent = shift;
    my $refline = shift;
    my $refcolumn = shift;
    my $match = shift;

    local $_ = $$refcurrent;
    my $line = $$refline;
    my $column = $$refcolumn;

    if(!defined($match)) {
	my $blackhole;
	$match = \$blackhole;
    }

    $$match = "";
    while(/^[^$characters]/s) {
	my $submatch = "";

	if(s/^[^$characters\n\t\'\"]*//s) {
	    $submatch .= $&;
	}

	if(s/^\'//) {
	    $submatch .= "\'";
	    while(/^./ && !s/^\'//) {
		s/^([^\'\\]*)//s;
		$submatch .= $1;
		if(s/^\\//) {
		    $submatch .= "\\";
		    if(s/^(.)//s) {
			$submatch .= $1;
			if($1 eq "0") {
			    s/^(\d{0,3})//s;
			    $submatch .= $1;
			}
		    }
		}
	    }
	    $submatch .= "\'";

	    $$match .= $submatch;
	    $column += length($submatch);
	} elsif(s/^\"//) {
	    $submatch .= "\"";
	    while(/^./ && !s/^\"//) {
		s/^([^\"\\]*)//s;
		$submatch .= $1;
		if(s/^\\//) {
		    $submatch .= "\\";
		    if(s/^(.)//s) {
			$submatch .= $1;
			if($1 eq "0") {
			    s/^(\d{0,3})//s;
			    $submatch .= $1;
			}
		    }
		}
	    }
	    $submatch .= "\"";

	    $$match .= $submatch;
	    $column += length($submatch);
	} elsif(s/^\n//) {
	    $submatch .= "\n";

	    $$match .= $submatch;
	    $line++;
	    $column = 0;
	} elsif(s/^\t//) {
	    $submatch .= "\t";

	    $$match .= $submatch;
	    $column = $column + 8 - $column % 8;
	} else {
	    $$match .= $submatch;
	    $column += length($submatch);
	}
    }

    $$refcurrent = $_;
    $$refline = $line;
    $$refcolumn = $column;
    return 1;
}

########################################################################
# _update_c_position

sub _update_c_position {
    my $self = shift;

    local $_ = shift;
    my $refline = shift;
    my $refcolumn = shift;

    my $line = $$refline;
    my $column = $$refcolumn;

    while($_) {
	if(s/^[^\n\t\'\"]*//s) {
	    $column += length($&);
	}

	if(s/^\'//) {
	    $column++;
	    while(/^./ && !s/^\'//) {
		s/^([^\'\\]*)//s;
		$column += length($1);
		if(s/^\\//) {
		    $column++;
		    if(s/^(.)//s) {
			$column += length($1);
			if($1 eq "0") {
			    s/^(\d{0,3})//s;
			    $column += length($1);
			}
		    }
		}
	    }
	    $column++;
	} elsif(s/^\"//) {
	    $column++;
	    while(/^./ && !s/^\"//) {
		s/^([^\"\\]*)//s;
		$column += length($1);
		if(s/^\\//) {
		    $column++;
		    if(s/^(.)//s) {
			$column += length($1);
			if($1 eq "0") {
			    s/^(\d{0,3})//s;
			    $column += length($1);
			}
		    }
		}
	    }
	    $column++;
	} elsif(s/^\n//) {
	    $line++;
	    $column = 0;
	} elsif(s/^\t//) {
	    $column = $column + 8 - $column % 8;
	}
    }

    $$refline = $line;
    $$refcolumn = $column;
}

########################################################################
# parse_c_block

sub parse_c_block {
    my $self = shift;

    my $refcurrent = shift;
    my $refline = shift;
    my $refcolumn = shift;

    my $refstatements = shift;
    my $refstatements_line = shift;
    my $refstatements_column = shift;

    local $_ = $$refcurrent;
    my $line = $$refline;
    my $column = $$refcolumn;

    $self->_parse_c_until_one_of("\\S", \$_, \$line, \$column);

    my $statements;
    if(s/^\{//) {
	$column++;
	$statements = "";
    } else {
	return 0;
    }

    $self->_parse_c_until_one_of("\\S", \$_, \$line, \$column);

    my $statements_line = $line;
    my $statements_column = $column;

    my $plevel = 1;
    while($plevel > 0) {
	my $match;
	$self->_parse_c_until_one_of("\\{\\}", \$_, \$line, \$column, \$match);

	$column++;

	$statements .= $match;
	if(s/^\}//) {
	    $plevel--;
	    if($plevel > 0) {
		$statements .= "}";
	    }
	} elsif(s/^\{//) {
	    $plevel++;
	    $statements .= "{";
	} else {
	    return 0;
	}
    }

    $$refcurrent = $_;
    $$refline = $line;
    $$refcolumn = $column;
    $$refstatements = $statements;
    $$refstatements_line = $statements_line;
    $$refstatements_column = $statements_column;

    return 1;
}

########################################################################
# parse_c_declaration

sub parse_c_declaration {
    my $self = shift;

    my $found_declaration = \${$self->{FOUND_DECLARATION}};
    my $found_function = \${$self->{FOUND_FUNCTION}};

    my $refcurrent = shift;
    my $refline = shift;
    my $refcolumn = shift;

    local $_ = $$refcurrent;
    my $line = $$refline;
    my $column = $$refcolumn;

    $self->_parse_c_until_one_of("\\S", \$_, \$line, \$column);

    my $begin_line = $line;
    my $begin_column = $column + 1;

    my $end_line = $begin_line;
    my $end_column = $begin_column;
    $self->_update_c_position($_, \$end_line, \$end_column);

    if(!&$$found_declaration($begin_line, $begin_column, $end_line, $end_column, $_)) {
	return 1;
    }
    
    # Function
    my $function = shift;

    my $linkage = shift;
    my $calling_convention = shift;
    my $return_type = shift;
    my $name = shift;
    my @arguments = shift;
    my @argument_lines = shift;
    my @argument_columns = shift;

    # Variable
    my $type;

    if(0) {
	# Nothing
    } elsif(s/^(?:DEFAULT|DECLARE)_DEBUG_CHANNEL\s*\(\s*(\w+)\s*\)\s*//s) { # FIXME: Wine specific kludge
	$self->_update_c_position($&, \$line, \$column);
    } elsif(s/^__ASM_GLOBAL_FUNC\(\s*(\w+)\s*,\s*//s) { # FIXME: Wine specific kludge
	$self->_update_c_position($&, \$line, \$column);
	$self->_parse_c_until_one_of("\)", \$_, \$line, \$column);
	if(s/\)//) {
	    $column++;
	}
    } elsif(s/^(?:jump|strong)_alias//s) { # FIXME: GNU C library specific kludge
    } elsif(s/^extern\s*\"C\"\s*{//s) {
	$self->_update_c_position($&, \$line, \$column);
    } elsif(s/^(?:__asm__|asm)\s*\(//) {
	$self->_update_c_position($&, \$line, \$column);
    } elsif($self->parse_c_typedef(\$_, \$line, \$column)) {
	# Nothing
    } elsif($self->parse_c_variable(\$_, \$line, \$column, \$linkage, \$type, \$name)) {
	# Nothing
    } elsif($self->parse_c_function(\$_, \$line, \$column, \$function)) {
	if(&$$found_function($function))
	{
	    my $statements = $function->statements;
	    my $statements_line = $function->statements_line;
	    my $statements_column = $function->statements_column;

	    if(defined($statements)) {
		if(!$self->parse_c_statements(\$statements, \$statements_line, \$statements_column)) {
		    return 0;
		}
	    }
	}
    } else {
	$self->_parse_c_error($_, $line, $column, "declaration");
    }
    
    $$refcurrent = $_;
    $$refline = $line;
    $$refcolumn = $column;

    return 1;
}

########################################################################
# parse_c_declarations

sub parse_c_declarations {
    my $self = shift;

    my $refcurrent = shift;
    my $refline = shift;
    my $refcolumn = shift;

    return 1;
}

########################################################################
# parse_c_expression

sub parse_c_expression {
    my $self = shift;

    my $refcurrent = shift;
    my $refline = shift;
    my $refcolumn = shift;

    my $found_function_call = \${$self->{FOUND_FUNCTION_CALL}};

    local $_ = $$refcurrent;
    my $line = $$refline;
    my $column = $$refcolumn;

    $self->_parse_c_until_one_of("\\S", \$_, \$line, \$column);

    while($_) {
	if(s/^(.*?)(\w+\s*\()/$2/s) {
	    $self->_update_c_position($1, \$line, \$column);

	    my $begin_line = $line;
	    my $begin_column = $column + 1;
	    
	    my $name;
	    my @arguments;
	    my @argument_lines;
	    my @argument_columns;
	    if(!$self->parse_c_function_call(\$_, \$line, \$column, \$name, \@arguments, \@argument_lines, \@argument_columns)) {
		return 0;
	    }
	    
	    if(&$$found_function_call($begin_line, $begin_column, $line, $column, $name, \@arguments))
	    {
		while(defined(my $argument = shift @arguments) &&
		      defined(my $argument_line = shift @argument_lines) &&
		      defined(my $argument_column = shift @argument_columns))
		{
		    $self->parse_c_expression(\$argument, \$argument_line, \$argument_column);
		}
	    }
	} else {
	    $_ = "";
	}
    }

    $self->_update_c_position($_, \$line, \$column);

    $$refcurrent = $_;
    $$refline = $line;
    $$refcolumn = $column;

    return 1;
}

########################################################################
# parse_c_file

sub parse_c_file {
    my $self = shift;

    my $found_comment = \${$self->{FOUND_COMMENT}};
    my $found_line = \${$self->{FOUND_LINE}};

    my $refcurrent = shift;
    my $refline = shift;
    my $refcolumn = shift;
    
    local $_ = $$refcurrent;
    my $line = $$refline;
    my $column = $$refcolumn;

    my $declaration = "";
    my $declaration_line = $line;
    my $declaration_column = $column;

    my $previous_line = 0;
    my $previous_column = -1;

    my $if = 0;
    my $if0 = 0;

    my $blevel = 1;
    my $plevel = 1;
    while($plevel > 0 || $blevel > 0) {
	my $match;
	$self->_parse_c_until_one_of("#/\\(\\)\\[\\]\\{\\};", \$_, \$line, \$column, \$match);
	
	if($line != $previous_line) {
	    &$$found_line($line);
	} elsif($column == $previous_column) {
	    $self->_parse_c_error($_, $line, $column, "file", "no progress");
	} else {
	    # &$$found_line("$line.$column");
	}
	$previous_line = $line;
	$previous_column = $column;

	# $output->write("file: $plevel $blevel: '$match'\n");

	if(!$declaration && $match =~ s/^\s+//s) {
	    $self->_update_c_position($&, \$declaration_line, \$declaration_column);
	}
	if(!$if0) {
	    $declaration .= $match;
	} else {
	    my $blank_lines = 0;

	    local $_ = $match;
	    while(s/^.*?\n//) { $blank_lines++; }

	    if(!$declaration) {
		$declaration_line = $line;
		$declaration_column = $column;
	    } else {
		$declaration .= "\n" x $blank_lines;
	    }

	}

	if(/^[\#\/]/) {
	    my $blank_lines = 0;
	    if(s/^\#\s*//) {
		my $preprocessor_line = $line;
		my $preprocessor_column = $column;

		my $preprocessor = $&;
		while(s/^(.*?)\\\s*\n//) {
		    $blank_lines++;
		    $preprocessor .= "$1\n";
		}
		if(s/^(.*?)(\/\*.*?\*\/)(.*?)\n//) {
		    $_ = "$2\n$_";
		    if(defined($3)) {
			$preprocessor .= "$1$3";
		    } else {
			$preprocessor .= $1;
		    }
		} elsif(s/^(.*?)(\/[\*\/].*?)?\n//) {
		    if(defined($2)) {
			$_ = "$2\n$_";
		    } else {
			$blank_lines++;
		    }
		    $preprocessor .= $1;
		}

		if($if0 && $preprocessor =~ /^\#\s*endif/) {
		    if($if0 > 0) {
			if($if > 0) {
			    $if--;
			} else {
			    $if0--;
			}
		    }
	        } elsif($preprocessor =~ /^\#\s*if/) {
		    if($preprocessor =~ /^\#\s*if\s*0/) {
			$if0++;
		    } elsif($if0 > 0) {
			$if++;
		    }
		}

		if(!$self->parse_c_preprocessor(\$preprocessor, \$preprocessor_line, \$preprocessor_column)) {
		     return 0;
		}
	    }

	    if(s/^\/\*.*?\*\///s) {
		&$$found_comment($line, $column + 1, $&);
	        local $_ = $&;
		while(s/^.*?\n//) {
		    $blank_lines++; 
		}
		if($_) {
		    $column += length($_);
		}
	    } elsif(s/^\/\/(.*?)\n//) {
		&$$found_comment($line, $column + 1, $&);
		$blank_lines++;
	    } elsif(s/^\///) {
		if(!$if0) {
		    $declaration .= $&;
		    $column++;
		}
	    }

	    $line += $blank_lines;
	    if($blank_lines > 0) {
		$column = 0;
	    }

	    if(!$declaration) {
		$declaration_line = $line;
		$declaration_column = $column;
	    } elsif($blank_lines > 0) {
		$declaration .= "\n" x $blank_lines;
	    }

	    next;
	}

	$column++;

	if($if0) {
	    s/^.//;
	    next;
	}

	if(s/^[\(\[]//) {
	    $plevel++;
	    $declaration .= $&;
	} elsif(s/^\]//) {
	    $plevel--;
	    $declaration .= $&;
	} elsif(s/^\)//) {
	    $plevel--;
	    $declaration .= $&;
	    if($plevel == 1 && $declaration =~ /^__ASM_GLOBAL_FUNC/) {
		if(!$self->parse_c_declaration(\$declaration, \$declaration_line, \$declaration_column)) {
		    return 0;
		}
		$self->_parse_c_until_one_of("\\S", \$_, \$line, \$column);
		$declaration = "";
		$declaration_line = $line;
		$declaration_column = $column;
	    }
	} elsif(s/^\{//) {
	    $blevel++;
	    $declaration .= $&;
	} elsif(s/^\}//) {
	    $blevel--;
	    $declaration .= $&;
	    if($declaration =~ /^typedef/s ||
	       $declaration =~ /^(?:const\s+|extern\s+|static\s+)*(?:struct|union)(?:\s+\w+)?\s*\{/s)
	    {
		# Nothing
	    } elsif($plevel == 1 && $blevel == 1) {
		if(!$self->parse_c_declaration(\$declaration, \$declaration_line, \$declaration_column)) {
		    return 0;
		}
		$self->_parse_c_until_one_of("\\S", \$_, \$line, \$column);
		$declaration = "";
		$declaration_line = $line;
		$declaration_column = $column;
	    } elsif($column == 1) {
		$self->_parse_c_error("", $line, $column, "file", "inner } ends on column 1");
	    }
	} elsif(s/^;//) {
	    $declaration .= $&;
	    if($blevel == 1 && 
	       $declaration !~ /^typedef/ && 
	       $declaration !~ /^(?:const\s+|extern\s+|static\s+)(?:struct|union)(?:\s+\w+)?\s*\{/s &&
	       $declaration =~ /^(?:\w+\s*)*(?:(?:\*\s*)+|\s+)(\w+)\s*\(\s*(?:(?:\w+\s*,\s*)*\w+\s*)?\)(.*?);/s && 
	       $1 ne "ICOM_VTABLE" && $2) # K&R 
	    {
		$self->_parse_c_warning($line, $column, "function $1: warning: function has K&R format");
	    } elsif($plevel == 1 && $blevel == 1) {
		$declaration =~ s/\s*;$//;
		if($declaration && !$self->parse_c_declaration(\$declaration, \$declaration_line, \$declaration_column)) {
		    return 0;
		}
		$self->_parse_c_until_one_of("\\S", \$_, \$line, \$column);
		$declaration = "";
		$declaration_line = $line;
		$declaration_column = $column;
	    }
	} elsif(/^\s*$/ && $declaration =~ /^\s*$/ && $match =~ /^\s*$/) {
	    $plevel = 0;
	    $blevel = 0;
	} else {
	    $self->_parse_c_error($_, $line, $column, "file", "'$declaration' '$match'");
	}
    }

    $$refcurrent = $_;
    $$refline = $line;
    $$refcolumn = $column;

    return 1;
}

########################################################################
# parse_c_function

sub parse_c_function {
    my $self = shift;

    my $file = \${$self->{FILE}};
    my $create_function = \${$self->{CREATE_FUNCTION}};

    my $refcurrent = shift;
    my $refline = shift;
    my $refcolumn = shift;

    my $reffunction = shift;
    
    local $_ = $$refcurrent;
    my $line = $$refline;
    my $column = $$refcolumn;

    my $linkage = "";
    my $calling_convention = "";
    my $return_type;
    my $name;
    my @arguments;
    my @argument_lines;
    my @argument_columns;
    my $statements;
    my $statements_line;
    my $statements_column;

    $self->_parse_c_until_one_of("\\S", \$_, \$line, \$column);

    my $begin_line = $line;
    my $begin_column = $column + 1;

    my $match;
    while($self->_parse_c('const|inline|extern|static|volatile|' . 
			  'signed(?=\\s+char|s+int|\s+long(?:\s+long)?|\s+short)|' .
			  'unsigned(?=\s+char|\s+int|\s+long(?:\s+long)?|\s+short)',
			  \$_, \$line, \$column, \$match)) 
    {
	if($match =~ /^extern|static$/) {
	    if(!$linkage) {
		$linkage = $match;
	    }
	}
    }


    if(0) {
	# Nothing
    } elsif($self->_parse_c('DECL_GLOBAL_CONSTRUCTOR', \$_, \$line, \$column, \$name)) { # FIXME: Wine specific kludge
	# Nothing
    } elsif($self->_parse_c('WINE_EXCEPTION_FILTER\(\w+\)', \$_, \$line, \$column, \$name)) { # FIXME: Wine specific kludge
	# Nothing
    } else {
	if(!$self->parse_c_type(\$_, \$line, \$column, \$return_type)) {
	    return 0;
	}

	$self->_parse_c("__cdecl|__stdcall|inline|CDECL|VFWAPIV|VFWAPI|WINAPIV|WINAPI|CALLBACK|WINE_UNUSED|PASCAL", 
			\$_, \$line, \$column, \$calling_convention);
	
	if(!$self->_parse_c('\w+', \$_, \$line, \$column, \$name)) { 
	    return 0; 
	}

	if(!$self->parse_c_tuple(\$_, \$line, \$column, \@arguments, \@argument_lines, \@argument_columns)) {
	    return 0; 
	}
    }

    my $kar;
    # FIXME: Implement proper handling of K&R C functions
    $self->_parse_c_until_one_of("{", \$_, \$line, \$column, $kar);

    if($kar) {
	$output->write("K&R: $kar\n");
    }

    if($_ && !$self->parse_c_block(\$_, \$line, \$column, \$statements, \$statements_line, \$statements_column)) {
	return 0;
    }

    my $end_line = $line;
    my $end_column = $column;
    
    $$refcurrent = $_;
    $$refline = $line;
    $$refcolumn = $column;

    my $function = &$$create_function;

    $function->file($$file);
    $function->begin_line($begin_line);
    $function->begin_column($begin_column);
    $function->end_line($end_line);
    $function->end_column($end_column);
    $function->linkage($linkage);
    $function->return_type($return_type); 
    $function->calling_convention($calling_convention);
    $function->name($name);
    # if(defined($argument_types)) {
    #     $function->argument_types([@$argument_types]);
    # }
    # if(defined($argument_names)) {
    #     $function->argument_names([@$argument_names]);
    # }
    $function->statements_line($statements_line);
    $function->statements_column($statements_column);
    $function->statements($statements);

    $$reffunction = $function;

    return 1;
}

########################################################################
# parse_c_function_call

sub parse_c_function_call {
    my $self = shift;

    my $refcurrent = shift;
    my $refline = shift;
    my $refcolumn = shift;

    my $refname = shift;
    my $refarguments = shift;
    my $refargument_lines = shift;
    my $refargument_columns = shift;

    local $_ = $$refcurrent;
    my $line = $$refline;
    my $column = $$refcolumn;

    my $name;
    my @arguments;
    my @argument_lines;
    my @argument_columns;

    if(s/^(\w+)(\s*)(?=\()//s) {
	$self->_update_c_position($&, \$line, \$column);

	$name = $1;

	if(!$self->parse_c_tuple(\$_, \$line, \$column, \@arguments, \@argument_lines, \@argument_columns)) {
	    return 0;
	}
    } else {
	return 0;
    }

    $$refcurrent = $_;
    $$refline = $line;
    $$refcolumn = $column;

    $$refname = $name;
    @$refarguments = @arguments;
    @$refargument_lines = @argument_lines;
    @$refargument_columns = @argument_columns;

    return 1;
}

########################################################################
# parse_c_preprocessor

sub parse_c_preprocessor {
    my $self = shift;

    my $found_preprocessor = \${$self->{FOUND_PREPROCESSOR}};

    my $refcurrent = shift;
    my $refline = shift;
    my $refcolumn = shift;

    local $_ = $$refcurrent;
    my $line = $$refline;
    my $column = $$refcolumn;

    $self->_parse_c_until_one_of("\\S", \$_, \$line, \$column);

    my $begin_line = $line;
    my $begin_column = $column + 1;

    if(!&$$found_preprocessor($begin_line, $begin_column, "$_")) {
	return 1;
    }
    
    if(0) {
	# Nothing
    } elsif(/^\#\s*define\s*(.*?)$/s) {
	$self->_update_c_position($_, \$line, \$column);
    } elsif(/^\#\s*else/s) {
	$self->_update_c_position($_, \$line, \$column);
    } elsif(/^\#\s*endif/s) {
	$self->_update_c_position($_, \$line, \$column);
    } elsif(/^\#\s*(?:if|ifdef|ifndef)?\s*(.*?)$/s) {
	$self->_update_c_position($_, \$line, \$column);
    } elsif(/^\#\s*include\s+(.*?)$/s) {
	$self->_update_c_position($_, \$line, \$column);
    } elsif(/^\#\s*undef\s+(.*?)$/s) {
	$self->_update_c_position($_, \$line, \$column);
    } else {
	$self->_parse_c_error($_, $line, $column, "preprocessor");
    }

    $$refcurrent = $_;
    $$refline = $line;
    $$refcolumn = $column;

    return 1;
}

########################################################################
# parse_c_statement

sub parse_c_statement {
    my $self = shift;

    my $refcurrent = shift;
    my $refline = shift;
    my $refcolumn = shift;

    my $found_function_call = \${$self->{FOUND_FUNCTION_CALL}};

    local $_ = $$refcurrent;
    my $line = $$refline;
    my $column = $$refcolumn;

    $self->_parse_c_until_one_of("\\S", \$_, \$line, \$column);

    $self->_parse_c('(?:case\s+)?(\w+)\s*:\s*', \$_, \$line, \$column);

    # $output->write("$line.$column: statement: '$_'\n");

    if(/^$/) {
	# Nothing
    } elsif(/^\{/) {
	my $statements;
	my $statements_line;
	my $statements_column;
	if(!$self->parse_c_block(\$_, \$line, \$column, \$statements, \$statements_line, \$statements_column)) {
	    return 0;
	}
	if(!$self->parse_c_statements(\$statements, \$statements_line, \$statements_column)) {
	    return 0;
	}
    } elsif(s/^(for|if|switch|while)\s*(?=\()//) {
	$self->_update_c_position($&, \$line, \$column);

	my $name = $1;

	my @arguments;
	my @argument_lines;
	my @argument_columns;
	if(!$self->parse_c_tuple(\$_, \$line, \$column, \@arguments, \@argument_lines, \@argument_columns)) {
	    return 0;
	}

	$self->_parse_c_until_one_of("\\S", \$_, \$line, \$column);
	if(!$self->parse_c_statement(\$_, \$line, \$column)) {
	    return 0;
	}
	$self->_parse_c_until_one_of("\\S", \$_, \$line, \$column);

	while(defined(my $argument = shift @arguments) &&
	      defined(my $argument_line = shift @argument_lines) &&
	      defined(my $argument_column = shift @argument_columns))
	{
	    $self->parse_c_expression(\$argument, \$argument_line, \$argument_column);
	}
    } elsif(s/^else//) {
	$self->_update_c_position($&, \$line, \$column);
	if(!$self->parse_c_statement(\$_, \$line, \$column)) {
	    return 0;
	}
    } elsif(s/^return//) {
	$self->_update_c_position($&, \$line, \$column);
	$self->_parse_c_until_one_of("\\S", \$_, \$line, \$column);
	if(!$self->parse_c_expression(\$_, \$line, \$column)) {
	    return 0;
	}
    } elsif($self->parse_c_expression(\$_, \$line, \$column)) {
	# Nothing
    } else {
	# $self->_parse_c_error($_, $line, $column, "statement");
    }

    $self->_update_c_position($_, \$line, \$column);

    $$refcurrent = $_;
    $$refline = $line;
    $$refcolumn = $column;

    return 1;
}

########################################################################
# parse_c_statements

sub parse_c_statements {
    my $self = shift;

    my $refcurrent = shift;
    my $refline = shift;
    my $refcolumn = shift;

    my $found_function_call = \${$self->{FOUND_FUNCTION_CALL}};

    local $_ = $$refcurrent;
    my $line = $$refline;
    my $column = $$refcolumn;

    $self->_parse_c_until_one_of("\\S", \$_, \$line, \$column);

    # $output->write("$line.$column: statements: '$_'\n");

    my $statement = "";
    my $statement_line = $line;
    my $statement_column = $column;

    my $previous_line = -1;
    my $previous_column = -1;

    my $blevel = 1;
    my $plevel = 1;
    while($plevel > 0 || $blevel > 0) {
	my $match;
	$self->_parse_c_until_one_of("\\(\\)\\[\\]\\{\\};", \$_, \$line, \$column, \$match);

	if($previous_line == $line && $previous_column == $column) {
	    $self->_parse_c_error($_, $line, $column, "statements", "no progress");
	}
	$previous_line = $line;
	$previous_column = $column;

	# $output->write("'$match' '$_'\n");

	$statement .= $match;
	$column++;
	if(s/^[\(\[]//) {
	    $plevel++;
	    $statement .= $&;
	} elsif(s/^[\)\]]//) {
	    $plevel--;
	    if($plevel <= 0) {
		$self->_parse_c_error($_, $line, $column, "statements");
	    }
	    $statement .= $&;
	} elsif(s/^\{//) {
	    $blevel++;
	    $statement .= $&;
	} elsif(s/^\}//) {
	    $blevel--;
	    $statement .= $&;
	    if($blevel == 1) {
		if(!$self->parse_c_statement(\$statement, \$statement_line, \$statement_column)) {
		    return 0;
		}
		$self->_parse_c_until_one_of("\\S", \$_, \$line, \$column);
		$statement = "";
		$statement_line = $line;
		$statement_column = $column;
	    }
	} elsif(s/^;//) {
	    if($plevel == 1 && $blevel == 1) {
		if(!$self->parse_c_statement(\$statement, \$statement_line, \$statement_column)) {
		    return 0;
		}

		$self->_parse_c_until_one_of("\\S", \$_, \$line, \$column);
		$statement = "";
		$statement_line = $line;
		$statement_column = $column;
	    } else {
		$statement .= $&;
	    }
	} elsif(/^\s*$/ && $statement =~ /^\s*$/ && $match =~ /^\s*$/) {
	    $plevel = 0;
	    $blevel = 0;
	} else {
	    $self->_parse_c_error($_, $line, $column, "statements");
	}
    }

    $self->_update_c_position($_, \$line, \$column);

    $$refcurrent = $_;
    $$refline = $line;
    $$refcolumn = $column;

    return 1;
}

########################################################################
# parse_c_tuple

sub parse_c_tuple {
    my $self = shift;

    my $refcurrent = shift;
    my $refline = shift;
    my $refcolumn = shift;

    # FIXME: Should not write directly
    my $items = shift;
    my $item_lines = shift;
    my $item_columns = shift;

    local $_ = $$refcurrent;

    my $line = $$refline;
    my $column = $$refcolumn;

    my $item;
    if(s/^\(//) {
	$column++;
	$item = "";
    } else {
	return 0;
    }

    my $item_line = $line;
    my $item_column = $column + 1;

    my $plevel = 1;
    while($plevel > 0) {
	my $match;
	$self->_parse_c_until_one_of("\\(,\\)", \$_, \$line, \$column, \$match);

	$column++;

	$item .= $match;
	if(s/^\)//) {
	    $plevel--;
	    if($plevel == 0) {
		push @$item_lines, $item_line;
		push @$item_columns, $item_column;
		push @$items, $item;
		$item = "";
	    } else {
		$item .= ")";
	    }
	} elsif(s/^\(//) {
	    $plevel++;
	    $item .= "(";
	} elsif(s/^,//) {
	    if($plevel == 1) {
		push @$item_lines, $item_line;
		push @$item_columns, $item_column;
		push @$items, $item;
		$self->_parse_c_until_one_of("\\S", \$_, \$line, \$column);
		$item_line = $line;
		$item_column = $column + 1;
		$item = "";
	    } else {
		$item .= ",";
	    }
	} else {
	    return 0;
	}
    }

    $$refcurrent = $_;
    $$refline = $line;
    $$refcolumn = $column;

    return 1;
}

########################################################################
# parse_c_type

sub parse_c_type {
    my $self = shift;

    my $refcurrent = shift;
    my $refline = shift;
    my $refcolumn = shift;

    my $reftype = shift;

    local $_ = $$refcurrent;
    my $line = $$refline;
    my $column = $$refcolumn;

    my $type;

    $self->_parse_c("const", \$_, \$line, \$column);

    if(0) {
	# Nothing
    } elsif($self->_parse_c('ICOM_VTABLE\(.*?\)', \$_, \$line, \$column, \$type)) {
	# Nothing
    } elsif($self->_parse_c('(?:enum\s+|struct\s+|union\s+)?\w+\s*(\*\s*)*', \$_, \$line, \$column, \$type)) {
	# Nothing
    } else {
	return 0;
    }
    $type =~ s/\s//g;

    $$refcurrent = $_;
    $$refline = $line;
    $$refcolumn = $column;

    $$reftype = $type;

    return 1;
}

########################################################################
# parse_c_typedef

sub parse_c_typedef {
    my $self = shift;

    my $refcurrent = shift;
    my $refline = shift;
    my $refcolumn = shift;

    my $reftype = shift;

    local $_ = $$refcurrent;
    my $line = $$refline;
    my $column = $$refcolumn;

    my $type;

    if($self->_parse_c("typedef", \$_, \$line, \$column)) {
	# Nothing
    } elsif($self->_parse_c('enum(?:\s+\w+)?\s*\{', \$_, \$line, \$column)) {
	# Nothing
    } else {
	return 0;
    }

    $$refcurrent = $_;
    $$refline = $line;
    $$refcolumn = $column;

    $$reftype = $type;

    return 1;
}

########################################################################
# parse_c_variable

sub parse_c_variable {
    my $self = shift;

    my $found_variable = \${$self->{FOUND_VARIABLE}};

    my $refcurrent = shift;
    my $refline = shift;
    my $refcolumn = shift;

    my $reflinkage = shift;
    my $reftype = shift;
    my $refname = shift;

    local $_ = $$refcurrent;
    my $line = $$refline;
    my $column = $$refcolumn;

    $self->_parse_c_until_one_of("\\S", \$_, \$line, \$column);

    my $begin_line = $line;
    my $begin_column = $column + 1;

    my $linkage = "";
    my $type = "";
    my $name = "";

    my $match;
    while($self->_parse_c('const|inline|extern|static|volatile|' . 
			  'signed(?=\\s+char|s+int|\s+long(?:\s+long)?|\s+short)|' .
			  'unsigned(?=\s+char|\s+int|\s+long(?:\s+long)?|\s+short)',
			  \$_, \$line, \$column, \$match)) 
    {
	if($match =~ /^extern|static$/) {
	    if(!$linkage) {
		$linkage = $match;
	    }
	}
    }

    my $finished = 0;

    if($finished) {
	# Nothing
    } elsif($self->_parse_c('SEQ_DEFINEBUF', \$_, \$line, \$column, \$match)) { # Linux specific
	$type = $match;
	$finished = 1;
    } elsif($self->_parse_c('DEFINE_GUID', \$_, \$line, \$column, \$match)) { # Windows specific
	$type = $match;
	$finished = 1;
    } elsif($self->_parse_c('DEFINE_REGS_ENTRYPOINT_\w+|DPQ_DECL_\w+|HANDLER_DEF|IX86_ONLY', # Wine specific
			    \$_, \$line, \$column, \$match))
    { 
	$type = $match;
	$finished = 1;
    } elsif($self->_parse_c('(?:struct\s+)?ICOM_VTABLE\s*\(\w+\)', \$_, \$line, \$column, \$match)) {
	$type = $match;
	$finished = 1;
    } elsif(s/^(?:enum\s+|struct\s+|union\s+)(\w+)?\s*\{.*?\}\s*//s) {
	$self->_update_c_position($&, \$line, \$column);

	if(defined($1)) {
	    $type = "struct $1 { }";
	} else {
	    $type = "struct { }";
	}
	if(defined($2)) {
	    my $stars = $2;
	    $stars =~ s/\s//g;
	    if($stars) {
		$type .= " $type";
	    }
	}
    } elsif(s/^((?:enum\s+|struct\s+|union\s+)?\w+)\s*(?:\*\s*)*//s) {
	$type = $&;
	$type =~ s/\s//g;
    } else {
	return 0;
    }

    # $output->write("$type: '$_'\n");

    if($finished) {
	# Nothing
    } elsif(s/^WINAPI\s*//) {
	$self->_update_c_position($&, \$line, \$column);
    }

    if($finished) {
	# Nothing
    } elsif(s/^(\((?:__cdecl)?\s*\*?\s*(?:__cdecl)?\w+\s*(?:\[[^\]]*\]\s*)*\))\s*\(//) {
	$self->_update_c_position($&, \$line, \$column);

	$name = $1;
	$name =~ s/\s//g;

	$self->_parse_c_until_one_of("\\)", \$_, \$line, \$column);
	if(s/^\)//) { $column++; }
	$self->_parse_c_until_one_of("\\S", \$_, \$line, \$column);

        if(!s/^(?:=\s*|,\s*|$)//) {
	    return 0;
	}
    } elsif(s/^(?:\*\s*)*(?:const\s+)?(\w+)\s*(?:\[[^\]]*\]\s*)*\s*(?:=\s*|,\s*|$)//) {
	$self->_update_c_position($&, \$line, \$column);

	$name = $1;
	$name =~ s/\s//g;
    } elsif(/^$/) {
	$name = "";
    } else {
	return 0;
    }
 
    # $output->write("$type: $name: '$_'\n");

    if(1) {
	# Nothing
    } elsif($self->_parse_c('(?:struct\s+)?ICOM_VTABLE\s*\(.*?\)', \$_, \$line, \$column, \$match)) {
	$type = "<type>";
	$name = "<name>";
    } elsif(s/^((?:enum\s+|struct\s+|union\s+)?\w+)\s*
		(?:\*\s*)*(\w+|\s*\*?\s*\w+\s*\))\s*(?:\[[^\]]*\]|\([^\)]*\))?
		(?:,\s*(?:\*\s*)*(\w+)\s*(?:\[[^\]]*\])?)*
	    \s*(?:=|$)//sx)
    {
	$self->_update_c_position($&, \$line, \$column);

	$type = $1;
	$name = $2;

	$type =~ s/\s//g;
	$type =~ s/^struct/struct /;
    } elsif(/^(?:enum|struct|union)(?:\s+(\w+))?\s*\{.*?\}\s*((?:\*\s*)*)(\w+)\s*(?:=|$)/s) {
	$self->_update_c_position($&, \$line, \$column);

	if(defined($1)) {
	    $type = "struct $1 { }";
	} else {
	    $type = "struct { }";
	}
	my $stars = $2;
	$stars =~ s/\s//g;
	if($stars) {
	    $type .= " $type";
	}

	$name = $3;
    } else {
	return 0;
    }

    if(!$name) {
        $name = "<name>";
    }

    $$refcurrent = $_;
    $$refline = $line;
    $$refcolumn = $column;

    $$reflinkage = $linkage;
    $$reftype = $type;
    $$refname = $name;

    if(&$$found_variable($begin_line, $begin_column, $linkage, $type, $name))
    {
	# Nothing
    }

    return 1;
}

1;

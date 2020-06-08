#!/usr/bin/perl
use strict;

	my $src = $ARGV[0];
	my $section_name = $ARGV[1];

	if('' eq $src)
	{
		print STDERR "No source text specified, nothing to be done\n";
		exit 1;
	}

	if($section_name =~ /([^0-9A-Za-z-_\.#=\s])/)
	{
		print STDERR "Invalid character \"$1\" in section name: $section_name\n";
		exit 1;
	}

	open my $SRC, "<$src";
	while(<$SRC>)
	{
		if(/$section_name/)
		{
			my $line = $_;
			while(<STDIN>)
			{
				print STDOUT $_;
			}
			print STDOUT $line;
			next;
		}

		print $_;
	}

exit 0;

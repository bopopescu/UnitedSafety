#!/usr/bin/perl
use strict;

	if(@ARGV <= 0)
	{
		print STDERR "Usage: $0 <base_dir>\n";
		exit 1;
	}

	my $base_dir = $ARGV[0];

	if(! ($base_dir =~ /\/$/))
	{
		$base_dir .= '/';
	}

	while(<STDIN>)
	{
		s/^/$base_dir/;
		s/ / $base_dir/g;

		print $_;
	}

exit 0;

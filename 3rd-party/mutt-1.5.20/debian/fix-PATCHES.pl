#!/usr/bin/perl -wi

use strict;

my ($PATCHES, @patches);
while (<>) {
	if (/^(---|Index:|diff) .*PATCHES/) {
		$PATCHES = 1;
	} elsif (/^(-|Index|diff)/) { # next patch
		$PATCHES = 0;
		print;
	} elsif ($PATCHES) {
		next if /^(===|\+\+\+|@@)/; # patch header
		next if /^ /; # context
		push @patches, $_;
	} else {
		print;
	}
	if (eof) {
		if (@patches) {
			print "--- a/PATCHES\n";
			print "+++ b/PATCHES\n";
			print "@@ -0,0 +" . scalar (@patches) . " @@\n";
			print @patches;
		}
		@patches = ();
		$PATCHES = 0;
	}
}


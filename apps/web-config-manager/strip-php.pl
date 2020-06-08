#!/usr/bin/perl
use strict;

{

	if(!(-e 'index.php' && -e 'phpliteadmin.php' && -e 'inc'))
	{
		print STDERR "Sanity check failed: Expected files are not present in working directory.\n";
		print STDERR "Expected to be ran from the web-config-manager \"var/www/htdocs\" directory.\n";
		exit 1;
	}

	open my $F, "find|grep '\\.\\(php\\|inc\\)\$'|";

	while(<$F>)
	{
		s/\n//;
		my $file = $_;

		print "Stripping \"$file\"...\n";
		`php5 -w "$file" > "${file}.tmp"; mv "${file}.tmp" "$file"`;
	}

}

exit 0;

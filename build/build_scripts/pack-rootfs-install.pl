#!/usr/bin/perl
use strict;

my $savedir = ($ARGV[0] ne '') ? $ARGV[0] : "../embedded-applications/mx28/rootfs-install";
$savedir =~ s/\n//;
if( ! -e $savedir) {
	print STDERR "\"$savedir\" does not exist\n";
	exit 1;
}
print "$savedir\n";

my $build_user = $ENV{'SUDO_USER'};
if( '' eq $build_user) {
	$build_user = $ENV{'USER'};
	if( '' eq $build_user) {
		print STDERR "build_user is not set (SUDO_USER and USER are not set)\n";
		exit 1;
	}
}

my $workdir = "/tmp/redstone-$build_user/pack-rootfs/$$";
`mkdir -p "$workdir"`;

	my $src = "/srv/rootfs-redstone-install-$build_user";
	my $tar = "$workdir/r.tar";

	# Find changes
		`rm -f "$savedir/rootfs-install.tar"`;
		if (-e "$savedir/.svn")
		{
			`cd "$savedir";svn update`;
		}
		else
		{
			`cd "$savedir";git-checkout "$savedir/rootfs-install.tar"`;
		}
		`cp -v "$savedir/rootfs-install.tar" "$workdir/"`;
		if($?) {
			print STDERR "Failed to copy reference rootfs\n";
			exit 1;
		}
		`mkdir -p "$workdir/ref"`;
		`sudo tar -C "$workdir/ref" -xf "$workdir/rootfs-install.tar"`;
		if($?) {
			print STDERR "Failed to untar reference rootfs\n";
			exit 1;
		}
		`sudo diff -ru "$workdir/ref/" "$src/" | grep -v "is a character special file\$" > "$savedir/changes.txt"`;
		if($?) {
			print STDERR "Failed to create rootfs differences\n";
			exit 1;
		}

	open my $FILES, ">$workdir/files.txt";
	open my $F, "cd \"$src\";sudo find|sort|sed 's/\\.\\///'|";
	while(<$F>) {
		s/\n//;
		if((-d "$src/$_")&&(!-l "$src/$_")) {
			if('' eq `sudo ls -1A "$src/$_"`) {
				print $FILES "$_\n";
			}
			
		} else {
			print $FILES "$_\n";
		}
	}
	`sudo tar -C "$src" -cf "$tar" -T "$workdir/files.txt"`;
	`mv -v "$workdir/r.tar" "$savedir/rootfs-install.tar"`;
	`sudo rm -rf "$workdir"`;

exit 0;

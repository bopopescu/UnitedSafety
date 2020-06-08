#!/usr/bin/perl
use strict;

my $savedir = ($ARGV[0] ne '') ? $ARGV[0] : "../embedded-applications/mx28/rootfs";
$savedir =~ s/\n//;
if( ! -e $savedir) {
	print STDERR "\"$savedir\" does not exist\n";
	exit 1;
}
print "$savedir\n";

my $build_user=$ENV{'M2M_BUILD_USER'};
if('' eq "$build_user") {
	$build_user=$ENV{'SUDO_USER'};
	if('' eq "$build_user") {
		$build_user=$ENV{'USER'};
		if('' eq "$build_user") {
			print STDERR  "M2M_BUILD_USER is not set (SUDO_USER and USER are not set)\n";
			exit 1;
		}
	}
}
print "Running as \"$build_user\"...\n";

my $workdir = "/tmp/redstone-$build_user/pack-rootfs/$$";
`mkdir -p "$workdir"`;

{
	my $src = "/srv/rootfs-redstone-def-$build_user";
	my $tar = "$workdir/r.tar";

	# Find changes
	my $rfs=('' ne $ENV{'USE_ROOTFS_FILE'}) ? $ENV{'USE_ROOTFS_FILE'} : 'rootfs-def.tar';
	`rm -f "$savedir/$rfs"`;

	if (-e "$savedir/.svn")
	{
		my $output = `cd "$savedir" && svn update 2>&1`;
		if(0 != "$?")
		{
			print STDERR "Failed to run \"svn update\" on \"$savedir/$rfs\"\n";
			print STDERR "Error msg: $output\n";
			exit 1;
		}
	}
	else
	{
		my $output = `cd "$savedir" && git-checkout "$rfs" 2>&1`;
		if(0 != "$?")
		{
			print STDERR "Failed to run \"git-checkout\" on \"$savedir/$rfs\"\n";
			print STDERR "Error msg: $output\n";
			exit 1;
		}
	}
	`cp -v "$savedir/$rfs" "$workdir/"`;
	if($?) {
		print STDERR "Failed to copy reference rootfs\n";
		exit 1;
	}
	`mkdir -p "$workdir/ref"`;
	`sudo tar -C "$workdir/ref" -xf "$workdir/$rfs"`;
	if($?) {
		print STDERR "Failed to untar reference rootfs\n";
		exit 1;
	}
	`sudo diff -ru "$workdir/ref/" "$src/" 2>&1 | grep -v ': No such file or directory\$' > "$workdir/changes.tmp"`;
	`grep -v "is a \\(character\\|block\\) special file\$" "$workdir/changes.tmp"  > "$savedir/changes.txt"`;

	# Source
	my $pwd = `pwd|tr -d '\\n'`;
	chdir "$src";

	`cd "$src";file \`sudo find\`|grep 'character special'|sed 's/:.*//' > "$workdir/src-character-special.list"`;
	`ls -l \`cat $workdir/src-character-special.list\` > "$workdir/src-character-special.txt"`;

	`cd "$src";file \`sudo find\`|grep 'block special'|sed 's/:.*//' > "$workdir/src-block-special.list"`;
	`ls -l \`cat $workdir/src-block-special.list\` > "$workdir/src-block-special.txt"`;

	# Reference
	chdir "$workdir/ref";

	`cd "$workdir/ref";file \`sudo find\`|grep 'character special'|sed 's/:.*//' > "$workdir/ref-character-special.list"`;
	`ls -l \`cat $workdir/ref-character-special.list\` > "$workdir/ref-character-special.txt"`;

	`cd "$workdir/ref";file \`sudo find\`|grep 'block special'|sed 's/:.*//' > "$workdir/ref-block-special.list"`;
	`ls -l \`cat $workdir/ref-block-special.list\` > "$workdir/ref-block-special.txt"`;

	chdir "$workdir";

	`diff -u ref-character-special.txt src-character-special.txt > changes.tmp`;
	`diff -u ref-block-special.txt src-block-special.txt >> changes.tmp`;

	chdir "$pwd";

	`cat "$workdir/changes.tmp" >> "$savedir/changes.txt"`;

	open my $FILES, ">$workdir/files.txt";
	open my $F, "cd \"$src\";sudo find|sort|sed 's/\\.\\///'|";

	while(<$F>)
	{
		s/\n//;

		if((-d "$src/$_")&&(!-l "$src/$_"))
		{

			if('' eq `sudo ls -1A "$src/$_"`)
			{
				print $FILES "$_\n";
			}

		}
		else
		{
			print $FILES "$_\n";
		}

	}

	`sudo tar -C "$src" -cf "$tar" -T "$workdir/files.txt"`;
	`mv -v "$workdir/r.tar" "$savedir/$rfs"`;
}

`sudo rm -rf "$workdir"`;
exit 0;

#!/usr/bin/perl
###############################################################################
# Usage: ./build.pl src-dir=<source directory> [rev=<SVN Revision>] [build_all]
# 
#        src-dir   - main path to the source code files
#        rev       - SVN Revision to build
#        build_all - flag to force building all components
###############################################################################
use strict;
use POSIX ":sys_wait_h";
use IO::Handle;
use Term::ANSIColor qw(:pushpop);
use Cwd;

my $g_project_name = 'redstone';

#sub use_svn(\%)
#{
#	my $context = $_[0];
#	return (defined $context->{'f_no_svn'}) ? 0 : 1;
#}

sub get_branch_id(\%$)
{
	my $context = $_[0];
	my $src_dir = $_[1];

#	if(use_svn(%$context))
#	{
#		my $svn_cmd = $context->{'svn_cmd'};
#		my $id = `cd "$src_dir"; if [ 0 = \$? ];then $svn_cmd info|grep "^URL: .*Branches/"|sed 's/.*Branches\\/\\([^\\/]\\+\\)\\(\\/[^\\/]\\+\\)\\?.*/\\1\\2/'; else echo -1; fi|tr -d '\n'|tr -d '\r'`;
#		$id =~ s/\//-/;

#		if($id =~ /-release$/)
#		{
#			$id =~ s/-release$//;
#		}

#		return $id;
#	}

	return '';
}

sub set_default_flag(\%$$)
{
	my $context = $_[0];
	my $item = $_[1];

	if(!defined $context->{$item})
	{
		$context->{$item} = $_[2];
	}

}

sub svn_path_to_url(\%$)
{
	my $context = $_[0];
	my $path = $_[1];
	my $url = `cd '$path'; if [ 0 = \$? ];then svn info|grep '^URL: http:'|tr -d '\\n'|sed 's/^URL: //'; else echo -1; fi`;
	return (-1 == $url) ? '' : $url;
}

sub full_item_path_to_relative(\%$)
{
	my $context = $_[0];
	my $path = $_[1];

	my $p = $context->{'embedded_apps_dir'};

	$path =~ s/^$p//;
	$path =~ s/^\/+//;
	return $path;
}

#============================================================================
# get_revision
#
# Description: Returns the current and last changed SVN revision for
#	"src_dir".
#
#	"src_dir" is the full path to the directory to get the SVN version
#	from.
#
# Return: Returns a revision array. Index 0 contains the current revision
#	for "src_dir", and index 1 contains the revision of the last change
#	to "src_dir".
#
#	If an error occurs then index 0 (the revision) will be set to -1.
#============================================================================
sub get_revision(\%$)
{
	my $context = $_[0];
	my $src_dir = $_[1];

	my $url;
	my $rev;
	my $last_changed_rev;

	if(use_svn(%$context))
	{
		my $svn_cmd = $context->{'svn_cmd'};

		if($context->{'use_git'})
		{
			$rev = `cd "$src_dir"; if [ 0 = \$? ];then $svn_cmd info|grep "^Revision:"|sed 's/.*: \\+\\(.*\\)/\\1/'; else echo -1; fi`;

			if($rev > -1)
			{
				$last_changed_rev = `cd "$src_dir";$svn_cmd info|grep "^Last Changed Rev:"|sed 's/.*: \\+\\(.*\\)/\\1/'`;
			}

			$rev =~ s/\n//;
			$last_changed_rev =~ s/\n//;
		}
		else
		{
			my $embedded_apps_dir = $context->{'embedded_apps_dir'};
			my $p = $src_dir;
			$p =~ s/^($embedded_apps_dir)//;

			$url = `cd '$embedded_apps_dir'; if [ 0 = \$? ];then svn info|grep '^URL: http:'|tr -d '\\n'|sed 's/^URL: //'; else echo -1; fi` . $p;

			if(-1 == $url)
			{
				$rev = -1;
			}
			else
			{
				open my $info, '-|', "svn info '$url'";

				while(<$info>)
				{

					if(/^Revision: ([0-9]+)/)
					{
						$rev = $1;
					}
					elsif(/^Last Changed Rev: ([0-9]+)/)
					{
						$last_changed_rev = $1;
					}

				}

				close $info;
			}

		}

	}

	return ($rev,$last_changed_rev,$url);
}

#============================================================================
# gen_revision
#
# Description: Same as "get_revision" except that "context->{'rev'}" will override
#	the revision used for the build (if it is specified), and the revision
#	cache in "build_dir/revisions/" will be updated with the revision in use.
#
# Parameters:
#
#	build_dir - If not the empty string, then the revision cache in
#	            "build_dir/revisions/" will be updated with the revision in use.
#
# Return: Returns a revision array. Index 0 contains the current revision
#	for "src_dir", and index 1 contains the revision of the last change
#	to "src_dir".
#
#	If an error occurs then index 0 (the revision) will be set to -1.
#============================================================================
sub gen_revision(\%$$$)
{
	my $context = $_[0];
	my $build_dir = $_[1];
	my $src_dir = $_[2];
	my $src_name = $_[3];

	my @rev = get_revision(%$context, $src_dir);

	if(-1 == $rev[0])
	{
		return @rev;
	}

	if(defined $context->{'rev'})
	{
		$rev[0] = $context->{'rev'};
	}

	if(-e "$build_dir/revisions")
	{
		open my $f, ">>$build_dir/revisions/$rev[1]";
		print $f "$src_name"
			,"\n\tcurrent-rev: $rev[0]"
			,"\n\t",`date`
			,"\n";
		close $f;
	}

	return @rev;
}

#============================================================================
# is_src_cached
#
# Description: Returns true if the desired source revision has already been
#	exported from SVN. False is returned otherwise, and the desired
#	revision must be exported from the SVN source tree.
#
#	This function is used to speed up the build process by not requiring
#	exporting of source code that has not changed.
#============================================================================
sub is_src_cached(\% $ $)
{
	my $context = $_[0];
	my $item_name = $_[1];
	my $src_dir = $_[2];
	my $build_dir = $context->{'build_dir'};

	# Get last changed rev from cache.
	my $cache_ver = `head -n2 "$build_dir/src_cache/$item_name/trulink_rev" 2>/dev/null|tail -n1|tr -d '\n'`;

	if(1 == $context->{'flags'}->{'norev'})
	{
		print "[norev build] ignoring cache\n";
		return 0;
	}
	elsif('' eq "$cache_ver")
	{
		print "source \"$src_dir\" is not cached\n";
		return 0;
	}
	else
	{
		print "cache_ver=$cache_ver\n";
	}

	my @rev = get_revision(%$context, $src_dir);

	if(-1 == $rev[0])
	{
		return 0;
	}

	print "svn revision=$rev[0], last changed=$rev[1]\n";

	my $override_rev = defined $context->{'rev'} ? 1 : 0;
	if($override_rev) {
		$rev[0] = $context->{'rev'};
		if($cache_ver == $rev[0]) {
			print "source \"$src_dir\" is cached at user selected rev\n";
			return 1;
		} else {
			print "source \"$src_dir\" is not cached at $cache_ver (user selected rev $rev[0])\n";
		}

	} else {
		if($cache_ver >= $rev[1]) {
			print "source \"$src_dir\" is cached\n";
			return 1;
		} else {
			print "source \"$src_dir\" is not cached\n";
		}
	}

	return 0;
}

#============================================================================
# safe_build_dir
#
# Description: Returns 1 (true) if the given directory is safe for build output,
#	and 0 (false) is returned if the directory is not safe.
#
#	Only a users home directory and the "/tmp/<sub dir>" directory are considered
#	safe. This is because the given directory may be modified with root permissions.
#============================================================================
sub safe_build_dir($)
{
	my $dir = $_[0];
	my $safe = 0;
	my $cwd = cwd();

	{
		chdir $dir;
		my $cwd = cwd();

		if(($cwd =~ /\/home\/[a-zA-Z0-9_-]+\/.*\/builddir(\/.*)?$/) or ($cwd =~ /\/tmp\/[a-zA-Z0-9_-]+(\/.*)?$/))
		{
			$safe = 1;
		}

	}

	if(!chdir($cwd))
	{
		print STDERR PUSHCOLOR RED;
		print STDERR "safe_build_dir: chdir(\"$cwd\") failed\n";
		print STDERR POPCOLOR;
		exit 1;
	}

	return $safe;
}

sub h_export_src(\%$$$)
{
	my $context = $_[0];
	my $rev = $_[1];
	my $src_dir = $_[2];
	my $des_dir = $_[3];

	if('' eq $des_dir)
	{
		print STDERR "No des_dir specified for \"$src_dir\", rev=\"$rev\"\n";
		return 1;
	}

	if(-d $des_dir)
	{

		if(!safe_build_dir($des_dir))
		{
			print STDERR PUSHCOLOR RED;
			print STDERR "h_export_src: Unsafe destination directory \"$des_dir\"\n";
			print STDERR POPCOLOR;
			exit 1;
		}

		`rm -rf "$des_dir"`;
	}

	if(1 == $context->{'flags'}->{'norev'})
	{
		`cp -a '$src_dir' '$des_dir'`;
		`sudo touch '$des_dir'/*`;

		if(0 == $?)
		{
			return 0;
		}

	}

	my $work_dir = $context->{'work_dir'};
	my $svn_cmd = $context->{'svn_cmd'};

	if($context->{'use_git'})
	{
		my $git_hash = `cd "$src_dir" && git-svn find-rev r$rev`;

		if((0 != $?) || ('' eq $git_hash))
		{
			print STDERR "h_export_src: SVN Revision $rev is not part of GIT repo \"$src_dir\"\n";
			print STDERR "git-svn info:\n", `git-svn info`;
			return 1;
		}

		$git_hash  =~ s/\n//;
		print `mkdir -p \"$des_dir\"`;

		if(0 != $?)
		{
			print STDERR "$? returned while creating directory \"$des_dir\"\n";
			return 1;
		}

		print "SRC_DIR=$src_dir\n";
		print `cd "$src_dir" && git-archive $git_hash > \"$work_dir/git.archive\"`;
		my $ret_git_archive=$?;

		if(0 != $ret_git_archive)
		{
			my $ret = `cd "$src_dir" && git-status --porcelain . 2&>1`;

			if(0 != $?)
			{
				print STDERR "$ret_git_archive returned while exporting via \"git-archive $git_hash\" src at \"$src_dir\"\n";
				return 1;
			}

			$ret =~ s/\n//;

			if('' == $ret)
			{
				print STDERR "\"$src_dir\" is not in revision $rev (git-hash=$git_hash)\n";
				return 2;
			}

		}

		print `cat \"$work_dir/git.archive\" | tar -C "$des_dir" -x`;

		if(0 != $?)
		{
			print STDERR "$? returned while extracting git-archive at \"$work_dir/git.archive\" for src \"$src_dir\"\n";
			return 1;
		}

	}
	else
	{
		my $src_url = $context->{'embedded_apps_svn_url'} . '/' . full_item_path_to_relative(%$context, $src_dir);
		`$svn_cmd export -r $rev '$src_url' '$des_dir'`;

		if(0 != $?)
		{
			print STDERR "h_export_src: $? returned while exporting SVN repo \"$src_dir\"\n";
			return 1;
		}

	}

	return 0;
}

# Description:
#
# Return:
#	0 is returned if the source item was skipped (not in revision for example)
#	1 is returned if the source item was exported
#	2 is returned if the source item was not exported because it is already cached
#	A netgative value is returned on error.
sub export_source
{
	my ($arg) = @_;

	my $context = $arg->{'context'};
	my $item_name = $arg->{'item_name'};
	my $item_src_dir = $arg->{'item_src_dir'};
	my $des_dir = $arg->{'des_dir'};

	my $build_dir = ('' eq $des_dir) ? $context->{'build_dir'} : '';
	my $build_user = $context->{'build_user'};

	my $skip = 0;
	my $result = 0;

	if((defined $arg->{'force'}) or (!is_src_cached(%$context, $item_name, $item_src_dir)))
	{
		print (((1 == $context->{'flags'}->{'norev'}) ? "Copying" : "Exporting") . " $item_name...\n");
		my @rev = gen_revision(%$context, $build_dir, $item_src_dir, $item_name);

		if(-1 == $rev[0])
		{
			$skip = 1;
		}
		else
		{
			my $ret = h_export_src(%$context, $rev[0], $item_src_dir, ('' ne $build_dir) ? "$build_dir/$item_name" : $des_dir);

			if(0 == $ret)
			{

				if(0 != "$?")
				{
					print STDERR "export_source: $? returned while exporting $item_name (skip with \"build_${item_name}=0\")\n";
					return -1;
				}

				if('' ne $build_dir)
				{
					`mkdir -p "$build_dir/src_cache/$item_name"`;
					open my $f, ">$build_dir/src_cache/$item_name/trulink_rev";
					print $f "$rev[0]\n$rev[1]\n$rev[2]\n";
					close $f;
				}

				$result = 1;
			}
			elsif(2 == $ret)
			{
				$skip = 1;
			}
			else
			{
				return -1
			}

		}

	}
	else
	{
		$result = 2;
	}

	return $result;
}

sub skip_item(\% $ $)
{
	my $context = $_[0];
	my $item_name = $_[1];
	my $reason = $_[2];

	$context->{'stats'}->{'item'}->{'skip_count'} += 1;
	push(@{$context->{'stats'}->{'item'}->{'skipped'}}, $item_name);

	print PUSHCOLOR BLUE;
	print "\nSkipping $item_name: $reason...\n";
	print POPCOLOR;
}

sub build_item(\% $ $)
{
	my $context = $_[0];
	my $item_name = $_[1];
	my $item_src_dir = $_[2];

	if(!defined $context->{'first_item'})
	{
		$context->{'first_item'} = $item_name;

		foreach my $f (keys %{$context->{'build_flag'}})
		{

			if(!defined $context->{$f})
			{
				print PUSHCOLOR RED;
				print "\nUnknown build flag \"", $f, "\"\n";
				print POPCOLOR;
				exit 1;
			}

		}

	}

	# ATS FIXME: Do not use "date" since system time changes will give incorrect timings. Use a
	#	monotonically increasing (always increasing) counter (such as CPU clock ticks since power up).
	my $i_time = `date +'%s'`;
	my $ret = h_build_item($context, $item_name, $item_src_dir);
	my $f_time = `date +'%s'`;

	my $t = $context->{'stats'}->{'item'}->{'build_time'}->{$item_name} = $f_time - $i_time;

	print "Completed in $t ", ($t != 1) ? "seconds" : "second", "\n";

	return $ret;
}

sub h_build_item(\% $ $)
{
	my $context = $_[0];
	my $item_name = $_[1];
	my $item_src_dir = $_[2];
	my $build_dir = $context->{'build_dir'};
	my $build_user = $context->{'build_user'};

	my $build_flag = 'build_' . $item_name;
	$build_flag =~ s/[-.]/_/g;

	# Override build flag with command-line parameter version
	if(defined $context->{'build_flag'}->{$build_flag})
	{
		$context->{$build_flag} = $context->{'build_flag'}->{$build_flag};
	}
	elsif(1 == $context->{'flags'}->{'build_all'})
	{
		$context->{$build_flag} = 1;
	}

	if($context->{$build_flag} <= 0)
	{
		skip_item(%$context, $item_name, "Build flag \"$build_flag\" is set to 0");
		return 0;
	}

	print PUSHCOLOR YELLOW
		"\n",
		"#=================================================================\n",
		"#= ";
	print PUSHCOLOR GREEN "Export and build $item_name\n";
	print POPCOLOR
		"#=================================================================\n";
	print POPCOLOR;

	my $result = export_source({
		context => \%$context,
		item_name => $item_name,
		item_src_dir => $item_src_dir});

	my $skip = 0;

	if(0 == $result)
	{
		$skip = 1;
	}
	elsif($result < 0)
	{
		exit 1;
	}

	if($skip)
	{
		skip_item(%$context, $item_name, "Skipping $item_name since it is not in this revision\n");
	}
	else
	{
		my $cmd = "export 'REDSTONE_BUILD_DIR=$build_dir'"
			. " 'REDSTONE_BUILD_USER=$build_user'"
			. " 'BUILD_DIR=$build_dir'"
			. " 'APP_BUILD_DIR=$build_dir/$item_name'"
			. " 'ROOTFS_DEF_DIR=$build_dir/rootfs-def'"
			. " 'INSTALL=1'"
			. " 'PROJECT_NAME=" . ${context}->{project}->{name} . "'"
			. " 'PROJECT_DISPLAY_NAME=" . ${context}->{project}->{display_name} . "'"
			. " 'REDSTONE_BUILD_INCLUDE_DIR=$build_dir/include'"
			. ";";

		if(-e "./scripts/build-${item_name}.sh" and (1 == $context->{'flags'}->{'use-deprecated-build-scripts'}))
		{
			print STDERR PUSHCOLOR MAGENTA "\n\nWARNING: Using deprecated build script \"./scripts/build-${item_name}.sh\", please remove this script...\n";
			print STDERR POPCOLOR;
			$cmd .= "./scripts/build-${item_name}.sh";
		}
		else
		{
			$cmd .= "./scripts/build-app.sh";
		}

		my $output=`TRULINK_JOBS="$context->{j}";$cmd`;

		if(0 != "$?")
		{
			print STDERR PUSHCOLOR RED "\nBuild Command: ";
			print STDERR POPCOLOR "$cmd\n\n";

			print STDERR "$output\n";

			print STDERR PUSHCOLOR RED "Failed to build $item_name (can skip with \"$build_flag=0\")\n";
			print STDERR POPCOLOR;
			exit 1;
		}

		$context->{'stats'}->{'item'}->{'build_count'} += 1;
		push(@{$context->{'stats'}->{'item'}->{'built'}}, $item_name);
	}

	return 0;
}

sub join_parallel_builds(\%)
{
	my $context = $_[0];
	my $pid;

	do
	{
		$pid = waitpid(-1, 0);

		if('' ne $context->{'pid'}->{$pid})
		{
			my $item_name = $context->{'pid'}->{$pid};
			my $f = $context->{'build'}->{$item_name}->{'pipe'}->{'r'};

			while(<$f>)
			{
				s/\n//;

				if(!/^[^:print:]*#/)
				{
					print "\t";
				}

				print $_, "\n";
			}
			print "Build $context->{pid}->{$pid} completed\n";

		}
		elsif($pid >= 0)
		{
			print "Process $pid completed\n";
		}

	}
	while $pid >= 0;

}

sub build_parallel_item(\% $ $)
{
	my $context = $_[0];
	my $item_name = $_[1];
	my $path = $_[2];

	if(!defined $context->{'flags'}->{'parallel'})
	{
		build_item(%$context, $item_name, $path);
		return;
	}

	pipe
		$context->{'build'}->{$item_name}->{'pipe'}->{'r'},
		my $write;

	print "Parallel Build: $item_name...\n";
	my $pid = fork();

	if(!$pid)
	{
		select($write);
		STDERR->fdopen($write,  'w' ) or die $!;
		build_item(%$context, $item_name, $path);
		exit 0;
	}

	$context->{'pid'}->{$pid} = $item_name;
}

#
# Standard call would be 
#  cd ~/RedStone/Software/build
#  ./build.pl srcdir=/home/dave/svn/tl5000 revName=$rev_name
#
{
	my %context;
	$context{'stats'}->{'item'}->{'skip_count'} = 0;
	$context{'stats'}->{'item'}->{'skipped'} = [];

	my $build_user = $ENV{'SUDO_USER'};

	if('' eq $build_user)
	{
		$build_user = $ENV{'USER'};

		if('' eq $build_user)
		{
			print STDERR "Could not determine build user (\"SUDO_USER\" and \"USER\" is not set)\n";
			exit 1;
		}

		$context{'build_user'} = $build_user;
	}

	#=================================================================
	#= Default settings
	#= srcdir will be the Redstone/Software/build directory
	#=================================================================
		my $work_dir = "/tmp/ats-build-$build_user/$$";
		my $build_rootfs = 1;
		$context{'srcdir'} = `pwd|tr -d '\n'` . '/../';
		$context{'unpack_rootfs'} = `echo -n "\$PWD"` . "/unpack-rootfs.sh";

		# ATS FIXME: Remove "use-deprecated-build-scripts" once all current software supports
		#	using "build-apps.sh". For now, new versions will set "use-deprecated-build-scripts" to
		#	to 0 (so no special changes are needed to re-build old software).
		$context{'flags'}->{'use-deprecated-build-scripts'} = 1;
		$context{'flags'}->{'build_all'} = 0;

		$context{'build_flag'} = {};
		$context{'j'} = int(`nproc` * 2);
		# End of default settings ================================

	#=================================================================
	#= Parse arguments
	#=================================================================
		{
			my $i = 1;

			my $known_value =
				{
					'srcdir' => 1,
					'rev'    => 1,
					'j'      => 1,
					'f_no_svn' => 1,
					'revName' => 1
				};

			my $known_flag =
				{
					'build_all' => 1,
					'no-nfs' => 1,
					'norev'  => 1,
					'parallel'  => 1
				};

			foreach my $arg (@ARGV)
			{

				if($arg =~ /^([^=]+)= *(.*)/)
				{
					my $key = $1;
					my $val = $2;

					if(!defined $known_value->{$key})
					{

						if($key =~ /^build_/)
						{
							$context{'build_flag'}->{$key} = $val;
						}
						else
						{
							print "Unknown value \"";
							print PUSHCOLOR RED "$key";
							print POPCOLOR "\"\n";
							exit 1;
						}

					}
					else
					{
						$context{$key} = $val;
					}

				}
				else
				{

					if(!defined $known_flag->{$arg})
					{
						print "Unknown flag \"";
						print PUSHCOLOR RED "$arg";
						print POPCOLOR "\"\n";
						exit 1;
					}

					$context{'flags'}->{$arg} = 1;
				}

				++$i;
			}

		}

	#=================================================================
	#= Prepare parameter dependant variables.
	#=================================================================
		my $srcdir = $context{'srcdir'};

		if(! -e "$srcdir")
		{
			print STDERR "Source directory \"$srcdir\" does not exist\n";
			exit 1;
		}

	#=================================================================
	#= Setup paths to source modules
	#=================================================================
		my $embedded_apps_dir = (-e "$srcdir/embedded-applications") ? "$srcdir/embedded-applications" : "$srcdir";
		$context{'embedded_apps_dir'} = $embedded_apps_dir;
		my $mx28_dir = "$embedded_apps_dir/mx28";
		my $rootfs_dir = "$mx28_dir/rootfs";
		# End of paths to source ================================

	#=================================================================
	#= Prepare parameters
	#=================================================================
		`mkdir -p "$work_dir"`;
		$context{'work_dir'} = $work_dir;

	#=================================================================
	#= Prepare build
	#=================================================================
		print "Max concurrent jobs for make/build: $context{j}\n";

		if(-e "$embedded_apps_dir/.svn")
		{
			$context{'svn_cmd'} = 'svn';
			$context{'embedded_apps_svn_url'} = svn_path_to_url(%context, $context{'embedded_apps_dir'});
		}
		else
		{
			$context{'svn_cmd'} = 'git-svn';
			$context{'use_git'} = 1;
		}

		{
			$context{'build_config'} = 1;
			my $des_dir = "$work_dir/";
			my $result = export_source({
				context => \%context,
				item_name => 'config',
				item_src_dir => "$embedded_apps_dir/config",
				des_dir => "$des_dir"});

			if(($result > 0) and -e "$des_dir/config.pl")
			{
				print "Using local config.\n";
				require "$des_dir/config.pl";
			}
			else
			{
				print "Using default config.\n";
				require "default-config.pl";
			}

			if(defined &init_config)
			{
				init_config({
					'context' => \%context});
			}

			if(!defined $context{'project'})
			{
				$context{'project'}->{'display_name'} = 'RedStone';
				$context{'project'}->{'name'} = 'redstone';
				$context{'project'}->{'encrypt_user'} = 'build@redstone.atsplace.com';
				$context{'project'}->{'decrypt_user'} = 'redstone@redstone.atsplace.com';
			}

			if(defined $context{'project'}->{'name'})
			{
				$g_project_name = $context{'project'}->{'name'};
			}

		}

		$context{'build_dir'} = "/home/$build_user/$g_project_name/builddir";
		my $build_dir = $context{'build_dir'};
		print "Build directory \"$build_dir\n";

		if(! -e "$build_dir")
		{
			print STDERR "Build directory \"$build_dir\" does not exist\n";
			exit 1;
		}

		`rm -rf "$build_dir/revisions"`;
		`mkdir -p "$build_dir/revisions"`;
		`mkdir -p "$build_dir/firmware"`;

		if(defined $context{'flags'}->{'force'})
		{
			print "Removing cache file \"src-cache.txt\" to force build\n";
			`rm -f $build_dir/src-cache.txt`;
		}

	#=================================================================
	#= Determine revision for build (only use last changed rev)
	#=================================================================
		{
#			$context{'branch_id'} = get_branch_id(%context, $embedded_apps_dir);
			$context{'branch_id'} = '';

			if(defined $context{'rev'})
			{
				$context{'build_rev'} = $context{'rev'};
			}
			else
			{
				my @a = gen_revision(%context, $build_dir, $embedded_apps_dir, 'embedded-applications');
				$context{'build_rev'} = @a[1];
			}

			$context{'build_full_version'} = '';

			if($context{'branch_id'} != '')
			{
				$context{'build_full_version'} = "$context{branch_id}" . ((1 == $context{'flags'}->{'norev'}) ? '-norev.' : '.') . $context{'build_rev'};
			}
			else
			{
				$context{'build_full_version'} = ((1 == $context{'flags'}->{'norev'}) ? 'norev.' : '') . $context{'build_rev'};
			}

		}

	#=================================================================
	#= Export rootfs
	#=================================================================
		if($build_rootfs)
		{
			print "Building rootfs\n";
			my $item_name = 'Rootfs';
			my @rev = gen_revision(%context, $build_dir, $rootfs_dir, $item_name);

			if(-1 == $rev[0])
			{
				$context{'stats'}->{'item'}->{'skip_count'} += 1;
				push(@{$context{'stats'}->{'item'}->{'skipped'}}, $item_name);
				print "Rootfs is not versioned, skipping...\n";
			}
			else
			{

				if(!is_src_cached(%context, $item_name, $rootfs_dir))
				{
					print (((1 == $context{'flags'}->{'norev'}) ? "Copying" : "Exporting") . " rootfs...\n");
					if(0 != h_export_src(%context, $rev[0], $rootfs_dir, "$work_dir/rootfs-def"))
					{
						exit 1;
					}
					`sudo rm -rf "$build_dir/rootfs-def"`;
					`cd "$work_dir/rootfs-def" && "$context{'unpack_rootfs'}" "$work_dir/rootfs-def" "$build_dir/rootfs-def"`;
					`mkdir -p "$build_dir/src_cache/$item_name"`;
					open my $f, ">$build_dir/src_cache/$item_name/trulink_rev";
					print $f "$rev[0]\n$rev[1]\n$rev[2]\n";
					close $f
				}

				`echo "$context{build_rev}" > "$work_dir/version"`;
				`date >> "$work_dir/version"`;

				if(length $context{'revName'})
				{
					`echo "$context{'revName'}.$context{build_rev}" >> "$work_dir/version"`;
					print "Revision name:$context{'revName'}.$context{build_rev}\n";
				}
				else
				{
					`echo "$context{build_full_version}" >> "$work_dir/version"`;
					print "Revision name:$context{build_full_version}\n";
				}
				# i.MX28
					`sudo rm -f "$build_dir/rootfs-def/version"`;
					`sudo cp "$work_dir/version" "$build_dir/rootfs-def/version"`;
					`sudo chmod 444 "$build_dir/rootfs-def/version"`;

				$context{'stats'}->{'item'}->{'build_count'} += 1;
				push(@{$context{'stats'}->{'item'}->{'built'}}, $item_name);
			}
		}

	#=================================================================
	#= Build configuration
	#=================================================================
		build_config(\%context, $embedded_apps_dir);

		if(defined $context{'project'})
		{
			open my $conf, ">$build_dir/firmware/project.conf";

			foreach my $key (keys %{$context{'project'}})
			{
				print $conf "export $key=\"", $context{'project'}->{$key}, "\"\n";
			}

			close $conf;
		}

	#=================================================================
	#= Prepare firmware
	#=================================================================
	print
		"\n",
		"#=================================================================\n",
		"#= Preparing firmware...\n",
		"#=================================================================\n";

		`sudo tar -C "$build_dir/rootfs-def/" -cjf "$build_dir/firmware/def-rootfs-$g_project_name.tar.bz2" .`;

	#=================================================================
	#= Create a root file system for use with Network File System
	#=================================================================
		if(!defined $context{'flags'}->{'no-nfs'})
		{
			my $nfs_dir = "/srv/rootfs-$g_project_name-nfs-${build_user}";
			`sudo rm -rf "$nfs_dir"`;
			`sudo mkdir -p "$nfs_dir" && sudo tar -C "$nfs_dir" -xf "$build_dir/firmware/def-rootfs-$g_project_name.tar.bz2"`;
			my $output = `sudo ./to-nfs.sh "$nfs_dir" 2>&1`;
			if(0 != "$?") {
				print STDERR "Failed to create Network File System: $output";
				exit 1;
			}
			print "Created NFS at: \"$nfs_dir\"\n"
		}

	#=================================================================
	#= Cleaning up
	#=================================================================
		`rm -rf $work_dir`;

	print
		"\n",
		"Build Statistics for ", ((defined $context{'project'}->{'display_name'}) ? ($context{'project'}->{'display_name'} . ' ') : ''), "$context{build_full_version}:\n";

	if($context{'stats'}->{'item'}->{'build_count'} > 0)
	{
		print "\tItems Built: $context{stats}->{item}->{build_count}\n";

		my $total_time = 0;

		foreach my $item (@{$context{'stats'}->{'item'}->{'built'}})
		{
			my $t = $context{'stats'}->{'item'}->{'build_time'}->{$item};

			if('' ne $t)
			{
				$total_time += $t;
				print PUSHCOLOR GREEN "\t\t$item";
				print POPCOLOR;
				print ", $t ", ($t != 1) ? "seconds" : "second", "\n";
			}
			else
			{
				print PUSHCOLOR GREEN "\t\t$item\n";
				print POPCOLOR;
			}

		}

		print "\n\t\tTotal build time: $total_time ", ($total_time != 1) ? "seconds" : "second", "\n\n";
	}

	if($context{'stats'}->{'item'}->{'skip_count'} > 0)
	{
		print "\tItems Skipped: $context{stats}->{item}->{skip_count}\n";
		print PUSHCOLOR CYAN;

		foreach my $item (@{$context{'stats'}->{'item'}->{'skipped'}})
		{
			print "\t\t$item\n";
		}

		print POPCOLOR "\n";
	}

}

exit 0;

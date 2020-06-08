#!/usr/bin/perl
use strict;

{
	if(@ARGV < 1) {
		print STDERR "usage: $0 <bsp_name>\n";
		exit 1;
	}
	my $bsp_name = $ARGV[0];

	my $bsp_dir = "BSP_$bsp_name";
	if(-e $bsp_dir) {
		print STDERR "Directory \"$bsp_dir\" already exists\n";
		exit 1;
	}
	`mkdir -p $bsp_dir`;

	#============================================================
	#= bsp.pro
	#============================================================
	my $lib_name = "BSP_$bsp_name";
	my $pro_name = "BSP_$bsp_name.pro";
	my $header_name = "BSP_$bsp_name.h";
	my $cpp_name = "BSP_$bsp_name.cpp";
	my $qbsp_cpp_name = "QBSP_$bsp_name.cpp";
	my $qbsp_header_name = "QBSP_$bsp_name.h";
	{
		open my $F, "cat reference/bsp.pro|";
		open my $DES, ">$bsp_dir/$pro_name";
		while(<$F>) {
			if(/^TARGET = /) {
				print $DES "TARGET = $lib_name\n";

			} elsif(/^(\s+)bsp.cpp\\/) {
				print $DES "$1$cpp_name\\\n";

			} elsif(/^(\s+)qbsp.cpp(\\?)/) {
				print $DES "$1$qbsp_cpp_name$2\n";

			} elsif(/^(\s+)bsp.h\\/) {
				print $DES "$1$header_name\\\n";

			} elsif(/^(\s+)qbsp.h(\\?)/) {
				print $DES "$1$qbsp_header_name$2\n";

			} else {
				print $DES $_;
			}
		}
		close $DES;
		close $F;
	}

	#============================================================
	#= bsp.h
	#============================================================
	my $def_name = uc "BSP_${bsp_name}_h";
	my $namespace = "ns" . uc $bsp_name;
	my $class_name = "BSP_$bsp_name";
	{
		open my $F, "cat reference/bsp.h|";
		open my $DES, ">$bsp_dir/$header_name";
		while(<$F>) {
			if(/^#ifndef BSP_H/) {
				print $DES "#ifndef $def_name\n";

			} elsif(/^#define BSP_H/) {
				print $DES "#define $def_name\n";

			} elsif(/^#include "bsp_global.h"/) {
				print $DES $_;
				print $DES "#include \"$qbsp_header_name\"\n";

			} elsif(/^class QBsp;/) {
				<$F>;

			} elsif(/^class BSPSHARED_EXPORT Bsp {/) {
				print $DES "class BSPSHARED_EXPORT $class_name {\n";

			} elsif(/^(\s+)Bsp\(\);/) {
				print $DES "$1$class_name();\n";

			} elsif(/^(\s+)(QBsp \*m_bsp;.*)/) {
				print $DES "$1${namespace}::$2\n";

			} elsif(/^#endif \/\/ BSP_H/) {
				print $DES "#endif // $def_name\n";

			} else {
				print $DES $_;
			}
		}
		close $DES;
		close $F;
	}

	#============================================================
	#= bsp.cpp
	#============================================================
	{
		open my $F, "cat reference/bsp.cpp|";
		open my $DES, ">$bsp_dir/$cpp_name";
		while(<$F>) {
			if(/^#include "bsp.h"/) {
				print $DES "#include \"$header_name\"\n";

			} elsif(/^#include "qbsp.h"/) {
				print $DES "#include \"$qbsp_header_name\"\n";

			} elsif(/^Bsp::Bsp\(\)/) {
				print $DES "${class_name}::${class_name}()\n";

			} elsif(/^(\s+)m_bsp = new (QBsp\(\);.*)/) {
				print $DES "$1m_bsp = new ${namespace}::${2}\n";

			} else {
				print $DES $_;
			}
		}
		close $DES;
		close $F;
	}

	`cp reference/bsp_global.h $bsp_dir/`;
	`cp reference/.gitignore $bsp_dir/`;

	#============================================================
	#= qbsp.h
	#============================================================
	my $qbsp_def_name = uc "QBSP_${bsp_name}_h";
	{
		open my $F, "cat reference/qbsp.h|";
		open my $DES, ">$bsp_dir/$qbsp_header_name";
		while(<$F>) {
			if(/^#ifndef QBSP_H/) {
				print $DES "#ifndef $qbsp_def_name\n";

			} elsif(/^#define QBSP_H/) {
				print $DES "#define $qbsp_def_name\n";

			} elsif(/^class\s+QBsp\s+:\s+public\s+QObject/) {
				print $DES "namespace $namespace {\n\n";
				print $DES $_;

			} elsif(/^#endif/) {
				print $DES "}; // namespace $namespace\n";
				print $DES "#endif // $qbsp_def_name";

			} elsif(/^Bsp::Bsp\(\)/) {
				print $DES "${class_name}::${class_name}()\n";
				
			} else {
				print $DES $_;
			}
		}
		close $DES;
		close $F;
	}

	#============================================================
	#= qbsp.cpp
	#============================================================
	{
		open my $F, "cat reference/qbsp.cpp|";
		open my $DES, ">$bsp_dir/$qbsp_cpp_name";
		while(<$F>) {
			if(/^#include "qbsp.h"/) {
				print $DES "#include \"$qbsp_header_name\"\n\n";
				print $DES "using namespace $namespace;\n";

			} else {
				print $DES $_;
			}
		}
		close $DES;
		close $F;
	}
}
exit 0;

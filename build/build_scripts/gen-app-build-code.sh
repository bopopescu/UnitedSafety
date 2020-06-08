#!/bin/bash

APP_NAME="$1"
SRC_DIR="$2"
if [ '' = "$APP_NAME" ];then
	echo "Error: Application name not specified"
	echo "Usage: $0 <application name> <source dir>"
	exit 1
fi

if [ '' = "$SRC_DIR" ];then
	echo "Error: Source directory not specified"
	echo "Usage: $0 <application name> <source dir>"
	exit 1
fi

VAR_NAME=`echo "$APP_NAME"|tr '-' '_'|tr '.' '_'`
BUILD_VAR="build_${VAR_NAME}"
BUILD_DIR_NAME="$APP_NAME"
BUILD_SCRIPT_NAME="build-${APP_NAME}.sh"
OUTPUT="build-${APP_NAME}.pl"

if [ -e $OUTPUT ];then
	echo "File \"$OUTPUT\" already exists\n"
	exit 1
fi

MY_BUILD_USER=$SUDO_USER
if [ '' = "$MY_BUILD_USER" ];then
	MY_BUILD_USER=$USER
	if [ '' = $USER ];then
		exit 0
	fi
fi
PID=$$
WORK_DIR="/tmp/$USER/gen-build.pl/$PID"

	mkdir -p "$WORK_DIR"
	TMP_BUILD_PL="$WORK_DIR/build.pl"
	cp "./build.pl" "$TMP_BUILD_PL"

	out=`echo -e "\t\t\\\$context{'$BUILD_VAR'} = 1;" | ./append.pl "$TMP_BUILD_PL" "# End of default settings"`;
	echo "$out" > "$TMP_BUILD_PL"

	out=`echo -e "\t\tmy \\$${VAR_NAME}_dir = \"\\$embedded_apps_dir/$SRC_DIR\";" | ./append.pl "$TMP_BUILD_PL" "# End of paths to source"`
	echo "$out" > "$TMP_BUILD_PL"

	s="	build_item(%context, '$APP_NAME', \$${VAR_NAME}_dir);"

	out=`echo "$s" | ./append.pl "$TMP_BUILD_PL" "# End of build app"`
	echo "$out" > "$TMP_BUILD_PL"

	# Generating build shell script
	s="
#!/bin/bash

APP_NAME='$APP_NAME'

	if [ '' = \"\$BUILD_DIR\" ];then
		echo \"BUILD_DIR is not set\"
		exit 1
	fi
	if [ ! -e \"\$BUILD_DIR\" ];then
		echo \"Build directory \\\"\$BUILD_DIR\\\" does not exist\"
		exit 1
	fi

	if [ '' = \"\$APP_BUILD_DIR\" ];then
		echo \"APP_BUILD_DIR is not set\"
		exit 1
	fi
	if [ ! -e \"\$APP_BUILD_DIR\" ];then
		echo \"Build directory \\\"\$APP_BUILD_DIR\\\" does not exist\"
		exit 1
	fi

	cd \"\$APP_BUILD_DIR\"

	if [ ! -e \"./build.ats\" ];then
		echo \"Standard \\\"build.ats\\\" script is missing\"
		exit 1
	fi

	./build.ats
	ret=\$?
	if [ 0 != \"\$ret\" ];then
		echo \"[\$ret]: Failed to run \\\"build.ats\\\" for \$APP_NAME\"
		exit 1
	fi

exit 0
"
	echo "$s"|tail -n +2 > "./scripts/build-${APP_NAME}.sh"
	chmod 755 "./scripts/build-${APP_NAME}.sh"

	chmod 755 "$TMP_BUILD_PL"
	cp -v "$TMP_BUILD_PL" "./build.pl.out"
	echo "created ./build.pl.out"
	rm -rf "$WORK_DIR"

exit 0;

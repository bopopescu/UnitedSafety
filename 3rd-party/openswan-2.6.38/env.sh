export C_INCLUDE_PATH=`pwd`/../gmp-5.0.5/
export LIBRARY_PATH=`pwd`/../gmp-5.0.5/.libs/
export LD_LIBRARY_PATH=`pwd`/../gmp-5.0.5/.libs/
export CC="${CROSS_COMPILE}gcc"
export LD="${CROSS_COMPILE}ld"
export AS="${CROSS_COMPILE}as"
export CPP="${CROSS_COMPILE}gcc -E"
export CXX="${CROSS_COMPILE}g++"
export AR="${CROSS_COMPILE}ar"
export NM="${CROSS_COMPILE}nm"
export STRIP="${CROSS_COMPILE}strip"
export OBJCOPY="${CROSS_COMPILE}objcopy"
export OBJDUMP="${CROSS_COMPILE}objdump"

export DESTDIR=/tmp/openswan.arm

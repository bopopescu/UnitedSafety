#!/bin/sh

WIFI_INT=ra0
tag='wificonfig'

Usage()
{
	echo "$0 ESSID AuthMode EncrypType [Password]"
	echo "ESSID     : The Access Point ESSID"
	echo ""
	echo "AuthMode : OPEN, SHARED, WEPAUTO, WPAPSK, WPA2PSK, WPAPSKWPA2PSK"
	echo "           OPEN           For open system"
	echo "           SHARED         For shared key system"
	echo "           WEPAUTO        For open/shared key system"
	echo "           WPAPSK         For WPA pre-shared key"
	echo "           WPA2PSK        For WPA2 pre-shared key"
	echo "           WPAPSKWPA2PSK  WPAPSK/WPA2PSK mix mode"

	echo ""
	echo "EncrypType: NONE, WEP, TKIP, AES, TKIPAES"
	echo "           NONE           For AuthMode=OPEN"
	echo "           WEP            For AuthMode=OPEN or SHARED"
	echo "           TKIP           For AuthMode=WPAPSK, WPA2PSK or mix mode"
	echo "           AES            For AuthMode=WPAPSK, WPA2PSK or mix mode"
	echo "           TKIPAES        TKIP/AES Mixed"

	echo ""
	echo "Password : Depend on and EncrypType "
	echo "           If EncrypType=NONE, no need such argument."
	echo "           Else, password should be set as the KEY or WPAPSK password."
	echo ""
}


if [ $# -ne 3 -a $# -ne 4 ] ; then
	Usage;
	exit 1;
fi

ssid=$1
AuthMode=$2
EncrypType=$3

# If get 4 arguments, the 4th argument is the password
if [ $# -eq 4 ] ; then
	password=$4
fi

# EncrypType is not NONE, but without Password set. Then failure.
if [ $EncrypType != "NONE" ] && [ -z $password ]; then
	echo "@@ERROR: Miss Password"
	logger -t "$tag" "@@ERROR: Miss Password"
	exit 1;
fi

if [ $AuthMode = "OPEN" ] && [ $EncrypType != "NONE" ] && [ $EncrypType != "WEP" ]; then
	echo "@@ERROR: open system encryptype=none/wep"
	logger -t "$tag" "@@ERROR: open system encryptype=none/wep"
	exit 1;
fi

if [ $AuthMode = "SHARED" -o $AuthMode = "WEPAUTO" ] && [ $EncrypType != "WEP" ]; then
	echo "@@ERROR: SHARED/WEPAUTO system encryptype=wep"
	logger -t "$tag" "@@ERROR: SHARED/WEPAUTO system encryptype=wep"
	exit 1;
fi

if [ $AuthMode = "WPAPSK" -o $AuthMode = "WPA2PSK" -o $AuthMode = "WPAPSKWPA2PSK" ] && [ $EncrypType != "TKIP" ] && [ $EncrypType != "AES" ] && [ $EncrypType != "TKIPAES" ]; then
	echo "@@ERROR: WPAPSK/WPA2PSK/WPAPSKWPA2PSK system encryptype=AES/TKIP/TKIPAES"
	logger -t "$tag" "@@ERROR: WPAPSK/WPA2PSK/WPAPSKWPA2PSK system encryptype=AES/TKIP/TKIPAES"
	exit 1;
fi


is_valid_10_or_26_hex()
{
	case $1 in
       ( *[!0-9A-Fa-f]* | "" ) return 1 ;;
       ( * )
           case ${#1} in
	        ( 10 | 26 ) return 0 ;;
	        ( * )       return 1 ;;
	       esac
    esac
}

is_valid_64_hex()
{
	case $1 in
       ( *[!0-9A-Fa-f]* | "" ) return 1 ;;
       ( * )
           case ${#1} in
	        ( 64 ) return 0 ;;
	        ( * )  return 1 ;;
	       esac
    esac
}

if [ $EncrypType = "WEP" ]; then
	is_valid_10_or_26_hex $password
	result=$?
	if [ $result = 1 ];then
		len=$(echo ${#password})
		if [ $len -ne 5 -a $len -ne 13 ]; then
			echo "@@ERROR: WEP key: require 5/13 ASCII characters or 10/26 hex numbers"
			logger -t "$tag" "@@ERROR: WEP key: require 5/13 ASCII characters or 10/26 hex numbers"
			exit 1;
		fi
	fi
fi

if [ $EncrypType != "WEP" -a $EncrypType != "NONE" ]; then
	is_valid_64_hex $password
	result=$?
	if [ $result = 1 ];then
		len=$(echo ${#password})
		if [ $len -lt 8 -o $len -gt 63 ]; then
			echo "@@ERROR: WPAPSK: require  8~63 ascii Characters or 64 hex numbers"
			logger -t "$tag" "@@ERROR: WPAPSK: require  8~63 ascii Characters or 64 hex numbers"
			exit 1;
		fi
	fi
fi

iwpriv $WIFI_INT set AuthMode=$AuthMode
iwpriv $WIFI_INT set EncrypType=$EncrypType
iwpriv $WIFI_INT set IEEE8021X=0

if [ $AuthMode = "OPEN" ]; then
	if [ $EncrypType = "WEP" ]; then
	    iwpriv $WIFI_INT set DefaultKeyID=1
	    iwpriv $WIFI_INT set Key1=$password
	fi
	iwpriv $WIFI_INT set SSID="$ssid"
elif [ $AuthMode = "SHARED" -o $AuthMode = "WEPAUTO" ]; then
	iwpriv $WIFI_INT set DefaultKeyID=1
	iwpriv $WIFI_INT set Key1=$password
	iwpriv $WIFI_INT set SSID="$ssid"
else
	iwpriv $WIFI_INT set SSID="$ssid"
	iwpriv $WIFI_INT set WPAPSK=$password
	iwpriv $WIFI_INT set SSID="$ssid"
fi

exit 0

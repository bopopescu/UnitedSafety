i=0

while true
do
	printf "blink add name=install app=/usr/redstone/install-blink.so\r" |socat - unix-connect:/var/run/redstone/i2c-gpio-monitor

	if [ 0 = $? ];then
		break
	fi

	i=`expr $i + 1`

	if [ "$i" -gt 3 ];then
		break
	fi

	sleep 1
done

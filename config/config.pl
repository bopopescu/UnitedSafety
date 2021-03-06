use strict;

sub init_config
{
	my ($arg) = @_;
	my $context = $arg->{'context'};

	$context->{'project'}->{'display_name'} = 'TRULink';
	$context->{'project'}->{'name'} = 'trulink';
	$context->{'project'}->{'encrypt_user'} = 'build@redstone.atsplace.com';
	$context->{'project'}->{'decrypt_user'} = 'redstone@redstone.atsplace.com';
	$context->{'flags'}->{'use-deprecated-build-scripts'} = 0;
}

sub build_config(\% $)
{
	my $context = $_[0];
	my $embedded_apps_dir = $_[1];

	$context->{'build_include'} = 1;
	$context->{'build_kernel'} = 1;
	$context->{'build_rootfs_install'} = 1;
	$context->{'build_gcc'} = 1;
	$context->{'build_ethtool'} = 0;
	$context->{'build_strace'} = 0;
	$context->{'build_zlib'} = 1;
	$context->{'build_ats_common'} = 1;
	$context->{'build_socket_interface'} = 1;
	$context->{'build_command_line_parser'} = 1;
	$context->{'build_logger'} = 1;
	$context->{'build_state_machine'} = 1;
	$context->{'build_buzzer_monitor'} = 1;
	$context->{'build_redstone_socketcan'} = 1;
	$context->{'build_can_odb2_monitor'} = 1;
	$context->{'build_trip_stats'} = 1;
	$context->{'build_can_seatbelt_monitor'} = 0;
	$context->{'build_FastTrac'} = 1;
	$context->{'build_uboot'} = 1;
	$context->{'build_bootstream'} = 1;
	$context->{'build_modem_monitor'} = 1;
	$context->{'build_ppp_2_4_5'} = 0;
	$context->{'build_libb64_1_2'} = 1;
	$context->{'build_i2c_tools_3_0_2'} = 0;
	$context->{'build_rtc_monitor'} = 1;
	$context->{'build_app_monitor'} = 1;
	$context->{'build_admin_client'} = 1;
	$context->{'build_libsocketcan_0_0_8'} = 1;
	$context->{'build_socketCAN'} = 1;
	$context->{'build_ncurses_5_9'} = 1;
	$context->{'build_mutt_1_5_20'} = 0;
	$context->{'build_system_watchdog'} = 1;
	$context->{'build_power_monitor'} = 1;
	$context->{'build_sqlite3_3_6_22'} = 0;
	$context->{'build_db_monitor'} = 1;
	$context->{'build_port_forward'} = 1;
	$context->{'build_get_wakeup'} = 1;
	$context->{'build_Libs'} = 1;
	$context->{'build_connect_ppp'} = 1;
	$context->{'build_network_monitor'} = 1;
	$context->{'build_state'} = 1;
	$context->{'build_message_assembler'} = 1;
	$context->{'build_packetizer'} = 1;
	$context->{'build_trip_monitor'} = 1;
	$context->{'build_periodic_msg_gen'} = 1;
	$context->{'build_heartbeat'} = 1;
	$context->{'build_crit_battery_check'} = 1;
	$context->{'build_ats_ralink_ap_2_7_1_0'} = 0;
	$context->{'build_ats_ralink_sta_2_6_1_4'} = 0;
	$context->{'build_libxml2_2_9_0'} = 0;
	$context->{'build_openssl_1_0_1g'} = 0;
	$context->{'build_php_5_4_10'} = 0;
	$context->{'build_gmp_5_0_5'} = 0;
	$context->{'build_openswan_2_6_38'} = 0;
	$context->{'build_ipsec_monitor'} = 0;
	$context->{'build_avl_monitor'} = 1;
	$context->{'build_feature_monitor'} = 1;
	$context->{'build_telit_monitor'} = 1;
	$context->{'build_can_dodge_seatbelt_monitor'} = 0;
	$context->{'build_can_ford_seatbelt_monitor'} = 0;
	$context->{'build_can_gm_seatbelt_monitor'} = 0;
	$context->{'build_ignition_monitor'} = 1;
	$context->{'build_libpcap_1_3_0'} = 1;
	$context->{'build_web_config_manager'} = 1;
	$context->{'build_packetizer_lib'} = 1;
	$context->{'build_packetizer_dash_socket'} = 0;
	$context->{'build_packetizer_dash'} = 0;
	$context->{'build_packetizer_calamps'} = 0;
	$context->{'build_gps_socket_server'} = 0;
	$context->{'build_i2c_gpio_monitor'} = 1;
	$context->{'build_plc_serial_monitor'} = 1;
	$context->{'build_wifi_monitor'} = 1;
	$context->{'build_wifi_client_monitor'} = 1;
	$context->{'build_iproute2_j1939'} = 0;
	$context->{'build_expat_2_0_1'} = 1;
	$context->{'build_redstone_socketcan_j1939'} = 0;
	$context->{'build_can_j1939_monitor'} = 0;
	$context->{'build_skybase'} = 0;
	$context->{'build_packetizer_iridium'} = 0;
	$context->{'build_libmodbus_3_0_4'} = 1;
	$context->{'build_modbus_monitor'} = 1;
	$context->{'build_zigbee_monitor'} = 1;
	$context->{'build_iridium_monitor'} = 1;
	$context->{'build_packetizer_cams'} = 0;
	$context->{'build_isc_lens'} = 1;
	$context->{'build_packetizer_inet'} = 1;
	$context->{'build_isc_modbus'} = 1;

	# Custom RedStone Includes
	my $include_dir = "$embedded_apps_dir/apps/include";

	# RedStone Boot-ROM, U-Boot, Linux Kernel and Drivers
	my $mx28_dir = "$embedded_apps_dir/mx28";
	my $bootstream_dir = "$mx28_dir/elftosb/imx-bootlets-src-2.6.35.3-1.1.0";
	my $uboot_dir = "$mx28_dir/u-boot-2009.08";
	my $kernel_dir = "$mx28_dir/linux-2.6.35.3";
	my $rootfs_dir = "$mx28_dir/rootfs";
	my $rootfs_install_dir = "$mx28_dir/rootfs-install";
	my $ats_ralink_ap_2_7_1_0_dir = "$mx28_dir/drivers/ats-ralink-ap-2.7.1.0";
	my $ats_ralink_sta_2_6_1_4_dir = "$mx28_dir/drivers/ats-ralink-sta-2.6.1.4";

	# 3rd Party Applications and Libraries
	my $libsocketcan_0_0_8_dir = "$embedded_apps_dir/3rd-party/libsocketcan-0.0.8";
	my $socketCAN_dir = "$embedded_apps_dir/3rd-party/socketCAN";
	my $ethtool_dir = "$embedded_apps_dir/3rd-party/ethtool-6+20091202";
	my $strace_dir = "$embedded_apps_dir/3rd-party/strace-4.8";
	my $zlib_dir = "$embedded_apps_dir/3rd-party/zlib-1.2.5";
	my $ppp_2_4_5_dir = "$embedded_apps_dir/3rd-party/ppp-2.4.5";
	my $libb64_1_2_dir = "$embedded_apps_dir/3rd-party/libb64-1.2";
	my $i2c_tools_3_0_2_dir = "$embedded_apps_dir/3rd-party/i2c-tools-3.0.2";
	my $ncurses_5_9_dir = "$embedded_apps_dir/3rd-party/ncurses-5.9";
	my $mutt_1_5_20_dir = "$embedded_apps_dir/3rd-party/mutt-1.5.20";
	my $sqlite3_3_6_22_dir = "$embedded_apps_dir/3rd-party/sqlite3-3.6.22";
	my $libxml2_2_9_0_dir = "$embedded_apps_dir/3rd-party/libxml2-2.9.0";
	my $openssl_1_0_1g_dir = "$embedded_apps_dir/3rd-party/openssl-1.0.1g";
	my $php_5_4_10_dir = "$embedded_apps_dir/3rd-party/php-5.4.10";
	my $gmp_5_0_5_dir = "$embedded_apps_dir/3rd-party/gmp-5.0.5";
	my $openswan_2_6_38_dir = "$embedded_apps_dir/3rd-party/openswan-2.6.38";
	my $iproute2_j1939_dir = "$embedded_apps_dir/3rd-party/iproute2-j1939";
	my $expat_2_0_1_dir = "$embedded_apps_dir/3rd-party/expat-2.0.1";
	my $libmodbus_3_0_4_dir = "$embedded_apps_dir/3rd-party/libmodbus-3.0.4";
	my $libpcap_1_3_0_dir = "$embedded_apps_dir/3rd-party/libpcap-1.3.0";

	# RedStone Applications and Libraries
	my $gcc_dir = "$embedded_apps_dir/apps/gcc";
	my $command_line_parser_dir = "$embedded_apps_dir/apps/command_line_parser";
	my $socket_interface_dir = "$embedded_apps_dir/apps/socket_interface";
	my $rs232_tcp_server_dir = "$embedded_apps_dir/apps/rs232-tcp-server";
	my $ats_common_dir = "$embedded_apps_dir/apps/ats-common";
	my $logger_dir = "$embedded_apps_dir/apps/logger";
	my $state_machine_dir = "$embedded_apps_dir/apps/state-machine";
	my $Libs_dir = "$embedded_apps_dir/apps/Libs";
	my $buzzer_monitor_dir = "$embedded_apps_dir/apps/buzzer-monitor";
	my $redstone_socketcan_dir = "$embedded_apps_dir/apps/redstone-socketcan";
	my $can_odb2_monitor_dir = "$embedded_apps_dir/apps/can-odb2-monitor";
	my $trip_stats_dir = "$embedded_apps_dir/apps/trip-stats";
	my $can_seatbelt_monitor_dir = "$embedded_apps_dir/apps/can-seatbelt-monitor";
	my $FastTrac_dir = "$embedded_apps_dir/apps/FastTrac";
	my $modem_monitor_dir = "$embedded_apps_dir/apps/modem-monitor";
	my $rtc_monitor_dir = "$embedded_apps_dir/apps/rtc-monitor";
	my $app_monitor_dir = "$embedded_apps_dir/apps/app-monitor";
	my $admin_client_dir = "$embedded_apps_dir/apps/admin-client";
	my $system_watchdog_dir = "$embedded_apps_dir/apps/system-watchdog";
	my $power_monitor_dir = "$embedded_apps_dir/apps/power-monitor";
	my $db_monitor_dir = "$embedded_apps_dir/apps/db-monitor";
	my $port_forward_dir = "$embedded_apps_dir/apps/port-forward";
	my $message_assembler_dir = "$embedded_apps_dir/apps/message-assembler";
	my $get_wakeup_dir = "$embedded_apps_dir/apps/tools/get-wakeup";
	my $connect_ppp_dir = "$embedded_apps_dir/apps/tools/connect-ppp";
	my $network_monitor_dir = "$embedded_apps_dir/apps/tools/network-monitor";
	my $state_dir = "$embedded_apps_dir/apps/tools/state";
	my $packetizer_dir = "$embedded_apps_dir/apps/packetizer";
	my $trip_monitor_dir = "$embedded_apps_dir/apps/trip-monitor";
	my $periodic_msg_gen_dir = "$embedded_apps_dir/apps/periodic-msg-gen";
	my $heartbeat_dir = "$embedded_apps_dir/apps/heartbeat/";
	my $crit_battery_check_dir = "$embedded_apps_dir/apps/crit-battery-check/";
	my $ipsec_monitor_dir = "$embedded_apps_dir/apps/ipsec-monitor";
	my $avl_monitor_dir = "$embedded_apps_dir/apps/avl-monitor";
	my $feature_monitor_dir = "$embedded_apps_dir/apps/feature-monitor";
	my $telit_monitor_dir = "$embedded_apps_dir/apps/telit-monitor";
	my $can_dodge_seatbelt_monitor_dir = "$embedded_apps_dir/apps/can-dodge-seatbelt-monitor";
	my $can_ford_seatbelt_monitor_dir = "$embedded_apps_dir/apps/can-ford-seatbelt-monitor";
	my $can_gm_seatbelt_monitor_dir = "$embedded_apps_dir/apps/can-gm-seatbelt-monitor";
	my $ignition_monitor_dir = "$embedded_apps_dir/apps/IgnitionMonitor";
	my $web_config_manager_dir = "$embedded_apps_dir/apps/web-config-manager";
	my $packetizer_lib_dir = "$embedded_apps_dir/apps/packetizer-lib";
	my $packetizer_dash_socket_dir = "$embedded_apps_dir/apps/packetizer-dash";
	my $packetizer_dash_dir = "$embedded_apps_dir/apps/packetizer_dash";
	my $packetizer_calamps_dir = "$embedded_apps_dir/apps/packetizer-calamps";
	my $packetizer_iridium_dir = "$embedded_apps_dir/apps/packetizer-iridium";
	my $packetizer_cams_dir = "$embedded_apps_dir/apps/packetizer-cams";
	my $packetizer_inet_dir = "$embedded_apps_dir/apps/packetizer-inet";
	my $gps_socket_server_dir = "$embedded_apps_dir/apps/gps-socket-server";
	my $i2c_gpio_monitor_dir = "$embedded_apps_dir/apps/i2c-gpio-monitor";
	my $plc_serial_monitor_dir = "$embedded_apps_dir/apps/plc-serial-monitor";
	my $wifi_monitor_dir = "$embedded_apps_dir/apps/wifi-monitor";
	my $wifi_client_monitor_dir = "$embedded_apps_dir/apps/tools/wifi-client-monitor";
	my $redstone_socketcan_j1939_dir = "$embedded_apps_dir/apps/redstone-socketcan-j1939";
	my $can_j1939_monitor_dir = "$embedded_apps_dir/apps/can-j1939-monitor";
	my $skybase_dir = "$embedded_apps_dir/apps/SkyBase";
	my $modbus_monitor_dir = "$embedded_apps_dir/apps/modbus-monitor";
	my $zigbee_monitor_dir = "$embedded_apps_dir/apps/zigbee-monitor";
	my $iridium_monitor_dir = "$embedded_apps_dir/apps/iridium-monitor";
	my $isc_lens_dir = "$embedded_apps_dir/apps/isc-lens";
	my $isc_modbus_dir = "$embedded_apps_dir/apps/isc-modbus";

	build_parallel_item(%$context, 'include', $include_dir);
	build_parallel_item(%$context, 'gcc', $gcc_dir);
	join_parallel_builds(%$context);

	build_parallel_item(%$context, 'rootfs-install', $rootfs_install_dir);
	build_parallel_item(%$context, 'kernel', $kernel_dir);
	build_parallel_item(%$context, 'uboot', $uboot_dir);
	build_parallel_item(%$context, 'ethtool', $ethtool_dir);
	build_parallel_item(%$context, 'strace', $strace_dir);
	build_parallel_item(%$context, 'zlib', $zlib_dir);
	build_parallel_item(%$context, 'sqlite3-3.6.22', $sqlite3_3_6_22_dir);
	build_parallel_item(%$context, 'ppp-2.4.5', $ppp_2_4_5_dir);
	build_parallel_item(%$context, 'libb64-1.2', $libb64_1_2_dir);
	build_parallel_item(%$context, 'ncurses-5.9', $ncurses_5_9_dir);
	build_parallel_item(%$context, 'ats-common', $ats_common_dir);
	build_parallel_item(%$context, 'socket_interface', $socket_interface_dir);
	build_parallel_item(%$context, 'command_line_parser', $command_line_parser_dir);
	join_parallel_builds(%$context);

	build_parallel_item(%$context, 'logger', $logger_dir);
	build_parallel_item(%$context, 'bootstream', $bootstream_dir);
	build_parallel_item(%$context, 'ats-ralink-ap-2.7.1.0', $ats_ralink_ap_2_7_1_0_dir);
	build_parallel_item(%$context, 'ats-ralink-sta-2.6.1.4', $ats_ralink_sta_2_6_1_4_dir);
	build_parallel_item(%$context, 'i2c-tools-3.0.2', $i2c_tools_3_0_2_dir);
	build_parallel_item(%$context, 'libsocketcan-0.0.8', $libsocketcan_0_0_8_dir);
	build_parallel_item(%$context, 'mutt-1.5.20', $mutt_1_5_20_dir);
	join_parallel_builds(%$context);

	build_parallel_item(%$context, 'socketCAN', $socketCAN_dir);
	build_parallel_item(%$context, 'libxml2-2.9.0', $libxml2_2_9_0_dir);
	join_parallel_builds(%$context);

	build_parallel_item(%$context, 'openssl-1.0.1g', $openssl_1_0_1g_dir);
	build_parallel_item(%$context, 'db-monitor', $db_monitor_dir);
	build_parallel_item(%$context, 'state-machine', $state_machine_dir);
	join_parallel_builds(%$context);

	build_parallel_item(%$context, 'Libs', $Libs_dir);
	build_parallel_item(%$context, 'php-5.4.10', $php_5_4_10_dir);
	build_parallel_item(%$context, 'buzzer-monitor', $buzzer_monitor_dir);
	build_parallel_item(%$context, 'FastTrac', $FastTrac_dir);
	build_parallel_item(%$context, 'port-forward', $port_forward_dir);
	build_parallel_item(%$context, 'redstone-socketcan', $redstone_socketcan_dir);
	build_parallel_item(%$context, 'expat-2.0.1', $expat_2_0_1_dir);
	build_parallel_item(%$context, 'libmodbus-3.0.4', $libmodbus_3_0_4_dir);
	build_item(%$context, 'gmp-5.0.5', $gmp_5_0_5_dir);
	build_item(%$context, 'openswan-2.6.38', $openswan_2_6_38_dir);
	join_parallel_builds(%$context);

	build_parallel_item(%$context, 'get-wakeup', $get_wakeup_dir);
	build_parallel_item(%$context, 'connect-ppp', $connect_ppp_dir);
	build_parallel_item(%$context, 'network-monitor', $network_monitor_dir);
	build_parallel_item(%$context, 'trip-stats', $trip_stats_dir);
	build_parallel_item(%$context, 'modem-monitor', $modem_monitor_dir);
	build_parallel_item(%$context, 'rtc-monitor', $rtc_monitor_dir);
	build_parallel_item(%$context, 'app-monitor', $app_monitor_dir);
	build_parallel_item(%$context, 'admin-client', $admin_client_dir);
	build_parallel_item(%$context, 'system-watchdog', $system_watchdog_dir);
	build_parallel_item(%$context, 'power-monitor', $power_monitor_dir);
	build_parallel_item(%$context, 'message-assembler', $message_assembler_dir);
	build_parallel_item(%$context, 'trip-monitor', $trip_monitor_dir);
	build_parallel_item(%$context, 'can-odb2-monitor', $can_odb2_monitor_dir);
	build_parallel_item(%$context, 'can-seatbelt-monitor', $can_seatbelt_monitor_dir);
	build_parallel_item(%$context, 'periodic-msg-gen', $periodic_msg_gen_dir);
	build_parallel_item(%$context, 'heartbeat', $heartbeat_dir);
	build_parallel_item(%$context, 'crit-battery-check', $crit_battery_check_dir);
	build_parallel_item(%$context, 'ipsec-monitor', $ipsec_monitor_dir);
	build_parallel_item(%$context, 'feature-monitor', $feature_monitor_dir);
	join_parallel_builds(%$context);

	build_parallel_item(%$context, 'state', $state_dir);
	build_parallel_item(%$context, 'avl-monitor', $avl_monitor_dir);
	build_parallel_item(%$context, 'telit-monitor', $telit_monitor_dir);
	build_parallel_item(%$context, 'can-dodge-seatbelt-monitor', $can_dodge_seatbelt_monitor_dir);
	build_parallel_item(%$context, 'can-ford-seatbelt-monitor', $can_ford_seatbelt_monitor_dir);
	build_parallel_item(%$context, 'can-gm-seatbelt-monitor', $can_gm_seatbelt_monitor_dir);
	build_parallel_item(%$context, 'ignition-monitor', $ignition_monitor_dir);
	build_parallel_item(%$context, 'libpcap-1.3.0', $libpcap_1_3_0_dir);
	build_parallel_item(%$context, 'web-config-manager', $web_config_manager_dir);
	build_parallel_item(%$context, 'packetizer-lib', $packetizer_lib_dir);
	build_parallel_item(%$context, 'iridium-monitor', $iridium_monitor_dir);
	join_parallel_builds(%$context);

	build_parallel_item(%$context, 'packetizer', $packetizer_dir);
	build_parallel_item(%$context, 'packetizer_dash_socket', $packetizer_dash_socket_dir);
	build_parallel_item(%$context, 'packetizer-cams', $packetizer_cams_dir);
	build_parallel_item(%$context, 'packetizer_dash', $packetizer_dash_dir);
	build_parallel_item(%$context, 'packetizer-calamps', $packetizer_calamps_dir);
	build_parallel_item(%$context, 'gps-socket-server', $gps_socket_server_dir);	
	build_parallel_item(%$context, 'i2c-gpio-monitor', $i2c_gpio_monitor_dir);
	build_parallel_item(%$context, 'plc-serial-monitor', $plc_serial_monitor_dir);
	build_parallel_item(%$context, 'wifi-monitor', $wifi_monitor_dir);
	build_parallel_item(%$context, 'wifi-client-monitor', $wifi_client_monitor_dir);
	build_parallel_item(%$context, 'iproute2-j1939', $iproute2_j1939_dir);
	build_parallel_item(%$context, 'redstone-socketcan-j1939', $redstone_socketcan_j1939_dir);
	join_parallel_builds(%$context);

	build_parallel_item(%$context, 'packetizer-iridium', $packetizer_iridium_dir);
	build_parallel_item(%$context, 'can-j1939-monitor', $can_j1939_monitor_dir);
	build_parallel_item(%$context, 'skybase', $skybase_dir);
	build_parallel_item(%$context, 'modbus-monitor', $modbus_monitor_dir);
	build_parallel_item(%$context, 'zigbee-monitor', $zigbee_monitor_dir);
	join_parallel_builds(%$context);
	build_parallel_item(%$context, 'isc-lens', $isc_lens_dir);
	build_parallel_item(%$context, 'isc-modbus', $isc_modbus_dir);
	build_parallel_item(%$context, 'packetizer-inet', $packetizer_inet_dir);
	join_parallel_builds(%$context);

	$context->{'build_config'} = 1;
	build_item(%$context, "config", "$embedded_apps_dir/config");
}

1;


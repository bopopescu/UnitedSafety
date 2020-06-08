#ifndef SYSMODBUSREGISTERS_H
#define SYSMODBUSREGISTERS_H

#define BUILDVER_LIMIT				10000

#define MODBUS_FUNC_ENABLE			0
#define FIRMWARE_VERSION			1
#define FIRMWARE_SUBVER_1			2
#define FIRMWARE_SUBVER_2			3
#define FIRMWARE_BUILD_VER			4
#define MANUFACTURE_MONTH_DATE		5
#define MANUFACTURE_YEAR			6
#define TECHINICIAN_INIT_1			7
#define TECHINICIAN_INIT_2			8
#define CURRENT_SITENAME_1			9
#define CURRENT_SITENAME_2			10
#define CURRENT_SITENAME_3			11
#define CURRENT_SITENAME_4			12
#define CURRENT_SITENAME_5			13
#define CURRENT_SITENAME_6			14
#define CURRENT_SITENAME_7			15
#define CURRENT_SITENAME_8			16
#define DEV_PART_NUMBER_1			17
#define DEV_PART_NUMBER_2			18
#define DEV_PART_NUMBER_3			19
#define DEV_PART_NUMBER_4			20
#define DEV_PART_NUMBER_5			21
#define DEV_PART_NUMBER_6			22
#define DEV_PART_NUMBER_7			23
#define DEV_PART_NUMBER_8			24
#define DEV_SERIAL_NUMBER_1			25
#define DEV_SERIAL_NUMBER_2			26
#define DEV_SERIAL_NUMBER_3			27
#define DEV_SERIAL_NUMBER_4			28
#define DEV_SERIAL_NUMBER_5			29
#define DEV_SERIAL_NUMBER_6			30
#define DEV_SERIAL_NUMBER_7			31
#define DEV_SERIAL_NUMBER_8			32
#define DEV_JOB_NUMBER_1			33
#define DEV_JOB_NUMBER_2			34
#define DEV_JOB_NUMBER_3			35
#define DEV_JOB_NUMBER_4			36
#define MANUFACT_LOCATION			37
#define HARDWARE_VERSION_1			38
#define HARDWARE_VERSION_2			39
#define HARDWARE_VERSION_3                      40
#define ISC_PROPERTY				41
#define DEV_TIME_LOW_WORD			42
#define DEV_TIME_HIGH_WORD			43
#define KEEPALIVE_INTERVAL			44
#define NOCRITICAL_INTERVAL			45		// obsoleted
#define CRITCIAL_INTERVAL			46
#define INET0_URL_ADDR1				47
#define INET0_URL_ADDR32			78
#define INET0_USER_NAME1			79
#define INET0_USER_NAME16			94
#define INET0_PASSWORD1				95
#define INET0_PASSWORD16			110
#define REGION_OPERATION			111
#define LENS_NETWORKNAME			112
#define LENS_WIRELESS_FEATURE_BITS	113
#define LENS_HOPS_NUMBER			114
#define LENS_MAX_PEER_NUMBER		115
#define LENS_PRIMARY_PUB_CH			116
#define LENS_SECOND_PUB_CH			117
#define LENS_ACTIVE_CH_MASK			118
#define LENS_ENCRYPT_TYPE			119
#define LENS_NETWORK_INTREVAL		120
#define LENS_TEAMMATE_LOST_INTERVAL	121
#define LENS_ENCRYPT_KEY_1			122
#define LENS_ENCRYPT_KEY_8			129
#define AWARE360_TRULINK_SN1            130
#define AWARE360_TRULINK_SN2            131
#define AWARE360_TRULINK_SN3            132
#define AWARE360_TRULINK_SN4            133
#define SATELLITE_CRITICAL_INTERVAL	134
#define ACTIVE_INET_SERVER_INDEX	135

#define INET1_URL_ADDR1				136
#define INET1_URL_ADDR32			167
#define INET1_USER_NAME1			168
#define INET1_USER_NAME16			183
#define INET1_PASSWORD1				184
#define INET1_PASSWORD16			199

#define INET2_URL_ADDR1				200
#define INET2_URL_ADDR32			231
#define INET2_USER_NAME1			232
#define INET2_USER_NAME16			247
#define INET2_PASSWORD1				248
#define INET2_PASSWORD16			263

#define INET3_URL_ADDR1				264
#define INET3_URL_ADDR32			295
#define INET3_USER_NAME1			296
#define INET3_USER_NAME16			311
#define INET3_PASSWORD1				312
#define INET3_PASSWORD16			327

#define LENS_RADIO_HARDWARE_VER		328

#define SIM_ICCID_ADDR1				329
#define SIM_ICCID_ADDR20			348
#define CELLULAR_IMEI_ADDR1			349
#define CELLULAR_IMEI_ADDR15		363
#define SATELLITE_IMEI_ADDR1		364
#define SATELLITE_IMEI_ADDR115		378
#define LCM_DEFAULT_PASSWD1			379
#define LCM_DEFAULT_PASSWD8			386

#define LENS_RADIO_STATUS			387
#define ENALBE_LENS_TEST_MODE		388
#define ETHERNET_ENABLE				389

#define COMMINT_PARAMETER			511
#define MODBUS_MAX_REGISTERS	    (COMMINT_PARAMETER+1)


#define LENS_PARAM_DB_NAME   "isc-lens"
#define ISC_MFG_DB_NAME      "isc-mfg"
#define SIM_DB_NAME	 		"Cellular"
#define CELLULAR_DB_NAME 	"RedStone"
#define SATELLITE_DB_NAME	"Iridium"


#define LENS_RADIO_HWVER_NAME		"RadioHWVer"
#define SIM_ICCID_PARAM_NAME	"CCID"
#define IMEI_PARAM_NAME			"IMEI"
#define DEFAULT_LCM_PASSWORD_NAME	"LCMPassword"
#define LENS_RADIO_STATUS_NAME		"RadioStatus"
#define ENABLE_LENS_TEST_MODE_NAME	"EnableTestMode"
#define ENABLE_LENS_TEST_MODE_VALUE	0xA1DE

int CheckAndLoadNewParameters(uint16_t Registers[], int length);
int CheckAndSaveParameters(uint16_t Registers[], int length);
uint16_t GetHolderRegister(int address);
int modbus_register_handler(modbus_t *ctx, const uint8_t *req, int req_length, modbus_mapping_t *mb_mapping);

extern modbus_mapping_t *mb_mapping;
#endif

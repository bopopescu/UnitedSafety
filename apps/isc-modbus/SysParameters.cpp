#include <modbus.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "modbus-private.h"
#include "SysModbusRegisters.h"
#include "ConfigDB.h"
#include "ats-common.h"

modbus_mapping_t *mb_mapping;

uint16_t GetHolderRegister(int address)
{
  if(mb_mapping == NULL || mb_mapping->tab_registers == NULL)
    return 0;
  else
    {
      if(address >= 0 && address < MODBUS_MAX_REGISTERS)
        {
          return mb_mapping->tab_registers[address];
        }
      return 0;
    }
}


int CheckAndLoadNewParameters(uint16_t Registers[], int length)
{
  char * byteAccess;
  int int_value;
  int int_value2, int_value3, int_value4;
  ats::String str_value;
  db_monitor::ConfigDB db;
  
  	
  
  printf("Set Modbus enabled\n");
  //Load default Parameters, 
  Registers[MODBUS_FUNC_ENABLE] = 1;			// Modbus Function enabled\n


  // Load the manufacture data
  int_value = db.GetInt(ISC_MFG_DB_NAME, "ManuMonthDay", 0x0101);
  Registers[MANUFACTURE_MONTH_DATE] = int_value;
  int_value = db.GetInt(ISC_MFG_DB_NAME, "ManuYear", 2019);
  Registers[MANUFACTURE_YEAR] = int_value;

  int_value = db.GetInt(ISC_MFG_DB_NAME, "TechInit1", '0');
  Registers[TECHINICIAN_INIT_1] = char(int_value);
  
  int_value = db.GetInt(ISC_MFG_DB_NAME, "TechInit2", '0');
  Registers[TECHINICIAN_INIT_2] = char(int_value);

  byteAccess = (char *)(&Registers[CURRENT_SITENAME_1]);
  str_value = db.GetValue(LENS_PARAM_DB_NAME, "SiteName", "");
  strncpy(byteAccess, str_value.c_str(), (CURRENT_SITENAME_8-CURRENT_SITENAME_1+1)*2);

  str_value = db.GetValue(ISC_MFG_DB_NAME, "PartNumber", "00000000");
  byteAccess = (char *)(&Registers[DEV_PART_NUMBER_1]);
  strncpy(byteAccess, str_value.c_str(), (DEV_PART_NUMBER_8-DEV_PART_NUMBER_1+1)*2);

  str_value = db.GetValue(LENS_PARAM_DB_NAME, "SerialNumber", "FFFFFFFF");
  byteAccess = (char *)(&Registers[DEV_SERIAL_NUMBER_1]);
  strncpy(byteAccess, str_value.c_str(), (DEV_SERIAL_NUMBER_8-DEV_SERIAL_NUMBER_1+1)*2);
  /* // Henry/Robert 
  printf("%x[%d], byte: %x: %s\n", (uint32_t)Registers, DEV_SERIAL_NUMBER_1, (uint32_t)byteAccess, byteAccess);
  printf("SerialNumber: %s\n", str_value.c_str());
  */

  str_value = db.GetValue(ISC_MFG_DB_NAME, "JobNumber", "0000");
  byteAccess = (char *)(&Registers[DEV_JOB_NUMBER_1]);
  strncpy(byteAccess, str_value.c_str(), (DEV_JOB_NUMBER_4-DEV_JOB_NUMBER_1+1)*2);

  // Load manufacture location data
  int_value = db.GetInt(ISC_MFG_DB_NAME, "ManuLocation", 0);
  Registers[MANUFACT_LOCATION] = int_value;

  // Load the hardware version data
  Registers[HARDWARE_VERSION_1] = 0x01;
  Registers[HARDWARE_VERSION_2] = 0x00;
  Registers[HARDWARE_VERSION_3] = 0x01;

  // Load Trulink serial number
  byteAccess = (char *)(&Registers[AWARE360_TRULINK_SN1]);
  ats::get_file_line(str_value, "/mnt/nvram/rom/sn.txt", 1);
  strncpy(byteAccess, str_value.c_str(), (AWARE360_TRULINK_SN4-AWARE360_TRULINK_SN1+1)*2);
  
  // Load the ISC property data
  int_value = db.GetInt(ISC_MFG_DB_NAME, "ISCProperty", 0);
  Registers[ISC_PROPERTY]		= int_value;

  // Load the device date, it will be loaded when it is read
  Registers[DEV_TIME_LOW_WORD] = 0x00;
  Registers[DEV_TIME_HIGH_WORD] = 0x00;

  int_value = db.GetInt(LENS_PARAM_DB_NAME, "INetKeepAliveMinutes", 5);
  Registers[KEEPALIVE_INTERVAL] =  int_value;

  int_value = db.GetInt(LENS_PARAM_DB_NAME, "NonCriticalInterval", 5);
  printf("Noncriticalinterval: %d\n", int_value);
  Registers[NOCRITICAL_INTERVAL] = int_value;

  // Load the critical data interval for cellular // Henry/Robert
  int_value = db.GetInt(LENS_PARAM_DB_NAME, "CriticalIntervalMinutes", 1);
  Registers[CRITCIAL_INTERVAL] = int_value;

#if 0 // Temporary
  str_value = db.GetValue(LENS_PARAM_DB_NAME, "INetURL", "https://inetapi.indsci.com");
#else
  str_value = db.GetValue(LENS_PARAM_DB_NAME, "INetURL", "https://inetnowstg.indsci.com");
#endif
  byteAccess = (char *)(&Registers[INET0_URL_ADDR1]);
  strncpy(byteAccess, str_value.c_str(), (INET0_URL_ADDR32-INET0_URL_ADDR1+1)*2);

  str_value = db.GetValue(LENS_PARAM_DB_NAME, "INetUsername", "username");
  byteAccess = (char *)(&Registers[INET0_USER_NAME1]);
  strncpy(byteAccess, str_value.c_str(), (INET0_USER_NAME16-INET0_USER_NAME1+1)*2);

  str_value = db.GetValue(LENS_PARAM_DB_NAME, "INetPassword", "password");
  byteAccess = (char *)(&Registers[INET0_PASSWORD1]);
  strncpy(byteAccess, str_value.c_str(), (INET0_PASSWORD16-INET0_PASSWORD1+1)*2);

  // Load the iNet1 information
  str_value = db.GetValue(LENS_PARAM_DB_NAME, "INet1URL", "https://inetnowstg.indsci.com");
  byteAccess = (char *)(&Registers[INET1_URL_ADDR1]);
  strncpy(byteAccess, str_value.c_str(), (INET1_URL_ADDR32-INET1_URL_ADDR1+1)*2);

  str_value = db.GetValue(LENS_PARAM_DB_NAME, "INet1Username", "username");
  byteAccess = (char *)(&Registers[INET1_USER_NAME1]);
  strncpy(byteAccess, str_value.c_str(), (INET1_USER_NAME16-INET1_USER_NAME1+1)*2);

  str_value = db.GetValue(LENS_PARAM_DB_NAME, "INet1Password", "password");
  byteAccess = (char *)(&Registers[INET1_PASSWORD1]);
  strncpy(byteAccess, str_value.c_str(), (INET1_PASSWORD16-INET1_PASSWORD1+1)*2);
  
  // Load the iNet2 information
  str_value = db.GetValue(LENS_PARAM_DB_NAME, "INet2URL", "https://inetapitest.indscisw.com");
  byteAccess = (char *)(&Registers[INET2_URL_ADDR1]);
  strncpy(byteAccess, str_value.c_str(), (INET2_URL_ADDR32-INET2_URL_ADDR1+1)*2);

  str_value = db.GetValue(LENS_PARAM_DB_NAME, "INet2Username", "username");
  byteAccess = (char *)(&Registers[INET2_USER_NAME1]);
  strncpy(byteAccess, str_value.c_str(), (INET2_USER_NAME16-INET2_USER_NAME1+1)*2);

  str_value = db.GetValue(LENS_PARAM_DB_NAME, "INet2Password", "password");
  byteAccess = (char *)(&Registers[INET2_PASSWORD1]);
  strncpy(byteAccess, str_value.c_str(), (INET2_PASSWORD16-INET2_PASSWORD1+1)*2);
  
  // Load the iNet3 information
  str_value = db.GetValue(LENS_PARAM_DB_NAME, "INet3URL", "https://inetuploadbelski.indscisw.com");
  byteAccess = (char *)(&Registers[INET3_URL_ADDR1]);
  strncpy(byteAccess, str_value.c_str(), (INET3_URL_ADDR32-INET3_URL_ADDR1+1)*2);

  str_value = db.GetValue(LENS_PARAM_DB_NAME, "INet3Username", "username");
  byteAccess = (char *)(&Registers[INET3_USER_NAME1]);
  strncpy(byteAccess, str_value.c_str(), (INET3_USER_NAME16-INET3_USER_NAME1+1)*2);

  str_value = db.GetValue(LENS_PARAM_DB_NAME, "INet3Password", "password");
  byteAccess = (char *)(&Registers[INET3_PASSWORD1]);
  strncpy(byteAccess, str_value.c_str(), (INET3_PASSWORD16-INET3_PASSWORD1+1)*2);

  int_value = db.GetInt(LENS_PARAM_DB_NAME, "SatelliteCriticalIntervalMinutes", 2);
  Registers[SATELLITE_CRITICAL_INTERVAL]		= int_value;
  
  // Load the active iNet Server index
  int_value = db.GetInt(LENS_PARAM_DB_NAME, "INetIndex", 0);
  Registers[ACTIVE_INET_SERVER_INDEX]		= int_value;
  
  // Load the region data
  int_value = db.GetInt(ISC_MFG_DB_NAME, "RegionOp", 1);
  Registers[REGION_OPERATION]		= int_value;

  int_value = db.GetInt(LENS_PARAM_DB_NAME, "NetworkName", 0);
  printf("Networkname: %d\n", int_value);
  Registers[LENS_NETWORKNAME] = int_value;

  int_value = db.GetInt(LENS_PARAM_DB_NAME, "FeatureBits", 0);
  printf("Feature bits: 0x%x\n", int_value);
  Registers[LENS_WIRELESS_FEATURE_BITS] = int_value;

  int_value = db.GetInt(LENS_PARAM_DB_NAME, "MaxHops", 5);
  printf("Max hops: %d\n", int_value);
  Registers[LENS_HOPS_NUMBER] = int_value;

  int_value = db.GetInt(LENS_PARAM_DB_NAME, "MaxPeers", 24);
  printf("Max Peers: %d\n", int_value);
  Registers[LENS_MAX_PEER_NUMBER] = int_value;

  int_value = db.GetInt(LENS_PARAM_DB_NAME, "PrimaryChannel", 4);
  printf("Primarychannel: 0x%x\n", int_value);
  Registers[LENS_PRIMARY_PUB_CH] = int_value;

  int_value = db.GetInt(LENS_PARAM_DB_NAME, "SecondaryChannel", 9);
  printf("Secondarychannel: 0x%x\n", int_value);
  Registers[LENS_SECOND_PUB_CH] = int_value;

  int_value = db.GetInt(LENS_PARAM_DB_NAME, "ChannelMask", 0xFFFF);
  printf("Channel mask: 0x%x\n", int_value);
  Registers[LENS_ACTIVE_CH_MASK] = int_value;

  // load the encryption type
  int_value = db.GetInt(LENS_PARAM_DB_NAME, "Encryption", 0x02); //Ammar/Henry: If there is no such parameter, it should use default value "2".
  //printf("Channel mask: 0x%x\n", int_value)
  Registers[LENS_ENCRYPT_TYPE] = int_value;

  int_value = db.GetInt(LENS_PARAM_DB_NAME, "NetworkInterval", 2);
  printf("Network interval: 0x%x\n", int_value);
  Registers[LENS_NETWORK_INTREVAL] = int_value;

  int_value = db.GetInt(LENS_PARAM_DB_NAME, "InstrumentLostSeconds", 180);
  printf("team lost: %d\n", int_value);
  Registers[LENS_TEAMMATE_LOST_INTERVAL] = int_value;

  byteAccess = (char *)(&Registers[LENS_ENCRYPT_KEY_1]);
  str_value = db.GetValue(LENS_PARAM_DB_NAME, "NetworkEncryption", "Aware360DevKey17");
  printf("Key: %s\n", str_value.c_str());
  strncpy(byteAccess, str_value.c_str(), (LENS_ENCRYPT_KEY_8-LENS_ENCRYPT_KEY_1+1)*2);
  // Load ICCID
  byteAccess = (char *)(&Registers[SIM_ICCID_ADDR1]);
  str_value = db.GetValue(SIM_DB_NAME, SIM_ICCID_PARAM_NAME, "FFFFFFFFFFFFFFFFFFFF");
  strncpy(byteAccess, str_value.c_str(), (SIM_ICCID_ADDR20-SIM_ICCID_ADDR1+1)*2);

  // Load Cellular IMEI
  byteAccess = (char *)(&Registers[CELLULAR_IMEI_ADDR1]);
  str_value = db.GetValue(CELLULAR_DB_NAME, IMEI_PARAM_NAME, "FFFFFFFFFFFFFFF");
  strncpy(byteAccess, str_value.c_str(), (CELLULAR_IMEI_ADDR15-CELLULAR_IMEI_ADDR1+1)*2);
  
  // Load Satellite IMEI
  byteAccess = (char *)(&Registers[SATELLITE_IMEI_ADDR1]);
  str_value = db.GetValue(SATELLITE_DB_NAME, IMEI_PARAM_NAME, "FFFFFFFFFFFFFFF");
  strncpy(byteAccess, str_value.c_str(), (SATELLITE_IMEI_ADDR115-SATELLITE_IMEI_ADDR1+1)*2);
    
  // Load default LCM password
  byteAccess = (char *)(&Registers[LCM_DEFAULT_PASSWD1]);
  str_value = db.GetValue(ISC_MFG_DB_NAME, DEFAULT_LCM_PASSWORD_NAME, "Defaultkey");
  strncpy(byteAccess, str_value.c_str(), (LCM_DEFAULT_PASSWD8-LCM_DEFAULT_PASSWD1+1)*2);
  
  // Load Modbus enable register
  int_value = db.GetInt(LENS_PARAM_DB_NAME, "ModbusEnabled", 0);
  Registers[MODBUS_FUNC_ENABLE] = (int_value == 0 ? 0 : 1);
  
  // Load Firmware Version
  str_value = db.GetValue(LENS_PARAM_DB_NAME, "FWVer", "3.0.0.0");
  sscanf(str_value.c_str(), "%d.%d.%d.%d", &int_value, &int_value2, &int_value3, &int_value4);
  Registers[FIRMWARE_VERSION] = int_value;
  Registers[FIRMWARE_SUBVER_1] = int_value2;
  Registers[FIRMWARE_SUBVER_2] = int_value3;
  Registers[FIRMWARE_BUILD_VER] = int_value4;
  // Load Hardware Version
  str_value = db.GetValue(LENS_PARAM_DB_NAME, "HWVer", "1.0.1");
  sscanf(str_value.c_str(), "%d.%d.%d", &int_value, &int_value2, &int_value3);
  Registers[HARDWARE_VERSION_1] = int_value;
  Registers[HARDWARE_VERSION_2] = int_value2;
  Registers[HARDWARE_VERSION_3] = int_value3;
  
  // Load LENS radio Hardware Version
  int_value = db.GetInt(LENS_PARAM_DB_NAME, LENS_RADIO_HWVER_NAME, 0);
  Registers[LENS_RADIO_HARDWARE_VER] = int_value;
  
  // Load Ethernet enable register
  int_value = db.GetInt(CELLULAR_DB_NAME, "Ethernet", 0);
  Registers[ETHERNET_ENABLE] = (int_value == 0 ? 0 : 1);
  return 0;
}

int CheckAndSaveParameters(uint16_t Registers[], int length)
{
  char * str_value;
  char int_string[12];
  char name_string[128];
  int int_value;
  db_monitor::ConfigDB db;

  // Keep alive interval
  int_value = (int)(Registers[COMMINT_PARAMETER]);
  if(int_value != 0)
    {
      printf("Write paratmers to database\n");

      // Save the manufacture date
      int_value = (int)Registers[MANUFACTURE_MONTH_DATE];
      sprintf(int_string, "%d", int_value);
      db.Set(ISC_MFG_DB_NAME, "ManuMonthDay", int_string);

      int_value = (int)Registers[MANUFACTURE_YEAR];
      sprintf(int_string, "%d", int_value);
      db.Set(ISC_MFG_DB_NAME, "ManuYear", int_string);
  
	  // Save the technician init
	  int_value = (int)Registers[TECHINICIAN_INIT_1];
      sprintf(int_string, "%d", int_value);
      db.Set(ISC_MFG_DB_NAME, "TechInit1", int_string);
  
	  int_value = (int)Registers[TECHINICIAN_INIT_2];
      sprintf(int_string, "%d", int_value);
      db.Set(ISC_MFG_DB_NAME, "TechInit2", int_string);

      // Save the part number 
	  str_value = (char *)(&Registers[DEV_PART_NUMBER_1]);
      strncpy(name_string, str_value, (DEV_PART_NUMBER_8-DEV_PART_NUMBER_1+1)*2);
      name_string[ (DEV_PART_NUMBER_8-DEV_PART_NUMBER_1+1)*2] = '\0';
      db.Set(ISC_MFG_DB_NAME, "PartNumber", name_string);
	  
	  // Save the Job number	  
	  str_value = (char *)(&Registers[DEV_JOB_NUMBER_1]);
      strncpy(name_string, str_value, (DEV_JOB_NUMBER_4-DEV_JOB_NUMBER_1+1)*2);
      name_string[ (DEV_JOB_NUMBER_4-DEV_JOB_NUMBER_1+1)*2 ] = '\0';
      db.Set(ISC_MFG_DB_NAME, "JobNumber", name_string);
	  
	  // Save manufacture location data
	  int_value = (int)Registers[MANUFACT_LOCATION];
      sprintf(int_string, "%d", int_value);
      db.Set(ISC_MFG_DB_NAME, "ManuLocation", int_string);
	  
	  // Save the ISC property data
	  int_value = (int)Registers[ISC_PROPERTY];
      sprintf(int_string, "%d", int_value);
      db.Set(ISC_MFG_DB_NAME, "ISCProperty", int_string);
	  
	  // save the region data
	  int_value = (int)Registers[REGION_OPERATION];
      sprintf(int_string, "%d", int_value);
      db.Set(ISC_MFG_DB_NAME, "RegionOp", int_string);
	  
	  // Site name
      str_value = (char *)(&Registers[CURRENT_SITENAME_1]);
      strncpy(name_string, str_value, (CURRENT_SITENAME_8 - CURRENT_SITENAME_1 + 1) *2);
      name_string[(CURRENT_SITENAME_8 - CURRENT_SITENAME_1+1)*2] = '\0';
      db.Set(LENS_PARAM_DB_NAME, "SiteName", name_string);

      // ISC Serial number
      str_value = (char *)(&Registers[DEV_SERIAL_NUMBER_1]);
      strncpy(name_string, str_value, (DEV_SERIAL_NUMBER_8 - DEV_SERIAL_NUMBER_1 + 1) *2);
      name_string[(DEV_SERIAL_NUMBER_8 - DEV_SERIAL_NUMBER_1+1)*2] = '\0';
      /* // Henry/Robert
      printf("%x[%d], write: %x: serialnumber: %s\n", (uint32_t)Registers, DEV_SERIAL_NUMBER_1, (uint32_t)str_value, name_string);
	  */
      db.Set(LENS_PARAM_DB_NAME, "SerialNumber", name_string);

      // Keep alive interval
      int_value = (int)(Registers[KEEPALIVE_INTERVAL]);
      sprintf(int_string, "%d", int_value);
      db.Set(LENS_PARAM_DB_NAME, "InstrumentKeepAliveMinutes", int_string);

      // Noncritical interval
      int_value = (int)(Registers[NOCRITICAL_INTERVAL]);
      sprintf(int_string, "%d", int_value);
      db.Set(LENS_PARAM_DB_NAME, "NonCriticalInterval", int_string);

      // Cellular critical data interval // Henry/Robert
      int_value = (int)(Registers[CRITCIAL_INTERVAL]);
      sprintf(int_string, "%d", int_value);
      db.Set(LENS_PARAM_DB_NAME, "CriticalIntervalMinutes", int_string);

	  // Satellite critical data interval // Henry/Robert
      int_value = (int)(Registers[SATELLITE_CRITICAL_INTERVAL]);
      sprintf(int_string, "%d", int_value);
      db.Set(LENS_PARAM_DB_NAME, "SatelliteCriticalIntervalMinutes", int_string);

      // LENS network name
      int_value = (int)(Registers[LENS_NETWORKNAME]);
      sprintf(int_string, "%d", int_value);
      db.Set(LENS_PARAM_DB_NAME, "NetworkName", int_string);
	  
      // LENS Feature bits
      int_value = (int)(Registers[LENS_WIRELESS_FEATURE_BITS]);
      sprintf(int_string, "%d", int_value);
      db.Set(LENS_PARAM_DB_NAME, "FeatureBits", int_string);

      // LENS hops number
      int_value = (int)(Registers[LENS_HOPS_NUMBER]);
      sprintf(int_string, "%d", int_value);
      db.Set(LENS_PARAM_DB_NAME, "MaxHops", int_string);

      // LENS MAX peer number
      int_value = (int)(Registers[LENS_MAX_PEER_NUMBER]);
      sprintf(int_string, "%d", int_value);
      db.Set(LENS_PARAM_DB_NAME, "MaxPeers", int_string);

      // LENS Primary public channel
      int_value = (int)(Registers[LENS_PRIMARY_PUB_CH]);
      sprintf(int_string, "%d", int_value);
      db.Set(LENS_PARAM_DB_NAME, "PrimaryChannel", int_string);

      // LENS Second public channel
      int_value = (int)(Registers[LENS_SECOND_PUB_CH]);
      sprintf(int_string, "%d", int_value);
      db.Set(LENS_PARAM_DB_NAME, "SecondaryChannel", int_string);

      // LENS Active channel mask
      int_value = (int)(Registers[LENS_ACTIVE_CH_MASK]);
      sprintf(int_string, "%d", int_value);
      db.Set(LENS_PARAM_DB_NAME, "ChannelMask", int_string);

      // LENS Encrypt type
	  int_value = (int)(Registers[LENS_ENCRYPT_TYPE]);
      sprintf(int_string, "%d", int_value);
      db.Set(LENS_PARAM_DB_NAME, "Encryption", int_string);
      

      // LENS network interval
      int_value = (int)(Registers[LENS_NETWORK_INTREVAL]);
      sprintf(int_string, "%d", int_value);
      db.Set(LENS_PARAM_DB_NAME, "NetworkInterval", int_string);
	  
      // LENS encryption key
      str_value = (char *)(&Registers[LENS_ENCRYPT_KEY_1]);
      strncpy(name_string, str_value, (LENS_ENCRYPT_KEY_8 - LENS_ENCRYPT_KEY_1 + 1) *2);
      name_string[ (LENS_ENCRYPT_KEY_8 - LENS_ENCRYPT_KEY_1+1)*2] = '\0';
      db.Set(LENS_PARAM_DB_NAME, "NetworkEncryption", name_string);

	  
      // LENS Group lost threshold
      int_value = (int)Registers[LENS_TEAMMATE_LOST_INTERVAL];
      sprintf(int_string, "%d", int_value);
      db.Set(LENS_PARAM_DB_NAME, "InstrumentLostSeconds", int_string);
	  
	  str_value = (char *)(&Registers[INET0_URL_ADDR1]);
      strncpy(name_string, str_value, (INET0_URL_ADDR32 - INET0_URL_ADDR1 + 1) *2);
      name_string[ (INET0_URL_ADDR32 - INET0_URL_ADDR1+1)*2] = '\0';
      db.Set(LENS_PARAM_DB_NAME, "INetURL", name_string);
	  
	  str_value = (char *)(&Registers[INET0_USER_NAME1]);
      strncpy(name_string, str_value, (INET0_USER_NAME16 - INET0_USER_NAME1 + 1) *2);
      name_string[ (INET0_USER_NAME16 - INET0_USER_NAME1+1)*2] = '\0';
      db.Set(LENS_PARAM_DB_NAME, "INetUsername", name_string);
	  
	  str_value = (char *)(&Registers[INET0_PASSWORD1]);
      strncpy(name_string, str_value, (INET0_PASSWORD16 - INET0_PASSWORD1 + 1) *2);
      name_string[ (INET0_PASSWORD16 - INET0_PASSWORD1+1)*2] = '\0';
      db.Set(LENS_PARAM_DB_NAME, "INetPassword", name_string);
	  
	  // Save iNet Server 1
	  str_value = (char *)(&Registers[INET1_URL_ADDR1]);
      strncpy(name_string, str_value, (INET1_URL_ADDR32 - INET1_URL_ADDR1 + 1) *2);
      name_string[ (INET1_URL_ADDR32 - INET1_URL_ADDR1+1)*2] = '\0';
      db.Set(LENS_PARAM_DB_NAME, "INet1URL", name_string);
	  
	  str_value = (char *)(&Registers[INET1_USER_NAME1]);
      strncpy(name_string, str_value, (INET1_USER_NAME16 - INET1_USER_NAME1 + 1) *2);
      name_string[ (INET1_USER_NAME16 - INET1_USER_NAME1+1)*2] = '\0';
      db.Set(LENS_PARAM_DB_NAME, "INet1Username", name_string);
	  
	  str_value = (char *)(&Registers[INET1_PASSWORD1]);
      strncpy(name_string, str_value, (INET1_PASSWORD16 - INET1_PASSWORD1 + 1) *2);
      name_string[ (INET1_PASSWORD16 - INET1_PASSWORD1+1)*2] = '\0';
      db.Set(LENS_PARAM_DB_NAME, "INet1Password", name_string);
	  
	  // Save iNet Server 2
	  str_value = (char *)(&Registers[INET2_URL_ADDR1]);
      strncpy(name_string, str_value, (INET2_URL_ADDR32 - INET2_URL_ADDR1 + 1) *2);
      name_string[ (INET2_URL_ADDR32 - INET2_URL_ADDR1+1)*2] = '\0';
      db.Set(LENS_PARAM_DB_NAME, "INet2URL", name_string);
	  
	  str_value = (char *)(&Registers[INET2_USER_NAME1]);
      strncpy(name_string, str_value, (INET2_USER_NAME16 - INET2_USER_NAME1 + 1) *2);
      name_string[ (INET2_USER_NAME16 - INET2_USER_NAME1+1)*2] = '\0';
      db.Set(LENS_PARAM_DB_NAME, "INet2Username", name_string);
	  
	  str_value = (char *)(&Registers[INET2_PASSWORD1]);
      strncpy(name_string, str_value, (INET2_PASSWORD16 - INET2_PASSWORD1 + 1) *2);
      name_string[ (INET2_PASSWORD16 - INET2_PASSWORD1+1)*2] = '\0';
      db.Set(LENS_PARAM_DB_NAME, "INet2Password", name_string);
	  
	  // Save iNet Server 3
	  str_value = (char *)(&Registers[INET3_URL_ADDR1]);
      strncpy(name_string, str_value, (INET3_URL_ADDR32 - INET3_URL_ADDR1 + 1) *2);
      name_string[ (INET3_URL_ADDR32 - INET3_URL_ADDR1+1)*2] = '\0';
      db.Set(LENS_PARAM_DB_NAME, "INet3URL", name_string);
	  
	  str_value = (char *)(&Registers[INET3_USER_NAME1]);
      strncpy(name_string, str_value, (INET3_USER_NAME16 - INET3_USER_NAME1 + 1) *2);
      name_string[ (INET3_USER_NAME16 - INET3_USER_NAME1+1)*2] = '\0';
      db.Set(LENS_PARAM_DB_NAME, "INet3Username", name_string);
	  
	  str_value = (char *)(&Registers[INET3_PASSWORD1]);
      strncpy(name_string, str_value, (INET3_PASSWORD16 - INET3_PASSWORD1 + 1) *2);
      name_string[ (INET3_PASSWORD16 - INET3_PASSWORD1+1)*2] = '\0';
      db.Set(LENS_PARAM_DB_NAME, "INet3Password", name_string);
	  
	  // Save active iNet server index
	  int_value = (int)Registers[ACTIVE_INET_SERVER_INDEX];
      sprintf(int_string, "%d", int_value);
      db.Set(LENS_PARAM_DB_NAME, "INetIndex", int_string);
	  
	  // Save the LCM default password
	  str_value = (char *)(&Registers[LCM_DEFAULT_PASSWD1]);
      strncpy(name_string, str_value, (LCM_DEFAULT_PASSWD8 - LCM_DEFAULT_PASSWD1 + 1) *2);
      name_string[ (LCM_DEFAULT_PASSWD8 - LCM_DEFAULT_PASSWD1+1)*2] = '\0';
      db.Set(ISC_MFG_DB_NAME, DEFAULT_LCM_PASSWORD_NAME, name_string);
  
	  // Save the Modbus Enabled register
	  int_value = ((int)Registers[MODBUS_FUNC_ENABLE] == 0) ? 0 : 1;
      sprintf(int_string, "%d", int_value);
      db.Set(LENS_PARAM_DB_NAME, "ModbusEnabled", int_string);
	
	  int_value = (int)Registers[ENALBE_LENS_TEST_MODE];
	  if(int_value == ENABLE_LENS_TEST_MODE_VALUE)
	  {
		  db.Set(LENS_PARAM_DB_NAME, ENABLE_LENS_TEST_MODE_NAME, "1");
	  }
	  else
	  {
		  db.Set(LENS_PARAM_DB_NAME, ENABLE_LENS_TEST_MODE_NAME, "0");
	  }
	
	  // Save the Ethernet Enabled register
	  int_value = ((int)Registers[ETHERNET_ENABLE] == 0) ? 0 : 1;
      sprintf(int_string, "%d", int_value);
      db.Set(CELLULAR_DB_NAME, "Ethernet", int_string);
	
      Registers[COMMINT_PARAMETER] = 0;
    }
  return 0;
}

int modbus_register_handler(modbus_t *ctx, const uint8_t *req, int req_length, modbus_mapping_t *mb_mapping)
{
  int offset = ctx->backend->header_length;
  int function = req[offset];
  uint16_t address = (req[offset+1] << 8) + (req[offset+2]);
  time_t cur_t;
  db_monitor::ConfigDB db;
  int int_value;
  /* // // Henry/Robert
  printf("Address: %d. %x, %x\n", address,
						 mb_mapping->tab_registers[AWARE360_TRULINK_SN1],
						  mb_mapping->tab_registers[AWARE360_TRULINK_SN2]);
	*/
  switch(address)
    {

    case FIRMWARE_SUBVER_1:
    case FIRMWARE_SUBVER_2:
    case FIRMWARE_BUILD_VER:
    case HARDWARE_VERSION_1:
    case HARDWARE_VERSION_2:
    case HARDWARE_VERSION_3:
    case AWARE360_TRULINK_SN1:
    case AWARE360_TRULINK_SN2:
    case AWARE360_TRULINK_SN3:
    case AWARE360_TRULINK_SN4:
	case LENS_RADIO_HARDWARE_VER:
      if (function == _FC_WRITE_SINGLE_REGISTER ||
          function == _FC_WRITE_MULTIPLE_REGISTERS ||
          function == _FC_WRITE_AND_READ_REGISTERS)
        return 0;
      else
        return 1;
    case DEV_TIME_LOW_WORD:
    case DEV_TIME_HIGH_WORD:
      if (function == _FC_READ_HOLDING_REGISTERS)
        {
          cur_t = time(NULL);
          mb_mapping->tab_registers[DEV_TIME_LOW_WORD] = (uint16_t)cur_t;
          mb_mapping->tab_registers[DEV_TIME_HIGH_WORD] = (uint16_t)((cur_t >> 16) & 0x0000FFFF);
        }
      break;
	case LENS_RADIO_STATUS:
		if (function == _FC_READ_HOLDING_REGISTERS)
		{
			// Load Radio status
			// Load Modbus enable register
			int_value = db.GetInt(LENS_PARAM_DB_NAME, DEFAULT_LCM_PASSWORD_NAME, 0);
			mb_mapping->tab_registers[LENS_RADIO_STATUS] = int_value;
		}
		else if (function == _FC_WRITE_SINGLE_REGISTER ||
          function == _FC_WRITE_MULTIPLE_REGISTERS ||
          function == _FC_WRITE_AND_READ_REGISTERS)
        return 0;
		
    case COMMINT_PARAMETER:
      printf("Access commit register\n");
      break;
    }

  if(address >= SIM_ICCID_ADDR1 && address <= SATELLITE_IMEI_ADDR115)
  {
	if (function == _FC_WRITE_SINGLE_REGISTER ||
	  function == _FC_WRITE_MULTIPLE_REGISTERS ||
	  function == _FC_WRITE_AND_READ_REGISTERS)
	return 0;
  }
	
  if(address >= MODBUS_MAX_REGISTERS)
    return 0;
  return 1;
}

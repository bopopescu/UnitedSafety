#include <boost/format.hpp>
#include "lens.h"
#include "AFS_Timer.h"
#include "LensRegisters.h"
#include <INET_IPC.h>
#include "INetConfig.h"
#include "ConfigDB.h"

extern INetConfig *g_pLensParms;  // read the db-config parameters
extern INET_IPC g_INetIPC;  // common data.
extern ATSLogger g_log;

struct spi_ioc_transfer xfer[2];

//--------------------------------------------------------------------------------------
Lens::Lens():fd(0), status(0), devName("/dev/spidev1.0")
{
	pthread_mutex_init(&m_SPIMutex, 0);
	//g_INetIPC.LENSSPIFailure(false); //ISCP-332

	if (spi_init("/dev/spidev1.0") < 0)
	{
		ats_logf(ATSLOG_ERROR, RED_ON "Can't open spi device" RESET_COLOR);
		exit(1);
	}
	ReadRegisters();  // loads the Registers into m_LensRegisters.
}

//--------------------------------------------------------------------------------------
void Lens::spi_read(int add1,int add2,int nbytes, char *destbuf)
{
	pthread_mutex_lock(&m_SPIMutex);
	int status;
	char buf[10];
	char buf2[4096];

	memset(buf, 0, sizeof buf);
	memset(buf2, 0, sizeof buf2);
	buf[0] = CMD_READ_BYTES;
	buf[1] = 0x00;
	buf[2] = add1;
	buf[3] = add2;
	xfer[0].tx_buf = (unsigned long)buf;
	xfer[0].len = 4; /* Length of  command to write*/
	xfer[1].rx_buf = (unsigned long) buf2;
	xfer[1].len = nbytes; /* Length of Data to read */
	status = ioctl(fd, SPI_IOC_MESSAGE(2), xfer);

	if (status < 0)
		perror("SPI_IOC_MESSAGE (spi_read)");
	else
		memcpy(destbuf, buf2, nbytes);

	pthread_mutex_unlock(&m_SPIMutex);
}

void Lens::spi_write(int add1,int add2,int nbytes,char *value)
{
	pthread_mutex_lock(&m_SPIMutex);
	unsigned char b[4096];
	int status;

	memset(b, 0, sizeof b);
	b[0] = CMD_WRITE;
	b[1] = 0x00;
	b[2] = add1;
	b[3] = add2;
	memcpy(b + 4, value, nbytes);
	xfer[0].tx_buf = (unsigned long)b;
	xfer[0].len = nbytes + 4; /* Length of  command to write*/
	status = ioctl(fd, SPI_IOC_MESSAGE(1), xfer);
	pthread_mutex_unlock(&m_SPIMutex);
	if (status < 0)
	{
		perror("SPI_IOC_MESSAGE (spi_write)");
		return;
	}
}

int Lens::spi_init(const ats::String &devname)
{
	const char* dev_fname = devname.c_str();
	fd = open(dev_fname, O_RDWR);
	__u8    mode, lsb, bits;
	__u32 speed = 1000000;

	if(fd < 0)
	{
		fprintf(stderr, "Failed to open \"%s\". (%d) %s\n", dev_fname, errno, strerror(errno));
		ats_logf(ATSLOG_ERROR, "%s, %d: Failed to open \"%s\" (%d) %s", __FILE__, __LINE__, dev_fname, errno, strerror(errno));
		g_INetIPC.LENSSPIFailure(true); //<ISCP-163>
		return 1;
	}

	if (ioctl(fd, SPI_IOC_RD_MODE, &mode) < 0)
	{
		perror("SPI rd_mode");
		return 1;
	}

	if (ioctl(fd, SPI_IOC_RD_LSB_FIRST, &lsb) < 0)
	{
		perror("SPI rd_lsb_fist");
		return 1;
	}

	if (ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits) < 0)
	{
		perror("SPI bits_per_word");
		return 1;
	}

	if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0)
	{
		perror("SPI max_speed_hz");
		return 1;
	}

	if (ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed) < 0)
	{
		perror("SPI max_speed_hz");
		return 1;
	}


	ats_logf(ATSLOG_DEBUG, "spi mode %d, %d bits %sper word, %d Hz max",mode, bits, lsb ? "(lsb first) " : "", speed);
	devName = dev_fname;
	return fd;
}
void Lens::reset()
{
	ats::write_file("/sys/class/gpio/gpio122/value", "0");
	usleep(50 * 1000);
	ats::write_file("/sys/class/gpio/gpio122/value", "1"); //drh
	usleep(50 * 1000);
}
char Lens::ReadInitFlag()
{
	char b;
	spi_read(0x0f, 0x00, 1, &b);
	ats_logf(ATSLOG_DEBUG, "Init flag %02x", b);
	m_LensRegisters.Initialize(b);
	return b;
}

char Lens::InitFlag()
{
	return m_LensRegisters.Initialize();
}

int Lens::GetRadioNetworkStatus()
{
	static char lastStatus = -1;
	if (!fd) return -1;
	char b;
	spi_read(0x0f, 0x27, 1, &b);
	if (lastStatus != b)
	{
		g_INetIPC.LensNetworkStatus(b);
		ats_logf(ATSLOG_DEBUG, "\x1b[1;44;37mRadio network status changed to: %02x!\x1b[0m", b);
		lastStatus = b;
	}
	status = b;
	return status;
}

char Lens::GetINetConnection()
{
	char b;
	spi_read(0x0f, 0x35, 1, &b);
	ats_logf(ATSLOG_DEBUG, "Init flag %02x", b);
	return b;
}

int Lens::set_sm_reg()
{
	while (1)
	{
		status = GetRadioNetworkStatus();
		if (status == 4) // need to wake up - should not happen
		{
			ats_logf(ATSLOG_DEBUG, "set_sm_reg status=%d", status);
			return -1;
		}

		ats_logf(ATSLOG_DEBUG, "set sm reg");
		char b = 0xff;
		spi_write(0x0f, 0x29, 1, &b); // leader qualification score (0???)

		b = 0x00;
		spi_write(0x0f, 0x2b, 1, &b); // current number of peers
		m_LensRegisters.NumberOfPeers(b);
		g_INetIPC.PeerCount(b);

		b = g_INetIPC.MaxPeers();
		spi_write(0x0f, 0x2c, 1, &b);  // max number of peers (24)
		usleep(100* 1000);  // don't let this run away on us.

		// now check them.
		spi_read(0x0f, 0x29, 1, &b); // leader qualification score (0???)
		if (b != 0xff)
			ats_logf(ATSLOG_ERROR, "Warning: Invalid leader qualification score = %d", b);
		spi_read(0x0f, 0x2b, 1, &b); // current number of peers
		if (b != 0x00)
			ats_logf(ATSLOG_ERROR, "Warning: Current number of peers is not zero at startup.  Currently %d peers", b);
		spi_read(0x0f, 0x2c, 1, &b); // max number of peers (24)
		if (b > 24)
			ats_logf(ATSLOG_ERROR, "Warning: Max Number of peers is > 24 - MaxPeers=%d", b);
		return 0;
	}
}

void Lens::IncrementPeerCount()
{
	char b;
	spi_read(0x0f, 0x2b, 1, (char *)&b);
	b++;
	ats_logf(ATSLOG_DEBUG, "\x1b[1;44;37mPeer count = %d" RESET_COLOR, b);
	spi_write(0x0f, 0x2b, 1, &b);
	m_LensRegisters.NumberOfPeers(b);
	g_INetIPC.PeerCount(b);
}

void Lens::DecrementPeerCount()
{
	char b;
	spi_read(0x0f, 0x2b, 1, (char *)&b);
	
	if( b >= 1)
	{
		b--;
		spi_write(0x0f, 0x2b, 1, &b);
		ats_logf(ATSLOG_DEBUG, "\x1b[1;44;37mPeer count = %d" RESET_COLOR, b);
		m_LensRegisters.NumberOfPeers(b);
		g_INetIPC.PeerCount(b);
	}
}

//--------------------------------------------------------------
//See page 35-37 - read the mac address from the radio
// and then build the static string and write it to
// memory
void Lens::WriteIdentifyGeneral()
{
	char b[128], t[128];
	spi_read(0x0f, 0x01, 8, b);  //read the mac address from the registers- page 41

	memset(t, 0, 128);
	memcpy(t + 1, b, 8);  // set the mac address - page 41
	memcpy(t + 9, static_string, sizeof static_string);

	// insert the serial number
	std::string strSN = g_pLensParms->SerialNum();
	strncpy(&t[12], strSN.c_str(), (strSN.length() <= 16)? strSN.length() : 16);
		
	// insert the insert UserName
	std::string strSite = g_pLensParms->SiteName();
	unsigned char len = strSite.length();
	t[0] = 38;

	if (strSite.length() <= 16)  // ignore site name if too long or not there.
	{
		unsigned char tsize = strSite.length();
		tsize &= 0x1F;  // zero out bits 5-7 - Sitename is 001LLLLL where L is length of string up to 16
		tsize |= 0x20;  // set as sitename
		t[38] = tsize;
		strncpy(&t[39], strSite.c_str(), len );
		t[0] += (len + 1);
	}
	
	t[0] += 1;  // add a byte for the checksum
	int checksum = 0;
	for(int i = 0; i < t[0] - 1; ++i)  // don't include the checksum therefore (t[0] - 1)
		checksum += t[i];

	t[(t[0] - 1)] = (char)checksum;
	LogHexData("WriteIdentifyGeneral: (write)", t, (t[0]), GREEN_ON);  //output to log file 
	spi_write(0x00, 0x00, (t[0]), t);
	spi_read(0x00, 0x00, (t[0]), b);
	LogHexData("WriteIdentifyGeneral: (read )", b, (t[0]), GREEN_ON);  //output to log file 

}

//--------------------------------------------------------------------------------------
// page 29
void Lens::set_identify_sensor_configuration()
{
	spi_write(0x01, 0x00, 16, (char *)identify_sensor_configuration);
}
//--------------------------------------------------------------------------------------
void Lens::SetINetConnectionFlag(char inetconn)
{
	spi_write(0x0F, 0x35, 1, (char *)&inetconn);
	m_LensRegisters.INetConnectionFlag(inetconn);
	
	if (inetconn != 0)
		inetconn = 3;

	spi_write(0x0E, 0x03, 1, &inetconn);  // for now use cellular as connection type.
}

//--------------------------------------------------------------------------------------
bool Lens::send_to_outgoing_mailbox(const char *data, int length)
{
	if (length > max_outgoing_buffer_size)
	{
		ats_logf(ATSLOG_ERROR, "%s, %d: buf size is bigger then mail box size", __FILE__, __LINE__);
		return false;
	}
	char buf[2];
	spi_read(0x10, 0x00, 2, buf);
	__u16 hp = (buf[0] << 8) | (buf[1] + 0x1004);
	//printf("outgoing mailbox head %04x\n", hp);
	spi_read(0x10, 0x02, 2, buf);
//	__u16 tpp = buf[0] << 8 | buf[1] + 0x1004;
//	ats_logf(ATSLOG_INFO, BLUE_ON "send_to_outgoing_mailbox: Start:Mailbox head %04x   tail %04x" RESET_COLOR, hp, tpp);
	//printf("outgoing mailbox tail %04x\n", tpp);
	//if (tpp == hp)
	{
		__u16 tp = 0;
		__u16 diff = 0x2000 - hp;
		if (diff > length )
		{
			tp = hp + length;
			spi_write((char)(hp>>8), (char)hp, length, (char*)data);
		}
		else
		{
			spi_write((unsigned char)hp>>8, (unsigned char)hp, diff, (char *)data);
			spi_write(0x10, 0x04, length - diff, (char*)(data + diff));
			tp = 0x1003 + length - diff;
		}
		//outgoing_headp = hp;
		// update tail point in shared memory
		__u16 tailOffset = tp - 0x1004;
		buf[0] = tailOffset >> 8;
		buf[1] = tailOffset;
		spi_write(0x10, 0x02, 2, buf);
//	ats_logf(ATSLOG_INFO, BLUE_ON "send_to_outgoing_mailbox: Updated tail: %04x" RESET_COLOR, tp);		
		//printf("update tail pointer  %04x outgoing\n", tp);

		//spi_read(0x10, 0x00, 2, buf);
		// printf("end ---- outgoing mailbox head %04x\n", buf[0] << 8 | buf[1]);
		//spi_read(0x10, 0x02, 2, buf);
		// printf("end ---- outgoing mailbox tail %04x\n", buf[0] << 8 | buf[1]);
		return true;
	}
	//printf("outgoing not ready");
	return false;
}

bool Lens::read_from_incoming_mailbox(char *data, int &length)
{
	char buf[2];
	spi_read(0x20, 0x00, 2, buf);
	__u16 hp = (buf[0] << 8) | (buf[1] + 0x2004);
	spi_read(0x20, 0x02, 2, buf);
	__u16 tp = (buf[0] << 8) | (buf[1] + 0x2004);
	
	//ats_logf(ATSLOG_ERROR, MAGENTA_ON "Mailbox  head:%d  tail:%d" RESET_COLOR, hp, tp);		
	if (tp > 0x3000 || tp < 0x2000 || hp > 0x3000 || hp < 0x2000) 
	{
		// if we are here we need to reset the LENS.
		ats_logf(ATSLOG_ERROR, RED_ON "Mailbox head/tail pointer conflict head:%d  tail:%d" RESET_COLOR, hp, tp);		
		StartRadio();
		usleep (50 * 1000);
		
		return false;
	}
	if(hp == tp) 
	{
		return false;
	}
	char _data[max_outgoing_buffer_size];
	memset(_data, 0, max_outgoing_buffer_size);
	spi_read(0x20, 0x04, 4092, _data);
#if 0
	printf("incoming mailbox head %04x tail %04x\n", hp, tp);
	for( int i = 0; i < 4092; ++i)
	{
		printf("%02x ", _data[i]);
	}
#endif
	if (hp < tp)
	{
		length = tp - hp + 1;
		//spi_read((unsigned char)hp>>8, ((unsigned char)hp), length, data);
		memcpy(data, _data + hp - 0x2004, length);
#if 0
		for( int i = 0; i < length; ++i)
		{
			printf("%02x ", data[i]);
		}
		printf("\n");
#endif
	}
	else
	{
		__u16 diff = 0x3000 - hp;
		length  = diff + tp - 0x2003;
		memcpy(data, _data + hp - 0x2004, diff);
		memcpy(data + diff, _data, tp - 0x2003);
		//spi_read((unsigned char)hp>>8, ((unsigned char)hp, diff, data);
		//spi_read(0x20, 0x04, tp - 0x2003, data + diff);
#if 0
		int i = 0;
		for( i = 0; i < length; ++i)
		{
			printf("%02x ", data[i]);
		}
		printf("\n");
#endif
	}
	hp = tp;
	if (hp >= 0x2000 && hp<=0x2003) hp = 0x2004;

	char b[2];
	__u16 headoffset = hp - 0x2004;
	b[0] = headoffset >> 8;
	b[1] = headoffset;
	spi_write(0x20, 0x00, 2, b);
	return true;
}

//-----------------------------------------------------------
// dump all the registers for the LENS on the TruLink
void Lens::ReadRegisters()
{
	char buf[100];
	spi_read(0x0F, 0x00, 0x36, buf);
	
	m_LensRegisters.SetData((unsigned char *)buf);
	g_INetIPC.SetLensRegisters(m_LensRegisters);
	
	db_monitor::ConfigDB db;
	db.Update("isc-lens", "RadioFWVer", m_LensRegisters.rawRadioProtocolVersion());
	std::string iValue = str( boost::format("%d") % (int)m_LensRegisters.RadioHardwareVersion());

	db.Update("isc-lens", "RadioHWVer", iValue);
}
//-----------------------------------------------------------
// read all the registers for the LENS then update the values from
// db-config and write them back to the LENS.
// TODO: finish this from db-config
void Lens::WriteRegisters()
{
	char buf[100];
	spi_read(0x0F, 0x00, 0x36, buf);
	short sval;
	
	buf[0x23] = (char)g_pLensParms->PrimaryChannel();
	buf[0x24] = (char)g_pLensParms->SecondaryChannel();
	sval = (short)g_pLensParms->ChannelMask();
	memcpy(&buf[0x25], &sval, 2);
	sval = (short)g_pLensParms->FeatureBits();
	memcpy(&buf[0x2d], &sval, 2);
	buf[0x28] = (char)g_pLensParms->MaxHops();
	spi_write(0x0F, 0x00, 0x36, buf);
	// reread the newly written registers so LensRegisters are valid
	ReadRegisters();
	dump_registers("/var/log/registers.log", "lens:WriteRegisters");
}
//-----------------------------------------------------------
// dump all the registers for the LENS on the TruLink
void Lens::dump_registers()
{
	char buf[100];
	spi_read(0x0F, 0x00, 0x36, buf);
	
	LensRegisters lr;
	lr.SetData((unsigned char *)buf);
	std::string lrData;
	
	lr.toStr(lrData);
	fprintf(stderr, "%s\n", lrData.c_str());
}//-----------------------------------------------------------
// dump all the registers for the LENS on the TruLink
void Lens::dump_registers(std::string fname, std::string title)
{
	char buf[100];
	spi_read(0x0F, 0x00, 0x36, buf);
	
	LensRegisters lr;
	lr.SetData((unsigned char *)buf);
	std::string lrData;
	
	lr.toStr(lrData);
	FILE *fp;
	if ((fp = fopen(fname.c_str(), "a")) != NULL)
	{
		fprintf(fp, "%s\n", title.c_str());
		fprintf(fp, "%s\n\n\n", lrData.c_str());
		fclose(fp);
	}
}

//-----------------------------------------------------------
// 2.2.1.1 - page 7
// MAC is the last 3 bytes of the MAC address.
// timeout = 0 - terse mode, 1-255 length in seconds to send verbose
void Lens::SetRemoteDeviceVerbose(char * mac, int timeout)
{
	char msg[16];
	msg[0] = 0x24;
	msg[1] = 0x24;
	msg[2] = 0x30; // message type
	msg[3] = 0x05; // length
	memcpy(&msg[4], mac, 3);
	msg[7] = (unsigned char)timeout;
	msg[8] = 0x23;
	msg[9] = 0x23;
	send_to_outgoing_mailbox(msg, 10);
}	

//-----------------------------------------------------------
// 2.2.1.2 - page 8 - Set Network Configuration
// updated to include channel mask Nov 21, 2018
// updated to include everything else August 2019
int Lens::SetNetworkConfiguration()
{
	unsigned char network_configuration[] = {0x24, 0x24, 0x61, 0x0e, 0x00, 0x01, 0x04, 0x09, 0x7d, 0xef, 0x02, 0x05, 0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0x23, 0x23};
	short net = (short)(g_pLensParms->NetworkName());
	network_configuration[4] = 	(unsigned char)((net>> 8) & 0xFF); //<Henry asked to change && to & in additon to ISCP-317>
	network_configuration[5] = 	(unsigned char)(net & 0xFF);
	network_configuration[6] = 	(unsigned char)(g_pLensParms->PrimaryChannel());
	network_configuration[7] = 	(unsigned char)(g_pLensParms->SecondaryChannel());
	short sval = (short)g_pLensParms->ChannelMask();
	network_configuration[8] = 	(unsigned char)((sval>> 8) & 0xFF);  //<ISCP-317 , && changed to &>
	network_configuration[9] = 	(unsigned char)(sval & 0xFF);
	network_configuration[10] = (unsigned char)g_pLensParms->NetworkInterval();
	network_configuration[11] = (unsigned char)g_pLensParms->MaxHops();
	sval = (short)g_pLensParms->FeatureBits();
	network_configuration[12] = 	(unsigned char)((sval>> 8) && 0xFF);
	network_configuration[13] = 	(unsigned char)(sval & 0xFF);
	network_configuration[14] = 	(unsigned char)(g_pLensParms->PowerLevel());

//	std::string str((char *)network_configuration, sizeof(network_configuration));

	int status;

	// loop until radio goes into the state we want.
	while (1)
	{
		ats_logf(ATSLOG_INFO, YELLOW_ON "%s,%d: Set Network Configuration %s\r" RESET_COLOR, __FILE__, __LINE__, toHex( (char *)network_configuration, sizeof(network_configuration)).c_str() );
		send_to_outgoing_mailbox((char *)network_configuration, sizeof(network_configuration));
		usleep(250000);
		status = GetRadioNetworkStatus();
		if ((status == 1) || (status == 2))
			return 0;
		if (status == 4) // need to wake up - should not happen
			return -1;
	}
}
//-----------------------------------------------------------
int Lens::SetNetworkEncryption()
{
//	char network_encryption[] = {0x24, 0x24, 0x65, 0x12, 0x01, 0x41, 0x77, 0x61, 0x72, 0x65, 0x33, 0x36, 0x30, 0x44, 0x65, 0x76, 0x4b, 0x65, 0x79, 0x31, 0x37 ,0x23, 0x23};
	char network_encryption[] = {0x24, 0x24, 0x65, 0x12, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 ,0x23, 0x23};
	
	char key[32];
	memset(key, ' ', 16);
	strncpy(key, g_pLensParms->NetworkEncryption().c_str(), 16);
	key[16] = '\0';
	network_encryption[4] = (char)g_pLensParms->EncryptionType();
	memcpy(&network_encryption[5], key, 16);
	memcpy(key, network_encryption, sizeof(network_encryption));
	key[sizeof(network_encryption)] = '\0';
	
	ats_logf(ATSLOG_INFO, YELLOW_ON "%s,%d: Setting Network Encryption %s\r" RESET_COLOR, __FILE__, __LINE__, (ats::to_hex(key)).c_str());
	send_to_outgoing_mailbox(network_encryption, sizeof(network_encryption));
	sleep(1);
	return 0;
}

//-----------------------------------------------------------
// page 10 - 2.2.1.3 Connect/Disconnect Network
void Lens::NetworkConnect()
{
	const char connect_network[] = {0x24, 0x24, 0x63, 0x02, 0x01 ,0x23, 0x23};
	send_to_outgoing_mailbox(connect_network, sizeof(connect_network));
}

//-----------------------------------------------------------
// page 10 - 2.2.1.3 Connect/Disconnect Network
void Lens::NetworkDisconnect()
{
	const char disconnect_network[] = {0x24, 0x24, 0x63, 0x02, 0x00 ,0x23, 0x23};
	send_to_outgoing_mailbox(disconnect_network, sizeof(disconnect_network));
	ats_logf(ATSLOG_INFO, RED_ON "Lens Network Disconnecting!" RESET_COLOR);
}

//------------------------------------------------------------------------------------------------------
// SetTestMode - send the Test Mode message with a variety of settable parameters.
//  This is to be used exclusively for the special FCC/PTCRB LENS firmware build.
// bytes 4-19 are the encryption key - set to all 00 for tests
// bytes 21-22 are the network - set to 00 00 for test.
void Lens::SetTestMode
(
	int channel
)
{
	if (channel < 0 || channel > 14)  // invalid channel
		return;

	char testmode[] = {0x24, 0x24, 0x78, 0x14, '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F', 0x00, 0x00, 0x01, 0x23, 0x23};
		
	testmode[20] = (char)channel;
	send_to_outgoing_mailbox(testmode, 25);
}

std::string Lens::toHex(char * buf, int len)
{
	std::string outstr, cstr;
	for (int i = 0; i < len; i++)
	{
		cstr = str (boost::format("%02x") % (int)buf[i]);
		outstr += cstr;
	}
	return outstr;
}


//------------------------------------------------------------------------------------------------------
// isConnected - is the radio status 3 or 6?
bool Lens::IsConnected()
{
	int status = GetRadioNetworkStatus();

	if (status == 0x03 || status == 0x06)
		return true;
		
	return false;
}


//------------------------------------------------------------------------------------------------------
// Instrument Status and data for Gateway - Page 51
//  Writes the data to 0x0E00
// Note - for the gateway you do not need to confirm the new_data-byte status
//
void Lens::WriteStatusData(char *buf, int len)
{
	if (len > 0 && len < 0x00FF)
		spi_write(0x0E, 0x00, len, buf);
}
//------------------------------------------------------------------------------------------------------
//  reads the first byte of the Instrument Status and Data memory to see if the radio has sent it to
//  the mesh network.
bool Lens::GetStatusDataRead()
{
	char c;
	spi_read(0x0E, 0x00, 1, &c);
	return (c == 0) ? true : false;
}
//-------------------------------------------------------------------------------------------------------------------------------
void Lens::LogHexData(const char *title, const char *data, const int len, const char*color)
{
	if (g_log.get_level() >= LOG_LEVEL_INFO)
	{
		ats::String s;
		char str[16];
		for(int i = 0; i < len; ++i)
		{
			if (i%10 == 0)  // write out the 0,10,20, etc byte as white for easier parsing.
				sprintf(str, RESET_COLOR "%02x %s", data[i], color);

			else
				sprintf(str, "%02x ", data[i]);
			s += str;
		}
		ats_logf(ATSLOG_INFO, "%s: %s %s" RESET_COLOR, title, color, s.c_str());
	}
}

//------------------------------------------------------------------------------------------------------
void Lens::Wakeup(int urgent_fd)
{
	int ret = ioctl(urgent_fd, ISC_WAKEUPRADIO);
	if (ret)
	{
		ats_logf(ATSLOG_ERROR, "%s, %d: Fail to call ioctl ISC_WAKEUPRADIO", __FILE__, __LINE__);
	}
	usleep(10000);
}

//------------------------------------------------------------------------------------------------------
//  returns true if able to write then read the memory chip on the TruLink.
bool Lens::LowLevelMemoryTest()
{
	char buf[128];
	for (short i = 0; i < 128; i++)
	{
		buf[i] = 0x30 + i %10;
	}
	
	unsigned char b[4096];
	int status;

	memset(b, 0, sizeof b);
	b[0] = CMD_WRMR;
	b[1] = 0x41;
	b[2] = 0x00;
	b[3] = 0x00;
	memcpy(b + 4, buf, 128);
	xfer[0].tx_buf = (unsigned long)b;
	xfer[0].len = 128 + 4; /* Length of  command to write*/
	status = ioctl(fd, SPI_IOC_MESSAGE(1), xfer);
	if (status < 0)
	{
		perror("SPI_IOC_MESSAGE (spi_write)");
		return false;
	}
	return true;
}

//------------------------------------------------------------------------------------------------------
// puts the LENS into upload mode
// See 2.2.1.12 PRIN-ICD_HOST-19 0x079 -- Enter Update Mode
//	Whisper Host Interface.
void Lens::SetUploadMode()
{
	char msg[16];
	msg[0] = 0x24;
	msg[1] = 0x24;
	msg[2] = 0x79; // Enter Update Mode message type
	msg[3] = 0x01; // length
	msg[4] = 0x23;
	msg[5] = 0x23;
	send_to_outgoing_mailbox(msg, 6);
}

//------------------------------------------------------------------------------------------------------
// returns true when upload mode established.  False if can't change after 30 seconds.
bool Lens::SwitchToUploadMode()
{
	printf("SwitchToUploadMode\n");
	AFS_Timer t;
	t.SetTime();
	int status = GetRadioNetworkStatus();
	
	reset();
	while (status != 8 && t.DiffTime() < 30)
	{
		usleep(100 * 1000);
		printf("Network status: %d (Line %d)\n", GetRadioNetworkStatus(), __LINE__);
		NetworkDisconnect();
		usleep(100 * 1000);
		printf("Network status: %d (Line %d)\n", GetRadioNetworkStatus(), __LINE__);
		int m_urgent_fd = open("/dev/lens-urgent", O_RDONLY);
		Wakeup(m_urgent_fd);
		close(m_urgent_fd);
		usleep(100 * 1000);
		printf("Network status: %d (Line %d)\n", GetRadioNetworkStatus(), __LINE__);

		SetUploadMode();
		usleep(100 * 1000);
		status = GetRadioNetworkStatus();
	}
	printf("SwitchToUploadMode Done: status is %d\n", status);
	if (status == 8)
		return true;
		
	return false;
}

//------------------------------------------------------------------------------------------------------
// runs everything required to start/restart the radio.  Includes wakeup, 
void Lens::StartRadio()
{
	reset();
	ats_logf(ATSLOG_DEBUG, CYAN_ON "StartRadio is resetting the LENS!" RESET_COLOR);		
	usleep (50 * 1000);
	int m_urgent_fd = open("/dev/lens-urgent", O_RDONLY);
	while (GetRadioNetworkStatus() == 0x04)
		Wakeup(m_urgent_fd);
	usleep (50 * 1000);
	ReadRegisters();  // loads the Registers into m_LensRegisters.
	NetworkConnect();
	while (SetNetworkConfiguration())	// wont return until radio status is 01 or 02 returns -1 if radio is asleep
		Wakeup(m_urgent_fd);
	WriteIdentifyGeneral();

	while (GetRadioNetworkStatus() == 0x04)
		Wakeup(m_urgent_fd);

	set_identify_sensor_configuration();

	while (GetRadioNetworkStatus() == 0x04)
		Wakeup(m_urgent_fd);

	SetNetworkEncryption();
	NetworkConnect();
	close(m_urgent_fd);
}
//------------------------------------------------------------------------------------------------------
// ReConnect Disconnects and Reconnects to the Radio network
void Lens::Reconnect()
{
	ats_logf(ATSLOG_DEBUG, CYAN_ON "StartRadio is resetting the LENS!" RESET_COLOR);		
	NetworkDisconnect();
	usleep (50 * 1000);
	int m_urgent_fd = open("/dev/lens-urgent", O_RDONLY);
	while (GetRadioNetworkStatus() == 0x04)
		Wakeup(m_urgent_fd);
	usleep (50 * 1000);
	NetworkConnect();
	while (SetNetworkConfiguration())	// wont return until radio status is 01 or 02 returns -1 if radio is asleep
		Wakeup(m_urgent_fd);
	WriteIdentifyGeneral();

	while (GetRadioNetworkStatus() == 0x04)
		Wakeup(m_urgent_fd);

	close(m_urgent_fd);
}



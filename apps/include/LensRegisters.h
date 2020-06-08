/*-------------------------------------------------------------------------------------------
Lens.h - definc the messages, registers, and item values for the Whisper Host Interface.

This is working off of the 11/13/2018 version of the document.  All page references will be
based on this document.

Dave Huff - Nov 19, 2018
-------------------------------------------------------------------------------------------*/


#pragma once
#include <string.h>
#include "LensMAC.h"
//----------------------------------------------------------------------------
// LensRegisters - class to define the onboard LENS registers.  
// Given a 73 byte buffer of raw data this will allow for the extraction and
// setting of any given register value.
//

class LensRegisters
{
private:
	unsigned char m_Initialize;
	unsigned char m_MAC[8];
	unsigned char m_RadioHardwareVersion;
	unsigned char m_RadioOSVersion[3];
	unsigned char m_RadioProtocolVersion[3];
	unsigned char m_NetworkID[2];
	unsigned char m_EncryptionType;
	unsigned char m_EncryptionKey[16];
	unsigned char m_PrimaryPublicChannel;
	unsigned char m_SecondaryPublicChannel;
	unsigned char m_ActiveChannelMask[2];
	unsigned char m_NetworkStatus;
	unsigned char m_MaxNumberOfHops;
	unsigned char m_LeaderQualificationScore;
	unsigned char m_NetworkInterval;
	unsigned char m_NumberOfPeers;
	unsigned char m_MaxNumberOfPeers;
	unsigned char m_FeatureBits[2];
	unsigned char m_ScriptCRC[2];
	unsigned char m_TransmitPowerSetting;
	unsigned char m_GatewayChannelMask[2];
	unsigned char m_CurrentListeningChannel;
	unsigned char m_INetConnectionFlag;

public:
	LensRegisters();
	virtual ~LensRegisters(){};
	
	void SetData(unsigned char *buf);  // this is the only way to load.  The reading of the registers is done through spi in the Lens class.
	std::string toStr(std::string &buf);  // output the whole register block as readable text.

public:
	// GETTERS
	unsigned char Initialize() {return m_Initialize;}
	void Initialize(char val){m_Initialize = val;}
	unsigned char *MAC(unsigned char * buf){memcpy(buf, m_MAC, 8); buf[8] = 0; return buf;}
	unsigned char RadioHardwareVersion(){return m_RadioHardwareVersion;}
	unsigned char *RadioOSVersion(unsigned char * buf){memcpy(buf, m_RadioOSVersion, 3); buf[3] = 0; return buf;}
	unsigned char *RadioProtocolVersion(unsigned char * buf){memcpy(buf, m_RadioProtocolVersion, 3); buf[3] = 0; return buf;}
	unsigned char *NetworkID(unsigned char * buf){memcpy(buf, m_NetworkID, 2); buf[2] = 0; return buf;}
	unsigned char EncryptionType() {return m_EncryptionType;}
	unsigned char *EncryptionKey(unsigned char * buf){memcpy(buf, m_EncryptionKey, 16); buf[16] = 0; return buf;}
	unsigned char PrimaryPublicChannel() {return m_PrimaryPublicChannel;}
	unsigned char SecondaryPublicChannel() {return m_SecondaryPublicChannel;}
	unsigned char *ActiveChannelMask(unsigned char * buf){memcpy(buf, m_ActiveChannelMask, 2); buf[2] = 0; return buf;}
	unsigned char NetworkStatus() {return m_NetworkStatus;}
	unsigned char MaxNumberOfHops() {return m_MaxNumberOfHops;}
	unsigned char LeaderQualificationScore() {return m_LeaderQualificationScore;}
	unsigned char NetworkInterval() {return m_NetworkInterval;}
	unsigned char NumberOfPeers() {return m_NumberOfPeers;}
	void  NumberOfPeers(const unsigned char val) { m_NumberOfPeers =  val;}
	unsigned char MaxNumberOfPeers() {return m_MaxNumberOfPeers;}
	unsigned char *FeatureBits(unsigned char * buf){memcpy(buf, m_FeatureBits, 2); buf[2] = 0; return buf;}
	unsigned char *ScriptCRC(unsigned char * buf){memcpy(buf, m_ScriptCRC, 2); buf[2] = 0; return buf;}
	unsigned char TransmitPowerSetting() {return m_TransmitPowerSetting;}
	unsigned char *GatewayChannelMask(unsigned char * buf){memcpy(buf, m_GatewayChannelMask, 8); buf[8] = 0; return buf;}
	unsigned char CurrentListeningChannel() {return m_CurrentListeningChannel;}
	unsigned char INetConnectionFlag() {return m_INetConnectionFlag;}
	void  INetConnectionFlag(const unsigned char val) { m_INetConnectionFlag =  val;}
	
	// SETTERS
	// FORMATTERS - output the values as human readable data.
	std::string Initialize(std::string &output);
	std::string MAC(std::string &output);
	std::string RadioHardwareVersion(std::string &output);
	std::string RadioOSVersion(std::string &output);
	std::string RadioProtocolVersion(std::string &output);
	std::string NetworkID(std::string &output);
	std::string EncryptionType(std::string &output);
	std::string EncryptionKey(std::string &output);
	std::string PrimaryPublicChannel(std::string &output);
	std::string SecondaryPublicChannel(std::string &output);
	std::string ActiveChannelMask(std::string &output);
	std::string ActiveChannelMask();  // this is just the unformatted hex value 
	std::string NetworkStatus(std::string &output);
	std::string MaxNumberOfHops(std::string &output);
	std::string LeaderQualificationScore(std::string &output);
	std::string NetworkInterval(std::string &output);
	std::string NumberOfPeers(std::string &output);
	std::string MaxNumberOfPeers(std::string &output);
	std::string FeatureBits(std::string &output);
	int FeatureBits() const; // this is just the unformatted int value 
	std::string ScriptCRC(std::string &output);
	std::string TransmitPowerSetting(std::string &output);
	std::string GatewayChannelMask(std::string &output);
	std::string CurrentListeningChannel(std::string &output);
	std::string INetConnectionFlag(std::string &output);

	// rawXXX - just provides the value - no titles or coloring - add as needed.
	std::string rawRadioProtocolVersion(std::string &output);
	std::string rawRadioProtocolVersion();  // the raw version is x.x.x
	std::string rawRadioProtocolVersion_Upload(std::string &output);  // the upload version is x-x-x
	int rawRadioOSVersion(){return m_RadioOSVersion[0];};
	
	LensMAC GetMAC();
};


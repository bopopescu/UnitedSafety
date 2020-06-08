/*******************************************************************************
 * Copyright (C) 2014 - 2019 Synapse Wireless, Inc.
 *
 * This software was developed by Synapse Wireless, Inc. for use by Industrial
 * Scientific Corporation.  It was developed and is being delivered under the
 * Design Service License provided in the SNAP(TM) Script Uploader Development
 * Proposal.  Use of this software must be in accordance with the terms and
 * conditions of the aforementioned license (included below for reference).
 *
 * --------------------------------------------------------------------------------
 * The Deliverables as defined herein, newly designed and developed by Synapse
 * pursuant to this agreement (collectively referred to as Licensed Technology)
 * shall be owned by Synapse and licensed to Customer according to the terms
 * outlined in this proposal and with the following limitations.
 *
 * No Intellectual Property (IP) Rights To Any Core Synapse Software And Other
 * Existing Technology Are Granted
 *
 * The Licensed Technology may be designed to interface or otherwise operate with
 * products owned or marketed by Synapse (Synapse Products), including but not
 * limited to software and modules for implementing, controlling, and monitoring
 * wireless networks and for executing and interpreting instructions of the
 * Software. Synapse Products shall include, but are not limited to, modules
 * currently or previously marketed by Synapse under the mark RF Engine® and the
 * Synapse Network Application Protocol software currently or previously marketed
 * by Synapse under the marks SNAPTM , PortalTM and SNAPTM Connect,
 * SNAP Lighting.com, SNAP Lighting Cloud Service as well as all improvements,
 * modifications, and derivatives thereof. Synapse Products shall also include all
 * software and hardware owned by Synapse as of the date of execution of this
 * Agreement, as well as all software and hardware conceived of, designed, or
 * developed by Synapse outside of the obligations of this Agreement. Customer
 * agrees that Synapse owns all rights, title, and interest in the Synapse Products
 * and except as otherwise expressly set forth herein, no licenses or intellectual
 * property rights in the Synapse Products are conveyed to Customer.
 *
 * Licenses To Core Synapse Software Are Not Waived
 *
 * To the extent that Customer desires to interface or operate the Licensed
 * Technology with any Synapse Product, then Customer shall separately purchase or
 * license such Synapse Product.
 *
 * License That Is Granted Is Limited to Modifications
 *
 * If Synapse, pursuant to its obligations under this Agreement, creates or
 * develops the Licensed Technology by modifying original software or hardware that
 * is owned by Synapse prior to such modifications, then Customer shall have a
 * world-wide, non-exclusive, royalty-free, and perpetual license to use the
 * modifications to the original software or hardware created or developed pursuant
 * to this Agreement for the sole purpose of designing, developing, manufacturing,
 * and distributing the products and nodes contemplated by this Agreement, but
 * Synapse shall retain all right, title, and interest in the software or hardware,
 * as well as all improvements, modifications, and derivatives of the original
 * software or hardware that are developed or otherwise created by Synapse.
 * Customer shall not have the right to sub-license the software that is licensed
 * to Customer pursuant to this Agreement, and Customer shall not make copies of
 * the licensed software or distribute the licensed software or hardware except to
 * the extent necessary to design, develop, manufacture, and distribute the
 * products and nodes contemplated by this Agreement.
 * --------------------------------------------------------------------------------
 ******************************************************************************/

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "atslogger.h"
#include "colordef.h"
#include "lens.h"
#include "script_uploader.h"
#include "bbuart.h"
#include "INetConfig.h"
#include <INET_IPC.h>

extern INetConfig *g_pLensParms;  // read the db-config parameters
extern ATSLogger g_log;
extern INET_IPC g_INetIPC;

#define STATIC_HOOKS 1

#define DESIRED_TICK_MS 100
#define TICK_SCALE 1000 // we are working with gettimeofday so resolution is in uSec 
#define TICK_DELAY DESIRED_TICK_MS * TICK_SCALE

/*****************************************************************************
 * We are declaring some flags for state information as required
****************************************************************************/
Snap_U8 script_crc_request_flag = 0;
Snap_U8 script_uploading_flag = 0;
Snap_U8 script_next_block_request_flag = 0;

/*****************************************************************************
 * Define some global vars
 ****************************************************************************/
int UART0 = 0;
Snap_U16 bl_req_offset = 0;
Snap_U8 bl_req_length = 0;

/*****************************************************************************
 * Incoming serial handlers just stuff the incoming data into circular buffers
 * for the main loop to process.
 ****************************************************************************/
 #define CIRC_BUF_SIZE 128

uint8_t snap_serial_buf[CIRC_BUF_SIZE];
uint8_t snap_serial_buf_read_idx = 0;
uint8_t snap_serial_buf_write_idx = 0;

static bool g_bSentInetNotification = false;

void checkForIncomingUARTData(void)
{
	int readLen = 0;
	char read_buf;

	do
	{
		//printf("Just before read\r\n");
		readLen = snapUartRead(UART0, &read_buf, 1);
		//printf("The read len in incoming data is %d\r\n", readLen);
		if (readLen == 1)
		{
			//printf("Sending data to SNAP FSM\r\n");

			snap_script_uploader_serial_rx((Snap_U8 *)(&read_buf), 1);
			if (snap_serial_buf_write_idx == CIRC_BUF_SIZE)
			{
				snap_serial_buf_write_idx = 0;
			}
		}
	} while(readLen != 0);
}

/*****************************************************************************
 * Hardware configuration
 ****************************************************************************/

void configure_3v3_uart()
{
	/* Setup /dev/ttySP2 to talk to the Synapse module
	 */
	snapUartConfig();
}



void sendNextBlock(Snap_U16 offset, Snap_U8 length)
{
	// We will be using a binary file and reading through it.
	// In theory the offset should match what we have read, not going to worry though
	// The length is what the programmer wants, but we may not can fill it.

	Snap_U8 val = 0;
	unsigned char buf[128];  // We know that the requested length will be less than 128.
	
	// open the file for binary read
	FILE * inf = fopen(g_pLensParms->UploadBin().c_str(), "rb");
	if (NULL == inf)
	{
		// abort the upgrade, maybe should exit.
		printf("ERROR: We failed to open the file\r\n");
		printf("\tMust abort the upgrade\r\n");
		if (!g_bSentInetNotification) //<ISCP-164>
		{
			ats_logf(ATSLOG_DEBUG, "LENS sending Script file - open failure\r");
			g_bSentInetNotification = true;
			AFS_Timer t;
			t.SetTime();
			std::string user_data = "988," + t.GetTimestampWithOS() + ", LENS Scrip Download Failure";
			user_data = ats::to_hex(user_data);

			send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
		}		
	}
	else
	{
		g_bSentInetNotification = false; // reset flag to send checksum error if occures again
		val = fseek(inf, offset, 0);  // seek in the file to the offset location from the beginning, returns 0 if successful
		// if val == 0 we are successful
		if (0 == val)
		{
			val = fread(buf, 1, length, inf); // attempt to read from the file, length bytes
			if (0 == val)
			{
				//printf("We have reached the end of the data.\r\n");
				// we have reached the end of the data
				snap_script_uploader_end_of_data();
			}
			// if val <= length, we are good to good, it should never be able to be longer than length.
			else if (val <= length)
			{
				//printf("Sending the next buffer to SM\r\n");
				/* Pass along the incoming data to the library */
				snap_script_uploader_data_block(buf, val);
			}
			else
			{
				printf("Magic, read more than we asked for!!!! \r\rn");
			}
			script_next_block_request_flag = 0;
		}
		// handle the file failing to sync, abort the upgrade
		else
		{
			// abort the upgrade, maybe should exit.
			printf("ERROR: We failed to seek to the offset in the file\r\n");
			printf("\tMust abort the upgrade\r\n");
		}
		fclose(inf);
	}
}

/*****************************************************************************
 * Script Uploader required helper functions
 ****************************************************************************/
void snap_script_uploader_request_next_block(Snap_U16 offset, Snap_U8 length)
{

	// It appears that we can not immediately send the next block, this function
	// must return first.  So we are storing off the offset and length, setting
	// a flag so that the loop below can send the block on the next run.
	bl_req_offset = offset;
	bl_req_length = length;
	script_next_block_request_flag = 1;
	
	//printf("Received a request for next block: %d: %d\r\n", offset, length);
}

void report_upload_done()
{
	printf("\r\n\r\nDONE\r\n");
}

#define MAX_SNAP_TX 262  // History lost on why this size.
Snap_U8 snap_tx_data[MAX_SNAP_TX];
Snap_U16 snap_tx_data_length = 0;

void snap_uart_tx_handler(const Snap_U8 * data, Snap_U16 length)
{
	int32_t i;
	//printf("In snap uart tx hdlr; length is %d\r\n", length);
	if(length > MAX_SNAP_TX)
	{
		return;
	}

	// we could use memcpy here, but brings in another library
	for(i = 0; i < length; i++)
	{
		snap_tx_data[i] = data[i];
	}  
	snap_tx_data_length = length;
}

Snap_U8 script_crc_requested()
{
	return script_crc_request_flag;
}

void clear_script_crc_request()
{
	script_crc_request_flag = 0;
}

Snap_U8 script_upload_started()
{
	return script_uploading_flag;
}

void clear_script_uploading_flag()
{
	script_uploading_flag = 0;
}

void sendUartData(void)
{
	Snap_U16 i = 0;
	//printf("Got into send Data\r\n");
	/* Send any outgoing serial traffic requested by the library to the SNAP module. */
	if(snap_tx_data_length > 0)
	{
		//printf("Sending data length: %d\r\n", snap_tx_data_length);
		snapUartWrite(UART0, &snap_tx_data[i], snap_tx_data_length);
		snap_tx_data_length = 0;
	}
}

/*
 * This method will test against a #define for the amount of time having passed.
 * If the time is greater than the #define, return 1 (true) else return 0.
 * This method uses methods that are depending on the system clock thus if it is changed
 * then the timing will not work correctly.
 * Tried to use some time stamps that were not system clock dependent but they are not suppored on the platform.
 */
unsigned int testTime(struct timeval tv1, struct timeval tv2)
{
	// If the seconds are equal then we should only have to worry with usec
	if (tv2.tv_sec == tv1.tv_sec)
	{
		return (TICK_DELAY < (tv2.tv_usec - tv1.tv_usec)); // 100mS < delta converted to mS
	}
	// test to see if only one second incremented so we have to determine the nSec time
	else if ((tv2.tv_sec - tv1.tv_sec) == 1)
	{
		long int delta = 0;
		delta = tv2.tv_usec + (1000000 - tv1.tv_usec); // time beyond the second flip and the time till
		return (TICK_DELAY < delta);  // 100mS < delta converted to mS
	}
	else
	{
		return 1; // more time than 1 second has pased, that is an issue.
	}
}

/*****************************************************************************
	UploadLensBootloader(std::string strExistingVer)
	
	Returns:	 0 - OK (script either up to date or loaded)
						-1 - Input CRC is wrong length
						-2 - CRC error
						-3 - Unable to switch to UploadMode
******************************************************************************/
int UploadLensBootloader(Lens &lens, std::string strExistingVer, bool bIsLensInitialized)
{
	// check that the script needs to be uploaded
	if (g_pLensParms->UploadVer() == strExistingVer)   	
		return 0;

	// now check that the CRC matches
	Snap_Script_Uploader_Status status;
	Snap_U16 scriptCRC = 0;
	
	struct timeval tv1;
	struct timeval tv2;

	g_INetIPC.LensScriptUploading(true);
	
	if (g_pLensParms->UploadCRC().length() > 4)
	{
		ats_logf(ATSLOG_ERROR, RED_ON "UploadLensBootloader: CRC must only be 4 characters %s" RESET_COLOR, g_pLensParms->UploadCRC().c_str());
		g_INetIPC.LensScriptUploading(false);
		g_INetIPC.ScriptUploadFailure(true);
		return -1;
	}
	else
	{
		scriptCRC = strtol((char *)(g_pLensParms->UploadCRC().c_str()), NULL, 16);
	}
	
	int count = 0;
	// now set the lens into upload mode
	if (bIsLensInitialized && !lens.SwitchToUploadMode())
	{
		ats_logf(ATSLOG_ERROR, RED_ON "UploadLensBootloader: Unable to switch to upload mode" RESET_COLOR);
		g_INetIPC.LensScriptUploading(false);
		g_INetIPC.ScriptUploadFailure(true);
		return -2;
	}
	//printf("Network status: %d (Line %d)\n", lens.GetRadioNetworkStatus(), __LINE__);
	ats_logf(ATSLOG_INFO, "UploadLensBootloader: The current API version is: %s", snap_script_uploader_version());  // print the current version

	/* Configure the serial interface SNAP */
	
	UART0 = snapUartConfig();
	if(UART0 < 0)   //< // <ISCP-163: LENS_UART_ERROR>
	{

		g_INetIPC.LENSUARTFailure(true); //<ISCP-163>
	}
	// Check current CRC
	ats_logf(ATSLOG_INFO, "UploadLensBootloader: requesting checksum");  // print the current version
	snap_script_uploader_request_script_checksum();
	ats_logf(ATSLOG_INFO, "UploadLensBootloader: checking checksum");  // print the current version
	script_crc_request_flag = 1;
	// wait for the crc to be available
	gettimeofday(&tv1, NULL);
	
//	if (bIsLensInitialized)
	{
		while(1)
		{
			printf("."); fflush(stdout);
			gettimeofday(&tv2, NULL);
		
			/* Tick the library every 100ms. */
			if(testTime(tv1, tv2))
			{
				// set the current time to measure our delta by
				tv1.tv_sec = tv2.tv_sec;
				tv1.tv_usec = tv2.tv_usec;
				//printf("sec: %ld\tuSec %ld\r\n", tv2.tv_sec, tv2.tv_usec);
			
				snap_script_uploader_tick_100ms();
			}
		
			// Send data out the UART if needed.
			sendUartData();
		
			// try to read the UART
			checkForIncomingUARTData();

			if(snap_script_uploader_checksum_available())
			{
				// once it is available compare it to the one retrieved
				int currentCRC = snap_script_uploader_checksum();
				clear_script_crc_request();
				if (scriptCRC == currentCRC)
				{
					ats_logf(ATSLOG_INFO, "UploadLensBootloader: Current script is up to date (CRC= script:%d current:%d)", scriptCRC, currentCRC);
					g_INetIPC.LensScriptUploading(false);
					g_INetIPC.ScriptUploadFailure(false);
					return 0;
				}
				break;
			}
			if (++count > 50)
			{
				g_INetIPC.LensScriptUploading(false);
				g_INetIPC.ScriptUploadFailure(false);
				return 0;
			}
//		usleep(20 * 1000);  // DRH - added to provide other tasks with time.  
		}
		ats_logf(ATSLOG_INFO, "UploadLensBootloader: checksum is different - loading the new firmware");
	}
//	else
//		ats_logf(ATSLOG_INFO, "UploadLensBootloader: Unable to initialize radio - loading the new firmware");

	script_uploading_flag = 1;
	snap_script_uploader_start_upload((Snap_U8 *)(g_pLensParms->UploadScript().c_str()), scriptCRC, 0);
	gettimeofday(&tv1, NULL);

	bool running = true;

	while(running)
	{
		printf("_"); fflush(stdout);
		gettimeofday(&tv2, NULL);
		
		/* Tick the library every 100ms. */
		if(testTime(tv1, tv2))
		{
			// set the current time to measure our delta by
			tv1.tv_sec = tv2.tv_sec;
			tv1.tv_usec = tv2.tv_usec;
			
			snap_script_uploader_tick_100ms();
		}
		
		/* Send any incoming serial traffic from the SNAP module to the library. */
		checkForIncomingUARTData();

		// Send data out the UART if needed.
		sendUartData();

		/* Are we there yet? */
		if(script_upload_started())
		{
			if (script_next_block_request_flag) 
			{
				sendNextBlock(bl_req_offset, bl_req_length);
			}
			if(snap_script_uploader_completed())
			{
				report_upload_done();
				clear_script_uploading_flag();
				close(UART0);
				g_INetIPC.LensScriptUploading(false);
				g_INetIPC.ScriptUploadFailure(false);
				return 0;
			}
		}

		/* If we encountered an error, print it out */
		status = snap_script_uploader_status();
		if(status != SCRIPT_UPLOADER_STATUS_OK)
		{
			ats_logf(ATSLOG_ERROR, RED_ON "UploadLensBootloader: Script upload failed.  Error code is: %0d" RESET_COLOR, status);

			snap_script_uploader_clear_error();
			running = false;
			close(UART0);
		}
		if (++count % 20 == 0)
			ats_logf(ATSLOG_ERROR, "Uploading new script %s.  Network Status is %d", g_pLensParms->UploadBin().c_str(), lens.GetRadioNetworkStatus());
	}
	g_INetIPC.LensScriptUploading(false);
	g_INetIPC.ScriptUploadFailure(false);
	return 0;
}


/* (c) Copyright 2014, Synapse Wireless */

/**
 * @file
 * This header file defines the Script Uploader API.  This API allows the
 * client to upload an unpacked spy file to a serially-attached SNAP node.
 *
 * This library is hardware/platform independent.  It relies on the client to
 * provide a set of hardware-specific functions that perform all of the
 * necessary IO and timer related functionality.  These functions are
 * provided by the client using the snap_script_uploader_config API call.
 * The requirements for these functions are specified in the documentation
 * of that API call.
 *
 * Using this API typically a three step process:
 * # Configure the API with the appropriate hardware-specific functions.
 * # Request the CRC of the currently loaded script.
 * # If it needs updating, start the script update process.
 * # Wait for the provided done callback to be triggered.
 */

#ifndef _script_uploader_h_
#define _script_uploader_h_

#include "syn_types.h"

/*******************************************************************************
 * Status
 ******************************************************************************/

/* Script Uploader Status Codes */
typedef enum {
  SCRIPT_UPLOADER_STATUS_OK = 0,
  SCRIPT_UPLOADER_STATUS_NO_RESPONSE_TO_ERASE_COMMAND,
  SCRIPT_UPLOADER_STATUS_NO_RESPONSE_TO_WRITE_DATA_BLOCK,
  SCRIPT_UPLOADER_STATUS_DATA_BLOCK_TOO_LONG,
  SCRIPT_UPLOADER_STATUS_UNREQUESTED_DATA_BLOCK_RECEIVED,
  SCRIPT_UPLOADER_STATUS_NO_DATA_BLOCK_RECEIVED,
  SCRIPT_UPLOADER_STATUS_INVALID_OFFSET_IN_WRITE_ACK,
  SCRIPT_UPLOADER_STATUS_NO_RESPONSE_REBOOT_RPC,
  SCRIPT_UPLOADER_STATUS_NO_RESPONSE_SCRIPT_NAME_RPC,
  SCRIPT_UPLOADER_STATUS_INVALID_SCRIPT_NAME,
  SCRIPT_UPLOADER_STATUS_NO_RESPONSE_SCRIPT_CRC_RPC,
  SCRIPT_UPLOADER_STATUS_INVALID_SCRIPT_CRC,
  SCRIPT_UPLOADER_STATUS_INVALID_STATUS_CODE
} Snap_Script_Uploader_Status;

/*
 * Configuration functions
 */

/**
 * Returns the API version number.
 * The API version number will follow a <major>.<minor>.<maintenance> numbering
 * scheme.  An increment of the first number indicates very different (and very
 * likely incompatible) functionality.  An increment of the second number indicates
 * the addition of new features that do not break compatibility.  An increment of
 * the third number indicates bug fixes.
 */
const Snap_U8 * snap_script_uploader_version(void);

#ifndef STATIC_HOOKS                               
/**
 * Configures the SNAP Packet Serial API.
 * Registers the provided functions as callbacks to be used by the script upload 
 * mechanism during the upload process.
 */
void snap_script_uploader_config(
  void (*script_uploader_request_next_block)(Snap_U16 offset, Snap_U8 length),
  void (*script_uploader_done_callback)(),
  void (*snap_uart_tx_handler)(const Snap_U8 * data, Snap_U16 len)
);
#endif
/*
 * Status functions
 */

/**
 * Provides the current error status of the API.
 * @return the last uncleared error, or if no errors, returns PS_STATUS_OK.
 */
Snap_Script_Uploader_Status snap_script_uploader_status(void);

/**
 * Clears the error status of the API.
 * Future calls to snap_script_uploader_status will return PS_STATUS_OK until
 * the next error occurs.
 */
void snap_script_uploader_clear_error(void);

/**
 * Sends a request for the CRC of the current script.
 * The CRC request is sent to the attached SNAP node.  When the node responds,
 * the response is passed to the provided callback function.
 * @param script_uploader_script_checksum the function to call with the returned checksum.
 */
#ifndef STATIC_HOOKS
void snap_script_uploader_request_script_checksum(
  void (*script_uploader_script_checksum)(Snap_U16 checksum)
);
#else
void snap_script_uploader_request_script_checksum();
#endif

/**
 * Checks if the script checksum has been received from the SNAP node.
 * @return 1 if the checksum is available, otherwise 0.
 */
Snap_U8 snap_script_uploader_checksum_available(void);

/**
 * Provides the received script checksum from the SNAP node.  If no checksum has
 * been provided, it returns 0.
 * @return the received script checksum.
 */
Snap_U16 snap_script_uploader_checksum(void);

/*
 * Upload control functions
 */

/**
 * Starts the script upload process.
 * Resets the upload process and calls the appropriate RPC functions over serial
 * to begin the upload process.  Information about the bytecode stream is
 * provided and stored for later use.  The script name and CRC will be checked
 * at the end of the process to confirm that the correct script was uploaded
 * successfully.
 * @param script_name the name of the script being uploaded
 * @param expected_crc the CRC of the script being uploaded
 * @param format the format ID of the incoming bytecode stream
 */
void snap_script_uploader_start_upload(const Snap_U8 * script_name, Snap_U16 expected_crc, Snap_U8 format);

/**
 *  Provides the next block of date from the bytecode stream.
 *  This should be called by a helper function to provide a previously requested
 *  block of data from the bytecode stream.
 *  @param data the block of data.  The data buffer is owned by the caller upon return.
 *  @param length the length of the provided block
 *  @note The length of the provided block may be less than the requested length
 *  if it is the last block of data.
 */
void snap_script_uploader_data_block(const Snap_U8 * data, Snap_U8 length);

/**
 * Notifies the script upload process that there is no more script data to upload.
 * Called by a helper function when the end of the script data is reached.
 */
void snap_script_uploader_end_of_data(void);

/**
 *  Notifies the script upload process that time has elapsed.
 *  Provides an indication of the passage of time to the script upload process.
 *  This allows for timeouts, retries, and the flagging of errors if there is
 *  a communication failure during the upload process.
 *  This may be called from the main loop of the application or from a timer ISR.
 */
void snap_script_uploader_tick_100ms(void);

/**
 * Aborts any currently running script upload process.
 * The error status will not be modified, so the current error state at the time of the
 * call can be checked.  The target SNAP module will be left with no script on it.
 */
void snap_script_uploader_abort(void);

/**
 * Checks if the script upload process is complete without error.
 * @return 1 if the script upload process completed without error, otherwise 0.
 */
Snap_U8 snap_script_uploader_completed(void);

/*
 * Incoming serial handler
 */

/**
 * Processes incoming serial data from the attached SNAP node.
 * Parses and interprets the packet serial RPC calls coming from the SNAP node and
 * triggers responses accordingly.
 * @param data the incoming serial data to process.  The data buffer is owned by the
 * caller upon return.
 * @param len the length of the incoming serial data
 */
void snap_script_uploader_serial_rx(const Snap_U8 * data, Snap_U16 len);

#endif /* _script_uploader_h_ */


#pragma once

#include "ats-common.h"

namespace ats
{

	// Description: Setups up port "p_port" using standard modem settings.
	//
	// Standard Settings Are (as documented in the "stty" application):
	//
	//	-brkint: breaks do not cause an interrupt signal
	//	-icrnl: do not translate carriage return to newline
	//	-imaxbel: do not "beep and do not flush a full input buffer on a character"
	//	-isig: do not "enable interrupt, quit, and suspend special characters"
	//	-icanon: do not "enable erase, kill, werase, and rprnt special characters"
	//	-iexten: do not "enable non-POSIX special characters"
	//
	//	-opost: do not "postprocess output"
	//
	//	-echo: do not echo input characters
	//
	// Return: 0 is returned on success, and a negative errno number is returned on error.
	int setup_modem_serial_port(const ats::String& p_port);

}

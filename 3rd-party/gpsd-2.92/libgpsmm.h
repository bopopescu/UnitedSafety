/* $Id: libgpsmm.h 6730 2009-12-05 23:54:40Z esr $ */
#ifndef _GPSD_GPSMM_H_
#define _GPSD_GPSMM_H_

/*
 * Copyright (C) 2005 Alfredo Pironti
 *
 * This software is distributed under a BSD-style license. See the
 * file "COPYING" for more information.
 *
 */
#include <sys/types.h>
#include "gps.h" //the C library we are going to wrap

class gpsmm {
	public:
		gpsmm();
		virtual ~gpsmm();
		struct gps_data_t* open(const char *host,const char *port); //opens the connection with gpsd, MUST call this before any other method
		struct gps_data_t* open(void); //open() with default values
		struct gps_data_t* send(const char *request); //put a command to gpsd and return the updated struct
		struct gps_data_t* stream(int); //set watcher and policy flags
		struct gps_data_t* poll(void); //block until gpsd returns new data, then return the updated struct
		bool waiting(void); //nonblocking check for data waitin
		void clear_fix(void);
		void enable_debug(int, FILE*);
	private:
		struct gps_data_t *gps_data;
		struct gps_data_t *to_user;	//we return the user a copy of the internal structure. This way she can modify it without
						//integrity loss for the entire class
		struct gps_data_t* backup(void) { *to_user=*gps_data; return to_user;}; //return the backup copy
		pthread_t *handler; //needed to handle the callback registration/deletion
};
#endif // _GPSD_GPSMM_H_

/* $Id: gpsmon.h 5333 2009-03-03 13:18:31Z esr $ */
/* gpsmon.h -- what monitor capabuilities look like */

#ifndef _GPSD_GPSMON_H_
#define _GPSD_GPSMON_H_

#define COMMAND_TERMINATE	-1
#define COMMAND_MATCH		1
#define COMMAND_UNKNOWN		0

struct monitor_object_t {
    /* a device-specific capability table for the monitor */
    bool (*initialize)(void);		/* paint legends on windows */
    void (*update)(void);		/* now paint the data */
    int (*command)(char[]);		/* interpret device-specfic commands */
    void (*wrap)(void);			/* deallocate storage */
    int min_y, min_x;			/* space required for device info */
    const struct gps_type_t *driver;	/* device driver table */
};

// Device-specific may need these.
extern bool monitor_control_send(unsigned char *buf, size_t len);
extern void monitor_fixframe(WINDOW *win);
extern void monitor_log(const char *fmt, ...);
extern void monitor_complain(const char *fmt, ...);

#define BUFLEN		2048

extern WINDOW *devicewin;
extern struct gps_device_t	session;
extern int gmt_offset;

#endif /* _GPSD_GPSMON_H_ */
/* gpsmon.h ends here */

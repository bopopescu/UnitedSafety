/* $Id: libgps_core.c 6994 2010-02-04 17:44:20Z esr $ */
/* libgps.c -- client interface library for the gpsd daemon */
#include <sys/time.h>
#include <stdio.h>
#ifndef S_SPLINT_S
#include <unistd.h>
#endif /* S_SPLINT_S */
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <math.h>
#include <locale.h>
#include <assert.h>

#include "gpsd.h"
#include "gps_json.h"

#ifndef S_SPLINT_S
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#else
#define AF_UNSPEC 0
#endif
#endif
#if defined (HAVE_SYS_SELECT_H)
#include <sys/select.h>
#endif

#ifdef S_SPLINT_S
extern char *strtok_r(char *, const char *, char **);
#endif /* S_SPLINT_S */

#if defined(TESTMAIN) || defined(CLIENTDEBUG_ENABLE)
#define LIBGPS_DEBUG
#endif /* defined(TESTMAIN) || defined(CLIENTDEBUG_ENABLE) */

struct privdata_t {
    bool newstyle;
    ssize_t waiting;
    char buffer[BUFSIZ];
};
#define PRIVATE(gpsdata) ((struct privdata_t *)gpsdata->privdata)

#ifdef LIBGPS_DEBUG
static int debuglevel = 0; 
static int waitcount = 0;
static FILE *debugfp;

void gps_enable_debug(int level, FILE *fp)
/* control the level and destination of debug trace messages */
{
    debuglevel = level;
    debugfp = fp;
    json_enable_debug(level-2, fp);
}

static void gps_trace(int errlevel, const char *fmt, ... )
/* assemble command in printf(3) style */
{
    if (errlevel <= debuglevel) {
	char buf[BUFSIZ];
	va_list ap;

	(void)strlcpy(buf, "libgps: ", BUFSIZ);
	va_start(ap, fmt) ;
	(void)vsnprintf(buf + strlen(buf), sizeof(buf)-strlen(buf), fmt, ap);
	va_end(ap);

	(void)fputs(buf, debugfp);
    }
}

# define libgps_debug_trace(args) (void) gps_trace args
#else
# define libgps_debug_trace(args) /*@i1@*/do { } while (0)
#endif /* LIBGPS_DEBUG */

/*@-nullderef@*/
int gps_open_r(const char *host, const char *port,
	       /*@out@*/struct gps_data_t *gpsdata)
{
    /*@ -branchstate @*/
    if (!gpsdata)
	return -1;
    if (!host)
	host = "localhost";
    if (!port)
	port = DEFAULT_GPSD_PORT;

    libgps_debug_trace((1, "gps_open_r(%s, %s)\n", host, port));    

    if ((gpsdata->gps_fd = netlib_connectsock(AF_UNSPEC, host, port, "tcp")) < 0) {
	errno = gpsdata->gps_fd;
	return -1;
    }

    gpsdata->set = 0;
    gpsdata->status = STATUS_NO_FIX;
    gps_clear_fix(&gpsdata->fix);

    /* set up for line-buffered I/O over the daemon socket */
    gpsdata->privdata = (void *)malloc(sizeof(struct privdata_t));
    if (gpsdata->privdata == NULL)
	return -1;
    PRIVATE(gpsdata)->newstyle = false;
    PRIVATE(gpsdata)->waiting = 0;
    /*@i2@*/PRIVATE(gpsdata)->buffer[0] = '\0';

    return 0;
    /*@ +branchstate @*/
}

/*@-compmempass -immediatetrans@*/
struct gps_data_t *gps_open(const char *host, const char *port)
/* open a connection to a gpsd daemon */
{
    static struct gps_data_t gpsdata;
    if (gps_open_r(host, port, &gpsdata) == -1)
	return NULL;
    else
	return &gpsdata; 
}
/*@+compmempass +immediatetrans@*/

/*@-compdef -usereleased@*/
int gps_close(struct gps_data_t *gpsdata)
/* close a gpsd connection */
{
    libgps_debug_trace((1, "gps_close()\n"));
    free(PRIVATE(gpsdata));
    gpsdata->gps_fd = -1;
    
    return 0;
}
/*@+compdef +usereleased@*/

void gps_set_raw_hook(struct gps_data_t *gpsdata,
		      void (*hook)(struct gps_data_t *, char *, size_t len))
{
    gpsdata->raw_hook = hook;
}

#ifdef LIBGPS_DEBUG
static void libgps_dump_state(struct gps_data_t *collect, time_t now)
{
    char *status_values[] = {"NO_FIX", "FIX", "DGPS_FIX"};
    char *mode_values[] = {"", "NO_FIX", "MODE_2D", "MODE_3D"};

    /* FIXME: We don't dump the entire state here yet */
    (void)fprintf(debugfp, "flags: (0x%04x) %s\n", 
		  collect->set, gpsd_maskdump(collect->set));
    if (collect->set & ONLINE_SET)
	(void)fprintf(debugfp, "ONLINE: %lf\n", collect->online);
    if (collect->set & TIME_SET)
	(void)fprintf(debugfp, "TIME: %lf\n", collect->fix.time);
    if (collect->set & LATLON_SET)
	(void)fprintf(debugfp, "LATLON: lat/lon: %lf %lf\n", 
		      collect->fix.latitude, collect->fix.longitude);
    if (collect->set & ALTITUDE_SET)
	(void)fprintf(debugfp, "ALTITUDE: altitude: %lf  U: climb: %lf\n",
	       collect->fix.altitude, collect->fix.climb);
    if (collect->set & SPEED_SET)
	(void)fprintf(debugfp, "SPEED: %lf\n", collect->fix.speed);
    if (collect->set & TRACK_SET)
	(void)fprintf(debugfp, "TRACK: track: %lf\n", collect->fix.track);
    if (collect->set & CLIMB_SET)
	(void)fprintf(debugfp, "CLIMB: climb: %lf\n", collect->fix.climb);
    if (collect->set & STATUS_SET)
	(void)fprintf(debugfp, "STATUS: status: %d (%s)\n",
	       collect->status, status_values[collect->status]);
    if (collect->set & MODE_SET)
	(void)fprintf(debugfp, "MODE: mode: %d (%s)\n",
	   collect->fix.mode, mode_values[collect->fix.mode]);
    if (collect->set & DOP_SET)
	(void)fprintf(debugfp, "DOP: satellites %d, pdop=%lf, hdop=%lf, vdop=%lf\n",
	   collect->satellites_used,
	   collect->dop.pdop, collect->dop.hdop, collect->dop.vdop);
    if (collect->set & VERSION_SET)
	(void)fprintf(debugfp, "VERSION: release=%s rev=%s proto=%d.%d\n",
		      collect->version.release, 
		      collect->version.rev,
		      collect->version.proto_major, 
		      collect->version.proto_minor);
    if (collect->set & POLICY_SET)
	(void)fprintf(debugfp, "POLICY: watcher=%s nmea=%s raw=%d scaled=%s timing=%s, devpath=%s\n",
		      collect->policy.watcher ? "true" : "false", 
		      collect->policy.nmea ? "true" : "false",
		      collect->policy.raw, 
		      collect->policy.scaled ? "true" : "false", 
		      collect->policy.timing ? "true" : "false", 
		      collect->policy.devpath);
    if (collect->set & SATELLITE_SET) {
	int i;

	(void)fprintf(debugfp, "SKY: satellites in view: %d\n", collect->satellites_visible);
	for (i = 0; i < collect->satellites_visible; i++) {
	    (void)fprintf(debugfp, "    %2.2d: %2.2d %3.3d %3.0f %c\n", collect->PRN[i], collect->elevation[i], collect->azimuth[i], collect->ss[i], collect->used[i]? 'Y' : 'N');
	}
    }
    if (collect->set & DEVICE_SET)
	(void)fprintf(debugfp, "DEVICE: Device is '%s', driver is '%s'\n", 
		      collect->dev.path, collect->dev.driver);
#ifdef OLDSTYLE_ENABLE
    if (collect->set & DEVICEID_SET)
	(void)fprintf(debugfp, "GPSD ID is %s\n", collect->dev.subtype);
#endif /* OLDSTYLE_ENABLE */
    if (collect->set & DEVICELIST_SET) {
	int i;
	(void)fprintf(debugfp, "DEVICELIST:%d devices:\n", collect->devices.ndevices);
	for (i = 0; i < collect->devices.ndevices; i++) {
	    (void)fprintf(debugfp, "%d: path='%s' driver='%s'\n", 
			  collect->devices.ndevices, 
			  collect->devices.list[i].path,
			  collect->devices.list[i].driver);
	}
    }

}
#endif /* LIBGPS_DEBUG */


/*@ -branchstate -usereleased -mustfreefresh -nullstate -usedef @*/
int gps_unpack(char *buf, struct gps_data_t *gpsdata)
/* unpack a gpsd response into a status structure, buf must be writeable */
{
    char *ns, *sp, *tp;
    int i;

    libgps_debug_trace((1, "gps_unpack(%s)\n", buf));

    /* detect and process a JSON response */
    if (buf[0] == '{') {
	const char *jp = buf, **next = &jp;
	while (next != NULL && *next != NULL && next[0][0] != '\0') {
	    libgps_debug_trace((1, 
				"gps_unpack() segment parse '%s'\n", 
				*next));
	    (void)libgps_json_unpack(*next, gpsdata, next);
#ifdef LIBGPS_DEBUG
	    if (debuglevel >= 1)
		libgps_dump_state(gpsdata, time(NULL));
#endif /* LIBGPS_DEBUG */

	}
#ifdef OLDSTYLE_ENABLE
	if (PRIVATE(gpsdata) != NULL)
	    PRIVATE(gpsdata)->newstyle = true;
#endif /* OLDSTYLE_ENABLE */
    }
#ifdef OLDSTYLE_ENABLE
    else
    {
	/*
	 * Get the decimal separator for the current application locale.
	 * This looks thread-unsafe, but it's not.  The key is that
	 * character assignment is atomic.
	 */
	static char decimal_point = '\0';
	if (decimal_point == '\0') {
	    struct lconv *locale_data = localeconv();
	    if (locale_data != NULL && locale_data->decimal_point[0] != '.')
		decimal_point = locale_data->decimal_point[0];
	}

	for (ns = buf; ns; ns = strstr(ns+1, "GPSD")) {
	    if (/*@i1@*/strncmp(ns, "GPSD", 4) == 0) {
		bool eol = false;
		/* the following should execute each time we have a good next sp */
		for (sp = ns + 5; *sp != '\0'; sp = tp+1) {
		    tp = sp + strcspn(sp, ",\r\n");
		    eol = *tp == '\r' || *tp == '\n';
		    if (*tp == '\0')
			tp--;
		    else
			*tp = '\0';

		    /*
		     * The daemon always emits the Anglo-American and SI
		     * decimal point.  Hack these into whatever the
		     * application locale requires if it's not the same.
		     * This has to happen *after* we grab the next
		     * comma-delimited response, or we'll lose horribly
		     * in locales where the decimal separator is comma.
		     */
		    if (decimal_point != '\0') {
			char *cp;
			for (cp = sp; cp < tp; cp++)
			    if (*cp == '.')
				*cp = decimal_point;
		    }

		    /* note, there's a bit of skip logic after the switch */

		    switch (*sp) {
		    case 'A':
			if (sp[2] == '?') {
				gpsdata->fix.altitude = NAN;
			} else {
			    (void)sscanf(sp, "A=%lf", &gpsdata->fix.altitude);
			    gpsdata->set |= ALTITUDE_SET;
			}
			break;
		    case 'B':
			if (sp[2] == '?') {
			    gpsdata->dev.baudrate = gpsdata->dev.stopbits = 0;
			} else
			    (void)sscanf(sp, "B=%u %*d %*s %u",
				   &gpsdata->dev.baudrate, &gpsdata->dev.stopbits);
			break;
		    case 'C':
			if (sp[2] == '?')
			    gpsdata->dev.mincycle = gpsdata->dev.cycle = 0;
			else {
			    if (sscanf(sp, "C=%lf %lf",
					 &gpsdata->dev.cycle,
				       &gpsdata->dev.mincycle) < 2)
				gpsdata->dev.mincycle = gpsdata->dev.cycle;
			}
			break;
		    case 'D':
			if (sp[2] == '?')
			    gpsdata->fix.time = NAN;
			else {
			    gpsdata->fix.time = iso8601_to_unix(sp+2);
			    gpsdata->set |= TIME_SET;
			}
			break;
		    case 'E':
			gpsdata->epe = gpsdata->fix.epx = gpsdata->fix.epy = gpsdata->fix.epv = NAN;
			/* epe should always be present if eph or epv is */
			if (sp[2] != '?') {
			    char epe[20], eph[20], epv[20];
			    (void)sscanf(sp, "E=%s %s %s", epe, eph, epv);
#define DEFAULT(val) (val[0] == '?') ? NAN : atof(val)
				/*@ +floatdouble @*/
				gpsdata->epe = DEFAULT(epe);
				gpsdata->fix.epx = DEFAULT(eph)/sqrt(2);
				gpsdata->fix.epy = DEFAULT(eph)/sqrt(2);
				gpsdata->fix.epv = DEFAULT(epv);
				/*@ -floatdouble @*/
#undef DEFAULT
				gpsdata->set |= PERR_SET | HERR_SET | VERR_SET;
			}
			break;
		    case 'F': /*@ -mustfreeonly */
			if (sp[2] == '?')
			    gpsdata->dev.path[0] = '\0';
			else {
			    /*@ -mayaliasunique @*/
			    strncpy(gpsdata->dev.path, sp+2, sizeof(gpsdata->dev.path));
			    /*@ +mayaliasunique @*/
			    gpsdata->set |= DEVICE_SET;
			}
			/*@ +mustfreeonly */
			break;
		    case 'I':
			/*@ -mustfreeonly */
			if (sp[2] == '?')
			    gpsdata->dev.subtype[0] = '\0';
			else {
			    (void)strlcpy(gpsdata->dev.subtype, sp+2, sizeof(gpsdata->dev.subtype));
			    gpsdata->set |= DEVICEID_SET;
			}
			/*@ +mustfreeonly */
			break;
		    case 'K':
			/*@ -nullpass -mustfreeonly -dependenttrans@*/
			if (sp[2] != '?') {
			    char *rc = strdup(sp);
			    char *sp2 = rc;
			    char *ns2 = ns;
			    memset(&gpsdata->devices, '\0', sizeof(gpsdata->devices));
			    gpsdata->devices.ndevices = (int)strtol(sp2+2, &sp2, 10);
			    (void)strlcpy(gpsdata->devices.list[0].path,
				    strtok_r(sp2+1," \r\n", &ns2),
				    sizeof(gpsdata->devices.list[0].path));
			    i = 0;
			    while ((sp2 = strtok_r(NULL, " \r\n",  &ns2))!=NULL)
				if (i < MAXDEVICES_PER_USER-1)
				    (void)strlcpy(gpsdata->devices.list[++i].path, 
						 sp2,
						 sizeof(gpsdata->devices.list[0].path));
			    free(rc);
			    gpsdata->set |= DEVICELIST_SET;
			    gpsdata->devices.time = timestamp();
			}
			/*@ +nullpass +mustfreeonly +dependenttrans@*/
			break;
		    case 'M':
			if (sp[2] == '?') {
			    gpsdata->fix.mode = MODE_NOT_SEEN;
			} else {
			    gpsdata->fix.mode = atoi(sp+2);
			    gpsdata->set |= MODE_SET;
			}
			break;
		    case 'N':
			if (sp[2] == '?')
			    gpsdata->dev.driver_mode = MODE_NMEA;
			else
			    gpsdata->dev.driver_mode = atoi(sp+2);
			break;
		    case 'O':
			if (sp[2] == '?') {
			    gpsdata->set = MODE_SET | STATUS_SET;
			    gpsdata->status = STATUS_NO_FIX;
			    gps_clear_fix(&gpsdata->fix);
			} else {
			    struct gps_fix_t nf;
			    char tag[MAXTAGLEN+1], alt[20];
			    char eph[20], epv[20], track[20],speed[20], climb[20];
			    char epd[20], eps[20], epc[20], mode[2];
			    char timestr[20], ept[20], lat[20], lon[20];
			    int st = sscanf(sp+2,
				   "%8s %19s %19s %19s %19s %19s %19s %19s %19s %19s %19s %19s %19s %19s %1s",
				    tag, timestr, ept, lat, lon,
				    alt, eph, epv, track, speed, climb,
				    epd, eps, epc, mode);
			    if (st >= 14) {
    #define DEFAULT(val) (val[0] == '?') ? NAN : atof(val)
				/*@ +floatdouble @*/
				nf.time = DEFAULT(timestr);
				nf.latitude = DEFAULT(lat);
				nf.longitude = DEFAULT(lon);
				nf.ept = DEFAULT(ept);
				nf.altitude = DEFAULT(alt);
				/* designed before we split eph into epx+epy */
				nf.epx = nf.epy = DEFAULT(eph)/sqrt(2);
				nf.epv = DEFAULT(epv);
				nf.track = DEFAULT(track);
				nf.speed = DEFAULT(speed);
				nf.climb = DEFAULT(climb);
				nf.epd = DEFAULT(epd);
				nf.eps = DEFAULT(eps);
				nf.epc = DEFAULT(epc);
				/*@ -floatdouble @*/
    #undef DEFAULT
				if (st >= 15)
				    nf.mode = (mode[0] == '?') ? MODE_NOT_SEEN : atoi(mode);
				else
				    nf.mode = (alt[0] == '?') ? MODE_2D : MODE_3D;
				if (alt[0] != '?')
				    gpsdata->set |= ALTITUDE_SET | CLIMB_SET;
				if (isnan(nf.epx)==0 && isnan(nf.epy)==0)
				    gpsdata->set |= HERR_SET;
				if (isnan(nf.epv)==0)
				    gpsdata->set |= VERR_SET;
				if (isnan(nf.track)==0)
				    gpsdata->set |= TRACK_SET | SPEED_SET;
				if (isnan(nf.eps)==0)
				    gpsdata->set |= SPEEDERR_SET;
				if (isnan(nf.epc)==0)
				    gpsdata->set |= CLIMBERR_SET;
				gpsdata->fix = nf;
				(void)strlcpy(gpsdata->tag, tag, MAXTAGLEN+1);
				gpsdata->set |= TIME_SET|TIMERR_SET|LATLON_SET|MODE_SET;
				gpsdata->status = STATUS_FIX;
				gpsdata->set |= STATUS_SET;
			    }
			}
			break;
		    case 'P':
			if (sp[2] == '?') {
			       gpsdata->fix.latitude = NAN;
			       gpsdata->fix.longitude = NAN;
			} else {
			    (void)sscanf(sp, "P=%lf %lf",
			       &gpsdata->fix.latitude, &gpsdata->fix.longitude);
			    gpsdata->set |= LATLON_SET;
			}
			break;
		    case 'Q':
			if (sp[2] == '?') {
			       gpsdata->satellites_used = 0;
			       gpsdata->dop.pdop = 0;
			       gpsdata->dop.hdop = 0;
			       gpsdata->dop.vdop = 0;
			} else {
			    (void)sscanf(sp, "Q=%d %lf %lf %lf %lf %lf",
				   &gpsdata->satellites_used,
				   &gpsdata->dop.pdop,
				   &gpsdata->dop.hdop,
				   &gpsdata->dop.vdop,
				   &gpsdata->dop.tdop,
				   &gpsdata->dop.gdop);
			    gpsdata->set |= DOP_SET;
			}
			break;
		    case 'S':
			if (sp[2] == '?') {
			    gpsdata->status = -1;
			} else {
			    gpsdata->status = atoi(sp+2);
			    gpsdata->set |= STATUS_SET;
			}
			break;
		    case 'T':
			if (sp[2] == '?') {
			    gpsdata->fix.track = NAN;
			} else {
			    (void)sscanf(sp, "T=%lf", &gpsdata->fix.track);
			    gpsdata->set |= TRACK_SET;
			}
			break;
		    case 'U':
			if (sp[2] == '?') {
			    gpsdata->fix.climb = NAN;
			} else {
			    (void)sscanf(sp, "U=%lf", &gpsdata->fix.climb);
			    gpsdata->set |= CLIMB_SET;
			}
			break;
		    case 'V':
			if (sp[2] == '?') {
			    gpsdata->fix.speed = NAN;
			} else {
			    (void)sscanf(sp, "V=%lf", &gpsdata->fix.speed);
			    /* V reply is in kt, fix.speed is in metres/sec */
			    gpsdata->fix.speed = gpsdata->fix.speed / MPS_TO_KNOTS;
			    gpsdata->set |= SPEED_SET;
			}
			break;
		    case 'X':
			if (sp[2] == '?')
			    gpsdata->online = -1;
			else {
			    (void)sscanf(sp, "X=%lf", &gpsdata->online);
			    gpsdata->set |= ONLINE_SET;
			}
			break;
		    case 'Y':
			if (sp[2] == '?') {
			    gpsdata->satellites_visible = 0;
			} else {
			    int j, i1, i2, i3, i5;
			    int PRN[MAXCHANNELS];
			    int elevation[MAXCHANNELS], azimuth[MAXCHANNELS];
			    int used[MAXCHANNELS];
			    double ss[MAXCHANNELS], f4;
			    char tag[MAXTAGLEN+1], timestamp[21];

			    (void)sscanf(sp, "Y=%8s %20s %d ",
				   tag, timestamp, &gpsdata->satellites_visible);
			    (void)strncpy(gpsdata->tag, tag, MAXTAGLEN);
			    if (timestamp[0] != '?') {
				gpsdata->set |= TIME_SET;
			    }
			    for (j = 0; j < gpsdata->satellites_visible; j++) {
				PRN[j]=elevation[j]=azimuth[j]=used[j]=0;
				ss[j]=0.0;
			    }
			    for (j = 0, gpsdata->satellites_used = 0; j < gpsdata->satellites_visible; j++) {
				if ((sp != NULL) && ((sp = strchr(sp, ':')) != NULL)) {
				    sp++;
				    (void)sscanf(sp, "%d %d %d %lf %d", &i1, &i2, &i3, &f4, &i5);
				    PRN[j] = i1;
				    elevation[j] = i2; azimuth[j] = i3;
				    ss[j] = f4; used[j] = i5;
				    if (i5 == 1)
					gpsdata->satellites_used++;
				}
			    }
			    /*@ -compdef @*/
			    memcpy(gpsdata->PRN, PRN, sizeof(PRN));
			    memcpy(gpsdata->elevation, elevation, sizeof(elevation));
			    memcpy(gpsdata->azimuth, azimuth,sizeof(azimuth));
			    memcpy(gpsdata->ss, ss, sizeof(ss));
			    memcpy(gpsdata->used, used, sizeof(used));
			    /*@ +compdef @*/
			}
			gpsdata->set |= SATELLITE_SET;
			break;
		    }

#ifdef LIBGPS_DEBUG
		    if (debuglevel >= 1)
			libgps_dump_state(gpsdata, time(NULL));
#endif /* LIBGPS_DEBUG */

		    /*
		     * Skip to next GPSD when we see \r or \n;
		     * we don't want to try interpreting stuff
		     * in between that might be raw mode data.
		     */
		    if (eol)
			break;
		}
	    }
	}
    }
#endif /* OLDSTYLE_ENABLE */

/*@ -compdef @*/
    if (gpsdata->raw_hook) {
	//libgps_debug_trace((stderr, "libgps: raw hook called on '%s'\n", buf));
	gpsdata->raw_hook(gpsdata, buf, strlen(buf));
    }
    libgps_debug_trace((1, "final flags: (0x%04x) %s\n", gpsdata->set, gpsd_maskdump(gpsdata->set)));
    return 0;
}
/*@ +compdef @*/
/*@ -branchstate +usereleased +mustfreefresh +nullstate +usedef @*/

bool gps_waiting(struct gps_data_t *gpsdata)
/* is there input waiting from the GPS? */
{
    fd_set rfds;
    struct timeval tv;

    libgps_debug_trace((1, "gps_waiting(): %d\n", waitcount++));
    if (PRIVATE(gpsdata)->waiting > 0)
	return true;

    FD_ZERO(&rfds);
    FD_SET(gpsdata->gps_fd, &rfds);
    tv.tv_sec = 0; tv.tv_usec = 1;
    /* all error conditions return "not waiting" -- crude but effective */
    return (select(gpsdata->gps_fd+1, &rfds, NULL, NULL, &tv) == 1);
}

int gps_poll(struct gps_data_t *gpsdata)
/* wait for and read data being streamed from the daemon */
{
    char *eol;
    double received = 0;
    ssize_t response_length;
    int status = -1;
    struct privdata_t *priv = PRIVATE(gpsdata);

    gpsdata->set &=~ PACKET_SET;
    for (eol = priv->buffer; *eol != '\n' && eol < priv->buffer+priv->waiting; eol++)
	continue;
    if (*eol != '\n')
        eol = NULL;

    if (eol == NULL) {
	/* read data: return -1 if no data waiting or buffered, 0 otherwise */
	status = (int)recv(gpsdata->gps_fd,
			   priv->buffer + priv->waiting,
			   sizeof(priv->buffer)-priv->waiting,
			   0);
	if (status > -1)
	    priv->waiting += status;
	else if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
	    return 0;
	else if (priv->waiting == 0)
	    return status;
	for (eol = priv->buffer; *eol != '\n' && eol < priv->buffer+priv->waiting; eol++)
	    continue;
	if (*eol != '\n')
	    eol = NULL;
	if (eol == NULL)
	    return 0;
    }

    assert(eol != NULL);
    // FIXME: Make the JSON parser stop on \n so this isn't needed
    *eol = '\0';
    response_length = eol - priv->buffer + 1;
    received = gpsdata->online = timestamp();
    status = gps_unpack(priv->buffer, gpsdata);
    /*@+matchanyintegral@*/
    memmove(priv->buffer, 
	    priv->buffer + response_length, 
	    priv->waiting - response_length);
    /*@-matchanyintegral@*/
    priv->waiting -= response_length;
    gpsdata->set |= PACKET_SET;

    return 0;
}

int gps_send(struct gps_data_t *gpsdata, const char *fmt, ... )
/* send a command to the gpsd instance */
{
    char buf[BUFSIZ];
    va_list ap;

    va_start(ap, fmt);
    (void)vsnprintf(buf, sizeof(buf)-2, fmt, ap);
    va_end(ap);
    if (buf[strlen(buf)-1] != '\n')
	(void)strlcat(buf, "\n", BUFSIZ);
    if (write(gpsdata->gps_fd, buf, strlen(buf)) == (ssize_t)strlen(buf))
	return 0;
    else
	return -1;
}

int gps_stream(struct gps_data_t *gpsdata, 
	       unsigned int flags, 
	       /*@null@*/void *d)
/* ask gpsd to stream reports at you, hiding the command details */
{
    char buf[GPS_JSON_COMMAND_MAX];

    if ((flags & (WATCH_JSON|WATCH_OLDSTYLE|WATCH_NMEA|WATCH_RAW))== 0) {
	if (PRIVATE(gpsdata)->newstyle || (flags & WATCH_NEWSTYLE)!=0)
	    flags |= WATCH_JSON;
        else
	    flags |= WATCH_OLDSTYLE;
    }
    if (flags & POLL_NONBLOCK)
	(void)fcntl(gpsdata->gps_fd, F_SETFL, O_NONBLOCK); 
    if ((flags & WATCH_DISABLE) != 0) {
	if ((flags & WATCH_OLDSTYLE) != 0) {
	    (void)strlcpy(buf, "w-", sizeof(buf));
	    if (gpsdata->raw_hook != NULL || (flags & WATCH_NMEA)!=0)
		(void)strlcat(buf, "r-", sizeof(buf));
	} else {
	    (void)strlcpy(buf, "?WATCH={\"enable\":false,", sizeof(buf));
	    if (flags & WATCH_JSON)
		(void)strlcat(buf, "\"json\":false,", sizeof(buf));
	    if (flags & WATCH_NMEA)
		(void)strlcat(buf, "\"nmea\":false,", sizeof(buf));
	    if (flags & WATCH_RAW)
		(void)strlcat(buf, "\"raw\":1,", sizeof(buf));
	    if (flags & WATCH_RARE)
		(void)strlcat(buf, "\"raw\":0,", sizeof(buf));
	    if (flags & WATCH_SCALED)
		(void)strlcat(buf, "\"scaled\":false,", sizeof(buf));
	    if (buf[strlen(buf)-1] == ',')
		buf[strlen(buf)-1] = '\0';
	    (void)strlcat(buf, "};", sizeof(buf));
	}
	libgps_debug_trace((1, "gps_stream() disable command: %s\n", buf));
	return gps_send(gpsdata, buf);
    } else /* if ((flags & WATCH_ENABLE) != 0) */{
	if ((flags & WATCH_OLDSTYLE) != 0) {
	    (void)strlcpy(buf, "w+x", sizeof(buf));
	    if (gpsdata->raw_hook != NULL || (flags & WATCH_NMEA)!=0)
		(void)strlcat(buf, "r+", sizeof(buf));
	} else {
	    (void)strlcpy(buf, "?WATCH={\"enable\":true,", sizeof(buf));
	    if (flags & WATCH_JSON)
		(void)strlcat(buf, "\"json\":true,", sizeof(buf));
	    if (flags & WATCH_NMEA)
		(void)strlcat(buf, "\"nmea\":true,", sizeof(buf));
	    if (flags & WATCH_RARE)
		(void)strlcat(buf, "\"raw\":1,", sizeof(buf));
	    if (flags & WATCH_RAW)
		(void)strlcat(buf, "\"raw\":2,", sizeof(buf));
	    if (flags & WATCH_SCALED)
		(void)strlcat(buf, "\"scaled\":true,", sizeof(buf));
	    /*@-nullpass@*//* shouldn't be needed, splint has a bug */
	    if (flags & WATCH_DEVICE)
		(void)snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
			       "\"device\":%s,", (char*)d);
	    /*@+nullpass@*/
	    if (buf[strlen(buf)-1] == ',')
		buf[strlen(buf)-1] = '\0';
	    (void)strlcat(buf, "};", sizeof(buf));
	}
	libgps_debug_trace((1, "gps_stream() enable command: %s\n", buf));
	return gps_send(gpsdata, buf);
    }
}

extern char /*@observer@*/ *gps_errstr(const int err)
{
    /* 
     * We might add our own error codes in the future, e.g for
     * protocol compatibility checks
     */
    return netlib_errstr(err); 
}

#ifdef TESTMAIN
/*
 * A simple command-line exerciser for the library.
 * Not really useful for anything but debugging.
 */

static void dumpline(struct gps_data_t *ud UNUSED, 
		     char *buf, size_t ulen UNUSED)
{
    puts(buf);
}

#ifndef S_SPLINT_S
#include <unistd.h>
#endif /* S_SPLINT_S */
#include <getopt.h>
#include <signal.h>

static void onsig(int sig)
{
    (void)fprintf(stderr, "libgps: died with signal %d\n", sig);
    exit(1);
}

/* must start zeroed, otherwise the unit test will try to chase garbage pointer fields. */
static struct gps_data_t gpsdata;

int main(int argc, char *argv[])
{
    struct gps_data_t *collect;
    char buf[BUFSIZ];
    int option;
    bool batchmode = false;
    int debug = 0;

    (void)signal(SIGSEGV, onsig);
    (void)signal(SIGBUS, onsig);

    while ((option = getopt(argc, argv, "bhsD:?")) != -1) {
	switch (option) {
	case 'b':
	    batchmode = true;
	    break;
	case 's':
	    (void)printf("Sizes: gpsdata=%zd rtcm2=%zd rtcm3=%zd ais=%zd compass=%zd raw=%zd devices=%zd policy=%zd version=%zd\n",
			 sizeof(struct gps_data_t),
			 sizeof(struct rtcm2_t),
			 sizeof(struct rtcm3_t),
			 sizeof(struct ais_t),
			 sizeof(struct compass_t),
			 sizeof(struct rawdata_t),
			 sizeof(collect->devices),
			 sizeof(struct policy_t),
			 sizeof(struct version_t));
	    exit(0);
	case 'D':
	    debug = atoi(optarg);
	    break;
	case '?':
	case 'h':
	default:
	    (void)fputs("usage: libgps [-b] [-d lvl] [-s]\n", stderr);
	    exit(1);
	}
    }

    gps_enable_debug(debug, stdout);
    if (batchmode) {
	while (fgets(buf, sizeof(buf), stdin) != NULL) {
	    if (buf[0] == '{' || isalpha(buf[0])) {
		gps_unpack(buf, &gpsdata);
		libgps_dump_state(&gpsdata, time(NULL));
	    }
	}
    } else if ((collect = gps_open(NULL, 0)) == NULL) {
	(void)fputs("Daemon is not running.\n", stdout);
	exit(1);
    } else if (optind < argc) {
	gps_set_raw_hook(collect, dumpline);
	strlcpy(buf, argv[optind], BUFSIZ);
	strlcat(buf,"\n", BUFSIZ);
	gps_send(collect, buf);
	gps_poll(collect);
	libgps_dump_state(collect, time(NULL));
	(void)gps_close(collect);
    } else {
	int	tty = isatty(0);

	gps_set_raw_hook(collect, dumpline);
	if (tty)
	    (void)fputs("This is the gpsd exerciser.\n", stdout);
	for (;;) {
	    if (tty)
		(void)fputs("> ", stdout);
	    if (fgets(buf, sizeof(buf), stdin) == NULL) {
		if (tty)
		    putchar('\n');
		break;
	    }
	    collect->set = 0;
	    gps_send(collect, buf);
	    gps_poll(collect);
	    libgps_dump_state(collect, time(NULL));
	}
	(void)gps_close(collect);
    }

    return 0;
}
/*@-nullderef@*/

#endif /* TESTMAIN */



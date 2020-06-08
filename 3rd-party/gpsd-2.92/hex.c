/* $Id: hex.c 6566 2009-11-20 03:51:06Z esr $ */
#ifndef S_SPLINT_S
#include <unistd.h>
#endif /* S_SPLINT_S */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "gpsd.h"

int gpsd_hexdump_level = -1;
/*
 * A wrapper around gpsd_hexdump to prevent wasting cpu time by hexdumping
 * buffers and copying strings that will never be printed. only messages at
 * level "N" and lower will be printed. By way of example, without any -D
 * options, gpsd probably won't ever call the real gpsd_hexdump. At -D2,
 * LOG_PROG (and higher) won't get to call the real gpsd_hexdump. For high
 * speed, chatty protocols, this can save a lot of CPU.
 */
char *gpsd_hexdump_wrapper(const void *binbuf, size_t binbuflen,
    int msg_debug_level)
{
#ifndef SQUELCH_ENABLE
    if (msg_debug_level <= gpsd_hexdump_level)
	return gpsd_hexdump(binbuf, binbuflen);
#endif /* SQUELCH_ENABLE */
    return "";
}

char /*@ observer @*/ *gpsd_hexdump(const void *binbuf, size_t binbuflen)
{
    static char hexbuf[MAX_PACKET_LENGTH*2+1];
#ifndef SQUELCH_ENABLE
    size_t i, j = 0;
    size_t len = (size_t)((binbuflen > MAX_PACKET_LENGTH) ? MAX_PACKET_LENGTH : binbuflen);
    const char *ibuf = (const char *)binbuf;
    const char *hexchar = "0123456789abcdef";

    if (NULL == binbuf || 0 == binbuflen) 
	return "";

    /*@ -shiftimplementation @*/
    for (i = 0; i < len; i++) {
	hexbuf[j++] = hexchar[ (ibuf[i]&0xf0)>>4 ];
	hexbuf[j++] = hexchar[ ibuf[i]&0x0f ];
    }
    /*@ +shiftimplementation @*/
    hexbuf[j] ='\0';
#else /* SQUELCH defined */
    hexbuf[0] = '\0';
#endif /* SQUELCH_ENABLE */
    return hexbuf;
}

int gpsd_hexpack(/*@in@*/const char *src, /*@out@*/char *dst, size_t len) {
/* hex2bin source string to destination - destination can be same as source */ 
    int i, k, l;

    /*@ -mustdefine @*/
    l = (int)(strlen(src) / 2);
    if ((l < 1) || ((size_t)l > len))
	return -2;

    for (i = 0; i < l; i++)
	if ((k = hex2bin(src+i*2)) != -1)
	    dst[i] = (char)(k & 0xff);
	else
	    return -1;
    (void)memset(dst+i, '\0', (size_t)(len-i));
    return l;
    /*@ +mustdefine @*/
}

/*@ +charint -shiftimplementation @*/
int hex2bin(const char *s)
{
    int a, b;

    a = s[0] & 0xff;
    b = s[1] & 0xff;

    if ((a >= 'a') && (a <= 'f'))
	a = a + 10 - 'a';
    else if ((a >= 'A') && (a <= 'F'))
	a = a + 10 - 'A';
    else if ((a >= '0') && (a <= '9'))
	a -= '0';
    else
	return -1;

    if ((b >= 'a') && (b <= 'f'))
	b = b + 10 - 'a';
    else if ((b >= 'A') && (b <= 'F'))
	b = b + 10 - 'A';
    else if ((b >= '0') && (b <= '9'))
	b -= '0';
    else
	return -1;

    return ((a<<4) + b);
}
/*@ -charint +shiftimplementation @*/

ssize_t hex_escapes(/*@out@*/char *cooked, const char *raw)
/* interpret C-style hex escapes */
{
    char c, *cookend;

    /*@ +charint -mustdefine -compdef @*/
    for (cookend = cooked; *raw != '\0'; raw++)
	if (*raw != '\\')
	    *cookend++ = *raw;
	else {
	    switch(*++raw) {
	    case 'b': *cookend++ = '\b'; break;
	    case 'e': *cookend++ = '\x1b'; break;
	    case 'f': *cookend++ = '\f'; break;
	    case 'n': *cookend++ = '\n'; break;
	    case 'r': *cookend++ = '\r'; break;
	    case 't': *cookend++ = '\r'; break;
	    case 'v': *cookend++ = '\v'; break;
	    case 'x':
		switch(*++raw) {
		case '0': c = 0x00; break;
		case '1': c = 0x10; break;
		case '2': c = 0x20; break;
		case '3': c = 0x30; break;
		case '4': c = 0x40; break;
		case '5': c = 0x50; break;
		case '6': c = 0x60; break;
		case '7': c = 0x70; break;
		case '8': c = 0x80; break;
		case '9': c = 0x90; break;
		case 'A': case 'a': c = 0xa0; break;
		case 'B': case 'b': c = 0xb0; break;
		case 'C': case 'c': c = 0xc0; break;
		case 'D': case 'd': c = 0xd0; break;
		case 'E': case 'e': c = 0xe0; break;
		case 'F': case 'f': c = 0xf0; break;
		default:
		    return -1;
		}
		switch(*++raw) {
		case '0': c += 0x00; break;
		case '1': c += 0x01; break;
		case '2': c += 0x02; break;
		case '3': c += 0x03; break;
		case '4': c += 0x04; break;
		case '5': c += 0x05; break;
		case '6': c += 0x06; break;
		case '7': c += 0x07; break;
		case '8': c += 0x08; break;
		case '9': c += 0x09; break;
		case 'A': case 'a': c += 0x0a; break;
		case 'B': case 'b': c += 0x0b; break;
		case 'C': case 'c': c += 0x0c; break;
		case 'D': case 'd': c += 0x0d; break;
		case 'E': case 'e': c += 0x0e; break;
		case 'F': case 'f': c += 0x0f; break;
		default:
		    return -2;
		}
		*cookend++ = c;
		break;
	    case '\\': *cookend++ = '\\'; break;
	    default:
		return -3;
	    }
	}
    return (ssize_t)(cookend - cooked);
    /*@ +charint +mustdefine +compdef @*/
}

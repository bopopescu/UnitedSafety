/* $Id: driver_aivdm.c 6983 2010-02-01 23:29:44Z esr $ */
/*
 * Driver for AIS/AIVDM messages.
 *
 * See the file AIVDM.txt on the GPSD website for documentation and references.
 *
 * Code for message types 1-15, 18-21, and 24 has been tested against
 * live data with known-good decodings. Code for message types 16-17,
 * 22-23, and 25-26 has not.
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#ifndef S_SPLINT_S
#include <unistd.h>
#endif /* S_SPLINT_S */
#include <time.h>
#include <stdio.h>

#include "gpsd.h"
#include "bits.h"

/**
 * Parse the data from the device
 */

static void from_sixbit(char *bitvec, uint start, int count, char *to)
{
    /*@ +type @*/
#ifdef S_SPLINT_S
    /* the real string causes a splint internal error */
    const char sixchr[] = "abcd";
#else
    const char sixchr[64] = "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^- !\"#$%&`()*+,-./0123456789:;<=>?";
#endif /* S_SPLINT_S */
    int i;
    char newchar;

    /* six-bit to ASCII */
    for (i = 0; i < count-1; i++) {
	newchar = sixchr[ubits(bitvec, start + 6*i, 6U)];
	if (newchar == '@')
	    break;
	else
	    to[i] = newchar;
    }
    to[i] = '\0';
    /* trim spaces on right end */
    for (i = count-2; i >= 0; i--)
	if (to[i] == ' ' || to[i] == '@')
	    to[i] = '\0';
	else
	    break;
    /*@ -type @*/
}

/*@ +charint @*/
bool aivdm_decode(const char *buf, size_t buflen, 
		  struct aivdm_context_t *ais_context, struct ais_t *ais)
{
#ifdef __UNUSED_DEBUG__
    char *sixbits[64] = {
	"000000", "000001", "000010", "000011", "000100",
	"000101", "000110", "000111", "001000", "001001",
	"001010", "001011", "001100", "001101",	"001110",
	"001111", "010000", "010001", "010010", "010011",
	"010100", "010101", "010110", "010111",	"011000",
	"011001", "011010", "011011", "011100",	"011101",
	"011110", "011111", "100000", "100001",	"100010",
	"100011", "100100", "100101", "100110",	"100111",
	"101000", "101001", "101010", "101011",	"101100",
	"101101", "101110", "101111", "110000",	"110001",
	"110010", "110011", "110100", "110101",	"110110",
	"110111", "111000", "111001", "111010",	"111011",
	"111100", "111101", "111110", "111111",   
    };
#endif /* __UNUSED_DEBUG__ */
    int nfields = 0;
    unsigned char *data, *cp = ais_context->fieldcopy;
    unsigned char ch, pad;
    int i;

    if (buflen == 0)
	return false;

    /* we may need to dump the raw packet */
    gpsd_report(LOG_PROG, "AIVDM packet length %zd: %s", buflen, buf);

    /* extract packet fields */
    (void)strlcpy((char *)ais_context->fieldcopy, buf, buflen);
    ais_context->field[nfields++] = (unsigned char *)buf;
    for (cp = ais_context->fieldcopy;
	 cp < ais_context->fieldcopy + buflen;
	 cp++)
	if (*cp == ',') {
	    *cp = '\0';
	    ais_context->field[nfields++] = cp + 1;
	}
    ais_context->await = atoi((char *)ais_context->field[1]);
    ais_context->part = atoi((char *)ais_context->field[2]);
    data = ais_context->field[5];
    pad = ais_context->field[6][0];
    gpsd_report(LOG_PROG, "await=%d, part=%d, data=%s\n",
		ais_context->await,
		ais_context->part,
		data);

    /* assemble the binary data */
    if (ais_context->part == 1) {
	(void)memset(ais_context->bits, '\0', sizeof(ais_context->bits));
	ais_context->bitlen = 0;
    }

    /* wacky 6-bit encoding, shades of FIELDATA */
    /*@ +charint @*/
    for (cp = data; cp < data + strlen((char *)data); cp++) {
	ch = *cp;
	ch -= 48;
	if (ch >= 40)
	    ch -= 8;
#ifdef __UNUSED_DEBUG__
	gpsd_report(LOG_RAW, "%c: %s\n", *cp, sixbits[ch]);
#endif /* __UNUSED_DEBUG__ */
	/*@ -shiftnegative @*/
	for (i = 5; i >= 0; i--) {
	    if ((ch >> i) & 0x01) {
		ais_context->bits[ais_context->bitlen / 8] |= (1 << (7 - ais_context->bitlen % 8));
	    }
	    ais_context->bitlen++;
	}
	/*@ +shiftnegative @*/
    }
    if (isdigit(pad))
	ais_context->bitlen -= (pad - '0');	/* ASCII assumption */
    /*@ -charint @*/

    /* time to pass buffered-up data to where it's actually processed? */
    if (ais_context->part == ais_context->await) {
	size_t clen = (ais_context->bitlen + 7)/8;
	gpsd_report(LOG_INF, "AIVDM payload is %zd bits, %zd chars: %s\n",
		    ais_context->bitlen, clen,
		    gpsd_hexdump_wrapper(ais_context->bits,
					 clen, LOG_INF));


#define UBITS(s, l)	ubits((char *)ais_context->bits, s, l)
#define SBITS(s, l)	sbits((char *)ais_context->bits, s, l)
#define UCHARS(s, to)	from_sixbit((char *)ais_context->bits, s, sizeof(to), to)
	ais->type = UBITS(0, 6);
	ais->repeat = UBITS(6, 2);
	ais->mmsi = UBITS(8, 30);
	gpsd_report(LOG_INF, "AIVDM message type %d, MMSI %09d:\n",
		    ais->type, ais->mmsi);
	switch (ais->type) {
	case 1:	/* Position Report */
	case 2:
	case 3:
	    if (ais_context->bitlen != 168) {
		gpsd_report(LOG_WARN, "AIVDM message type %d size not 168 bits (%zd).\n",
			    ais->type,
			    ais_context->bitlen);
		break;
	    }
	    ais->type1.status		= UBITS(38, 4);
	    ais->type1.turn		= SBITS(42, 8);
	    ais->type1.speed		= UBITS(50, 10);
	    ais->type1.accuracy	= (bool)UBITS(60, 1);
	    ais->type1.lon		= SBITS(61, 28);
	    ais->type1.lat		= SBITS(89, 27);
	    ais->type1.course		= UBITS(116, 12);
	    ais->type1.heading	= UBITS(128, 9);
	    ais->type1.second		= UBITS(137, 6);
	    ais->type1.maneuver	= UBITS(143, 2);
	    //ais->type1.spare	= UBITS(145, 3);
	    ais->type1.raim		= UBITS(148, 1)!=0;
	    ais->type1.radio		= UBITS(149, 20);
	    gpsd_report(LOG_INF,
			"Nav=%d TURN=%d SPEED=%d Q=%d Lon=%d Lat=%d COURSE=%d TH=%d Sec=%d\n",
			ais->type1.status,
			ais->type1.turn,
			ais->type1.speed,
			(uint)ais->type1.accuracy,
			ais->type1.lon,
			ais->type1.lat,
			ais->type1.course,
			ais->type1.heading,
			ais->type1.second);
	    break;
	case 4: 	/* Base Station Report */
	case 11:	/* UTC/Date Response */
	    if (ais_context->bitlen != 168) {
		gpsd_report(LOG_WARN, "AIVDM message type %d size not 168 bits (%zd).\n",
			    ais->type,
			    ais_context->bitlen);
		break;
	    }
	    ais->type4.year		= UBITS(38, 14);
	    ais->type4.month		= UBITS(52, 4);
	    ais->type4.day		= UBITS(56, 5);
	    ais->type4.hour		= UBITS(61, 5);
	    ais->type4.minute		= UBITS(66, 6);
	    ais->type4.second		= UBITS(72, 6);
	    ais->type4.accuracy		= UBITS(78, 1)!=0;
	    ais->type4.lon		= SBITS(79, 28);
	    ais->type4.lat		= SBITS(107, 27);
	    ais->type4.epfd		= UBITS(134, 4);
	    //ais->type4.spare		= UBITS(138, 10);
	    ais->type4.raim		= UBITS(148, 1)!=0;
	    ais->type4.radio		= UBITS(149, 19);
	    gpsd_report(LOG_INF,
			"Date: %4d:%02d:%02dT%02d:%02d:%02d Q=%d Lat=%d  Lon=%d epfd=%d\n",
			ais->type4.year,
			ais->type4.month,
			ais->type4.day,
			ais->type4.hour,
			ais->type4.minute,
			ais->type4.second,
			(uint)ais->type4.accuracy,
			ais->type4.lat,
			ais->type4.lon,
			ais->type4.epfd);
	    break;
	case 5: /* Ship static and voyage related data */
	    if (ais_context->bitlen != 424) {
		gpsd_report(LOG_WARN, "AIVDM message type 5 size not 424 bits (%zd).\n",
			    ais_context->bitlen);
		break;
	    }
	    ais->type5.ais_version  = UBITS(38, 2);
	    ais->type5.imo          = UBITS(40, 30);
	    UCHARS(70, ais->type5.callsign);
	    UCHARS(112, ais->type5.shipname);
	    ais->type5.shiptype     = UBITS(232, 8);
	    ais->type5.to_bow       = UBITS(240, 9);
	    ais->type5.to_stern     = UBITS(249, 9);
	    ais->type5.to_port      = UBITS(258, 6);
	    ais->type5.to_starboard = UBITS(264, 6);
	    ais->type5.epfd         = UBITS(270, 4);
	    ais->type5.month        = UBITS(274, 4);
	    ais->type5.day          = UBITS(278, 5);
	    ais->type5.hour         = UBITS(283, 5);
	    ais->type5.minute       = UBITS(288, 6);
	    ais->type5.draught      = UBITS(294, 8);
	    UCHARS(302, ais->type5.destination);
	    ais->type5.dte          = UBITS(422, 1);
	    //ais->type5.spare        = UBITS(423, 1);
	    gpsd_report(LOG_INF,
			"AIS=%d callsign=%s, name=%s destination=%s\n",
			ais->type5.ais_version,
			ais->type5.callsign,
			ais->type5.shipname,
			ais->type5.destination);
	    break;
	case 6: /* Addressed Binary Message */
	    if (ais_context->bitlen < 88 || ais_context->bitlen > 1008) {
		gpsd_report(LOG_WARN, "AIVDM message type 6 size is out of range (%zd).\n",
			    ais_context->bitlen);
		break;
	    }
	    ais->type6.seqno          = UBITS(38, 2);
	    ais->type6.dest_mmsi      = UBITS(40, 30);
	    ais->type6.retransmit     = (bool)UBITS(70, 1);
	    //ais->type6.spare        = UBITS(71, 1);
	    ais->type6.app_id         = UBITS(72, 16);
	    ais->type6.bitcount       = ais_context->bitlen - 88;
	    (void)memcpy(ais->type6.bitdata,
			 (char *)ais_context->bits+11,
			 (ais->type6.bitcount + 7) / 8);
	    gpsd_report(LOG_INF, "seqno=%d, dest=%u, id=%u, cnt=%zd\n",
			ais->type6.seqno,
			ais->type6.dest_mmsi,
			ais->type6.app_id,
			ais->type6.bitcount);
	    break;
	case 7: /* Binary acknowledge */
	case 13: /* Safety Related Acknowledge */
	{
	    unsigned int mmsi[4];
	    if (ais_context->bitlen < 72 || ais_context->bitlen > 168) {
		gpsd_report(LOG_WARN, "AIVDM message type %d size is out of range (%zd).\n",
			    ais->type,
			    ais_context->bitlen);
		break;
	    }
	    for (i = 0; i < sizeof(mmsi)/sizeof(mmsi[0]); i++)
		if (ais_context->bitlen > 40 + 32*i)
		    mmsi[i] = UBITS(40 + 32*i, 30);
		else
		    mmsi[i] = 0;
	    /*@ -usedef @*/
	    ais->type7.mmsi1 = mmsi[0];
	    ais->type7.mmsi2 = mmsi[1];
	    ais->type7.mmsi3 = mmsi[2];
	    ais->type7.mmsi4 = mmsi[3];
	    /*@ +usedef @*/
	    gpsd_report(LOG_INF, "\n");
	    break;
	}
	case 8: /* Binary Broadcast Message */
	    if (ais_context->bitlen < 56 || ais_context->bitlen > 1008) {
		gpsd_report(LOG_WARN, "AIVDM message type 8 size is out of range (%zd).\n",
			    ais_context->bitlen);
		break;
	    }
	    //ais->type8.spare        = UBITS(38, 2);
	    ais->type8.app_id =       UBITS(40, 16);
	    ais->type8.bitcount       = ais_context->bitlen - 56;
	    (void)memcpy(ais->type8.bitdata,
			 (char *)ais_context->bits+7,
			 (ais->type8.bitcount + 7) / 8);
	    gpsd_report(LOG_INF, "id=%u, cnt=%zd\n",
			ais->type8.app_id,
			ais->type8.bitcount);
	    break;
	case 9: /* Standard SAR Aircraft Position Report */
	    if (ais_context->bitlen != 168) {
		gpsd_report(LOG_WARN, "AIVDM message type 9 size not 168 bits (%zd).\n",
			    ais_context->bitlen);
		break;
	    }
	    ais->type9.alt		= UBITS(38, 12);
	    ais->type9.speed		= UBITS(50, 10);
	    ais->type9.accuracy		= (bool)UBITS(60, 1);
	    ais->type9.lon		= SBITS(61, 28);
	    ais->type9.lat		= SBITS(89, 27);
	    ais->type9.course		= UBITS(116, 12);
	    ais->type9.second		= UBITS(128, 6);
	    ais->type9.regional		= UBITS(134, 8);
	    ais->type9.dte		= UBITS(142, 1);
	    //ais->type9.spare		= UBITS(143, 3);
	    ais->type9.assigned		= UBITS(146, 1)!=0;
	    ais->type9.raim		= UBITS(147, 1)!=0;
	    ais->type9.radio		= UBITS(148, 19);
	    gpsd_report(LOG_INF,
			"Alt=%d SPEED=%d Q=%d Lon=%d Lat=%d COURSE=%d Sec=%d\n",
			ais->type9.alt,
			ais->type9.speed,
			(uint)ais->type9.accuracy,
			ais->type9.lon,
			ais->type9.lat,
			ais->type9.course,
			ais->type9.second);
	    break;
	case 10: /* UTC/Date inquiry */
	    if (ais_context->bitlen != 72) {
		gpsd_report(LOG_WARN, "AIVDM message type 10 size not 72 bits (%zd).\n",
			    ais_context->bitlen);
		break;
	    }
	    //ais->type10.spare        = UBITS(38, 2);
	    ais->type10.dest_mmsi      = UBITS(40, 30);
	    //ais->type10.spare2       = UBITS(70, 2);
	    gpsd_report(LOG_INF, "dest=%u\n", ais->type10.dest_mmsi);
	    break;
	case 12: /* Safety Related Message */
	    if (ais_context->bitlen < 72 || ais_context->bitlen > 1008) {
		gpsd_report(LOG_WARN, "AIVDM message type 12 size is out of range (%zd).\n",
			    ais_context->bitlen);
		break;
	    }
	    ais->type12.seqno          = UBITS(38, 2);
	    ais->type12.dest_mmsi      = UBITS(40, 30);
	    ais->type12.retransmit     = (bool)UBITS(70, 1);
	    //ais->type12.spare        = UBITS(71, 1);
	    from_sixbit((char *)ais_context->bits,
			72, ais_context->bitlen-72,
			ais->type12.text);
	    gpsd_report(LOG_INF, "seqno=%d, dest=%u\n",
			ais->type12.seqno,
			ais->type12.dest_mmsi);
	    break;
	case 14:	/* Safety Related Broadcast Message */
	    if (ais_context->bitlen < 40 || ais_context->bitlen > 1008) {
		gpsd_report(LOG_WARN, "AIVDM message type 14 size is out of range (%zd).\n",
			    ais_context->bitlen);
		break;
	    }
	    //ais->type14.spare          = UBITS(38, 2);
	    from_sixbit((char *)ais_context->bits,
			40, ais_context->bitlen-40,
			ais->type14.text);
	    gpsd_report(LOG_INF, "\n");
	    break;
	case 15:	/* Interrogation */
	    if (ais_context->bitlen < 88 || ais_context->bitlen > 168) {
		gpsd_report(LOG_WARN, "AIVDM message type 15 size is out of range (%zd).\n",
			    ais_context->bitlen);
		break;
	    }
	    (void)memset(&ais->type15, '\0', sizeof(ais->type15));
	    //ais->type14.spare         = UBITS(38, 2);
	    ais->type15.mmsi1		= UBITS(40, 30);
	    ais->type15.type1_1		= UBITS(70, 6);
	    ais->type15.type1_1		= UBITS(70, 6);
	    ais->type15.offset1_1	= UBITS(76, 12);
	    //ais->type14.spare2        = UBITS(88, 2);
	    if (ais_context->bitlen > 90) {
		ais->type15.type1_2	= UBITS(90, 6);
		ais->type15.offset1_2	= UBITS(96, 12);
		//ais->type14.spare3    = UBITS(108, 2);
		if (ais_context->bitlen > 110) {
		    ais->type15.type2_1	= UBITS(90, 6);
		    ais->type15.offset2_1	= UBITS(96, 12);
		    //ais->type14.spare4	= UBITS(108, 2);
		}
	    }
	    gpsd_report(LOG_INF, "\n");
	    break;
	case 16:	/* Assigned Mode Command */
	    if (ais_context->bitlen != 96 && ais_context->bitlen != 144) {
		gpsd_report(LOG_WARN, "AIVDM message type 16 size is out of range (%zd).\n",
			    ais_context->bitlen);
		break;
	    }
	    ais->type16.mmsi1		= UBITS(40, 30);
	    ais->type16.offset1		= UBITS(70, 12);
	    ais->type16.increment1	= UBITS(82, 10);
	    if (ais_context->bitlen < 144)
		ais->type16.mmsi2=ais->type16.offset2=ais->type16.increment2 = 0;
	    else {
		ais->type16.mmsi2	= UBITS(92, 30);
		ais->type16.offset2	= UBITS(122, 12);
		ais->type16.increment2	= UBITS(134, 10);
	    }
	    gpsd_report(LOG_INF, "\n");
	    break;
	case 17:	/* GNSS Broadcast Binary Message */
	    if (ais_context->bitlen < 80 || ais_context->bitlen > 816) {
		gpsd_report(LOG_WARN, "AIVDM message type 17 size is out of range (%zd).\n",
			    ais_context->bitlen);
		break;
	    }
	    //ais->type17.spare         = UBITS(38, 2);
	    ais->type17.lon		= UBITS(40, 18);
	    ais->type17.lat		= UBITS(58, 17);
	    ais->type8.bitcount       = ais_context->bitlen - 56;
	    (void)memcpy(ais->type17.bitdata,
			 (char *)ais_context->bits + 10,
			 (ais->type8.bitcount + 7) / 8);
	    gpsd_report(LOG_INF, "\n");
	    break;
	case 18:	/* Standard Class B CS Position Report */
	    if (ais_context->bitlen != 168) {
		gpsd_report(LOG_WARN, "AIVDM message type 18 size not 168 bits (%zd).\n",
			    ais_context->bitlen);
		break;
	    }
	    ais->type18.reserved	= UBITS(38, 8);
	    ais->type18.speed		= UBITS(46, 10);
	    ais->type18.accuracy	= UBITS(56, 1)!=0;
	    ais->type18.lon		= SBITS(57, 28);
	    ais->type18.lat		= SBITS(85, 27);
	    ais->type18.course		= UBITS(112, 12);
	    ais->type18.heading		= UBITS(124, 9);
	    ais->type18.second		= UBITS(133, 6);
	    ais->type18.regional	= UBITS(139, 2);
	    ais->type18.cs		= UBITS(141, 1)!=0;
	    ais->type18.display 	= UBITS(142, 1)!=0;
	    ais->type18.dsc     	= UBITS(143, 1)!=0;
	    ais->type18.band    	= UBITS(144, 1)!=0;
	    ais->type18.msg22   	= UBITS(145, 1)!=0;
	    ais->type18.assigned	= UBITS(146, 1)!=0;
	    ais->type18.raim		= UBITS(147, 1)!=0;
	    ais->type18.radio		= UBITS(148, 20);
	    gpsd_report(LOG_INF,
			"reserved=%d speed=%d accuracy=%d lon=%d lat=%d course=%d heading=%d sec=%d\n",
			ais->type18.reserved,
			ais->type18.speed,
			(uint)ais->type18.accuracy,
			ais->type18.lon,
			ais->type18.lat,
			ais->type18.course,
			ais->type18.heading,
			ais->type18.second);
	    break;	
	case 19:	/* Extended Class B CS Position Report */
	    if (ais_context->bitlen != 312) {
		gpsd_report(LOG_WARN, "AIVDM message type 19 size not 312 bits (%zd).\n",
			    ais_context->bitlen);
		break;
	    }
	    ais->type19.reserved     = UBITS(38, 8);
	    ais->type19.speed        = UBITS(46, 10);
	    ais->type19.accuracy     = UBITS(56, 1)!=0;
	    ais->type19.lon          = SBITS(57, 28);
	    ais->type19.lat          = SBITS(85, 27);
	    ais->type19.course       = UBITS(112, 12);
	    ais->type19.heading      = UBITS(124, 9);
	    ais->type19.second       = UBITS(133, 6);
	    ais->type19.regional     = UBITS(139, 4);
	    UCHARS(143, ais->type19.shipname);
	    ais->type19.shiptype     = UBITS(263, 8);
	    ais->type19.to_bow       = UBITS(271, 9);
	    ais->type19.to_stern     = UBITS(280, 9);
	    ais->type19.to_port      = UBITS(289, 6);
	    ais->type19.to_starboard = UBITS(295, 6);
	    ais->type19.epfd         = UBITS(299, 4);
	    ais->type19.raim         = UBITS(302, 1)!=0;
	    ais->type19.dte          = UBITS(305, 1)!=0;
	    ais->type19.assigned     = UBITS(306, 1)!=0;
	    //ais->type19.spare      = UBITS(307, 5);
	    gpsd_report(LOG_INF,
			"reserved=%d speed=%d accuracy=%d lon=%d lat=%d course=%d heading=%d sec=%d name=%s\n",
			ais->type19.reserved,
			ais->type19.speed,
			(uint)ais->type19.accuracy,
			ais->type19.lon,
			ais->type19.lat,
			ais->type19.course,
			ais->type19.heading,
			ais->type19.second,
			ais->type19.shipname);
	    break;
	case 20:	/* Data Link Management Message */
	    if (ais_context->bitlen < 72 || ais_context->bitlen > 160) {
		gpsd_report(LOG_WARN, "AIVDM message type 20 size is out of range (%zd).\n",
			    ais_context->bitlen);
		break;
	    }
	    //ais->type20.spare		= UBITS(38, 2);
	    ais->type20.offset1		= UBITS(40, 12);
	    ais->type20.number1		= UBITS(52, 4);
	    ais->type20.timeout1	= UBITS(56, 3);
	    ais->type20.increment1	= UBITS(59, 11);
	    ais->type20.offset2		= UBITS(70, 12);
	    ais->type20.number2		= UBITS(82, 4);
	    ais->type20.timeout2	= UBITS(86, 3);
	    ais->type20.increment2	= UBITS(89, 11);
	    ais->type20.offset3		= UBITS(100, 12);
	    ais->type20.number3		= UBITS(112, 4);
	    ais->type20.timeout3	= UBITS(116, 3);
	    ais->type20.increment3	= UBITS(119, 11);
	    ais->type20.offset4		= UBITS(130, 12);
	    ais->type20.number4		= UBITS(142, 4);
	    ais->type20.timeout4	= UBITS(146, 3);
	    ais->type20.increment4	= UBITS(149, 11);
	    break;
	case 21:	/* Aid-to-Navigation Report */
	    if (ais_context->bitlen < 272 || ais_context->bitlen > 360) {
		gpsd_report(LOG_WARN, "AIVDM message type 21 size is out of range (%zd).\n",
			    ais_context->bitlen);
		break;
	    }
	    ais->type21.aid_type = UBITS(38, 5);
	    from_sixbit((char *)ais_context->bits, 
			43, 21, ais->type21.name);
	    if (strlen(ais->type21.name) == 20 && ais_context->bitlen > 272)
		from_sixbit((char *)ais_context->bits, 
			    272, (ais_context->bitlen - 272)/6, 
			    ais->type21.name+20);
	    ais->type21.accuracy     = UBITS(163, 1);
	    ais->type21.lon          = SBITS(164, 28);
	    ais->type21.lat          = SBITS(192, 27);
	    ais->type21.to_bow       = UBITS(219, 9);
	    ais->type21.to_stern     = UBITS(228, 9);
	    ais->type21.to_port      = UBITS(237, 6);
	    ais->type21.to_starboard = UBITS(243, 6);
	    ais->type21.epfd         = UBITS(249, 4);
	    ais->type21.second       = UBITS(253, 6);
	    ais->type21.off_position = UBITS(259, 1)!=0;
	    ais->type21.regional     = UBITS(260, 8);
	    ais->type21.raim         = UBITS(268, 1)!=0;
	    ais->type21.virtual_aid  = UBITS(269, 1)!=0;
	    ais->type21.assigned     = UBITS(270, 1)!=0;
	    //ais->type21.spare      = UBITS(271, 1);
	    gpsd_report(LOG_INF,
			"name=%s accuracy=%d lon=%d lat=%d sec=%d\n",
			ais->type21.name,
			(uint)ais->type19.accuracy,
			ais->type19.lon,
			ais->type19.lat,
			ais->type19.second);
	    break;
	case 22:	/* Channel Management */
	    if (ais_context->bitlen != 168) {
		gpsd_report(LOG_WARN, "AIVDM message type 22 size not 168 bits (%zd).\n",
			    ais_context->bitlen);
		break;
	    }
	    ais->type22.channel_a    = UBITS(40, 12);
	    ais->type22.channel_b    = UBITS(52, 12);
	    ais->type22.txrx         = UBITS(64, 4);
	    ais->type22.power        = UBITS(68, 1);
	    ais->type22.addressed    = UBITS(139, 1);
	    if (!ais->type22.addressed) {
		ais->type22.area.ne_lon       = SBITS(69, 18);
		ais->type22.area.ne_lat       = SBITS(87, 17);
		ais->type22.area.sw_lon       = SBITS(104, 18);
		ais->type22.area.sw_lat       = SBITS(122, 17);
	    } else {
		ais->type22.mmsi.dest1             = SBITS(69, 30);
		ais->type22.mmsi.dest2             = SBITS(104, 30);
	    }
	    ais->type22.band_a       = UBITS(140, 1);
	    ais->type22.band_b       = UBITS(141, 1);
	    ais->type22.zonesize     = UBITS(142, 3);
	    break;
	case 23:	/* Group Assignment Command */
	    if (ais_context->bitlen != 160) {
		gpsd_report(LOG_WARN, "AIVDM message type 23 size not 160 bits (%zd).\n",
			    ais_context->bitlen);
		break;
	    }
	    ais->type23.ne_lon       = SBITS(40, 18);
	    ais->type23.ne_lat       = SBITS(58, 17);
	    ais->type23.sw_lon       = SBITS(75, 18);
	    ais->type23.sw_lat       = SBITS(93, 17);
	    ais->type23.stationtype  = UBITS(110, 4);
	    ais->type23.shiptype     = UBITS(114, 8);
	    ais->type23.txrx         = UBITS(144, 4);
	    ais->type23.interval     = UBITS(146, 4);
	    ais->type23.quiet        = UBITS(150, 4);
	    break;
	case 24:	/* Class B CS Static Data Report */
	    switch (UBITS(38, 2)) {
	    case 0:
		if (ais_context->bitlen != 160) {
		    gpsd_report(LOG_WARN, "AIVDM message type 24A size not 160 bits (%zd).\n",
				ais_context->bitlen);
		    break;
		}
		UCHARS(40, ais_context->shipname);
		//ais->type24.a.spare	= UBITS(160, 8);
		return false;	/* data only partially decoded */
	    case 1:
		if (ais_context->bitlen != 168) {
		    gpsd_report(LOG_WARN, "AIVDM message type 24B size not 168 bits (%zd).\n",
				ais_context->bitlen);
		    break;
		}
		(void)strlcpy(ais->type24.shipname, 
			      ais_context->shipname,
			      sizeof(ais_context->shipname));
		ais->type24.shiptype = UBITS(40, 8);
		UCHARS(48, ais->type24.vendorid);
		UCHARS(90, ais->type24.callsign);
		if (AIS_AUXILIARY_MMSI(ais->mmsi))
		    ais->type24.mothership_mmsi   = UBITS(132, 30);
		else {
		    ais->type24.dim.to_bow        = UBITS(132, 9);
		    ais->type24.dim.to_stern      = UBITS(141, 9);
		    ais->type24.dim.to_port       = UBITS(150, 6);
		    ais->type24.dim.to_starboard  = UBITS(156, 6);
		}
		//ais->type24.b.spare	    = UBITS(162, 8);
		break;
	    }
	    gpsd_report(LOG_INF, "\n");
	    break;
	case 25:	/* Binary Message, Single Slot */
	    /* this check and the following one reject line noise */
	    if (ais_context->bitlen < 40 || ais_context->bitlen > 168) {
		gpsd_report(LOG_WARN, "AIVDM message type 25 size not between 40 to 168 bits (%zd).\n",
			    ais_context->bitlen);
		break;
	    }
	    ais->type25.addressed	= (bool)UBITS(38, 1);
	    ais->type25.structured	= (bool)UBITS(39, 1);
	    if (ais_context->bitlen < (40 + (16*ais->type25.structured) + (30*ais->type25.addressed))) {
		gpsd_report(LOG_WARN, "AIVDM message type 25 too short for mode.\n");
		break;
	    }
	    if (ais->type25.addressed)
		ais->type25.dest_mmsi   = UBITS(40, 30);
	    if (ais->type25.structured)
		ais->type25.app_id      = UBITS(40+ais->type25.addressed*30,16);
	    /*
	     * Not possible to do this right without machinery we
	     * don't yet have.  The problem is that if the addressed
	     * bit is on the bitfield start won't be on a byte
	     * boundary. Thus the formulas below (and in message type 26)
	     * will work perfectly for brodacst messages, but for addressed
	     * messages the retrieved data will be led by thr 30 bits of
	     * the destination MMSI
	     */
	    ais->type25.bitcount       = ais_context->bitlen - 40 - 16*ais->type25.structured;
	    (void)memcpy(ais->type25.bitdata,
			 (char *)ais_context->bits+5 + 2 * ais->type25.structured,
			 (ais->type25.bitcount + 7) / 8);
	    gpsd_report(LOG_INF, "addressed=%d, structured=%d, dest=%u, id=%u, cnt=%zd\n",
			ais->type25.addressed,
			ais->type25.structured,
			ais->type25.dest_mmsi,
			ais->type25.app_id,
			ais->type25.bitcount);		
	    break;
	case 26:	/* Binary Message, Multiple Slot */
	    if (ais_context->bitlen < 60 || ais_context->bitlen > 1004) {
		gpsd_report(LOG_WARN, "AIVDM message type 26 size is out of range (%zd).\n",
			    ais_context->bitlen);
		break;
	    }
	    ais->type26.addressed	= (bool)UBITS(38, 1);
	    ais->type26.structured	= (bool)UBITS(39, 1);
	    if (ais->type26.addressed)
		ais->type26.dest_mmsi      = UBITS(40, 30);
	    if (ais->type26.structured)
		ais->type26.app_id      = UBITS(40+ais->type26.addressed*30,16);
	    ais->type26.bitcount        = ais_context->bitlen - 60 - 16*ais->type26.structured;
	    (void)memcpy(ais->type26.bitdata,
			 (char *)ais_context->bits+5 + 2 * ais->type26.structured,
			 (ais->type26.bitcount + 7) / 8);
	    gpsd_report(LOG_INF, "addressed=%d, structured=%d, dest=%u, id=%u, cnt=%zd\n",
			ais->type26.addressed,
			ais->type26.structured,
			ais->type26.dest_mmsi,
			ais->type26.app_id,
			ais->type26.bitcount);
	    break;
	default:
	    gpsd_report(LOG_INF, "\n");
	    gpsd_report(LOG_ERROR, "Unparsed AIVDM message type %d.\n",ais->type);
	    break;
	}
#undef UCHARS
#undef SBITS
#undef UBITS

	/* data is fully decoded */
	return true;
    }

    /* we're still waiting on another sentence */
    return false;
}
/*@ -charint @*/

/* driver_aivdm.c ends here */


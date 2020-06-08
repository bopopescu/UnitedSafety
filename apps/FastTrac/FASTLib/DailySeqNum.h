#pragma once
#include <stdlib.h>
//! Class DailySeqNum - keeps an incrementing
//	sequence number on a daily basis.	In other words the sequence number
//	is reset to zero on the first startup of a new day (based on the day of the
//	month passed in - adjusted for local time not UTC time).
//	Multiple restarts in one day will result in an ongoing incremental number
class DailySeqNum
{
private:
	unsigned char m_SeqNum;
	short m_DayOfMonth;

public:
	DailySeqNum()
	{
		m_SeqNum = 0;
		m_DayOfMonth = 1;

// ATS FIXME: Make this an "ifdef" or delete the code.
/* removed for RedStone
		FILE *fp;
		char buf[32];

		fp = fopen("/home/root/FASTTrack/seqNum.txt", "r");
		if (fp != NULL)	// file is there - read the seqnum
		{
			if (fgets(buf, 32, fp))
				m_SeqNum = atoi (buf);
			if (fgets(buf, 32, fp))
				m_DayOfMonth = atoi (buf);
			fclose(fp);
		}
		else	// file not there - first time?
		{
			WriteSeqNum();	// write out the zero value
		}
*/
	}

	void UpdateDay(short dayOfMonth)
	{
		if (dayOfMonth != m_DayOfMonth)
		{
			m_SeqNum = 0;
			m_DayOfMonth = dayOfMonth;
			WriteSeqNum();
		}
	}

	unsigned char GetSeqNum()	// returns the incremented sequence number
	{
		++m_SeqNum;
 // ATS FIXME: Make this an "ifdef" or delete the code.
 //	 WriteSeqNum();
		return m_SeqNum;
	};

	void WriteSeqNum()
	{
		FILE *fp;

		fp = fopen("/home/root/FASTTrack/seqNum.txt", "w");

		if (fp != NULL)	// file is there - read the seqnum
		{
			fprintf(fp, "%d\n%d\n", m_SeqNum, m_DayOfMonth);
			fclose(fp);
		}
	}
};


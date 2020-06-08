#pragma once

#include <unistd.h>
#include <time.h>
#include <string>

// A class to determine time differences
//
// To use the class call SetTime()
// to set the system time
//
// Call DiffTime() to get the difference
// in whole seconds between SetTime and now.
// 
//
class AFS_Timer
{
  struct timespec m_time;

public:
  AFS_Timer()
  {
    SetTime();  // load up startup time as default
  };

  void SetTime()
  {
    clock_gettime(CLOCK_REALTIME, &m_time);
  };

  typedef long long LL;

  // Description: Returns the time elapsed in nanoseconds
  // Return: The elapsed time in nanoseconds.
  LL ElapsedTime() const
  {
		struct timespec t;
		clock_gettime(CLOCK_REALTIME, &t);
		const LL &f = ((LL) t.tv_sec) * ((LL)1000000000) + ((LL)t.tv_nsec);
		const LL &i = ((LL) m_time.tv_sec) * ((LL)1000000000) + ((LL)m_time.tv_nsec);
		return f - i;
  }

  // Description: Returns the time elapsed in seconds.
  // Return: The elapsed time in seconds.
  int DiffTime() const
  {
		return int(ElapsedTime() / ((LL)1000000000));
  };

  //  Description: Returns the time elapsed in milliseconds.
  // Return: The elapsed time in milliseconds.
  int DiffTimeMS() const
  {
	  return ElapsedTime() / ((LL)1000000);
  };

  // Description: Resets the timer value to the current time, and blocks
  //	(goes to sleep) for msDelayTime milliseconds.
  //
  //	NOTE: This function will block/sleep for at least msDelayTime, but
  //		may block/sleep slightly longer due to operating system load and
  //		responsiveness.
  void Pause(short msDelayTime)
  {
	  SetTime();
	  usleep(int(msDelayTime) * 1000);
  }
	//------------------------------------------------------------------------------
	std::string GetTimestamp()
	{
		char s[32];  // should be enough space.
		s[0] = '\0';
		struct tm *tm;
		tm = localtime(&(m_time.tv_sec));
		if (tm != NULL)
		{
			strftime(s, 31, "%Y-%m-%dT%H:%M:%S", tm);
		}

		return std::string(s);
	}
	std::string GetTimestampWithOS()
	{
		char s[32];  // should be enough space.
		s[0] = '\0';
		struct tm *tm;
		tm = localtime(&(m_time.tv_sec));
		if (tm != NULL)
		{
			strftime(s, 31, "%Y-%m-%dT%H:%M:%S.000-0000", tm);
		}

		return std::string(s);
	}
};



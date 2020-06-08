#pragma once

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/select.h>
// a small class to handle the select call in linux.
// This allows the addition of multiple descriptors on a
// onetime only basis or a permanent basis.
// It also allows the remove of file descriptors

// to use:
//
//  with several file descriptors fd1, fd2, fd3 to be looked at all the time and
//  fd4 to be looked at just this time:
//
//  CSelect mSelect;
//  mSelect.SetDelay(0, 2000); // delay for 1/5 of a second on read for data to show up
//  mSelect.Add(fd1);
//  mSelect.Add(fd2);
//  mSelect.Add(fd3);
//  mSelect.AddOnce(fd4);
//  if (mSelect.Select())
//  {
//    if (mSelect.HasData(fd1)
//      ...
//  }

class CSelect
{
private:

  fd_set         m_Inputs;  // fd_set for the select call for com input
  int            m_maxFD;   // max FD for select() call.
  struct timeval tv;
  int *m_FDs, m_nFDs;
  int *m_OneTimeFDs, m_nOneTimeFDs;
  bool m_DelayEnabled;
  int m_Secs, m_uSecs;

public:
  CSelect();
  ~CSelect();

  void Add(const int fd);  // add a file descriptor for each time through
  void AddOnce(const int fd); // add a file descriptor for the next select only

  bool HasData(const int fd);  // return true if data on that fd

  bool Remove(const int fd); // not from the OneTime list.
  void Reset(); // clear the file descriptors list

  bool Select();  // run the select call

  void SetDelay(int secs, int uSecs); // set the time delay for the select call
  //********************************************************************
  // EnableDelay(bool enable) - enables the timeout delay value in select
  //                            If 'enable' is set to false then Select() blocks
  //                            until one or more of the fds are readable.
  void EnableDelay(bool enable) { m_DelayEnabled = enable; }
private:
  void BuildFDList();
  void DeleteFDs()
  {
    if (m_nFDs > 0)
      delete m_FDs;
    m_nFDs = 0;
  }
  void DeleteOneTimeFDs()
  {
    if (m_nOneTimeFDs > 0)
      delete m_OneTimeFDs;
    m_nOneTimeFDs = 0;
  }

};



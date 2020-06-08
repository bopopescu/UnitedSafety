#include "CSelect.h"



//Constructor - null out the lists and set a default delay of 1msec
//
CSelect::CSelect()
{
  SetDelay(0, 1000);
  m_DelayEnabled = true;
  m_FDs = NULL;
  m_nFDs = 0;
  m_OneTimeFDs = NULL;
  m_nOneTimeFDs = 0;
  FD_ZERO(&m_Inputs);
  m_maxFD = 0;
}

// Destructor - delete all allocated memory
CSelect::~CSelect()
{
  DeleteFDs();
  DeleteOneTimeFDs();
}

// Set the delay time in seconds and microseconds.  This is how long
// the select call will stall for waiting for data on all the ports before
// it returns.
void CSelect::SetDelay(const int Secs, const int uSecs) // set the time delay for the select call
{
  m_Secs = Secs;
  m_uSecs = uSecs;
}

// Add the given fd to the next select call but not any subsequent ones.  I don't
// imagine that this will get much use but you want it for the odd time that it will
// be needed.
void CSelect::AddOnce(const int fd) // add a file descriptor for the next select only
{
  int i;
  
  // check for repeats
  for (i = 0; i < m_nOneTimeFDs; i++)
    if (m_OneTimeFDs[i] == fd)
      return;

  int * tmp;
  tmp = new int [m_nOneTimeFDs + 1];

  for (i = 0; i < m_nOneTimeFDs; i++)
    tmp[i] = m_OneTimeFDs[i];
  
  tmp[m_nOneTimeFDs] = fd;
  i = m_nOneTimeFDs + 1;
  DeleteOneTimeFDs();
  m_nOneTimeFDs = i;
  m_OneTimeFDs = tmp;
}

// Add an FD to the list.  The FD will be checked on every select call
// going forward until the CSelect object dies or the Remove function is 
// called with the FD.
void CSelect::Add(const int fd)  // add a file descriptor for each time through
{
  int i;
  
  // check for repeats
  for (i = 0; i < m_nFDs; i++)
    if (m_FDs[i] == fd)
      return;

  int * tmp;
  tmp = new int [m_nFDs + 1];

  for (i = 0; i < m_nFDs; i++)
    tmp[i] = m_FDs[i];
  
  tmp[m_nFDs] = fd;
  i = m_nFDs + 1;
  DeleteFDs();
  m_nFDs = i;
  m_FDs = tmp;
}

// Run the select call to look for data on the ports.
//
bool CSelect::Select()  // run the select call
{
  BuildFDList();

  if(m_DelayEnabled)
  {
	  tv.tv_sec = m_Secs;  // set this every time because tv gets modified in select call.
	  tv.tv_usec = m_uSecs;
	  
    if (select(m_maxFD, &m_Inputs, NULL, NULL, &tv) > 0)
      return true;
  }
  else
  {
    if (select(m_maxFD, &m_Inputs, NULL, NULL, NULL) > 0)
      return true;
  }
  return false;
}

// clear all of the FDs from both lists
void CSelect::Reset() // clear the file descriptors list
{
  DeleteFDs();
  DeleteOneTimeFDs();
}

// True if a select call indicates that there is data on the port 
//
bool CSelect::HasData(const int fd)  // return true if data on that fd
{
  if (fd > 0 && FD_ISSET(fd, &m_Inputs))
    return true;

  return false;
}


//-----------------------------------------------------------------------------
// BuildFDList - internal call
// build the list of file descriptors for the select call
//
void CSelect::BuildFDList()
{
  int i;
  // Initialize the input set
  FD_ZERO(&m_Inputs);
  m_maxFD  = -1;

  for (i = 0; i < m_nFDs; i++)
  {
    if (m_FDs[i] > 0)
      FD_SET(m_FDs[i], &m_Inputs);

    if (m_FDs[i] > m_maxFD)
      m_maxFD = m_FDs[i];
  }

  for (i = 0; i < m_nOneTimeFDs; i++)
  {
    FD_SET(m_OneTimeFDs[i], &m_Inputs);
    if (m_OneTimeFDs[i] > m_maxFD)
      m_maxFD = m_OneTimeFDs[i];
  }

  m_maxFD += 1;

  // get rid of the one time FDs
  DeleteOneTimeFDs();
}

// Remove an FD from the list
//  returns true if it was found and deleted
//          false if it wasn't in the list
bool CSelect::Remove(const int fd)
{
  if (m_nFDs == 0)  // if no FDs - can't get rid of any!
    return false;

  // test for 1 only case
  if (m_nFDs == 1)
  {
    if (fd == m_FDs[0])
    {
      DeleteFDs();
      return true;
    }
    return false;  // nothing changes
  }

  int *tmp = new int [m_nFDs - 1];
  int i, j;
  
  for (i = 0, j = 0; i < m_nFDs; i++)
  {
    if (m_FDs[i] != fd)
    {
      tmp[j++] = m_FDs[i];
    }
  }
  delete [] m_FDs;
  m_FDs = tmp;
  m_nFDs -= 1;

  return (j == i);
}



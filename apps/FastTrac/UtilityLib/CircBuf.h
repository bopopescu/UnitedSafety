
/*$============================================================================
  Name:        CircBuf

  Declare as

  CircBuf<char, 100L> cb;
==============================================================================*/
#pragma once

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

template <class T, long m_nSize=10>
class CircBuf
{
private:
  long  m_Head;                // Next location to place incoming data
  long  m_Tail;                // Next location to read data
  long  m_nElements;           // Total number of m_Elements between m_Head and m_Tail
  T     m_Elements[m_nSize];   // allocated array of m_Elements of type T

public:

  //---------------------------------------------------------------------------
  CircBuf(void)                   // Constructor
  {
    m_Head = 0;
    m_Tail = 0;
    m_nElements = 0;
  };

  //---------------------------------------------------------------------------
  CircBuf(const CircBuf<T, m_nSize> &buffer) // Copy Constructor
  {
    m_Head = buffer.m_Head;
    m_Tail = buffer.m_Tail;
    m_nElements = buffer.m_nElements;
    memcpy(m_Elements, buffer.m_Elements, m_nSize * sizeof(T));
  };

  //---------------------------------------------------------------------------
  // Add a bunch of elements to the buffer
  short Add(T *data, long num)
  {
    short rc = true;

    if (m_nSize == 0)
      return false;

    if (num > m_nSize)
    {
      rc = false;
    }
    else
    {
      for (int i = 0; i < num; i++)
      {
        m_Elements[m_Head] = data[i];
        m_Head = (m_Head + 1L) % m_nSize;
        m_nElements++;

        if (m_nElements > m_nSize)
          m_nElements = m_nSize;

        if (m_nElements == m_nSize)
        {
          m_Tail = m_Head;
        }
      }
    }
    return rc;
  };

  //--------------------------------------------------------------------------
  // Add a single element
  short Add(T data)
  {
    short rc = true;

    if (m_nSize == 0)
      return rc;

    m_Elements[m_Head] = data;
    m_Head = (m_Head + 1L) % m_nSize;
    m_nElements++;

    if (m_nElements > m_nSize)
      m_nElements = m_nSize;

    // WE ARE ABOUT TO WRAP BUFFER
    // SO THE NEXT LOCATION TO READ (m_Tail) IS NOW THE m_Head
    if (m_nElements == m_nSize)
    {
      m_Tail = m_Head;
    }

    return rc;
  };


  //---------------------------------------------------------------------------
  // Reset the buffer
  void Reset(void)
  {
    m_Head = 0;
    m_Tail = 0;
    m_nElements = 0;
  };

  //---------------------------------------------------------------------------
  // return the number of m_Elements in the buffer
  long  GetNumElements(void) const {return m_nElements;};


  //---------------------------------------------------------------------------
  // Peek at the character at location
  T Peek(long location)
  {
    location = (location + m_Tail) % m_nSize;
    return(m_Elements[location]);
  };


  //---------------------------------------------------------------------------
  // Peek into a specific location and copy num T's into the
  // data buffer
  short Peek(T *data, long max_data, long num, long location)
  {
    if (m_nSize == 0 || num < 1L)
      return 0;

    if (num > max_data ||  num > m_nElements)
      return 0;

    location = (location + m_Tail) % m_nSize;

    long first;
    first = num;
    if (first > m_nSize - m_Tail)
      first = m_nSize - m_Tail;

    long second = 0L;
    if (second  < num - first)
      second = num - first;

    memcpy(data, &m_Elements[location], first * sizeof(T));
    memcpy(&data[first], m_Elements, second * sizeof(T));

    return true;
  };


  //---------------------------------------------------------------------------
  // Delete num T's from the CircBuffer - starting at the specified char
  short Delete(long start, long num)
  {
    if (m_nSize == 0 || num < 1L)
      return 0;

    if ( (num + start) >= m_nElements)  // deleting too much so wipe out from
    {                                  // from start to the end of the buffer
      m_Head = (m_Tail + start) % m_nSize;
      m_nElements = start;
      return 1;
    }

    long remaining;
    remaining = m_nElements - (num + start);  // how many chars remaing at end
    for (long i = 0; i < remaining; i++)
    {
      m_Elements[(m_Tail + start + i) % m_nSize] =
          m_Elements[(m_Tail + start + num +i) % m_nSize];
    }

    m_Head = (m_Tail + start + remaining) % m_nSize;
    m_nElements -= num;
    return true;
  };

  //---------------------------------------------------------------------------
  // Remove num T's from the CircBuffer - starting at the m_Tail
  short Extract(T *data, long max_data, long num)
  {
    if (m_nSize == 0 || num < 1L)
      return 0;

    if (num > max_data ||  num > m_nElements)
    {
      return 0;
    }

    long first;
    first = num;
    if (first > m_nSize - m_Tail)
      first = m_nSize - m_Tail;

    long second = 0L;
    if (second  < num - first)
      second = num - first;

    memcpy(data, &m_Elements[m_Tail], first * sizeof(T));
    memcpy(&data[first], m_Elements, second * sizeof(T));

    m_Tail = (m_Tail + num) % m_nSize;
    m_nElements -= num;
    return true;
  };


  //---------------------------------------------------------------------------
  // Remove 1 T from the CircBuffer - starting at the m_Tail
  short Extract(T &data)
  {
    short rc = 1;

    if (m_nSize == 0 || m_nElements < 1)
    {
      return 0;
    }

    data = m_Elements[m_Tail];
    m_Tail = (m_Tail + 1) % m_nSize;
    --m_nElements;

    return rc;
  };


  //---------------------------------------------------------------------------
  // Remove num T's from the CircBuffer - starting at the m_Tail - don't fill an output buffer
  short Toss(long num)
  {
     short rc = 1;

    if (m_nSize == 0 || num < 1L)
      return 0;

    if (num > m_nElements)
    {
      return 0;
    }

    m_Tail = (m_Tail + num) % m_nSize;
    m_nElements -= num;
    return rc;
  };


  //---------------------------------------------------------------------------
  // Find 'data' in the buffer starting at the specified char
  //and return the index (-1 if not found)
  long  Find(const T &data, long start)
  {
    if (m_nSize < start || m_nElements  < start)
      return -2;

    long rc = 0;
    long scan_m_Tail = (start + m_Tail) % m_nSize;

    do
    {
      if (m_Elements[scan_m_Tail] == data)
        return (start + rc);

      ++rc;

      scan_m_Tail = (scan_m_Tail + 1L) % m_nSize;

    } while (scan_m_Tail != m_Head);
    return -1L;
  };

  //---------------------------------------------------------------------------
  // Find 'data' in the buffer and return the index (-1 if not found)
  long  Find(const T &data)
  {
    long rc = -1L;
    long scan_m_Tail = m_Tail;

    if (m_nSize == 0 || m_nElements == 0)
      return rc;

    do
    {
      ++rc;

      if (m_Elements[scan_m_Tail] == data)
      {
        return rc;
      }

      scan_m_Tail = (scan_m_Tail + 1L) % m_nSize;

    } while (scan_m_Tail != m_Head);

    return -1L;
  };

  //---------------------------------------------------------------------------
  // Find a matching array of 'data' in the buffer
  //and return the index (-1 if not found) of the starting char
  long  Find(const T *data, long nData)
  {
    long len = 0;
    long ix = 0;

    while (1)
    {
      ix = 0;

      if ((len = Find(data[0], len) ) < 0)
        return len;

      while (ix < nData)
      {
        ix++;

        if ( data[ix] != Peek(len + ix) )
	{
          len++;  // skip to next item
          break;  // and start the search again
        }
      }
      return len;

    }
    return -1;
  };
  //---------------------------------------------------------------------------
  // Toss the first character
  short Toss(void)
  {
    short rc = 0;

    if (m_Head != m_Tail)
    {
      m_Tail = (m_Tail + 1L) % m_nSize;
      --m_nElements;
      rc = 1;
    }

   return rc;
  };

  //---------------------------------------------------------------------------
  // GetLine - look for \n and return all the chars up to there, including the
  //           \n in the strLine buffer (up to maxChars)
  //           return - number of chars
  //                  - -1 - no \n
  //                    -2 - not enough chars in strLine to copy data
  //                    -3 - extract failed.
  //
  short GetLine(T *strLine, short maxChars)
  {
    long cr, lf;

    // strip any leading \r\n

    cr = Find('\r');  // 0 based so num chars is cr + 1
    lf = Find('\n');  // 0 based so num chars is cr + 1

    while (cr == 0 || lf == 0)
    {
      Toss();
      cr = Find('\r');  // 0 based so num chars is cr + 1
      lf = Find('\n');  // 0 based so num chars is cr + 1
    }

    if (cr == -1)
      cr = Find('\n');  // 0 based so num chars is cr + 1

    if (cr == 0 || cr == -1)
      return -1;

    if (cr + 1 > maxChars)
      return -2;

    if (Extract(strLine, maxChars, cr + 1) == 0)
      return -3;  // this should NEVER happen!

    return cr + 1;  // returns actual number of chars

  }

  void DumpHex()
  {
    printf("CircBuf Dump\n\n  Num Elements = %ld\n", m_nElements);

    long scan_m_Tail = (m_Tail) % m_nSize;

    while (scan_m_Tail != m_Head)
    {
      printf("%x ", m_Elements[scan_m_Tail]);
      scan_m_Tail = (scan_m_Tail + 1L) % m_nSize;
    };
    printf("\n");
  };
};


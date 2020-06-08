/*$============================================================================
  Name:        CharBuf - implement a circular character buffer
==============================================================================*/
#pragma once

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

class CharBuf
{
private:
  long  m_Head;                // Next location to place incoming data
  long  m_Tail;                // Next location to read data
  long  m_nElements;           // Total number of m_Elements between m_Head and m_Tail
  unsigned short m_nSize;       // Total size of the character buffer
  char  *m_Elements;   // allocated array of m_Elements of type T

public:

  //---------------------------------------------------------------------------
  CharBuf(void)                   // Constructor
  {
    m_Head = 0;
    m_Tail = 0;
    m_nElements = 0;
    m_nSize = 0;
	m_Elements = NULL;
  };

  bool SetSize(unsigned short nChars)
  {
    if (m_nSize > 0)  // already got one. Get rid of it
    {
      delete [] m_Elements;
      m_nSize = 0;  /// set in case new fails so that we don't delete twice
    }

//    try
    {
      m_Elements = new char [nChars];
    }
//    catch (exception & e)
//    {
//      printf("Error allocating memory in CharBuf.\n");
//      return false;
//    }

    if (m_Elements == NULL)
      return false;

    m_nSize = nChars;
    return true;
  }


  //---------------------------------------------------------------------------
  // Add a bunch of elements to the buffer
  short Add(const char *data, long num)
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
  short Add(char data)
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
  // Peek at the character at location - (0) is the first char in the buf
  char Peek(long location)
  {
    location = (location + m_Tail) % m_nSize;
    return(m_Elements[location]);
  };


  //---------------------------------------------------------------------------
  // Peek into a specific location and copy num T's into the 
  // data buffer
  bool Peek(char *data, long max_data, long num, long location)
  {
    if (m_nSize == 0 || num < 1L)
      return false;

    if (num > max_data ||  num > m_nElements)
      return false;

    location = (location + m_Tail) % m_nSize;

    long first;
    first = num;
    if (first > m_nSize - m_Tail)
      first = m_nSize - m_Tail;

    long second = 0L;
    if (second  < num - first)
      second = num - first;
 
    memcpy(data, &m_Elements[location], first * sizeof(char));
    memcpy(&data[first], m_Elements, second * sizeof(char));

    return true;
  };


  //---------------------------------------------------------------------------
  // Delete num T's from the CharBuffer - starting at the specified char
  bool Delete(long start, long num)
  {
    if (m_nSize == 0 || num < 1L)
      return false;

    if ( (num + start) >= m_nElements)  // deleting too much so wipe out from
    {                                  // from start to the end of the buffer
      m_Head = (m_Tail + start) % m_nSize;
      m_nElements = start;
      return true;
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
  // Remove num chars from the CharBuffer - starting at the m_Tail
  bool Extract(char *data, long max_data, long num)
  {
    if (m_nSize == 0 || num < 1L)
      return false;

    if (num > max_data ||  num > m_nElements)
    {
      return false;
    }

    long first;
    first = num;
    if (first > m_nSize - m_Tail)
      first = m_nSize - m_Tail;

    long second = 0L;
    if (second  < num - first)
      second = num - first;
 
    memcpy(data, &m_Elements[m_Tail], first * sizeof(char));
    memcpy(&data[first], m_Elements, second * sizeof(char));
    data[num] = '\0';

    m_Tail = (m_Tail + num) % m_nSize;
    m_nElements -= num;
    return true;
  };

 
  //---------------------------------------------------------------------------
  // Remove the chars from start to end, including both start and end
  // Discard everything up to start
  //
  bool Extract(char *data, long max_data, long startx, long endx)
  {
    data[0] = '\0';
    
    if (m_nSize == 0)
      return false;

    if (endx < startx)
      return false;

    if ((endx - startx) > max_data ||  endx > m_nElements)
    {
      return false;
    }

    Toss(startx);  // delete everything up to startx
    return Extract(data, max_data, endx - startx);
  };

 
  //---------------------------------------------------------------------------
  // Remove 1 T from the CharBuffer - starting at the m_Tail
  bool Extract(char &data)
  {
    if (m_nSize == 0 || m_nElements < 1)
    {
      return false;
    }

    data = m_Elements[m_Tail];
    m_Tail = (m_Tail + 1) % m_nSize;
    --m_nElements;

    return true;
  };


  //---------------------------------------------------------------------------
  // Remove num T's from the CharBuffer - starting at the m_Tail - don't fill an output buffer
  bool Toss(long num)
  {
    if (m_nSize == 0 || num < 1L)
      return false;

    if (num > m_nElements)
    {
      return false;
    }

    m_Tail = (m_Tail + num) % m_nSize;
    m_nElements -= num;
    return true;
  };


  //---------------------------------------------------------------------------
  // Find 'data' in the buffer starting at the specified char
  //and return the index (-1 if not found)
  long  Find(const char data, long start)
  {
    if (m_nSize < start || m_nElements  < start)
      return -1;

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
  long  Find(char data)
  {
    long rc = -1L;
    long scan_m_Tail = m_Tail;

    if (m_nSize == 0 || m_nElements == 0)
      return -2;

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
  long  Find(const char *data, long nData)
  {
    long len = 0;
    long ix = 0;

    if (m_nSize == 0 || m_nElements == 0 || m_nElements < nData)
      return -1;

    while (1)
    { 
      ix = 0;

      if ((len = Find(data[ix], len) ) < 0)
        return len;

      while (ix < nData - 1)
      {
        ix++;

        if ( data[ix] != Peek(len + ix) )
        {
          len++;  // skip to next item
          goto continueSearching;
//          break;  // and start the search again
        }
      }
      return len;
continueSearching:
;
    }
    return -1;
  };
  //---------------------------------------------------------------------------
  // Toss the first character
  bool Toss(void)
  {
    if (m_Head != m_Tail)
    {
      m_Tail = (m_Tail + 1L) % m_nSize;
      --m_nElements;
      return true;
    }

   return false;
  };

  //--------------------------------------------------------------------------
  // GetLine - look for line terminating character defined by 'c' and return
  //           all the chars up to there including the terminating char in the
  //           strLine buffer (up to maxChars)
  // -1 - terminating char not found
  // -2 - not enough space in strLine for buffer (len of buffer > maxChars)
  // -3 - unable to extract the line for some reason (0 length string?)
  // > 0 - length of  string returned in strLine.
  //
  short GetLine(char *strLine, short maxChars, char c)
  {
    long cr;
    cr = Find(c);

    if(cr <= -1)
      return -1;

    if(cr + 1 > maxChars)
    {
      Delete(0,cr);
      return -2;
    }

    if (Extract(strLine, maxChars, cr +1) == false)
      return -3;

    return cr + 1;
  }

  //---------------------------------------------------------------------------
  // GetLine - look for \n and return all the chars up to there, including the
  //           \n in the strLine buffer (up to maxChars)
  //           return - number of chars
  //                  - -1 - no \n
  //                    -2 - not enough chars in strLine to copy data
  //
  short GetLine(char *strLine, short maxChars)
  {
    long cr;
    cr = Find('\r');  // 0 based so num chars is cr + 1

    if (cr == -1)
      cr = Find('\n');  // 0 based so num chars is cr + 1

    if (cr <= -1)
      return -1;

	char c = Peek(cr + 1);

    if (c == '\r' || c == '\n')
		cr++;

    if (cr + 1 > maxChars)
    {
      Delete(0, cr);
      return -2;
    }

    if (Extract(strLine, maxChars, cr + 1) == false)
      return -3;

    return cr + 1;  // returns actual number of chars

  };
//---------------------------------------------------------------------------
  // GetLine - starting frmo startChar look for \n and return all the chars up to there, including the
  //           \n in the strLine buffer (up to maxChars)
  //           return - number of chars
  //                  - -1 - no \n
  //                    -2 - not enough chars in strLine to copy data
  //
  short GetLine(char *strLine, const short startChar, short maxChars)
  {
    long cr;
    cr = Find('\r', startChar);  // 0 based so num chars is cr + 1

    if (cr == -1)
      cr = Find('\n', startChar);  // 0 based so num chars is cr + 1

    if (cr == -1)
      return -1;

	char c = Peek(cr + 1);

    if (c == '\r' || c == '\n')
		cr++;

    if ((cr + 1 - startChar) > maxChars)
    {
//      printf("CharBuf::GetLine cr found at %d\n",cr);
      Delete(startChar, cr);
      return -2;
    }

    if (Extract(strLine, maxChars, startChar, cr + 1) == false)
      return -3;

    return cr + 1;  // returns actual number of chars

  };

  void Dump()
  {
    printf("CharBuf Dump\n\n  Num Elements = %ld\n", m_nElements);

    long scan_m_Tail = (m_Tail) % m_nSize;

    while (scan_m_Tail != m_Head)
    {
      printf("%c", m_Elements[scan_m_Tail]);
      scan_m_Tail = (scan_m_Tail + 1L) % m_nSize;
    };
    printf("\n");
  };


  //---------------------------------------------------------------------------------------
  void DumpHex()
  {
    printf("CharBuf Dump -->>  Num Elements = %ld  tail %ld head %ld\r\n", m_nElements, m_Tail, m_Head);
    long scan_m_Tail = (m_Tail) % m_nSize;
    long scan_m_Head = (m_Head) % m_nSize;
    short i = 0;
    char buf1[32], buf2[128], hexBuf[8];

    if (m_nElements == 0)
      return;

    if (scan_m_Head < 0)
      scan_m_Head += m_nSize;

    buf1[16] = '\0';
    buf2[0] = '\0';
    
    while (scan_m_Tail != scan_m_Head)
    {
      sprintf(hexBuf, "%02X  ", (unsigned char)m_Elements[scan_m_Tail]);
      strcat(buf2, hexBuf);
      if (m_Elements[scan_m_Tail] >= 32 && m_Elements[scan_m_Tail] <= 126)
        buf1[i] = m_Elements[scan_m_Tail];
      else
        buf1[i] = '.';

      if (++i == 16)
      {
        i = 0;
        printf("%s::  %s\r\n", buf1, buf2);
        buf1[16] = '\0';
        buf2[0] = '\0';        
      }

      scan_m_Tail = (scan_m_Tail + 1L) % m_nSize;
    };

    if (i != 0)
      printf("%s::  %s\r\n", buf1, buf2);
  };
};


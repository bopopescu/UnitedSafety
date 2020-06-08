#pragma once

class MovingAverage
{
  private:
    unsigned long m_nSize;
    unsigned long m_nCurrent;
    double *m_Data;

  public:
   MovingAverage(){m_nSize = 0; m_nCurrent = 0; m_Data = NULL;};
   ~MovingAverage(){delete [] m_Data;};

   double Average()
   {
     short max;

     if (m_nCurrent < m_nSize)
       max = m_nCurrent;
     else
       max = m_nSize;

     double avg = 0;
     for (short i = 0; i < max; i++)
       avg += m_Data[i];

     if (max == 0)
       return 0.0;

     return (avg / (double)max);
   };

   bool SetSize(short avgSize)
   {
     delete [] m_Data;

     m_Data = (double *)new double [avgSize];
     m_nSize = avgSize;

     if (m_Data)
       return true;
     else 
       return false;
   }

   void Add(double dVal)
   {
     if (m_nSize == 0 || m_Data == NULL)
       return;

     m_Data[m_nCurrent % m_nSize] = dVal;
     m_nCurrent++;
   }

   void Reset()
   {
     m_nCurrent = 0;
   }
   
   void Reset(double newVal)  // used to force the average to a new value
   {
     m_nCurrent = m_nSize;
     for (unsigned long i = 0; i < m_nSize; i++)
       m_Data[i] = newVal;
   }

   unsigned long GetNumEntries()  // return the number of entries (up to m_nSize)
   {
     if (m_nCurrent < m_nSize)
       return m_nCurrent;
     else
       return m_nSize;
   }
};

class SimpleAverage
{
private:
  unsigned long m_nSize;
  double m_Avg;

public:
  SimpleAverage(){Reset();};
  ~SimpleAverage(){};

  void Reset(){m_nSize = 0; m_Avg = 0.0;}
  void Reset(double val)
  {
    m_nSize = 1;
    m_Avg = val;
  }
  void Add(double val)
  {
    double tot = m_Avg * m_nSize + val;
    m_nSize++;
    m_Avg = tot / m_nSize;
  }
  
  double GetAverage(){return m_Avg;}
};

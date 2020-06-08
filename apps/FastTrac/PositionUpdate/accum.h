// Accum: used to accumulate a series of values and trigger
// and action when a limit has been passed.
//
//  The Add function returns true if the accumulated total exceeds the
//  cutoff.  If it does the accumulated total is returned to 0.
//  m_Total is the overall total - it is never reset unless Reset is called

class Accum
{
private:
  double m_Total;
  double m_curTotal;
  double m_cutoff;
public:
  Accum() : m_curTotal(0.0), m_cutoff(1000.0){}
  Accum(double cutoff) : m_curTotal(0.0), m_cutoff(cutoff){}

  void Setup(double cutoff){m_cutoff = cutoff; Reset();};  
  bool Add(double val)
  {
  	m_curTotal += fabs(val);
	  m_Total += fabs(val);
	
  	if (m_curTotal >= m_cutoff)
	  {
	    m_curTotal = 0.0;
  	  return true;
	  }
  	return false;
  };
  
  void Reset(){m_curTotal = 0; m_Total = 0;};
  double GetOverallTotal(){return m_Total;};
  double GetCurTotal(){return m_curTotal;};
};

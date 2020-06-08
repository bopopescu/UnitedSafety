#include "utility.h"
#include <sys/stat.h> 

bool FileExists(const char *strFilename)
{
  struct stat stFileInfo;
  bool blnReturn;
  int intStat;

  // Attempt to get the file attributes
  intStat = stat(strFilename, &stFileInfo);
  
  if(intStat == 0) 
  {
    // We were able to get the file attributes
    // so the file obviously exists.
    blnReturn = true;
  }
  else 
  {
    // We were not able to get the file attributes.
    // This may mean that we don't have permission to
    // access the folder which contains this file. If you
    // need to do that level of checking, lookup the
    // return values of stat which will give you
    // more details on why stat failed.
    blnReturn = false;
  }
  
  return(blnReturn);
}


void ParseCSV(vector<string> &record, const string& line, char delimiter)
{
  int linepos=0;
  int inquotes=false;
  char c;
  int linemax=line.length();
  string curstring;
  record.clear();
     
  while(line[linepos]!=0 && linepos < linemax)
  {
    c = line[linepos];
     
    if (!inquotes && curstring.length()==0 && c=='"')
    {
      //beginquotechar
      inquotes=true;
    }
    else if (inquotes && c=='"')
    {
      //quotechar
      if ( (linepos+1 <linemax) && (line[linepos+1]=='"') )
      {
        //encountered 2 double quotes in a row (resolves to 1 double quote)
        curstring.push_back(c);
        linepos++;
      }
      else
      {
        //endquotechar
        inquotes=false;
      }
    }
    else if (!inquotes && c==delimiter)
    {
      //end of field
      record.push_back( curstring );
      curstring="";
    }
    else if (!inquotes && (c=='\r' || c=='\n') )
    {
      record.push_back( curstring );
      return;
    }
    else
    {
      curstring.push_back(c);
    }
    linepos++;
  }
  record.push_back( curstring );
  return;
}

//=================================================================================================
// StripCRLF - removes trailing CR/LF until 0 string length or
//             non CRLF in last char.  Returns the pointer to the 
//             string so it can be used in place e.g printf("%s", StripCRLF(str))
// NOTE - MODIFIES the string in place - does not make a copy!
char * StripCRLF(char * str)
{
	while ( strlen(str) )
	{
		char *p = str + strlen(str) - 1;
		if (*p == 0x0A || *p == 0x0D)
			*p = 0x00;
		else
			return str;
	}
	return str;
}

//---------------------------------------------------------------------------------------
// Do whatever it takes to determine if the internet is available
//  First check that we have a gateway (/tmp/ramdisk/inet.good exists)
//  Then we check that a ping to google gets through.
//
bool IsInternetAvailable()
{
//	if (!FileExists("/tmp/ramdisk/inet.good")	)
//		return false;

	system("/usr/bin/testping");

	if (!FileExists("/tmp/ramdisk/ping.good")	)
		return false;

	return true;
}

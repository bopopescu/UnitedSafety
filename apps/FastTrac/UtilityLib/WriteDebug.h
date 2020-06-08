
#pragma once

void WriteDebug(const char *src, const int line);
void WriteDebug(const char *src, const int line, const char *str);
void WriteDebug(const char *src, const int line, const char *str, int ival);
void WriteDebug(const char *src, const int line, const char *str, unsigned long ival);
void WriteDebug(const char *src, const int line, const char *str, double dval);
void WriteDebug(const char *src, const int line, const char *str, const char *strval);
void WriteDebugHex(const char *src, const int line, const char *data, short len);
void WriteDebugHex2(const char *data, short len);  // logs to debug.hex only
void EraseDebug();
void CheckDebugFileSize(); // if > 500K restart debug file.
void WriteDebugString(const char *str);
void WriteDebugString(const char *str, int ival);
void WriteDebugString(const char *str, const char * strval);
void WriteDebugString(const char *str1, const char *str2, const char * str3);


#define WDB WriteDebug(__FILE__, __LINE__);


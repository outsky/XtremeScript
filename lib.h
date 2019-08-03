#ifndef lib_h
#define lib_h

void fatal(const char *func, int line, char *msg);

#define FATAL(msg) fatal(__FUNCTION__, __LINE__, msg)

const char* snapshot(const char* code, int pos, int count);

#endif

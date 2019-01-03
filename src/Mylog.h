/*
 * Mylog.h
 *
 *  Created on: Sep 18, 2017
 *      Author: gyd
 */
#pragma once
#ifndef MYLOG_H_
#define MYLOG_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>   // fopen

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <string.h>   // strlen strerror
#include <errno.h>    // errno
#include <sys/stat.h> // mkdir stat
#ifdef WINVER
#include <direct.h>    // _mkdir
#endif
// my head files
//#include "sql_conn_cpp.h"
#include "getDate.h"
using namespace std;
/*
 * lock
 */
class Mylog {
public:
	Mylog();
	Mylog(const char *filename);
	~Mylog();
	void setLogFile(const char *filename);
	void setMaxFileSize(long maxsize);
	int logException(const string& logMsg);
	int logException(const unsigned char * logMsg, int length);  // mainly for log hexadecimal
//	int logException(sql::SQLException &e, const char* file, const char* func, const int& line);
	int checkSize();
	int shrinkLogFile();

	string mstr_logfile;
private:
	unsigned long m_filesize;
	long max_filesize;


};


#endif /* MYLOG_H_ */

/*
 * Mytimer.h
 *
 *  Created on: Nov 25, 2017
 *      Author: gyd
 */

#ifndef SRC_MYTIMER_H_
#define SRC_MYTIMER_H_
#include <time.h>
#ifdef WINVER
#include <windows.h>
#endif
#include <sys/timeb.h>
class Mytimer {
public:
	Mytimer();
	~Mytimer();

	void start();
	long long stop();

private:
	long long  m_timeBegin,m_timeEnd;
	long long  elapsed_time;
};

#endif /* SRC_MYTIMER_H_ */

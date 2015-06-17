// Lab 7: return a timestamp according to qemu system time (Host's GMT+0 time)
// the timestamp counts from 2000/01/01 00:00:00

#include <inc/lib.h>

#define SEC_PER_MIN	60
#define SEC_PER_HOUR	3600
#define SEC_PER_DAY	86400
#define SEC_PER_MONTH	2678400
#define SEC_PER_YEAR	32140800 

void
umain(int argc, char **argv)
{
	int ts = sys_get_time();

	cprintf("GMT+0 20%02d/%02d/%02d %02d:%02d:%02d\n", 
		(ts/SEC_PER_YEAR), (ts/SEC_PER_MONTH)%12, (ts/SEC_PER_DAY)%31, 
		(ts/SEC_PER_HOUR)%24, (ts/SEC_PER_MIN)%60, ts%60);

	cprintf("Timestamp: %d\n", ts);
}

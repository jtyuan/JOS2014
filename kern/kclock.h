/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_KCLOCK_H
#define JOS_KERN_KCLOCK_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#define SEC_PER_MIN	60
#define MIN_PER_HOUR 	60
#define HOUR_PER_DAY	24
#define DAY_PER_MONTH	31
#define MONTH_PER_YEAR	12

#define	IO_RTC		0x070		/* RTC port */

#define	MC_NVRAM_START	0xe	/* start of NVRAM: offset 14 */
#define	MC_NVRAM_SIZE	50	/* 50 bytes of NVRAM */

#define RTC_SECOND	0x00	/* 00 - 59 (bcd) */
#define RTC_MINUTE	0x02	/* 00 - 59 (bcd) */
#define RTC_HOUR	0x04	/* 00 - 23 (bcd) */
#define RTC_DAYOFMONTH	0x07	/* 01 - 31 (bcd) */
#define RTC_MONTH 	0x08	/* 01 - 12 (bcd) */
#define RTC_YEAR	0x09    /* 00 - 99 (bcd) */

/* NVRAM bytes 7 & 8: base memory size */
#define NVRAM_BASELO	(MC_NVRAM_START + 7)	/* low byte; RTC off. 0x15 */
#define NVRAM_BASEHI	(MC_NVRAM_START + 8)	/* high byte; RTC off. 0x16 */

/* NVRAM bytes 9 & 10: extended memory size */
#define NVRAM_EXTLO	(MC_NVRAM_START + 9)	/* low byte; RTC off. 0x17 */
#define NVRAM_EXTHI	(MC_NVRAM_START + 10)	/* high byte; RTC off. 0x18 */

/* NVRAM bytes 34 and 35: extended memory POSTed size */
#define NVRAM_PEXTLO	(MC_NVRAM_START + 34)	/* low byte; RTC off. 0x30 */
#define NVRAM_PEXTHI	(MC_NVRAM_START + 35)	/* high byte; RTC off. 0x31 */

/* NVRAM byte 36: current century.  (please increment in Dec99!) */
#define NVRAM_CENTURY	(MC_NVRAM_START + 36)	/* RTC offset 0x32 */

unsigned mc146818_read(unsigned reg);
void mc146818_write(unsigned reg, unsigned datum);
unsigned bcd2dec(unsigned datum);

#endif	// !JOS_KERN_KCLOCK_H

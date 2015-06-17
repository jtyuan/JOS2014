// Lab 7: list/filter snapshot according to time
// Usage:
// ssfilter [-Fl(a/b)] [path] [year] [month] [day] [hour]
// 	F: add slash after dirs
// 	l: list detail information
// 	a: above(after) given time
// 	b: below(before) given time
//
//	year: 	[2000, 2099]
//	month: 	[1, 12]
// 	day: 	[1, 31]
//	hour:	[0, 23]


#include <inc/lib.h>

#define SEC_PER_MIN	60
#define SEC_PER_HOUR	3600
#define SEC_PER_DAY	86400
#define SEC_PER_MONTH	2678400
#define SEC_PER_YEAR	32140800 

int req_ts;
int flag[256];

void ssdir(const char*, const char*);
void ss1(const char*, bool, bool, off_t, const char*);
bool is_snapshot(const char *name);
int  extract_date(const char *name);
void cat_path(char *dst, const char *src);
bool in_range(int ts);

void
ss(const char *path, const char *prefix)
{
	int r;
	struct Stat st;

	if ((r = stat(path, &st)) < 0) {
		cprintf("stat %s: %e\n", path, r);
		exit();
	}
	if (st.st_isdir)
		ssdir(path, prefix);
	else
		ss1(0, st.st_isdir, st.st_islink, st.st_size, path);
}

void
ssdir(const char *path, const char *prefix)
{
	int fd, n;
	struct File f;

	if ((fd = open(path, O_RDONLY)) < 0) {
		cprintf("open %s: %e\n", path, fd);
		exit();
	}
	while ((n = readn(fd, &f, sizeof f)) == sizeof f)
		if (is_snapshot(f.f_name) && in_range(extract_date(f.f_name)))
			ss1(prefix, f.f_type==FTYPE_DIR, f.f_type==FTYPE_LNK, f.f_size, f.f_name);
	if (n > 0)
		panic("short read in directory %s", path);
	if (n < 0)
		panic("error reading directory %s: %e", path, n);
}

void
ss1(const char *prefix, bool isdir, bool islink, off_t size, const char *name)
{
	const char *sep;
	int ts;

	flag['C']++;

	if(flag['l'])
		printf("%11d %c ", size, isdir ? 'd' : (islink ? 'l' : '-'));
	if(prefix) {
		if (prefix[0] && prefix[strlen(prefix)-1] != '/')
			sep = "/";
		else
			sep = "";
		printf("%s%s", prefix, sep);
	}
	printf("%s", name);
	if(flag['F'] && isdir)
		printf("/");

	if ((ts = extract_date(name)) > 0) {
		cprintf("(GMT+0 20%02d/%02d/%02d %02d:%02d:%02d)", 
		(ts/SEC_PER_YEAR), (ts/SEC_PER_MONTH)%12, (ts/SEC_PER_DAY)%31, 
		(ts/SEC_PER_HOUR)%24, (ts/SEC_PER_MIN)%60, ts%60);
	}
	printf("\n");
}

// judge whether ts is in the requested range
// flag['a'] > 0: above or equal to req_ts
// flag['b'] > 0: below or equal to req_ts
// flag['a'] == 0 && flag['a'] == 0: ts equals to req_ts
bool in_range(int ts)
{
	if (!req_ts)
		return true;

	if (flag['h'])
		ts = ts - ts % SEC_PER_HOUR;
	else if (flag['d'])
		ts = ts - ts % SEC_PER_DAY;
	else if (flag['m'])
		ts = ts - ts % SEC_PER_MONTH;
	else if (flag['y'])
		ts = ts - ts % SEC_PER_YEAR;

	// cprintf("%d %d\n", req_ts, ts);

	if (flag['a'] && ts < req_ts)
		return false;
	if (flag['b'] && ts > req_ts)
		return false;
	// if no a/b/e, then default as strict equavalence
	if (!(flag['a'] || flag['b']) && ts != req_ts)
		return false;

	return true;
}

// extract the timestamp string from name
// return the int type of the timestamp
int 
extract_date(const char *name)
{
	char *pos = strrchr(name, '@');
	pos++;
	return atoi(pos);
}

// concatenate 2 strings to form an absolute path of the fs
void
cat_path(char *dst, const char *src)
{
	if (dst[strlen(dst)-1] != '/' && src[0] != '/')
		strcat(dst, "/");
	strcat(dst, src);
}

void
usage(void)
{
	// a = after, b = before, e = equal
	printf("usage: ssfilter [-Fl(a/b)] [path] [year] [month] [day] [hour] \n");
	exit();
}

void
umain(int argc, char **argv)
{
	int i, year, month, day, hour;
	struct Argstate args;
	char path[256];
	
	argstart(&argc, argv, &args);
	while ((i = argnext(&args)) >= 0)
		switch (i) {
		case 'F':
		case 'l':
		case 'a':
		case 'b':
			flag[i]++;
			break;
		default:
			usage();
		}

	if (flag['a'] && flag['b'])
		usage();

	year = 0;
	month = 0;
	day = 0;
	hour = 0;

	if (argc > 2) {
		year = atoi(argv[2]) % 100;
		flag['y']++;
	}
	if (argc > 3) {
		month = atoi(argv[3]) % 12;
		if (!month)
			month = 12;
		flag['m']++;
	}
	if (argc > 4) {
		day = atoi(argv[4]) % 31;
		if (!day)
			day = 31;
		flag['d']++;
	}
	if (argc > 5) {
		hour = atoi(argv[5]) % 24;
		flag['h']++;
	}

	req_ts = year * SEC_PER_YEAR + month * SEC_PER_MONTH
		+ day * SEC_PER_DAY + hour * SEC_PER_HOUR;

	// cprintf("requested ts: %d in path: %s\n", req_ts, argv[1]);

	if (argc == 1)
		ss("/", "");
	else {
		ss(argv[1], argv[1]);
	}

	if (flag['C'] == 0) {
		if (argc == 1) {
			argv[1][0] = '/';
			argv[1][1] = '\0';
		}
		cprintf("No Snapshot under %s matching the given time\n", argv[1]);
	}
}


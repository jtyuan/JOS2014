#include <inc/lib.h>

int flag;
int recur;
int verbose;

void ssdir(const char*, const char*);
void ss1(const char*, bool, off_t, const char*, const char*);
bool is_snapshot(const char*);
void cat_path(char *dst, const char *src);

void
snapshot(const char *path, const char *ts)
{
	int r;
	struct Stat st;

	if ((r = stat(path, &st)) < 0)
		panic("stat %s: %e", path, r);
	if (st.st_isdir && flag != 'n')
		ssdir(path, ts);
	else
		ss1(0, st.st_isdir, st.st_size, path, ts);
}

void
ssdir(const char *path, const char *ts)
{
	int fd, n, r;
	struct File f;
	char buf[256];

	// cprintf("%s\n", path);

	if ((fd = open(path, O_RDONLY)) < 0)
		panic("open %s: %e", path, fd);
	while ((n = readn(fd, &f, sizeof f)) == sizeof f)
		if (f.f_name[0] && !is_snapshot(f.f_name)) {
			ss1(path, f.f_type==FTYPE_DIR, f.f_size, f.f_name, ts);
			if (recur && f.f_type == FTYPE_DIR) {
				strcpy(buf, path);
				cat_path(buf, f.f_name);
				ssdir(buf, ts);
			}
		}
	if (n > 0)
		panic("short read in directory %s", path);
	if (n < 0)
		panic("error reading directory %s: %e", path, n);
}

void
ss1(const char *prefix, bool isdir, off_t size, const char *name, const char *ts)
{
	int r, rfd, wfd, lfd;
	size_t offset, len;
	size_t old_offset, old_len;
	struct Stat st;
	char src_path[256] = "\0";
	char dst_path[256], old_path[256];
	if (prefix)
		strcpy(src_path, prefix);
	cat_path(src_path, name);

	strcpy(dst_path, src_path);
	strcat(dst_path, "@");
	strcat(dst_path, ts);

	if (flag == 'n') {
		if (verbose) {
			if ((r = spawnl("/cp", "cp", "-rv", src_path, dst_path, (char*)0)) < 0)
				panic("snapshot: spawn /cp: %e", r);
		} else {
			if ((r = spawnl("/cp", "cp", "-r", src_path, dst_path, (char*)0)) < 0)
				panic("snapshot: spawn /cp: %e", r);
		}
		if (r > 0) {
			wait(r);
			cprintf("Snapshot finished: %s\n", dst_path);
		}
		return;
	}

	if (verbose)
		cprintf("%s\n", src_path);

	if (flag == 'c') {
		if ((rfd = open(src_path, O_RDONLY)) < 0) {
			cprintf("snapshot: open %s: %e\n", src_path, rfd);
			return;
		}
		if ((lfd = open(LINK_RECORD, O_RDWR)) < 0) {
			cprintf("snapshot: open %s: %e\n", LINK_RECORD, lfd);
			return;
		}
		if ((r = fstat(lfd, &st)) < 0)
			panic("stat %s: %e", LINK_RECORD, r);

		seek(lfd, st.st_size);

		offset = st.st_size;
		len = strlen(dst_path);

		if ((r = write(lfd, dst_path, len)) != len)
			panic("write %s: %e", LINK_RECORD, r);
		
		snap(rfd, offset, len, &old_offset, &old_len);

		if (old_offset != LINK_CLEAN && old_len != LINK_CLEAN) {
			seek(lfd, old_offset);
			read(lfd, old_path, old_len);
			if ((r = spawnl("/link", "link", dst_path, old_path, (char*)0)) < 0)
				panic("snapshot: spawn /link: %e", r);
		}

		close(rfd);
		closr(lfd);
	}
}

void
cat_path(char *dst, const char *src)
{
	if (dst[strlen(dst)-1] != '/' && src[0] != '/')
		strcat(dst, "/");
	strcat(dst, src);
}

bool is_snapshot(const char *name)
{
	char *pos = strrchr(name, '@');
	if (pos == NULL)
		return false;
	while (*(++pos)) {
		if (!isdigit(*pos))
			return false;
	}
	return true;
}


void
usage(void)
{
	// naïve/cow/incremental
	printf("usage: snapshot [-(n|c|i)rv] [file...]\n");
	exit();
}

void
umain(int argc, char **argv)
{
	int i;
	struct Argstate args;
	char ts[32];

	itoa(sys_get_time(), ts, 10);

	flag = 'n';
	recur = 0;
	verbose = 0;

	argstart(&argc, argv, &args);
	while ((i = argnext(&args)) >= 0)
		switch (i) {
		case 'n':
		case 'c':
		case 'i':
			flag = i;
			break;
		case 'r':
			recur = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
		}

	if (argc == 1) 
		snapshot("/", ts);
	else {
		for (i = 1; i < argc; i++)
			snapshot(argv[i], ts);
	}
}

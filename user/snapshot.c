#include <inc/lib.h>

int flag;
int recur;
int verbose;
int debug;

char src_path[1024];
char dst_path[1024], old_path[1024];

void ssdir(const char*, const char*);
void ss1(const char*, bool, bool, off_t, const char*, const char*);
bool is_snapshot(const char*);
void cat_path(char *dst, const char *src);

void
snapshot(const char *path, const char *dst)
{
	int r;
	struct Stat st;

	if ((r = stat(path, &st)) < 0) {
		cprintf("stat %s: %e\n", path, r);
		exit();
	}
	if (st.st_isdir && flag != 'n')  {
		if ((r = spawnl("/mkdir", "mkdir", dst, (char*)0)) < 0) {
			cprintf("spawn %s: %e\n", "mkdir", r);
			return;
		}
		if (r >= 0)
			wait(r);
		ssdir(path, dst);
	}
	else
		ss1(path, st.st_isdir, st.st_islink, st.st_size, 0, dst);
}

void
ssdir(const char *path, const char *dst)
{
	char buf[64], buf_pre[64];
	int fd, n, r;
	struct File f;

	if (debug) {
		cprintf("\n-----------------------------\n");
		cprintf("%s %s\n", path, dst);
	}

	if ((fd = open(path, O_RDONLY)) < 0) {
		cprintf("open %s: %e\n", path, fd);
		exit();
	}
	// cprintf("survived\n");
	while ((n = readn(fd, &f, sizeof f)) == sizeof f)
		// cprintf("%s\n", f.f_name);
		if (f.f_name[0] && !is_snapshot(f.f_name)) {
			if (debug)
				cprintf("%s\n", f.f_name);
			ss1(path, f.f_type==FTYPE_DIR, f.f_type==FTYPE_LNK, f.f_size, f.f_name, dst);
			if (recur && f.f_type == FTYPE_DIR) {
				strcpy(buf, path);
				cat_path(buf, f.f_name);
				strcpy(buf_pre, dst);
				cat_path(buf_pre, f.f_name);
				ssdir(buf, buf_pre);
			}
		}
	if (n > 0)
		panic("short read in directory %s", path);
	if (n < 0)
		panic("error reading directory %s: %e", path, n);
	close(fd);
}

void
ss1(const char *path, bool isdir, bool islink, off_t size, const char *name, const char *dst)
{

	if (islink)
		return;

	int r, rfd, wfd, lfd;
	size_t offset, len;
	size_t old_offset, old_len;
	struct Stat st;
	
	if (path)
		strcpy(src_path, path);
	if (name)
		cat_path(src_path, name);

	if (dst)
		strcpy(dst_path, dst);
	if (name)
		cat_path(dst_path, name);

	// cprintf("%s %s\n", src_path, dst_path);

	if (flag == 'n') {
		if (debug) {
			cprintf("DEBUG MODE: NAIVE SNAPSHOT\n");
			if ((r = spawnl("/cp", "cp", "-rvd", src_path, dst_path, (char*)0)) < 0) {
				cprintf("snapshot: spawn /cp: %e\n", r);
				exit();
			}
		} else if (verbose) {
			if ((r = spawnl("/cp", "cp", "-rv", src_path, dst_path, (char*)0)) < 0) {
				cprintf("snapshot: spawn /cp: %e\n", r);
				exit();
			}
		} else {
			if ((r = spawnl("/cp", "cp", "-r", src_path, dst_path, (char*)0)) < 0) {
				cprintf("snapshot: spawn /cp: %e\n", r);
				exit();
			}
		}
		if (r > 0)
			wait(r);
		return;
	}

	if (isdir) {
		if ((r = spawnl("/mkdir", "mkdir", dst_path, (char*)0)) < 0) {
			cprintf("spawn %s: %e\n", "mkdir", r);
			return;
		}
		if (r >= 0)
			wait(r);
		return;
	}

	if (flag == 'c') {
		if ((rfd = open(src_path, O_RDONLY)) < 0) {
			cprintf("snapshot: open %s: %e\n", src_path, rfd);
			return;
		}
		if ((lfd = open(LINK_RECORD, O_RDWR | O_CREAT)) < 0) {
			cprintf("snapshot: open %s: %e\n", LINK_RECORD, lfd);
			return;
		}
		if ((r = fstat(lfd, &st)) < 0) {
			cprintf("stat %s: %e\n", LINK_RECORD, r);
			exit();
		}

		seek(lfd, st.st_size);

		offset = st.st_size;
		len = strlen(dst_path);

		if ((r = write(lfd, dst_path, len)) != len) {
			cprintf("write %s: %e\n", LINK_RECORD, r);
			exit();
		}

		snap(rfd, offset, len, &old_offset, &old_len);

		if ((r = spawnl("/link", "link", src_path, dst_path, (char*)0)) < 0) {
			cprintf("snapshot: spawn /link: %e\n", r);
			exit();
		}
		if (r >= 0) {
			wait(r);
			if (verbose)
				cprintf("Snapshot file made: %s\n", dst_path);

		}

		if (old_len != FILE_CLEAN) {
			seek(lfd, old_offset);
			read(lfd, old_path, old_len);
			old_path[old_len] = '\0';
			if (debug)
				cprintf("Redirecting old link: %s(len:%d)\n", old_path, old_len);
	
			if ((r = spawnl("/link", "link", dst_path, old_path, (char*)0)) < 0) {
				cprintf("snapshot: spawn /link: %e\n", r);
				exit();
			}
			if (r >= 0)
				wait(r);
		}

		close(rfd);
		close(lfd);
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
	// naïve/cow
	printf("usage: snapshot [-(n|c)rdv] [file...]\n");
	exit();
}

void
umain(int argc, char **argv)
{
	int i;
	struct Argstate args;
	char ts[32];
	char prefix[256];

	itoa(sys_get_time(), ts, 10);

	flag = 'c';
	recur = 0;
	verbose = 1;
	debug = 0;

	argstart(&argc, argv, &args);
	while ((i = argnext(&args)) >= 0)
		switch (i) {
		case 'n':
		case 'c':
			flag = i;
			break;
		case 'r':
			recur = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'd':
			debug = 1;
			break;
		default:
			usage();
		}

	if (argc == 1) {
		prefix[0] = '\0';
		strcpy(prefix, "/");
		strcat(prefix, "@");
		strcat(prefix, ts);
		snapshot("/", prefix);
	}
	else {
		for (i = 1; i < argc; i++) {
			prefix[0] = '\0';
			strcpy(prefix, argv[i]);
			strcat(prefix, "@");
			strcat(prefix, ts);
			snapshot(argv[i], prefix);
		}
	}
	cprintf("Snapshot finished: %s\n", prefix);
}
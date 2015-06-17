// Lab 7: main entry 
// Usage:
// snapshot [-(n|c)rdv] [file...]
// 	   n: naive method (Split-Mirror)
//     c: Copy-on-Write
//     r: recursive
//     d: display debug info
//     v: verbose mode
//

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

// Make snapshot for a directory
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
	
	while ((n = readn(fd, &f, sizeof f)) == sizeof f)
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

// Make a snapshot for file path/name, save it to dst/name
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
		// naive/split-mirror strategy
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
		// simply make a new dir for dir files
		if ((r = spawnl("/mkdir", "mkdir", dst_path, (char*)0)) < 0) {
			cprintf("spawn %s: %e\n", "mkdir", r);
			return;
		}
		if (r >= 0)
			wait(r);
		return;
	}

	if (flag == 'c') {
		// copy-on-write strategy
		if ((rfd = open(src_path, O_RDONLY)) < 0) {
			cprintf("snapshot: open %s: %e\n", src_path, rfd);
			return;
		}

		// read the file that saves all the cow-links
		if ((lfd = open(LINK_RECORD, O_RDWR | O_CREAT)) < 0) {
			cprintf("snapshot: open %s: %e\n", LINK_RECORD, lfd);
			return;
		}
		if ((r = fstat(lfd, &st)) < 0) {
			cprintf("stat %s: %e\n", LINK_RECORD, r);
			exit();
		}

		// get the offset and len for the new cow-link
		seek(lfd, st.st_size);
		offset = st.st_size;
		len = strlen(dst_path);
		if ((r = write(lfd, dst_path, len)) != len) {
			cprintf("write %s: %e\n", LINK_RECORD, r);
			exit();
		}

		// tell fs to save cow-link in (offset, len) pair
		snap(rfd, offset, len, &old_offset, &old_len);

		// make a link between the original one and the cow-file
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
			// redirect old cow-link that was still linking to the original file
			// to the new cow-file, so that the disk space is free from such duplicates
			// (only one copy for a sequence of unmodified snapshots)

			// read the path of the old cow-file
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
	verbose = 0;
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

#include <inc/lib.h>

int recur;
int verbose;
int debug;

char src_file[1024], dst_file[1024];
char src_buf[1024];

void cp1(const char *src_path, const char *dst_path, const char *f_name);
void cpdir(const char *src_path, const char *dst_path);
bool is_snapshot(const char*);
void cat_path(char *dst, const char *src);

void
cp(const char *src_path, char *dst_path)
{
	int rfd, wfd, n, r;
	struct File f;
	struct Stat rst, wst;

	rfd = -1;
	wfd = -1;

	// strcpy(dst_path, dst_path_in);
	// cprintf("%s %s\n", src_path, dst_path);

	if ((rfd = open(src_path, O_RDONLY)) < 0) {
		cprintf("open %s: %e\n", src_path, rfd);
		goto end;
	}

	if ((r = fstat(rfd, &rst)) < 0) {
		cprintf("stat %s: %e\n", src_path, r);
		goto end;
	}


	if (rst.st_isdir) {
		if (!recur) {
			cprintf("cp: %s is a directory (not copied).\n", src_path);
			goto end;
		}
		if ((wfd = open(dst_path, O_RDWR | O_MKDIR | O_EXCL)) < 0) {
			// cprintf("open %s: %e\n", dst_path, wfd);
			// return;
			if (wfd == -E_FILE_EXISTS) {
				cat_path(dst_path, rst.st_name);
				if ((wfd = open(dst_path, O_RDWR | O_MKDIR | O_TRUNC)) < 0) {
					cprintf("open %s: %e\n", dst_path, wfd);
					goto end;
				}
			}
		}
		
		if ((r = fstat(wfd, &wst)) < 0) {
			cprintf("stat %s: %e\n", dst_path, r);
			goto end;
		}
		if (!wst.st_isdir)
			cprintf("cp: %s: Not a directory\n", dst_path);

		cpdir(src_path, dst_path);

	} else {
		if ((wfd = open(dst_path, O_WRONLY | O_CREAT)) < 0) {
			cprintf("open %s: %e\n", dst_path, wfd);
			goto end;
		}
		if ((r = fstat(wfd, &wst)) < 0) {
			cprintf("stat %s: %e\n", dst_path, r);
			goto end;
		}
		if (wst.st_isdir)
			cp1(src_path, dst_path, rst.st_name);
		else
			cp1(src_path, dst_path, 0);
	}

end:
	if (rfd > 0)
		close(rfd);
	if (wfd > 0)
		close(wfd);
}

void
cpdir(const char *src_path, const char *dst_path)
{
	char src_buf[64], dst_buf[64];
	int fd, n, r;
	struct File f;
	
	if (debug) {
		cprintf("\n-----------------------------\n");
		cprintf("%s %s\n", src_path, dst_path);
	}

	if ((fd = open(src_path, O_RDONLY)) < 0) {
		cprintf("open %s: %e\n", src_path, fd);
		exit();
	}

	while ((n = readn(fd, &f, sizeof f)) == sizeof f) {
		if (f.f_name[0] && !is_snapshot(f.f_name)) {
			if (debug)
				cprintf("%s\n", f.f_name);
			if (f.f_type == FTYPE_DIR) {
				src_buf[0] = '\0';
				dst_buf[0] = '\0';
				strcpy(src_buf, src_path);
				strcpy(dst_buf, dst_path);
				if (debug) {
					cprintf("_1%s %s\n", src_buf, dst_buf);
					cprintf("_s%s %s\n", src_path, dst_path);
				}
				cat_path(src_buf, f.f_name);
				cat_path(dst_buf, f.f_name);
				if (debug) {
					cprintf("_2%s %s\n", src_buf, dst_buf);
					cprintf("_s%s %s\n", src_path, dst_path);
				}
				if ((r = spawnl("/mkdir", "mkdir", dst_buf, (char*)0)) < 0)
					return;
				if (r >= 0)
					wait(r);

				if (f.f_size > 0)
					cpdir(src_buf, dst_buf);
			} else {
				strcpy(src_file, src_path);
				cat_path(src_file, f.f_name);
				cp1(src_file, dst_path, f.f_name);
			}
		}
	}

	close(fd);
}

void
cp1(const char *src_file, const char *dst_path, const char *f_name) 
{
	int rfd, wfd, r, n;
	
	dst_file[0] = '\0';
	strcpy(dst_file, dst_path);
	if (f_name)
		cat_path(dst_file, f_name);

	if (verbose)
		cprintf("Copying from %s to %s\n", src_file, dst_file);
	if ((rfd = open(src_file, O_RDONLY)) < 0) {
		cprintf("open %s: %e\n", src_file, rfd);
		return;
	}
	if ((wfd = open(dst_file, O_WRONLY | O_CREAT)) < 0) {
		cprintf("open %s: %e\n", dst_file, wfd);
		return;
	}
	while ((n = read(rfd, src_buf, sizeof(src_buf))) > 0)
		if ((r = write(wfd, src_buf, n)) != n) {
			cprintf("write error copying %s to %s: %e\n", src_file, dst_file, r);
			exit();
		}
	if (n < 0)
		panic("error reading %s: %e", src_file, n);
	close(rfd);
	close(wfd);
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
cat_path(char *dst, const char *src)
{
	if (dst[strlen(dst)-1] != '/' && src[0] != '/')
		strcat(dst, "/");
	strcat(dst, src);
}

void
usage(void)
{
	printf("usage: cp [-rv] source_file buf_dst\n");
	exit();
}


void
umain(int argc, char **argv)
{
	int i;
	struct Argstate args;

	recur = 0;
	verbose = 0;
	debug = 0;

	argstart(&argc, argv, &args);
	while ((i = argnext(&args)) >= 0)
		switch (i) {
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


	if (argc < 3)
		usage();
	else {
		cp(argv[1], argv[2]);
	}
}

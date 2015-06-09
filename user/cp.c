#include <inc/lib.h>

int recur;
char buf[8196];

void cp1(const char *src_path, const char *dst_path, bool isdir, const char *f_name);
void cat_path(char *dst, const char *src);

void
cp(const char *src_path, const char *dst_path_in)
{
	int rfd, wfd, n, r;
	struct File f;
	struct Stat rst, wst;
	char buf[256];
	char dst_path[256];

	strcpy(dst_path, dst_path_in);

	if ((rfd = open(src_path, O_RDONLY)) < 0) {
		cprintf("open %s: %e\n", src_path, rfd);
		return;
	}


	if ((r = fstat(rfd, &rst)) < 0)
		panic("stat %s: %e", src_path, r);

	if (rst.st_isdir) {
		if (!recur) {
			cprintf("cp: %s is a directory (not copied).\n", src_path);
			return;
		}
		if ((wfd = open(dst_path, O_WRONLY | O_MKDIR)) < 0) {
			cprintf("open %s: %e\n", dst_path, wfd);
			return;
		}
		if ((r = fstat(wfd, &wst)) < 0)
			panic("stat %s: %e", dst_path, r);
		if (!wst.st_isdir) {
			cprintf("cp: %s: Not a directory\n", dst_path);
		}
		// both src and dst are dir
		while ((n = readn(rfd, &f, sizeof f)) == sizeof f) 
			if (f.f_name[0]) {
				cp1(src_path, dst_path, f.f_type==FTYPE_DIR, f.f_name);
				if (f.f_type == FTYPE_DIR) {
					strcpy(buf, src_path);
					strcat(buf, f.f_name);
					cp(buf, dst_path);
				}
			}
	} else {
		if ((wfd = open(dst_path, O_WRONLY | O_CREAT)) < 0) {
			cprintf("open %s: %e\n", dst_path, wfd);
			return;
		}
		if ((r = fstat(wfd, &wst)) < 0)
			panic("stat %s: %e", dst_path, r);
		if (wst.st_isdir) {
			close(wfd);
			cat_path(dst_path, rst.st_name);
		}
		cp1(src_path, dst_path, 0, 0);
	}
	close(rfd)
;	close(wfd);
}

void
cp1(const char *src_path_, const char *dst_path_, bool isdir, const char *f_name) 
{
	int rfd, wfd, r, n;
	char src_path[256];
	char dst_path[256];
	strcpy(src_path, src_path_);
	strcpy(dst_path, dst_path_);
	if (f_name) {
		cat_path(src_path, f_name);
		cat_path(dst_path, f_name);
	}

	if (isdir) {
		spawnl("/mkdir", dst_path);
	} else {
		if ((rfd = open(src_path, O_RDONLY)) < 0) {
			cprintf("open %s: %e\n", src_path, rfd);
			return;
		}
		if ((wfd = open(dst_path, O_WRONLY | O_CREAT)) < 0) {
			cprintf("open %s: %e\n", dst_path, wfd);
			return;
		}
		while ((n = read(rfd, buf, sizeof(buf))) > 0)
			if ((r = write(wfd, buf, n)) != n)
				panic("write error copying %s to %s: %e", src_path, dst_path, r);
		if (n < 0)
			panic("error reading %s: %e", src_path, n);
	}
	
}

void
cat_path(char *dst, const char *src)
{
	if (dst[strlen(dst)-1] != '/')
		strcat(dst, "/");
	strcat(dst, src);
}

void
usage(void)
{
	printf("usage: cp [-r] source_file dst_file\n");
	exit();
}


void
umain(int argc, char **argv)
{
	int i;
	struct Argstate args;

	recur = 0;

	argstart(&argc, argv, &args);
	while ((i = argnext(&args)) >= 0)
		switch (i) {
		case 'r':
			recur = 1;
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

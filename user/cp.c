#include <inc/lib.h>

void cp(const char *src_path, const char *dst_path_in)
{
	int rfd, wfd, n, r;
	struct Stat st;
	char buf[256];
	char dst_path[256];

	strcpy(dst_path, dst_path_in);

	if ((rfd = open(src_path, O_RDONLY)) < 0) {
		cprintf("open %s: %e\n", src_path, rfd);
		return;
	}
	if ((wfd = open(dst_path, O_WRONLY | O_CREAT)) < 0) {
		cprintf("open %s: %e\n", dst_path, wfd);
		return;
	}


	if ((r = fstat(wfd, &st)) < 0)
		panic("stat %s: %e", dst_path_in, r);
	if (st.st_isdir) {
		close(wfd);
		if (dst_path[strlen(dst_path)-1] != '/')
			strcat(dst_path, "/");
		if ((r = fstat(rfd, &st)) < 0)
			panic("stat %s: %e", src_path, r);
		strcat(dst_path, st.st_name);
		if ((wfd = open(dst_path, O_WRONLY | O_CREAT)) < 0) {
			cprintf("open %s: %e\n", dst_path, wfd);
			return;
		}
	}
	
	while ((n = read(rfd, buf, sizeof(buf))) > 0)
		if ((r = write(wfd, buf, n)) != n)
			panic("write error copying %s to %s: %e", src_path, dst_path, r);
	if (n < 0)
		panic("error reading %s: %e", src_path, n);
	
	close(rfd)
;	close(wfd);
}

void
usage(void)
{
	printf("usage: cp source_file dst_file\n");
	exit();
}


void
umain(int argc, char **argv)
{
	if (argc < 3)
		usage();
	else {
		cp(argv[1], argv[2]);
	}
}

#include <inc/lib.h>

void link(const char *src_path, const char *dst_path)
{
	int wfd, n;
	if ((wfd = open(dst_path, O_WRONLY | O_EXCL | O_LINK)) < 0) { 
		cprintf("open %s: %e\n", dst_path, wfd);
		return;
	}
	
	write(wfd, src_path, strlen(src_path));
	
	close(wfd);

}

void
usage(void)
{
	printf("usage: link source_file target_file\n");
	exit();
}

void
umain(int argc, char **argv)
{
	if (argc < 3) 
		usage();
	else {
		link(argv[1], argv[2]);
	}
}


// Lab 7: link two files
// Usage:
// link source_file target_file
//		source_file contains actual data
// 		target_file will link to source_file


#include <inc/lib.h>

// link two files, after this, dst_path will be the link-file
// pointing at the src_path
void link(const char *src_path, const char *dst_path)
{
	int rfd, wfd, n, r;
	struct Stat st;

	if ((rfd = open(src_path, O_RDONLY)) < 0 ) {
		cprintf("open %s: %e\n", src_path, rfd);
	}

	if ((r = fstat(rfd, &st)) < 0)
		panic("stat %s: %e", src_path, r);
	close(rfd);

	if (st.st_isdir) {
		cprintf("link: %s is a directory.\n", src_path);

		return;
	}

	// creating the link-file
	if ((wfd = open(dst_path, O_TRUNC | O_WRONLY | O_LINK)) < 0) { 
		cprintf("open %s: %e\n", dst_path, wfd);
		return;
	}
	
	// save src_path as the content of link-file
	write(wfd, src_path, strlen(src_path));
	write(wfd, "\0", 1);
	
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


// Lab 7: create a directory

#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	int fd;
	
	binaryname = "mkdir";

	if (argc < 2) {
		cprintf("usage: mkdir path/to/dir\n");
		return;
	}

	if ((fd = open(argv[1], O_MKDIR | O_EXCL)) < 0) {
		cprintf("open %s: %e\n", argv[1], fd);
		return;
	}

	close(fd);
}

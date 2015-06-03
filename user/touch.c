#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	int rfd;
	char buf[512];

	if ((rfd = open(argv[1], O_WRONLY | O_CREAT)) < 0)
		panic("open %s: %e", argv[1], rfd);

	close(rfd);
}

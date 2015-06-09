#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	int rfd;

	if ((rfd = open(argv[1], O_WRONLY | O_CREAT)) < 0)
		panic("open %s: %e", argv[1], rfd);

	close(rfd);
}

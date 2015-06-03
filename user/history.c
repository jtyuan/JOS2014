#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	int rfd;
	char buf[512];
	int n;

	if ((rfd = open("/.history", O_RDONLY | O_CREAT)) < 0)
		panic("open /.history: %e", rfd);

	while ((n = read(rfd, buf, sizeof buf-1)) > 0)
		sys_cputs(buf, n);

	close(rfd);
}

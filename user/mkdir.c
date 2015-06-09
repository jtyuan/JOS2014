#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	int fd, fd2;

	if ((fd = open(argv[1], O_MKDIR)) < 0)
		panic("open %s: %e", argv[1], fd);

	close(fd);


	// char path[256];
	// strcpy(path, argv[1]);
	// if (path[strlen(path)-1] != '/')
	// 	strcat(path, "/");
	// strcat(path, ".pwd");

	// if ((fd2 = open(path, O_WRONLY | O_CREAT)) < 0)
	// 	panic("open %s: %e", path, fd);

	// write(fd2, argv[1], strlen(argv[1]));

	// close(fd2);
}

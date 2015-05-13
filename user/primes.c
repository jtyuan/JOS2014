// Concurrent version of prime sieve of Eratosthenes.
// Invented by Doug McIlroy, inventor of Unix pipes.
// See http://swtch.com/~rsc/thread/.
// The picture halfway down the page and the text surrounding it
// explain what's going on here.
//
// Since NENVS is 1024, we can print 1022 primes before running out.
// The remaining two environments are the integer generator at the bottom
// of main and user/idle.

#include <inc/lib.h>

unsigned
primeproc(void)
{
	int i, id, p;
	envid_t envid;

	// fetch a prime from our left neighbor
top:
// cprintf("fuckme1\n");
	p = ipc_recv(&envid, 0, 0);
	cprintf("CPU %d: %d ", thisenv->env_cpunum, p);
// cprintf("fuckme1.5\n");
	// fork a right neighbor to continue the chain
	if ((id = fork()) < 0)
		panic("fork: %e", id);
	if (id == 0)
		goto top;
// cprintf("fuckme2\n");
	// filter out multiples of our prime
	while (1) {
		i = ipc_recv(&envid, 0, 0);
		cprintf("???? %d\n", p);
		if (i % p)
			ipc_send(id, i, 0, 0);
	}
}

void
umain(int argc, char **argv)
{
	int i, id;

	// fork the first prime process in the chain
	if ((id = fork()) < 0)
		panic("fork: %e", id);
	if (id == 0)
		primeproc();

	// feed all the integers through
	for (i = 2; ; i++)
		ipc_send(id, i, 0, 0);
}


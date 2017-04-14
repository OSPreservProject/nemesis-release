/* udptelnet.c:  send a UDP packet for each line of input typed on stdin
 *  (c) 1997 Austin Donnelly, <and1000@cam.ac.uk>
 *
 * This program comes with NO WARRANTY.  Use at your own risk.  It may
 * be freely redistributed under the GNU Public Licence.
 *
 * $Id: udptelnet.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <errno.h>

#ifdef __alpha__
extern void herror(const char *s);
#endif

#define BUFSIZE  1024

#define DEF_SEND_PORT htons(4000)
#define ADDR_LEN sizeof(struct sockaddr_in)

#ifdef USE_PROMPT
#define PROMPT ":-> %s\n"
#else
#define PROMPT "%s"
#endif

void die(const char *m)
{
    printf("%s: %s\n", m, strerror(errno));
    exit(1);
}

void child(int fd, pid_t pid);
void parent(int fd, pid_t pid);

int main(int argc, char **argv)
{
    struct sockaddr_in	srcaddr, dstaddr;
    int			fd;
    int			port;
    struct hostent	*hent;
    pid_t		pid;

    if (argc < 2 || argc > 3)
    {
	printf("usage: %s <hostname> [ <portnumber> ]\n",
	       argv[0]);
	exit(2);
    }

    if (argc == 3)
	port = htons(atoi(argv[2]));
    else
	port = DEF_SEND_PORT;

    hent = gethostbyname(argv[1]);
    if (!hent)
    {
	herror("gethostbyname");
	exit(1);
    }

    srcaddr.sin_family = AF_INET;
    srcaddr.sin_port   = 0;
    srcaddr.sin_addr.s_addr= INADDR_ANY;

    dstaddr.sin_family = AF_INET;
    dstaddr.sin_port   = port;
    memcpy(&dstaddr.sin_addr.s_addr, hent->h_addr, 4);

    fd = socket(srcaddr.sin_family, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0)
	die("socket");

    if (bind(fd, (struct sockaddr *)&srcaddr, ADDR_LEN) < 0)
	die("bind");

    if (connect(fd, (struct sockaddr *)&dstaddr, ADDR_LEN) != 0)
	die("connect");

    printf("udptelnet: sending to %s:%d\n", argv[1], ntohs(port));

    /* fork to get async rx and tx */
    pid = fork();
    if (pid == -1)
	die("fork");

    if (pid == 0)
	child(fd, getppid());
    else
	parent(fd, pid);

    printf("can't happen!\n");
    return 0;
}

/* one child reads from stdin, and spits out to the fd */
void child(int fd, pid_t pid)
{
    char buf[1024];
    char *p;
    int nbytes, sent;

    for(;;)
    {
	p = fgets(buf, 1024, stdin);
	if (!p)
	{
	    printf("EOF on stdin, exiting\n");
	    kill(pid, SIGTERM);
	    exit(0);
	}
	
	nbytes = strlen(buf)+1;
	sent = send(fd, buf, nbytes, 0 /*flags*/);
	if (sent != nbytes)
	{
	    printf("short write on TX socket\n");
	    kill(pid, SIGTERM);
	    exit(0);
	}
    }
}

void parent(int fd, pid_t pid)
{
    char buf[1024];
    int nbytes;

    for(;;)
    {
	nbytes = recv(fd, buf, 1023, 0/*flags*/);
	buf[nbytes] = '\000';
	printf(PROMPT, buf);
	fflush(stdout);
    }
}

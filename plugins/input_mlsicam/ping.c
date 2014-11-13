
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/signal.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

/*
 * Note: on some systems dropping root makes the process dumpable or
 * traceable. In that case if you enable dropping root and someone
 * traces ping, they get control of a raw socket and can start
 * spoofing whatever packets they like. SO BE CAREFUL.
 */
#ifdef __linux__
#define SAFE_TO_DROP_ROOT
#endif

#if defined(__GLIBC__) && (__GLIBC__ >= 2)
#define icmphdr			icmp

#else
#define ICMP_MINLEN	28
#define inet_ntoa(x) inet_ntoa(*((struct in_addr *)&(x)))
#endif


#define	DEFDATALEN	(64 - 8)	/* default data length */
#define	MAXIPLEN	60
#define	MAXICMPLEN	76
#define	MAXPACKET	(DEFDATALEN + MAXIPLEN + MAXICMPLEN)/* max packet size */

struct sockaddr whereto;	/* who to ping */
int datalen = DEFDATALEN;
int s;				/* socket file descriptor */
u_char outpack[MAXPACKET];
static int ident;		/* process id to identify our packets */

/* counters */

static long ntransmitted=0;	/* sequence # for outbound packets = #sent */




/* protos */

static int in_cksum(u_short *addr, int len);
static void pinger(void);
static int pr_pack(char *buf, int cc, struct sockaddr_in *from);
int
ping(char *dest_ip, int count);
/*
int main(int argc, char *argv[])
{
	if(ping(argv[1],3)==0)
		printf("Ok!\n");
	else
		printf("Fail!\n");
	return 0;
}
*/
int
ping(char *dest_ip, int count)
{
	struct sockaddr_in *to;
	int i;
	int packlen,fail=0;
	static u_char  packet[192];
	char *target;


	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
		if (errno==EPERM) {
			fprintf(stderr, "ping: ping must run as root\n");
		}
		else perror("ping: socket");
		return -1;
	}
	target = dest_ip;

	memset(&whereto, 0, sizeof(struct sockaddr));
	to = (struct sockaddr_in *)&whereto;
	to->sin_family = AF_INET;
	if (inet_aton(target, &to->sin_addr)) {
		;
	}
	else {
		return -1;
	}

	packlen = datalen + MAXIPLEN + MAXICMPLEN;
	//packet = malloc((u_int)packlen);
	//if (!packet) {
	//	(void)fprintf(stderr, "ping: out of memory.\n");
	//	return(-1);
	//}

	ident = 0xFFFF;

	//(void)printf("PING %s: %d data bytes\n", inet_ntoa(*(struct in_addr *)&to->sin_addr.s_addr), datalen);

	for (i=0;i<count;i++) {
		pinger();
		struct sockaddr_in from;
		register int cc;
		size_t fromlen;
		fromlen = sizeof(from);
		if ((cc = recvfrom(s, (char *)packet, packlen, 0,
		    (struct sockaddr *)&from, &fromlen)) < 0) {
			if (errno == EINTR)
				continue;
			perror("ping: recvfrom");
			fail++;
			continue;
		}
		else if(pr_pack((char *)packet, cc, &from)==0)
			fail=0;
		else
			fail++;
		//sleep(1);
	}
	/* NOTREACHED */
	return fail;
}


#if !defined(__GLIBC__) || (__GLIBC__ < 2)
#define icmp_type type
#define icmp_code code
#define icmp_cksum checksum
#define icmp_id un.echo.id
#define icmp_seq un.echo.sequence
#define icmp_gwaddr un.gateway
#endif /* __GLIBC__ */

#define ip_hl ihl
#define ip_v version
#define ip_tos tos
#define ip_len tot_len
#define ip_id id
#define ip_off frag_off
#define ip_ttl ttl
#define ip_p protocol
#define ip_sum check
#define ip_src saddr
#define ip_dst daddr

/*
 * pinger --
 * 	Compose and transmit an ICMP ECHO REQUEST packet.  The IP packet
 * will be added on by the kernel.  The ID field is our UNIX process ID,
 * and the sequence number is an ascending integer.  The first 8 bytes
 * of the data portion are used to hold a UNIX "timeval" struct in VAX
 * byte-order, to compute the round-trip time.
 */
static void
pinger(void)
{
	register struct icmphdr *icp;
	register int cc;
	int i;

	icp = (struct icmphdr *)outpack;
	icp->icmp_type = ICMP_ECHO;
	icp->icmp_code = 0;
	icp->icmp_cksum = 0;
	icp->icmp_seq = ntransmitted++;
	icp->icmp_id = ident;			/* ID */

	cc = datalen + 8;			/* skips ICMP portion */
	/* compute ICMP checksum here */
	icp->icmp_cksum = in_cksum((u_short *)icp, cc);

	i = sendto(s, (char *)outpack, cc, 0, &whereto,
	    sizeof(struct sockaddr));
	if (i < 0 || i != cc)  {
		if (i < 0)
			perror("ping: sendto");
	}
}

/*
 * pr_pack --
 *	Print out the packet, if it came from us.  This logic is necessary
 * because ALL readers of the ICMP socket get a copy of ALL ICMP packets
 * which arrive ('tis only fair).  This permits multiple copies of this
 * program to be run without having intermingled output (or statistics!).
 */
int
pr_pack(char *buf, int cc, struct sockaddr_in *from)
{
	register struct icmphdr *icp;
	struct iphdr *ip;
	//struct timeval tv;
	int hlen;
	/* Check the IP header */
	ip = (struct iphdr *)buf;
	hlen = ip->ip_hl << 2;
	if (cc < datalen + ICMP_MINLEN) {
		return 1;
	}
	/* Now the ICMP part */
	cc -= hlen;
	icp = (struct icmphdr *)(buf + hlen);
	if (icp->icmp_type == ICMP_ECHOREPLY) {
		//(void)printf("%d bytes from %s: icmp_seq=%u\n", cc,
		//inet_ntoa(*(struct in_addr *)&from->sin_addr.s_addr),icp->icmp_seq);
		//(void)printf(" ttl=%d\n", ip->ip_ttl);
		return 0;
	}else
	{
		//printf("Timeout!\n");
		return 1;
	}
}

/*
 * in_cksum --
 *	Checksum routine for Internet Protocol family headers (C Version)
 */
static int
in_cksum(u_short *addr, int len)
{
	register int nleft = len;
	register u_short *w = addr;
	register int sum = 0;
	u_short answer = 0;

	/*
	 * Our algorithm is simple, using a 32 bit accumulator (sum), we add
	 * sequential 16 bit words to it, and at the end, fold back all the
	 * carry bits from the top 16 bits into the lower 16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1) {
		*(u_char *)(&answer) = *(u_char *)w ;
		sum += answer;
	}

	/* add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return(answer);
}



/* Copyright 2000
*	Atrus Trivalie Productions.  All rights reserved.
*
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
* 3. All advertising materials mentioning features or use of this software
*    must display the following acknowledgement:
* This product includes software developed by Atrus Trivalie Productions
* and its contributors.
* 4. Neither the name of Atrus Trivalie Productions
*    nor the names of its contributors
*    may be used to endorse or promote products derived from this software
*    without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE ATRUS TRIVALIE PRODUCTIONS AND CONTRIBUTORS ``AS IS'' AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED.  IN NO EVENT SHALL ATRUS TRIVALIE PRODUCTIONS OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
*/

/*
*
* Bind to predefined IP address (option -a) by Valts Silaputnins <valts@tornis.lv>
*
*/
static char *copyright = "(c) 2000 Atrus Trivalie Productions";

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <config.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <limits.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include "globals.h"

/* this part needs a little cleaning... */


int udp_socket;
int groupid = 1;
int bcast_interval = BROADCAST_INTERVAL;
int bcast_server_interval;
fd_set socks;
int sequence, sqcounter, sqcountercur = 0;  /* This is TrivialSecurity (tm)  Sequence numbers must match or the client doesn't listen to broadcasts */
long starttime, lastbcast;
int bcast_enable = 1;

char myip[20];
char message[MESSAGE_LEN];
char outmessage[MESSAGE_LEN];
struct sockaddr_in bcast;                       /* set to the broadcast address */
struct sockaddr_in server_address;              /* used to init server port bindings */
struct sockaddr_in their_addr;  		/* used in recieves */
struct sockaddr_in send_addr;			/* used in sending localized replies */

void setnonblocking(int sock) {
         int opts;

         opts = fcntl(sock,F_GETFL);
         if (opts < 0) {
                 perror("fcntl(F_GETFL)");
                 exit(EXIT_FAILURE);
         }
         opts = (opts | O_NONBLOCK);
         if (fcntl(sock,F_SETFL,opts) < 0) {
                 perror("fcntl(F_SETFL)");
                 exit(EXIT_FAILURE);
         }
         return;
 }

void build_select_list() {
         int listnum;         /* Current item in connectlist for for loops */

         FD_ZERO(&socks);

         FD_SET(udp_socket,&socks);
}

void picknewsequence() {
	
	sequence = random() / 10000;
	sqcounter = random() / 10000000;
        sqcountercur = 0;
        sprintf(outmessage, "snts %s %s %d resetsequence %d", PROTOVERSION, myip, groupid, sequence);

        sendto(udp_socket, outmessage, sizeof(outmessage), 0, (struct sockaddr *)&bcast, sizeof(bcast));

}

int process_message() {
	char t1[50];
	char t2[50];
	char t3[50];
	char t4[50];
	char t5[50];
	char t6[50];
	int i1, i2, i3 = 0;
	struct timeval timet;
	struct timezone zone;
	long timecheck, utime;
	sscanf(message, "%49s %49s %49s %d %49s %49s %49s", t1, t2, t3, &i1, t4, t5, t6);	
	if (strcmp("snts", t1) != 0) {
		syslog(LOG_WARNING, "garbled message - incorrect header");
		return -1;
	} else if (strcmp(t2, PROTOVERSION) != 0) {	
		syslog(LOG_WARNING, "incorrect protocol version");
		return -1;
	} else if (strcmp(myip, t3) == 0) {
		/* it comes from myself, no need to listen */
		
		return -1;
	}
	send_addr.sin_family = AF_INET;
	send_addr.sin_addr.s_addr = inet_addr(t3);
	send_addr.sin_port = htons(SNTSPORT);
	
	if (strcmp("host", t4) == 0) {
		if (i1 == groupid) {
			sprintf(outmessage, "snts %s %s %d hostresponse %d", PROTOVERSION, myip, groupid, sequence);
			sendto(udp_socket, outmessage, sizeof(outmessage), 0, (struct sockaddr *)&send_addr, sizeof(send_addr));
		}
	} else if (strcmp("timeb", t4) == 0) {
		if (i1 == groupid) {
			/* uhoh, I heard some elses time broadcast :)  I'm going to power myself down */
			if (bcast_enable == 1) {
				/* so we don't have these at every broadcast */
				sprintf(outmessage, "heard time broadcast from %s in my group.  Shutting down broadcasts for %d seconds", t3, bcast_server_interval);
				syslog(LOG_WARNING, outmessage);
			}
			
			bcast_enable = 0;
			time(&lastbcast); /* store when this happened so we can auto resume */

                        /* WHile we're at it, look like a client and set our clock :) */
                        if (bcast_enable == 0) {

                        	sscanf(t5, "%d", &i2);
				sscanf(t6, "%ld", &utime);
				timet.tv_sec = utime;
				timet.tv_usec = 10000;		/* some made up number */
				time(&timecheck);
				if (timecheck - utime > 3600 || utime - timecheck > 3600) {
					syslog(LOG_WARNING, "time skew exceeded 1 hour.");
				}
				settimeofday((struct timeval *)&timet, NULL);
			}		
			
			
			
		}
	}
			
			
}	
	

void timebcast() {
	struct timeval stime;
	struct timezone zone;
	long timenow;
	time(&timenow);

	if (bcast_enable == 1) {
		gettimeofday((struct timeval *)&stime, (struct timezone *)&zone);
		/* Timezone is no longer used in modern systems, but for compatibility, passed anyway */
		sprintf(outmessage, "snts %s %s %d timeb %d %ld", PROTOVERSION, myip, groupid,sequence ,stime.tv_sec);
		sequence++;
		sqcountercur++;
		if (sqcountercur > sqcounter) {
			picknewsequence();
		}
		sendto(udp_socket, outmessage, sizeof(outmessage), 0, (struct sockaddr *)&bcast, sizeof(bcast));
	} else {
		if (timenow - lastbcast > bcast_server_interval) {
			syslog(LOG_WARNING, "resuming broadcasts - wire seems empty");
			bcast_enable = 1;
		}
	}
	
}

int main(int argc, char *argv[]) {
         char scrap[500],bindto[128];
         struct hostent *myself;
         struct itimerval timers;
         struct itimerval oldtimers;
	 struct sigaction sig;
         struct timeval timeout;  /* Timeout for select */
         int readsocks;       /* Number of sockets ready for reading */
         int i, i2, addr_len;	
	 int sock;
	 int reuse_addr = 1, bind_to_other = 0;
	 int broadcast_enable = 1; /* sort of senseless without */
	 long utime;
	
	/* Suck in command line options */
	if (argc > 1) {
		/* something was passed */
		for(i=1;i<argc;i++) {
			if (strcmp("-g", argv[i]) == 0) {
				sscanf(argv[i+1], "%d", &groupid);
				if (groupid < 1) {
					printf("sntsd: group must be > 0\n");
					exit(1);
				}
				i++;  /* so the for isn't called and the # considered a bad option */
			} else if (strcmp("-a", argv[i]) == 0) {
				sscanf(argv[i+1], "%s", &bindto);
				bind_to_other=1;
				if (strlen(bindto)>16) { /* Just checking for too long input */
					printf("sntsd: IP address is too long\n");
					exit(1);
				}
				i++;
                        } else if (strcmp("-i", argv[i]) == 0) {

				i++;
			} else {
				printf("sntsd: bad command line option %s.\n", argv[i]);
				exit(1); 	
			}
		}
	}
	
	/* this is to allow for multiple hosts on the same group.  If twice the interval is reached, the host is assumed down and this host starts broadcasting */
	bcast_server_interval = bcast_interval * 2;
	
	
	
	/* Get socket */
	
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == -1) {
		perror("socket");
		exit(1);
	}
	
	/* So that we can re-bind to it without TIME_WAIT problems */
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr,sizeof(reuse_addr));
	/* Broadcast enable */
	
	setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable));
	
	/* Set socket to non-blocking with our setnonblocking routine */
        setnonblocking(sock);
        memset((char *) &server_address, 0, sizeof(server_address));
        server_address.sin_family = AF_INET;
	
        if (bind_to_other == 0) server_address.sin_addr.s_addr = htonl(INADDR_ANY); else
    				server_address.sin_addr.s_addr = inet_addr(bindto);
	
	server_address.sin_port = htons(SNTSPORT);
        if (bind(sock, (struct sockaddr *) &server_address, sizeof(server_address)) < 0 ) {
                 perror("bind");
                 close(sock);
                 exit(1);
        }
        /* Daemon ourselves */
	daemon(1,1);
	
	//signal(SIGALRM, timebcast);
	/* Linux doesn't like "signal" :( */

	sig.sa_handler = &timebcast;
	sig.sa_flags = SA_RESTART;
	sigaction(SIGALRM, &sig, NULL);

         oldtimers.it_interval.tv_sec = bcast_interval;
         oldtimers.it_value.tv_sec = bcast_interval;
         timers.it_interval.tv_sec = bcast_interval;
         timers.it_value.tv_sec = bcast_interval;


         if(setitimer(ITIMER_REAL, &timers, NULL)) {
	   printf("Failure setting timer (blame Linux)\n");
	   exit(1);
	 }

	/* Get info on myself or use passed IP address */
      if (bind_to_other == 0) { /* Lookup my IP  */
	
 	i = gethostname(scrap, sizeof(scrap));
	myself = gethostbyname(scrap);
        if (myself == NULL) {
                syslog(LOG_ERR, "Can't lookup my own IP - quiting");
                exit(1);
        }
       sprintf(myip, "%s", inet_ntoa(*((struct in_addr *)myself->h_addr)));

      } else sprintf(myip,"%s",bindto); /* Use specified IP */
      
	/* syslog starting message */
	sprintf(scrap, "sntsd version %s proto version %s starting up", VERSION, PROTOVERSION);
	syslog(LOG_NOTICE, scrap);
	sprintf(scrap, "starting using groupid: %d", groupid);
	syslog(LOG_NOTICE, scrap);
	sprintf(scrap, "using IP: %s",myip);
	syslog(LOG_NOTICE, scrap);
	
	udp_socket = sock;
	bcast.sin_family = AF_INET;
	bcast.sin_addr.s_addr = inet_addr(BCAST_ADDR);
	bcast.sin_port = htons(SNTSPORT);
	
	time(&starttime);
		
	/* pick random sequence number */

#ifdef COMPAT_SRANDOM
	time(&utime);
	srandom(utime);
#else
	srandomdev();	/* init using /dev/urandom on systems which have it */
#endif
	picknewsequence();
	
	
	/* Send 'master coming up' message */
	sprintf(outmessage, "snts %s %d hostup", PROTOVERSION, groupid);

	sendto(udp_socket, outmessage, sizeof(outmessage), 0, (struct sockaddr *)&bcast, sizeof(bcast));
	
	
	while (1) {
	/* i=recvfrom(udp_socket, message, sizeof(message), 0, (struct sockaddr *)&their_addr, &addr_len); */
		timeout.tv_sec = 10;
	        timeout.tv_usec = 0;
	        build_select_list();
	        readsocks = select(udp_socket+1, &socks, (fd_set *) 0, (fd_set *) 0, &timeout);

	        if (readsocks == 0) {
			
                } else if (readsocks > 0) {
		        i=recvfrom(udp_socket, message, sizeof(message), 0, (struct sockaddr *)&their_addr, &addr_len);
		
		
		        message[i] = '\0';
		
		        process_message();
		 }
}
}

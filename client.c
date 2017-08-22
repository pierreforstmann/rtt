/*
*
* client.c
*
* 16.08.2015 PF created
* 23.08.2015 PF added -p <port> -h <host>
* 23.08.2014 PF addded -nr <number of round trips> -ps <size of data>
* 20.09.2015 PF added time measurement
* 20.09.2013 PF added sleep option: -st
* 27.09.2015 PF ms can be set to 0
* 27.09.2015 PF addded getrusage
* 25.10.2015 PF added macros for buffer
* 25.10.2015 PF compute number if static array items in fmsg
* 25.10.2015 PF added free for malloc
* 25.10.2015 PF moved code form main to check_args
* 25.10.2015 PF renamed sigh to sighadler and added all signails
* 25.10.2015 PF getaddrinfo full use (using man 3 getaddrinfo code)
* 25.10.2015 PF moved code from main to socket_and_connect
* 25.10.2015 PF moved code from main to send_round_trip_num_and_data_size
*------------------------------------------------------------------------
*/

/* to avoid following gcc errors with -std=c90 or -std=c99
 * "storage size isn't known" for struct timespec 
 * "storage_size isn't known" for struct sigaction
 * "dereferencing pointer to incomplete type" for addrinfo pointer
*/ 
#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */

/* to avoid warnings with -std=C99 for strsignal */
/* because  #define __USE_XOPEN2K8	1 fails */
extern char *strsignal (int __sig);

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/times.h>
#include <time.h>
#include <sys/resource.h>
#include <signal.h>
#include <arpa/inet.h>

#define MAX_MSG_SIZE 200
#define MAX_ARG_SIZE 128

struct s_args {
 long server_port;
 struct addrinfo *addr;
 struct addrinfo *result;
 long num_rt;
 long data_size;
 int ms;
} s_args;

/* global */
int g_cur_rtn = 0;

void set_cur_rtn(int cur_rtn)
{
        g_cur_rtn = cur_rtn;
}

int get_cur_rtn()
{
        return g_cur_rtn;
}

void sig_handler(int sigid)
{
        /* cannot call log_msg_XXX because automatic exit */
        fprintf(stderr, "client: current RT nr = %d\n", get_cur_rtn());
        fprintf(stderr, "client: unexpected signal: %s: exit \n",strsignal(sigid));
        exit(-1);
}

char *fmsg(int msg_num)
{
 static char msg[][MAX_MSG_SIZE] = {
 "usage: client -p <port> -h <node> -nr <number of RT> -ps <size> -st <ms> (not enough or too many args)\n",
 "usage: client -p <port> -h <node> -nr <number of RT> -ps <size> -st <ms> (first argument must be: -p\n",
 "usage: client -p <port> -h <node> -nr <number of RT> -ps <size> -st <ms> (-p argument must be numeric)\n",
 "usage: client -p <port> -h <node> -nr <number of RT> -ps <size> -st <ms> (%d must be > 1024)\n",
 "usage: client -p <port> -h <node> -nr <number of RT> -ps <size> -st <ms> (%d must be < 10000 )\n",
 "usage: client -p <port> -h <node> -nr <number of RT> -ps <size> -st <ms> (missing -h)\n",
 "usage: client -p <port> -h <node> -nr <number of RT> -ps <size> -st <ms> (getaadrinfo error): %s\n",
 "usage: client -p <port> -h <node> -nr <number of RT> -ps <size> -st <ms> (missing -nr)\n",
 "usage: client -p <port> -h <node> -nr <number of RT> -ps <size> -st <ms> (-nr mst be numeric): %s\n",
 "usage: client -p <port> -h <node> -nr <number of RT> -ps <size> -st <ms> (missing -ps)\n",
 "usage: client -p <port> -h <node> -nr <number of RT> -ps <size> -st <ms> (-ps must be numeric): %s\n",
 "usage: client -p <port> -h <node> -nr <number of RT> -ps <size> -st <ms> (missing -st): \n",
 "usage: client -p <port> -h <node> -nr <number of RT> -ps <size> -st <ms> (-st must be numeric): %s\n"
 };
 int max_msg_nr = 0;

 max_msg_nr = sizeof(msg)/MAX_MSG_SIZE;

 if (msg_num >= 0 && msg_num < max_msg_nr)
        return msg[msg_num];
 else {
        fprintf(stderr, "INTERNAL ERROR: fmsg: msg_num = %d \n",msg_num);
        exit (-1);
 }
}

void log_msg(int msg_nr)
{
        fprintf(stderr, fmsg(msg_nr));
        exit(-1);
}

void log_msg_arg1(int msg_nr, int arg1)
{
        char buf[MAX_ARG_SIZE];
        sprintf(buf, fmsg(msg_nr), arg1);
        fprintf(stderr, buf);
        exit(-1);
}
void log_msg_args1(int msg_nr, char *arg1)
{
        char buf[MAX_ARG_SIZE];
        sprintf(buf, fmsg(msg_nr), arg1);
        fprintf(stderr, buf);
        exit(-1);
}

void check_args(int argc, char *argv[], struct s_args *p)
{

        int i;
        long server_port;
        long num_rt;
        long data_size;
        int ms;
        int rc;

        for (i=0; i < argc; i++) {
                printf("%d: argv = %s \n", i, argv[i]);
        }

        if (argc != 11 ) {
                log_msg(0);
        }
        if (strcmp(argv[1],"-p") != 0) {
                log_msg(1);
        }
        if ((server_port = strtol(argv[2], NULL, 0)) == 0) {
                log_msg(2);
        }
        if (server_port < 1025) {
                log_msg_arg1(3, server_port);
        }
        if (server_port > 9999 ) {
                log_msg_arg1(4, server_port);
        }
        p->server_port = server_port;

        if (strcmp(argv[3], "-h") != 0) {
                log_msg(5);
        }
        if ((rc = getaddrinfo(argv[4], argv[2], p->addr, &(p->result))) != 0 ) {
                fprintf(stderr, fmsg(6), gai_strerror(rc));
                exit(-1);
        }

        if (strcmp(argv[5],"-nr") != 0) {
                log_msg(7);
        }
        if ((num_rt = strtol(argv[6], NULL, 0)) == 0) {
                log_msg_args1(8, argv[6]);
        }
        p->num_rt = num_rt;

        if (strcmp(argv[7],"-ps") != 0) {
                log_msg(9);
        }
        if ((data_size = strtol(argv[8], NULL, 0)) == 0) {
                log_msg_args1(10, argv[8]);
        }
        p->data_size = data_size;

        if (strcmp(argv[9],"-st") != 0) {
                log_msg(11);
        }
        if (((ms = strtol(argv[10], NULL, 0)) == 0) && (strcmp(argv[10],"0") != 0)) {
                log_msg_args1(12, argv[10]);
        }
        p->ms = ms;
}

void init_sig()
{
         struct sigaction sa_s;
         int sn;

         sa_s.sa_handler = sig_handler;
         sa_s.sa_flags = 0;
         sigemptyset(&(sa_s.sa_mask));
         for (sn=0; sn <= SIGUNUSED; sn++)
                sigaction(sn, &sa_s, NULL);
}

int socket_and_connect(struct addrinfo *ps)
{
        struct addrinfo *rp;
        int client_socket;
        int success = 0;

        for (rp = ps; rp != NULL; rp = rp->ai_next)
        {
                if ((client_socket=socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
                        continue;
                }

                if (connect(client_socket, rp->ai_addr, rp->ai_addrlen) != - 1)
                {
                        success = 1;
                        break;
                }
                else {
                        continue;
                }
        }
        if (success > 0 )
                printf("client: connected ... \n");
        else {
                printf("client: cannot connect: exit. \n");
                exit(-1);
        }
        return client_socket;
}

void send_round_trip_num_and_data_size(int client_socket, long num_rt, long data_size)
{
        char buf[MAX_ARG_SIZE];

        memcpy((void *)buf, (void *)&num_rt, sizeof(num_rt));
        if (write(client_socket, buf, sizeof(num_rt)) == - 1) {
                perror("write rt_num failed\n");
                exit(-1);
        }

        memcpy((void *)buf,  (void *)&data_size, sizeof(data_size));
        if (write(client_socket, buf, sizeof(data_size)) == - 1) {
                perror("write data_size failed\n");
                exit(-1);
        }
}

char *alloc_data(long data_size)
{
        char *data;
        int i;

        data = (char *)malloc(data_size);
        if ( data ==  NULL ) {
              fprintf(stderr, "client: mmaloc %ld failed \n",data_size);
              exit(-1);
        }
        for (i=0; i < data_size; i++)
                *(data + i) = 'P';
        return data;
}

int main(int argc, char*argv[])
{
        int client_socket;
        int i;
        struct addrinfo hints, *s_addr;
        int rc, rc2;
        long num_rt;
        long data_size;
        char *data;
        struct tms tms_s;
        clock_t start_ct, end_ct;
        int ms;
        struct timespec ts_s;
        struct rusage ru_s;
        struct s_args args_s;

        init_sig();

        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = 0;
        hints.ai_protocol = 0;
        args_s.addr = &hints;

        check_args(argc, argv, &args_s);

        s_addr = args_s.result;
        num_rt = args_s.num_rt;
        data_size = args_s.data_size;
        ms = args_s.ms;

        printf("args OK.\n");
        printf("num_rt = %ld \n", num_rt);
        printf("data_size = %ld \n", data_size);

        client_socket = socket_and_connect(s_addr);

        send_round_trip_num_and_data_size(client_socket, num_rt, data_size);

        data = alloc_data(data_size);

        ts_s.tv_sec = ms / 1000;
        ts_s.tv_nsec = (ms % 1000) * 1000000;

        printf("client: start %ld round trips ...\n", num_rt);
        start_ct = times(&tms_s);
        for (i=0; i < num_rt; i++)
        {
                set_cur_rtn(i);

                if ((rc=write(client_socket, data, data_size)) == -1 ) {
                        perror("write failed");
                        fprintf(stderr, "client: current RT nr = %d \n",get_cur_rtn());
                        return (-1);
                }
                if ( rc == 0 )
                        printf("0 byte written ??? \n");

                if (ms > 0 ) {
                        rc2 = nanosleep(&ts_s, NULL);
                        if (rc2 != 0) {
                                perror("nanosleep failed");
                                return(-1);
                        }
                }
        }

        free(data);

        rc = getrusage(RUSAGE_SELF, &ru_s);
        end_ct = times(&tms_s);
        printf("client ...: OK\n");
        printf("elapsed clock ticks : %ld (%ld clocks ticks per second) \n",
               end_ct - start_ct, sysconf(_SC_CLK_TCK));
        printf("user clock ticks    : %ld \n", tms_s.tms_utime);
        printf("kernel clock ticks  : %ld \n", tms_s.tms_stime);
        printf("req. sleeping time  : %ld s\n", (num_rt * ms) / 1000 );
        /*  */
        printf("elapsed time        : %f s \n",(end_ct - start_ct) / (double)sysconf(_SC_CLK_TCK));
        printf("user CPU time       : %ld.%ld s \n", ru_s.ru_utime.tv_sec, ru_s.ru_utime.tv_usec);
        printf("kernel CPU time     : %ld.%ld s \n", ru_s.ru_stime.tv_sec, ru_s.ru_stime.tv_usec);
        /* */
        return(0);
}


/*
*
* server.c
*
* round trip test server: to test network bandwidth and speed
*  
* Copyright (C) 2017 Pierre Forstmann 
* 
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*----------------------------------------------------------------------
*/

/* to avoid following gcc errors with -std=c90 or -std=c99
 * "storage size isn't known" for struct timespec
 * "storage_size isn't known" for struct sigaction
 * "dereferencing pointer to incomplete type" for addrinfo pointer
 * 
*/
#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */

/* to avoid warnings with -std=C99 for strsignal */
/* because  #define __USE_XOPEN2K8      1 fails */
extern char *strsignal (int __sig);

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>

extern int errno;

#define MAX_ARG_SIZE 128
#define TOTAL_MAX_ARG_SIZE (10*128)

/* to be accessed in signal handler */
static FILE *g_f;

void set_log_file(FILE *f)
{
        g_f = f;
}

FILE *get_log_file()
{
        return g_f;
}

FILE *log_init()
{
        FILE *f;
        f = fopen("/tmp/server.log", "w");
        if (f == NULL)
        {
                fprintf(stderr, "server: cannot open log");
                exit(-1);
        }
        set_log_file(f);
        return f;
}
/*
*  caller must NOT use \n: it will be adeded automatically at each call
*
*/
 void log_msg(FILE *f, char *msg)
{
        time_t time_sec;
        char *buf;
        char *fmsg;

        time_sec = time(NULL);
        if (time_sec != - 1)
        {
                /* static data pointer not thread-safe */
                buf=ctime(&time_sec);
                /* remove \n from ctime return value */
                buf[strlen(buf)-1] = ' ';
        }
        else
        {
                fprintf(f, "time error ! \n");
                fflush(f);
                exit(-1);
        }
        fmsg = malloc(strlen(buf) + 1 + strlen(msg) + 1);
        if (fmsg == NULL)
        {
                fprintf(f, "server: malloc %d failed \n",
                           (int)(strlen(buf) + 1 + strlen(msg) + 1));
                exit(-1);
        }
        strcpy(fmsg, buf);
        fmsg = strcat(fmsg, msg);
        fprintf(f, fmsg);
        fflush(f);
        free(fmsg);
}

void log_msg_arg1(FILE *f, char *msg, int arg1)
{
        char buf[MAX_ARG_SIZE];
        sprintf(buf, msg, arg1);
        log_msg(f, buf);
}

void log_msg_arg1s(FILE *f, char *msg, char *arg1)
{
        char buf[MAX_ARG_SIZE];
        sprintf(buf, msg, arg1);
        log_msg(f, buf);
}

void log_args(FILE *f, int argc, char *argv[])
{
        int i;
        char buf[TOTAL_MAX_ARG_SIZE];
        for (i=0; i < argc; i++) {
                sprintf(buf, "argv nr %d = %s \n", i, argv[i]);
                log_msg(f, buf);
        }
}

long check_args(FILE *f, int argc, char *argv[])
{
        long local_port = 0;

        if (argc != 3 ) {
                log_msg(f, "usage: server -p <port> (not enough or too many args)\n");
                exit(-1);
        }
        if (strcmp(argv[1],"-p") != 0) {
                log_msg(f, "usage: server -p <port> (first argument must be: -p\n");
                exit(-1);
        }
        if ((local_port = strtol(argv[2], NULL, 0)) == 0) {
                log_msg(f, "usage: server -p <port> (second argument must be numeric)\n");
                exit(-1);
        }

        if (local_port < 1025) {
                log_msg_arg1(f, "usage: server -p <port> (%ld must be > 1024)\n", local_port);
                exit(-1);
        }

        if (local_port > 9999 ) {
                log_msg_arg1(f, "usage: server -p <port> (%ld must be < 10000 )\n", local_port);
                exit(-1);
        }
        return local_port;
}

void sig_handler(int sigid)
{
        FILE *f;

        f = get_log_file();
        /* exit on SIGHUP */
        if (sigid == SIGHUP)
        {
                log_msg(f, "server: expected signal SIGHUP: exit \n");
                exit(0);
        }
        log_msg_arg1s(f, "server: unexpected signal: %s: exit \n",strsignal(sigid));
        exit(-1);

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

int server_main(int argc, char* argv[])
{
        FILE *f;
        long local_port;
        int server_socket;
        struct sockaddr_in server_socket_name;
        int optval = 1;
        int client_socket;
        char buf[MAX_ARG_SIZE];
        char *data;
        int rc;
        long num_rt;
        long data_size;
        int num_test = 0;
        int cur_bytes_read = 0;
        long total_bytes_read =0;

        f = log_init();
        log_args(f, argc, argv);
        log_msg_arg1(f, "server: pid = %d \n",getpid());

        init_sig();

        local_port = check_args(f, argc, argv);

        if ((server_socket=socket(PF_INET, SOCK_STREAM, 0)) == -1) {
                log_msg(f, "cannot create socket: ");
                log_msg(f, strerror(errno));
                exit (-1);
        }

        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));

        memset((char *)&server_socket_name, 0, sizeof(struct sockaddr_in));
        server_socket_name.sin_family = AF_INET;
        server_socket_name.sin_port = htons(local_port);
        server_socket_name.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(server_socket, (struct sockaddr *)&server_socket_name, sizeof(struct sockaddr_in)) == - 1) {
                log_msg(f, "cannot bind sccket: ");
                log_msg(f, strerror(errno));
                exit (-1);
        }

        if (listen(server_socket,1) == - 1) {
                log_msg(f, "cannot listen: ");
                log_msg(f, strerror(errno));
                exit (-1);

        }

        while (1)
        {
                log_msg(f, "waiting for new connection ...\n");
                if ((client_socket = accept(server_socket, NULL, NULL)) == -1 ) {
                        log_msg(f, "cannot accept: ");
                        log_msg(f, strerror(errno));
                        exit(-1);
                }
                log_msg(f, "client connected. \n");

                if (read(client_socket, buf, sizeof(num_rt)) == - 1) {
                        log_msg(f, "read rt_num failed: ");
                        log_msg(f, strerror(errno));
                        exit(-1);
                }
                memcpy((void *)&num_rt, (void *)buf, sizeof(num_rt));

                if (read(client_socket, (void *)buf, sizeof(data_size)) == - 1) {
                        log_msg(f, "read data_size failed: ");
                        log_msg(f, strerror(errno));
                        exit(-1);
                }
                memcpy((void *)&data_size, buf, sizeof(data_size));
                log_msg_arg1(f, "num_rt = %ld \n",num_rt);
                log_msg_arg1(f, "data_size = %ld \n",data_size);

                data = (char *)malloc(data_size);
                if ( data ==  NULL ) {
                        log_msg_arg1(f, "malloc %ld failed \n",data_size);
                        exit(-1);
                }
                num_test++;
                total_bytes_read = 0;
                log_msg_arg1(f, "test nr=%d started.\n", num_test);
                log_msg(f, "start reading ... \n");
                while (total_bytes_read < num_rt * data_size)
                {
                        cur_bytes_read = 0;
                        while (cur_bytes_read < data_size)
                        {
                        if ((rc=read(client_socket, data, data_size - cur_bytes_read )) == -1 ) {
                                log_msg(f, "read failed: ");
                                log_msg(f, strerror(errno));
                                exit (-1);
                                }
                        total_bytes_read += rc;
                        if ( rc == 0 ) {
                                log_msg(f, "unexpected: 0 byte read: exit.\n");
                                exit(-1);
                        }
                        if (rc < data_size)
                                cur_bytes_read = cur_bytes_read + rc;
                        if (cur_bytes_read == data_size)
                                break;
                        if (total_bytes_read == num_rt * data_size)
                                break;
                        }
                }
                log_msg(f, "... reading OK\n");
                close(client_socket);
                log_msg(f, "socket closed\n");
                log_msg_arg1(f, "test nr=%d ended.\n", num_test);

                free(data);
        }

        return(0);
}

int daemonize(int argc, char* argv[])
{
        int rc;
        int pid;
        int fd;
	int maxfd;

        pid = fork();
        if (pid < 0 )
        {
                fprintf(stderr, "fork failed\n");
                return -1;
        }
        if ( pid > 0 )
                return 0;

        setpgid(0, 0);
        setsid();

        pid = fork();
        if (pid < 0 )
        {
                fprintf(stderr, "fork failed\n");
                return -1;
        }
        if ( pid > 0 )
                return 0;

	/* not a good way to close all open file descriptors ? */
	maxfd=sysconf(_SC_OPEN_MAX);
	for(fd=0; fd<maxfd; fd++)
    		close(fd);

        umask(022);
        chdir("/tmp");

        rc = server_main(argc, argv);
        return rc;
}

int main(int argc, char*argv[])

{
        int rc;

        rc = daemonize(argc, argv);
        return rc;
}



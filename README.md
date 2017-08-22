# RTT 
round trip test
# Description
This code allows to test network round trips on Linux.
It does more than ftp because it allows to choose:
* the number of data transfer
* the size of each data transfer
* a delay between each transfer on client side.
# Installation
Make sure that gcc is installed and run:
`make all`.
You need to install RTT on client and server machines.
# Usage
On server machine, start the server giving as unique argument the TCP/IP port number:
`
$ ./server -p 2000
$ cat /tmp/server.log
Tue Aug 22 14:03:02 2017 argv nr 0 = ./server 
Tue Aug 22 14:03:02 2017 argv nr 1 = -p 
Tue Aug 22 14:03:02 2017 argv nr 2 = 2000 
Tue Aug 22 14:03:02 2017 server: pid = 11784 
Tue Aug 22 14:03:02 2017 waiting for new connection ...
$ 
`
On client machine: start the client and specify:
* -p  : server port number
* -h  : server host name
* -nr : number of round trips
* -ps : size of each data transfer in bytes
* -st : time between each data transfer on client side
For example:
`
$ ./client -p 2000 -h localhost -nr 1000 -ps 10000000 -st 0
0: argv = ./client 
1: argv = -p 
2: argv = 2000 
3: argv = -h 
4: argv = localhost 
5: argv = -nr 
6: argv = 1000 
7: argv = -ps 
8: argv = 10000000 
9: argv = -st 
10: argv = 0 
args OK.
num_rt = 1000 
data_size = 10000000 
client: connected ... 
client: start 1000 round trips ...
client ...: OK
elapsed clock ticks : 173 (100 clocks ticks per second) 
user clock ticks    : 4 
kernel clock ticks  : 173 
req. sleeping time  : 0 s
elapsed time        : 1.730000 s 
user CPU time       : 0.43987 s 
kernel CPU time     : 1.733488 s 
`

# Limitations
RTT can only be used to test single connection between a client machine and a server machine.
The server does not support multiple clients and it does not reject multiple clients.
You cannot run multiple server becuase the server log file is hardcoded.
# License
License used is GPL.

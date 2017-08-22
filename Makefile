# Makefile
# 
#  RTT (Round Trip Test)
#
#
CFLAGS = -std=c99 -Wall -Wextra -g
#
clean:	
	rm client server
#
client: 
	$(CC) -o client $(CFLAGS) client.c
#
server:
	$(CC) -o server $(CFLAGS) server.c
#
all:	client server

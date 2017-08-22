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
	$(CC) $(CFLAGS) client.c
#
server:
	$(CC) $CFLAGS) server.c
#
all:	client server

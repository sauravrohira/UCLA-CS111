.SILENT:

default: client server

client:
	gcc -g -Wall -Wextra lab1b-client.c -o lab1b-client

server:
	gcc -g -Wall -Wextra lab1b-server.c -o lab1b-server

clean:
	rm -R -f lab1b-505000422.tar.gz *.txt *.o lab1b-client lab1b-server *.dSYM

dist:
	tar -czvf lab1b-505000422.tar.gz lab1b*.c README Makefile



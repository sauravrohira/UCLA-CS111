.SILENT:

default:
		gcc -Wall -Wextra -lmraa -lm lab4c_tcp.c -o lab4c_tcp
		gcc -Wall -Wextra -lmraa -lm -lssl -lcrypto lab4c_tls.c -o lab4c_tls  

dist:
		tar -cvzf lab4c-505000422.tar.gz lab4c_tcp.c lab4c_tls.c README Makefile

clean:
		rm -rf lab4c_tcp lab4c_tsl *.tar.gz


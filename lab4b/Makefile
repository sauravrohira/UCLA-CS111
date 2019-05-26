.SILENT:

default:
		gcc -Wall -Wextra -lmraa -lm lab4b.c -o lab4b 

dist:
		tar -cvzf lab4b-505000422.tar.gz lab4b.c README Makefile tests.sh

clean:
		rm -rf lab4b *.tar.gz
		
check: clean default
		./tests.sh 

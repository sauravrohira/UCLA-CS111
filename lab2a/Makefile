.SILENT:

default: build

build: lab2_add.c lab2_list.c
	gcc -Wall -Wextra -pthread lab2_add.c -o lab2_add
	gcc -Wall -Wextra -pthread lab2_list.c SortedList.c -o lab2_list

tests: clean build
	./add_tests.sh > lab2_add.csv 
	./list_tests.sh > lab2_list.csv

graphs: tests
	gnuplot lab2_add.gp
	gnuplot lab2_list.gp

dist: graphs
	tar -cvzf lab2a-505000422.tar.gz lab2_add.c lab2_list.c Makefile README SortedList.c SortedList.h *.png *.csv *.gp add_tests.sh list_tests.sh

clean:
	rm -rf lab2_add lab2_list *.dSYM *.tar.gz

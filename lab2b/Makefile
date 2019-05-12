.SILENT:

default: build

build: lab2_list.c
	gcc -Wall -Wextra -pthread -g lab2_list.c SortedList.c -o lab2_list -lprofiler

tests: clean build
	./tests.sh > lab2b_list.csv 2> error.txt

profile: default
	rm -f ./raw.gperf profile.out
	LD_PRELOAD=/usr/lib64/libprofiler.so
	CPUPROFILE=./raw.gperf ./lab2_list --iterations=1000 --threads=12 --sync=s
	pprof --text ./lab2_list ./raw.gperf > profile.out
	pprof --list=listOperations ./lab2_list ./raw.gperf >> profile.out
	rm -f ./raw.gperf profile.gperf lab2_list

graphs: tests
	gnuplot lab2b_list.gp

dist: graphs
	tar -cvzf lab2b-505000422.tar.gz lab2_list.c Makefile README SortedList.c SortedList.h *.png *.csv *.gp tests.sh profile.out

clean:
	rm -rf lab2_list *.dSYM *.tar.gz error.txt

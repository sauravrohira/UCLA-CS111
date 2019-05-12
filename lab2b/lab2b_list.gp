#! /usr/bin/gnuplot
#
# purpose:
#	 generate data reduction graphs for the multi-threaded list project
#
# input: lab2b_list.csv
#	1. test name
#	2. # threads
#	3. # iterations per thread
#	4. # lists
#	5. # operations performed (threads x iterations x (ins + lookup + delete))
#	6. run time (ns)
#	7. run time per operation (ns)
#    8. wait-for-lock time (ns)
#
# output:
#	lab2b_1.png ... throughput vs. number of threads for mutex and spin-lock synchronized list operations.
#	lab2b_2.png ... mean time per mutex wait and mean time per operation for mutex-synchronized list operations.
#	lab2b_3.png ... successful iterations vs. threads for each synchronization method.
#	lab2b_4.png ... throughput vs. number of threads for mutex synchronized partitioned lists.
#	lab2b_5.png ... throughput vs. number of threads for spin-lock-synchronized partitioned lists.



# general plot parameters
set terminal png
set datafile separator ","

# lab2b_1:
set title "Scalability-1: Throughput vs Number of Threads"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:25]
set ylabel "Throughput(operations/s)"
set logscale y 10
set output 'lab2b_1.png'
set key right top

plot \
     "< grep -e 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'list w/ mutex' with linespoints lc rgb 'blue', \
     "< grep -e 'list-none-s,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'list w/ spin-lock' with linespoints lc rgb 'green'

# lab2b_2:
set title "Scalability-2: Wait-for-lock Time, Avg. Operation Time vs Num. Threads"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:25]
set ylabel "Time(ns)"
set logscale y 10
set output 'lab2b_2.png'
set key left top

plot \
     "< grep -e 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):($8) \
	title 'wait-for-lock time' with linespoints lc rgb 'blue', \
     "< grep -e 'list-none-s,[0-9]*,1000,1,' lab2b_list.csv" using ($2):($7) \
	title 'avg. runtime per operation' with linespoints lc rgb 'green'

# lab2b_3:
set title "Scalability-3: Number of Successful Iterations vs Number of Threads"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:17]
set ylabel "Successful Iterations"
set logscale y
set output 'lab2b_3.png'
set key right top

plot \
     "< grep -e 'list-id-none,[0-9]*,[0-9]*,4,' lab2b_list.csv" using ($2):($3) \
	title 'unsynchronized' with points lc rgb 'green', \
     "< grep -e 'list-id-m,[0-9]*,[0-9]*,4,' lab2b_list.csv" using ($2):($3) \
	title 'mutex' with points lc rgb 'red', \
     "< grep -e 'list-id-s,[0-9]*,[0-9]*,4,' lab2b_list.csv" using ($2):($3) \
	title 'spin-lock' with points lc rgb 'blue'

# lab2b_4:
set title "Scalability-4: Aggregate Throughput vs Number of Threads (Mutex)"
set xlabel "Threads"
set logscale x 2
unset xrange
set xrange [0.75:13]
set ylabel "Throughput"
set logscale y
unset yrange
set output 'lab2b_4.png'
set key right top

plot \
     "< grep -e 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '1 list' with linespoints lc rgb 'red', \
     "< grep -e 'list-none-m,[0-9]*,1000*,4,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '4 lists' with linespoints lc rgb 'blue', \
     "< grep -e 'list-none-m,[0-9]*,1000,8,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '8 lists' with linespoints lc rgb 'green', \
     "< grep -e 'list-none-m,[0-9]*,1000,16,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '16 lists' with linespoints lc rgb 'yellow'

# lab2b_5:
set title "Scalability-5: Aggregate Throughput vs Number of Threads (Spin-Lock)"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:13]
set ylabel "Throughput"
set logscale y 
set output 'lab2b_5.png'
set key right top

plot \
     "< grep -e 'list-none-s,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '1 list' with linespoints lc rgb 'red', \
     "< grep -e 'list-none-s,[0-9]*,1000*,4,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '4 lists' with linespoints lc rgb 'blue', \
     "< grep -e 'list-none-s,[0-9]*,1000,8,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '8 lists' with linespoints lc rgb 'green', \
     "< grep -e 'list-none-s,[0-9]*,1000,16,' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '16 lists' with linespoints lc rgb 'yellow'
#!/bin/sh

#lab2b_1 and lab2b_2:
threads_1=( 1, 2, 4, 8, 12, 16, 24 )

for k in "${threads_1[@]}"
    do 
        ./lab2_list --threads=$k --iterations=1000 --sync=m
    done

for k in "${threads_1[@]}"
    do 
        ./lab2_list --threads=$k --iterations=1000 --sync=s
    done

#lab2b_3:
threads_3=( 1, 4, 8, 12, 16 )
iterations_3=( 1, 2, 4, 8, 16 )
iterations_3_y=( 10, 20, 40, 80 )

for i in "${iterations_3[@]}"
   do
       for k in "${threads_3[@]}"
           do 
               ./lab2_list --threads=$k --iterations=$i --yield=id --lists=4
           done
   done

for i in "${iterations_3_y[@]}"
    do
        for k in "${threads_3[@]}"
            do 
                ./lab2_list --threads=$k --iterations=$i --yield=id --sync=m --lists=4
            done
    done

for i in "${iterations_3_y[@]}"
    do
        for k in "${threads_3[@]}"
            do 
                ./lab2_list --threads=$k --iterations=$i --yield=id --sync=s --lists=4
            done
    done

#lab2b_4:
threads_4_5=( 1, 2, 4, 8, 12 )
lists_4_5=( 4, 8, 16 )

for i in "${lists_4_5[@]}"
    do
        for k in "${threads_4_5[@]}"
            do 
                ./lab2_list --threads=$k --iterations=1000 --lists=$i --sync=m 
            done
    done

#lab2b_5:
threads_4_5=( 1, 2, 4, 8, 12 )
lists_4_5=( 4, 8, 16 )

for i in "${lists_4_5[@]}"
    do
        for k in "${threads_4_5[@]}"
            do 
                ./lab2_list --threads=$k --iterations=1000 --lists=$i --sync=s
            done
    done
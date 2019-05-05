#!/bin/sh

#list-1:
iterations_1=( 10, 100, 1000, 10000, 20000 )

for i in "${iterations_1[@]}"
    do
        ./lab2_list --iterations=$i
    done

#list-2:
iterations_2=( 1, 10, 100, 1000 )
iterations_2_y=( 1, 2, 4, 8, 16, 32 )
threads_2=( 2, 4, 8, 12 )

for i in "${iterations_2[@]}"
    do
        for k in "${threads_2[@]}"
            do 
                ./lab2_list --threads=$k --iterations=$i
            done
    done

for i in "${iterations_2_y[@]}"
    do
        for k in "${threads_2[@]}"
            do 
                ./lab2_list --threads=$k --iterations=$i --yield=i
            done
    done

for i in "${iterations_2_y[@]}"
    do
        for k in "${threads_2[@]}"
            do 
                ./lab2_list --threads=$k --iterations=$i --yield=d
            done
    done

for i in "${iterations_2_y[@]}"
    do
        for k in "${threads_2[@]}"
            do 
                ./lab2_list --threads=$k --iterations=$i --yield=il
            done
    done

for i in "${iterations_2_y[@]}"
    do
        for k in "${threads_2[@]}"
            do 
                ./lab2_list --threads=$k --iterations=$i --yield=dl
            done
    done

#list 3:
iterations_3=( 1, 2, 4, 8, 16, 32 )
threads_3=( 2, 4, 6, 8, 12 )

for i in "${iterations_3[@]}"
    do
        for k in "${threads_3[@]}"
            do 
                ./lab2_list --threads=$k --iterations=$i --yield=i --sync=m
            done
    done

for i in "${iterations_3[@]}"
    do
        for k in "${threads_3[@]}"
            do 
                ./lab2_list --threads=$k --iterations=$i --yield=d --sync=m
            done
    done

for i in "${iterations_3[@]}"
    do
        for k in "${threads_3[@]}"
            do 
                ./lab2_list --threads=$k --iterations=$i --yield=il --sync=m
            done
    done

for i in "${iterations_3[@]}"
    do
        for k in "${threads_3[@]}"
            do 
                ./lab2_list --threads=$k --iterations=$i --yield=dl --sync=m
            done
    done

for i in "${iterations_3[@]}"
    do
        for k in "${threads_3[@]}"
            do 
                ./lab2_list --threads=$k --iterations=$i --yield=i --sync=s
            done
    done

for i in "${iterations_3[@]}"
    do
        for k in "${threads_3[@]}"
            do 
                ./lab2_list --threads=$k --iterations=$i --yield=d --sync=s
            done
    done

for i in "${iterations_3[@]}"
    do
        for k in "${threads_3[@]}"
            do 
                ./lab2_list --threads=$k --iterations=$i --yield=il --sync=s
            done
    done

for i in "${iterations_3[@]}"
    do
        for k in "${threads_3[@]}"
            do 
                ./lab2_list --threads=$k --iterations=$i --yield=dl --sync=s
            done
    done

#list-4:
threads_4=( 1, 2, 4, 8, 12, 16, 24 )

for i in "${threads_4[@]}"
    do
        ./lab2_list --threads=$i --iterations=1000 --sync=m
    done

for i in "${threads_4[@]}"
    do
        ./lab2_list --threads=$i --iterations=1000 --sync=s
    done


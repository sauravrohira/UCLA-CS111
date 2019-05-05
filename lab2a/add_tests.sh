#!/bin/bash


#add-none
#iterations_initial=( 100, 1000, 10000, 100000 )
#threads_initial=( 2, 4, 6, 8, 10, 12, 14, 16 )
#for i in "${iterations_initial[@]}"
#    do
#        for k in "${threads_initial[@]}"
#            do 
#                for j in {1..5}
#                    do
#                        ./lab2_add --threads=$k --iterations=$i
#                   done
#            done
#    done

#add-1
iterations_1=( 10, 20, 40, 80, 100, 1000, 10000, 100000 )
threads_1=( 2, 4, 8, 12 )

for i in "${iterations_1[@]}"
    do
        for k in "${threads_1[@]}"
            do 
                ./lab2_add --threads=$k --iterations=$i --yield
            done
    done

#add-2:
iterations_2=( 100, 1000, 10000, 100000 )
threads_2=( 2, 8 )

for i in "${iterations_2[@]}"
    do
        for k in "${threads_2[@]}"
            do 
                ./lab2_add --threads=$k --iterations=$i --yield
            done
    done

for i in "${iterations_2[@]}"
    do
        for k in "${threads_2[@]}"
            do 
                ./lab2_add --threads=$k --iterations=$i 
            done
    done

#add-3:
iterations_3=( 10, 20, 40, 80, 100, 1000, 10000, 100000 )

for i in "${iterations_3[@]}"
    do
        ./lab2_add --iterations=$i
    done

#add-4:

iterations_4=( 10, 20, 40, 80, 100, 1000, 10000)
iterations_4_s=( 10, 20, 40, 80, 100, 1000 )
threads_4=( 2, 4, 8, 12 )

 for i in "${iterations_4[@]}"
    do
        for k in "${threads_4[@]}"
            do 
                ./lab2_add --threads=$k --iterations=$i --yield
            done
    done

for i in "${iterations_4[@]}"
    do
        for k in "${threads_4[@]}"
            do 
                ./lab2_add --threads=$k --iterations=$i --yield --sync=m
            done
    done

for i in "${iterations_4_s[@]}"
    do
        for k in "${threads_4[@]}"
            do 
                ./lab2_add --threads=$k --iterations=$i --yield --sync=s
            done
    done

for i in "${iterations_4[@]}"
    do
        for k in "${threads_4[@]}"
            do 
                ./lab2_add --threads=$k --iterations=$i --yield --sync=c
            done
    done

#add-5:
threads_5=( 1, 2, 4, 8, 12 )

for i in "${threads_5[@]}"
    do 
        ./lab2_add --threads=$i --iterations=10000 
    done

for i in "${threads_5[@]}"
    do 
        ./lab2_add --threads=$i --iterations=10000 --sync=m
    done

for i in "${threads_5[@]}"
    do 
        ./lab2_add --threads=$i --iterations=10000 --sync=s
    done

for i in "${threads_5[@]}"
    do 
        ./lab2_add --threads=$i --iterations=10000 --sync=c
    done
#!/bin/bash

function run_test() {
    for nthr in 1 2 4 8 16 32; do
        ./$1 $nthr > /dev/null
        echo "Number of threads are $nthr"
        #for i in `seq 1 3`; do
            ./$1 $nthr
        #done
        echo
    done
}


echo "Test DEQ using TTAS lock"
run_test "test-deq-lock"

echo "Test DEQ using lock Free Algorithm"
run_test "test-deq-lockfree"

#echo "TEST WAIT FREE FIFO QUEUE"
#run_test "test-waitfree-fifo"




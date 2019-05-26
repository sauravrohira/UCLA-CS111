#!/bin/bash

./lab4b --period=1 --scale=C --log=test.txt 2> error.txt <<-EOF
SCALE=F
PERIOD=2
STOP
START
LOG testing
OFF
EOF

if [ $? == 0 ] && [ -s test.txt ] && [ ! -s error.txt ] && grep -q [0-9][0-9]\:[0-9][0-9]\:[0-9][0-9] test.txt && grep -q "SHUTDOWN" test.txt
then 
    echo "PASSES SMOKETEST"
else 
    echo "FAILS SMOKETEST"
fi

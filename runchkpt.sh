#!/bin/bash

echo "############# Compiling"
make
rm -f *.db *.rel

echo "############# Running checkpoint"
./checkpt
echo " "
sleep 1
rm -f *.db *.rel


#!/bin/bash
echo "############# Compiling"
make
./cleanup.sh

echo "############# Running checkpoint"
./checkpt
echo " "
sleep 1
./cleanup.sh

echo "############ Running HeapFile unittests"
./fileunittests
echo " "
sleep 1
./cleanup.sh

echo "############ Running HeapFileScanner unittests"
./filescannerunittests
echo " "
sleep 1
./cleanup.sh

echo "############ Running checksaved"
./checksaved
echo " "
sleep 1
./cleanup.sh

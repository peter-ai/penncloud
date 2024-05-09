#!/bin/bash

# compile coordinator server
cd coordinator
make clean
make
./coordinator -s 2 -b 3
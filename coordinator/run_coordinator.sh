#!/bin/bash

# compile coordinator server
cd coordinator
make clean
make
./coordinator -s 1 -b 3
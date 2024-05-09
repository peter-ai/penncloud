#!/bin/bash

# compile coordinator server
cd coordinator
make clean
make
./coordinator -s 3 -b 3
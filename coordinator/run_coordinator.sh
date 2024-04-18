#!/bin/bash

# compile coordinator server
cd coordinator
make clean
make
./coordinator -v -s 1
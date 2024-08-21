#!/bin/bash

cd loadbalancer  # This line is commented out or removed
make clean
make
./loadbalancer_main 3 #change # to the specified number of frontend servers that will be ran
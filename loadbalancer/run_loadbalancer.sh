#!/bin/bash

# compile http server
cd ../http_server
make clean
make 
cd ..

cd loadbalancer  # This line is commented out or removed
make clean
make
./loadbalancer_main 1 #change # to the specified number of frontend servers that will be ran
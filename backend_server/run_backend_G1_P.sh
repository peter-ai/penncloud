#!/bin/bash

# compile utils
cd ../utils
make clean
make
cd ..

# compile routes
cd backend_server
make clean
make
rm -rf KVS_6100
mkdir KVS_6100
./backend_main -c 6100 -t 5
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
rm -rf KVS_6200
mkdir KVS_6200
./backend_main -c 6200 -t 5
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
rm -rf KVS_6001
mkdir KVS_6001
./backend_main -c 6001 -t 5
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
rm -rf KVS_6000
mkdir KVS_6000
./backend_main -c 6000 -t 5
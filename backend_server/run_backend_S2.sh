#!/bin/bash

# # compile utils
# cd ../utils
# make clean
# make
# cd ..

# # compile routes
# cd backend_server
# make clean
# make
rm -rf KVS_6020
mkdir KVS_6020
./backend_main -c 6020 -t 5
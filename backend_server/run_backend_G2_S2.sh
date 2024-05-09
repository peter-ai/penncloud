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
rm -rf KVS_6220
mkdir KVS_6220
./backend_main -c 6220 -t 5
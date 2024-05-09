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
rm -rf KVS_6130
mkdir KVS_6130
./backend_main -c 6130 -t 5
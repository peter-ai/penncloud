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
rm -rf KVS_6230
mkdir KVS_6230
./backend_main -c 6230 -t 5
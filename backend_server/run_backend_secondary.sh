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
rm -rf KVS_6010
mkdir KVS_6010
./backend_main -c 6010 -t 5
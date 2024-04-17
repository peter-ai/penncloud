#!/bin/bash

# make sure you're in the example_frontend directory

# compile utils
cd ../utils
make clean
make
cd ..

# compile routes
cd backend_server
make clean
make
./backend_main -c 4999 -p 6000 -s a -e z -t 5
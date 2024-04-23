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
./backend_main -p 6000 -t 5
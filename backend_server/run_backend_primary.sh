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
./backend_main -c 6001 -g 7501 -t 5
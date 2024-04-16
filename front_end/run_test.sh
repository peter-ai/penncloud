#!/bin/bash

# make sure you're in the example_frontend directory

# compile utils
cd ../utils
make clean
make
cd ..

# compile http server as library
cd http_server
make clean
make
cd ..

# compile coordinator server
cd coordinator
make clean
make
cd ..

# compile routes
cd front_end
make clean
make
./auth_routes
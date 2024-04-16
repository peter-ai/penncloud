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

cd front_end
make clean
make
cd ./main


# # compile routes
# cd example_frontend
# make clean
# make
# ./routes




#!/bin/bash

cd utils
make clean
make
cd ..

cd http_server
make clean
make
cd ..

make clean
make
./test
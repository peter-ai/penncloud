#!/bin/bash

# compile http server
cd ../http_server
make clean
make 
cd ..

# compile front end main and run
cd front_end
make clean
make
./frontend_main -p 8001

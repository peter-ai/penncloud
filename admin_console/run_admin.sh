#!/bin/bash

# compile http server
cd http_server
make clean
make 
cd ..

# compile admin console main and run
cd admin_console
make clean
make
./admin_main

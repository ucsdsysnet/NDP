#!/bin/bash

make clean
make
pushd datacenter
make clean
make htsim_dctcp_host_delay
popd

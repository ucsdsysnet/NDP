#!/bin/bash

SEED=`date +%N`
FLOWSIZE=20
NODES=54
SSTHRESH=15
NCONNS=(100 200 300 400 500 600 700 800 900 1000 1100 1200 1300 1400 1500 1600 1700 1800 1900 2000 2100 2200 2300 2400 2500 2600 2700)
DELAYS=(1 3 5 10 15)
PERCENTILE=90

echo "seed="$SEED " flowsize="$FLOWSIZE" nodes=" $NODES " ssthresh="$SSTHRESH " percentile="$PERCENTILE

for delay in ${DELAYS[@]}; do
SAVEDIR=./results/nodes_"$NODES"_flowsize_"$FLOWSIZE"_ssthresh_"$SSTHRESH"_delay_$delay
mkdir -p $SAVEDIR
    for nconn in ${NCONNS[@]}; do
        echo "Running delay="$delay", nconn="$nconn"..."
        ../datacenter/htsim_dctcp_host_delay -seed $SEED -nodes $NODES -conns $nconn -sub 1 -ssthresh $SSTHRESH -flowsize $FLOWSIZE > $SAVEDIR/nconn_$nconn.out
    done
./compile_data.py $SAVEDIR/*.out -p $PERCENTILE -o ./results/nodes_"$NODES"_flowsize_"$FLOWSIZE"_ssthresh_"$SSTHRESH"_delay_"$delay"_percentile_"$PERCENTILE".data
done

./graph_data.py ./results/nodes_"$NODES"_flowsize_"$FLOWSIZE"_ssthresh_"$SSTHRESH"*.data -o nodes_"$NODES"_flowsize_"$FLOWSIZE"_ssthresh_"$SSTHRESH"_percentile_"$PERCENTILE".png -t "nodes="$NODES" flowsize="$FLOWSIZE" ssthresh="$SSTHRESH
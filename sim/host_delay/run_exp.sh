#!/bin/bash

SEED=`date +%N`
FLOWSIZE=20
NODES=54
SSTHRESH=15
RTO=10
NCONNS=(100 200 300 400 500 600 700 800 900 1000)
DELAYS=(1 3 5 10 15)
PERCENTILE=90

echo "seed="$SEED " flowsize="$FLOWSIZE" nodes=" $NODES " ssthresh="$SSTHRESH " rto="$RTO " percentile="$PERCENTILE

for delay in ${DELAYS[@]}; do
SAVEDIR=./results/nodes_"$NODES"_flowsize_"$FLOWSIZE"_ssthresh_"$SSTHRESH"_rto_"$RTO"_delay_"$delay"
mkdir -p $SAVEDIR
    for nconn in ${NCONNS[@]}; do
        echo "Running delay="$delay", nconn="$nconn"..."
        out_file=$SAVEDIR/nconn_"$nconn".out
        log_file=$SAVEDIR/nconn_"$nconn".log
        ../datacenter/htsim_dctcp_host_delay -o $out_file -seed $SEED -nodes $NODES -conns $nconn -sub 1 -ssthresh $SSTHRESH -rto $RTO -flowsize $FLOWSIZE -ackdelay $delay > $log_file
    done
./compile_data.py $SAVEDIR/*.log $SAVEDIR/*.out -p $PERCENTILE -o ./results/nodes_"$NODES"_flowsize_"$FLOWSIZE"_ssthresh_"$SSTHRESH"_rto_"$RTO"_delay_"$delay"_percentile_"$PERCENTILE".data
done

./graph_data.py ./results/nodes_"$NODES"_flowsize_"$FLOWSIZE"_ssthresh_"$SSTHRESH"_rto_"$RTO"*.data -o nodes_"$NODES"_flowsize_"$FLOWSIZE"_ssthresh_"$SSTHRESH"_rto_"$RTO"_percentile_"$PERCENTILE".png -t "nodes="$NODES" flowsize="$FLOWSIZE" ssthresh="$SSTHRESH

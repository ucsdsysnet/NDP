#!/usr/bin/env python3
import argparse
import re
import numpy as np

def get_percentile(data, percentile=90):
    return np.percentile(data, percentile)

def parse_args():
    parser = argparse.ArgumentParser(description="compile the data from a test into a graph")
    parser.add_argument("input_files", type=str, nargs='+',
                        help="input files to process")
    parser.add_argument("-o", "--output", type=str, default="graph.png",
                        help="output line data filename")
    parser.add_argument("-p", "--percentile", type=int, default=90,
                        help="percentile to calculate for CDF")
    return parser.parse_args()


def parse_file(file, percentile):
    matcher = re.compile("Flow (?P<flowname>\S+) (?P<flowsize>\d+) finished after (?P<flowtime>\S+) us")

    flow_times = []
    with open(file, "r") as f:
        for line in f:
            if not line.startswith("Flow ") or "tcpsrc" in line:
                continue

            flow_results = matcher.match(line).groupdict()
            if not flow_results:
                continue

            f_name = flow_results["flowname"]
            f_size = flow_results["flowsize"]
            f_time = flow_results["flowtime"]

            flow_times.append(float(f_time))
    return (len(flow_times), get_percentile(flow_times, percentile))


def write_output_data(output_data, output_file):
    with open(output_file, "w") as f:
        f.write(str(output_data))


if __name__ == "__main__":
    args = parse_args()
    input_files = args.input_files
    output_data = []
    for file in input_files:
        n_flows, result = parse_file(file, args.percentile)
        output_data.append((n_flows, result))
    output_data = sorted(output_data, key=lambda x: x[0])
    write_output_data(output_data, args.output)

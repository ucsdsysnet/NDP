#!/usr/bin/env python3
import argparse
import ast
import re
import graph_utils

percentile=0

def parse_args():
    parser = argparse.ArgumentParser(description="Graph output data from flow experiments")
    parser.add_argument("input_files", type=str, nargs="+",
                        help="Input data files to graph")
    parser.add_argument("-o", "--output", type=str, default="graph.png",
                        help="Output filename to save graph")
    parser.add_argument("-t", "--title", type=str, default="Flow completion time percentiles by number of connections",
                        help="Graph title")
    return parser.parse_args()


def parse_data_file(file):
    matcher = re.compile(".*nodes_(?P<nodes>\d+)_flowsize_(?P<flowsize>\d+)_ssthresh_(?P<ssthresh>\d+)_delay_(?P<delay>\d+)_percentile_(?P<percentile>\d+).*")
    file_info = matcher.match(file)
    if not file_info:
        import pdb; pdb.set_trace()
    percentile = int(file_info.groupdict()["percentile"])

    with open(file, "r") as f:
        data = f.read()
        line_data = ast.literal_eval(data)
        return (zip(*line_data), "%s us" % file_info.groupdict()["delay"])


if __name__ == "__main__":
    args = parse_args()
    input_files = args.input_files
    exps = {}
    for file in input_files:
        line, label = parse_data_file(file)
        exp = {'line': line, 'label': label}
        exps[file] = exp
    graph_utils.plot_results(exps, args.output, title=args.title,
                             xlabel="Number of flows", ylabel="%dth percentile FCT (us)" % percentile,
                             markersize=0.0, legend_title="ACK Delay (us)")

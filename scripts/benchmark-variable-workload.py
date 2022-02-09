import argparse
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
from matplotlib.patches import ConnectionPatch
import subprocess
import re
import pickle
import json

from scipy.interpolate import interp1d
from os import chdir, environ
from pathlib import Path
from datetime import datetime
from itertools import chain
from collections import deque


class Bench:
    def __init__(self, struct, args, path, show, save_plots, save_data, debug, test, per_ts, window_size, title, runs, static):
        self.struct = struct
        self.args = args
        self.path = path
        self.show = show
        self.save_plots = save_plots
        self.save_data = save_data
        self.debug = debug
        self.test = test
        self.per_ts = per_ts
        self.window_size = window_size
        self.title = title
        self.runs = runs
        self.static = static

    def compile(self, relaxation):
        # Compile the test for all of the structs, also check thah they exist
        my_env = environ.copy()
        if relaxation:
            my_env["RELAXATION_ANALYSIS"] = "1"

        my_env["OPS_PER_TS"] = str(self.per_ts)
        if self.debug:
            my_env["VERSION"] = 'O3'
        else:
            my_env["VERSION"] = 'O4'

        my_env["TEST"] = self.test

        try:
             subprocess.check_output(['make',  f'{self.struct}'], env=my_env)
        except Exception as e:
            exit(e)

    def extend_duration(self):
        self.args = [entry if entry.strip().split()[0] != "-d" else f"-d {int(entry.strip().split()[1])*self.extend}" for entry in self.args]

    def reset_duration(self):
        self.args = [entry if entry.strip().split()[0] != "-d" else f"-d {int(entry.strip().split()[1])/self.extend}" for entry in self.args]

    def run_test(self):
        program_path = get_root_path() / 'bin' / self.struct
        args = [program_path] + self.args
        test_out = None

        for i in range(3):
            try:
                test_out = subprocess.check_output(args, timeout=600).decode('utf8')
                null_returns = int(re.search(rf"Null_Count , (\d+)", test_out).group(1))

                if null_returns == 0:
                    break

            except Exception as e:
                print(f'failed run due to:\n{e}')
                pass

        return test_out


    def parse_throughput(self, output):
        # Parses out the throughput per thread at each moment, as well as the width
        # Timestamps start at 0
        # List of (timestamp, throughput, window width)
        nbr_threads = int(re.search(r"num_threads , (\d+)", output)[1])
        # nbr_prod = int(re.search(r"Nbr producers: (\d+)", output)[1])
        ops_per_timestamp = int(re.search(r"Updates per timestamp: (\d+)", output)[1])

        producers = []
        consumers = []
        for thread in range(nbr_threads):
            prod_stamps = [(int(ts), int(width)) for (ts, width) in re.findall(rf"\[{thread}\] prod timestamp \d+: (\d+) ns, (\d+) ", output)]
            cons_stamps = [(int(ts), int(width)) for (ts, width) in re.findall(rf"\[{thread}\] cons timestamp \d+: (\d+) ns, (\d+) ", output)]

            if prod_stamps:
                stamps = prod_stamps
                prod = True
            else:
                stamps = cons_stamps
                prod = False

            piecewise_throughputs = []

            for ((t_at, w_at), (t_next, _)) in zip(stamps, stamps[1:]):
                if t_next == 1 and t_at == 1:
                    print("WARNING: INTERNAL ISSUE with two successive sleeps in C output")
                if t_next == 1:
                    # We have hit the start of a sleep
                    piecewise_throughputs.append((t_at/1e9, 0, 0))
                elif t_at == 1:
                    # We have hit the end of a stop
                    piecewise_throughputs.append(((t_next - 2)/1e9, 0, 0))
                else:
                    # Just add some normal throughput
                    piecewise_throughputs.append((t_at/1e9, ops_per_timestamp/(t_next - t_at)*1e9, w_at))

            if prod:
                producers.append(piecewise_throughputs)
            else:
                consumers.append(piecewise_throughputs)

        start_ts = min(thread[0][0] for thread in chain(producers, consumers))
        producers = [[(ts - start_ts, through, width) for (ts, through, width) in thread] for thread in producers]
        consumers = [[(ts - start_ts, through, width) for (ts, through, width) in thread] for thread in consumers]

        return producers, consumers

    def save(self, producer_lat_ts, consumer_lat_ts, producer_latency, consumer_latency, producer_width, consumer_width, producer_active, consumer_active):
        if self.save_data:
            self.path.mkdir(parents=True)
            # Takes a while to save all this, so for a while we just skip it...
            with open(self.path / 'performance.json', 'w') as f:
              json.dump({
                  "producer_lat_ts": list(producer_lat_ts),
                  "consumer_lat_ts": list(consumer_lat_ts),
                  "producer_latency": list(producer_latency),
                  "consumer_latency": list(consumer_latency),
                  "producer_width": list(producer_width),
                  "consumer_width": list(consumer_width),
                  "producer_active": list(producer_active),
                  "consumer_active": list(consumer_active)
                }, f, indent=4)

            with open(self.path / 'bench', 'wb') as f:
                pickle.dump(self, f, -1)

            with open(self.path / 'bench_dict.txt', 'w') as f:
                f.write(json.dumps({k:str(v) for (k, v) in self.__dict__.items()}, indent=2))


    def plot(self, producer_lat_ts, consumer_lat_ts, producer_latency, consumer_latency, producer_width, consumer_width, producer_active, consumer_active):
        if self.__dict__.get('static', False):
            producer_width = consumer_width

        # The number of samples at the beginning and end we ignore from the large plots
        prune = 100

        # Adjusted to fit article dimensions
        width = 800
        height = width * 0.66

        # Create a new figure with the specified dimensions
        fig = plt.figure(figsize=(width / 100, height / 100), dpi=100)

        # Create a GridSpec object for layout with 2 rows for the subfigures and 1 column
        gs = gridspec.GridSpec(2, 1, height_ratios=[1, 1])

        # Create the top subplot for ax2 and ax3
        ax2 = plt.subplot(gs[0])
        # Make ax2 and ax3 share the same x-axis
        ax3 = ax2.twinx()

        # Create the bottom subplot for ax1 which will share the x-axis with ax2 and ax3
        ax1 = plt.subplot(gs[1], sharex=ax2)

        # Text to main plot (bottom subplot)
        ax2.set_title(self.title, fontsize=26)
        ax1.set_ylabel("Latency", fontsize=20)
        ax1.set_xlabel("Time (s)", fontsize=20)

        # ax2 settings (left y-axis of top subplot)
        ax2.set_ylabel("Rank Error", fontsize=20)
        color_ax2 = 'crimson'

        # Set the left axis as red?
        # ax2.tick_params(axis='y', labelcolor=color_ax2, color=color_ax2)
        # ax3.spines['left'].set_color(color_ax2) # Have to use ax3 as it overwrites this spine when we twinx
        # ax2.yaxis.label.set_color(color_ax2)

        # ax3 settings (right y-axis of top subplot)
        ax3.set_ylabel("Active producers", fontsize=20)
        color_ax3 = 'limegreen'
        ax3.tick_params(axis='y', labelcolor=color_ax3, color=color_ax3)
        ax3.spines['right'].set_color(color_ax3)
        ax3.yaxis.label.set_color(color_ax3)

        # Hide x-axis label of top subplot to avoid overlap
        plt.setp(ax2.get_xticklabels(), visible=False)

        # Plot latency
        ax1.plot(producer_lat_ts[prune:-prune], producer_latency[prune:-prune], alpha=0.8, label='Producer latency', color='navy')
        ax1.plot(consumer_lat_ts[prune:-prune], consumer_latency[prune:-prune], alpha=0.8, label='Consumer latency', color='maroon')

        # Plot the relaxation bound
        depth = int([arg.strip().split(' ')[-1] for arg in self.args if "-l " in arg][0])
        ax2.plot(consumer_lat_ts[prune:-prune], [(w-1)*depth for w in consumer_width[prune:-prune]], alpha=0.8, label='Rank error bound', color=color_ax2)
        ax2.plot(producer_lat_ts[prune:-prune], [(w-1)*depth for w in producer_width[prune:-prune]], alpha=0.8, label='Tail error', color='orange')

        # Plot number of active producer consumers
        ax3.plot(producer_lat_ts[prune:-prune], producer_active[prune:-prune], alpha=0.7, label='Producers active', linestyle=(0, (1, 1)), linewidth=2, color=color_ax3)
        # ax3.plot(consumer_lat_ts[prune:-prune], consumer_active[prune:-prune], alpha=0.8, label='Consumers active', linestyle=":", color=color_ax3)

        ax1.legend(loc='upper left')
        ax2.legend(loc='upper left')

        plt.tight_layout()
        if self.save_plots:
            self.path.mkdir(parents=True, exist_ok=True)
            plt.savefig(self.path / f'{self.path.name}.pdf', format = 'pdf')

        if self.show:
            plt.show()

def median_window(data, window_size):
    """
    Slides a median window over the input, of width (2*window_size + 1) and
    outputs the smoothed series as a numpy array (preserving the length).
    """
    # Convert input to a numpy array to handle both lists and pandas 1D arrays
    data_array = np.array(data)

    # Prepare an array to store the result with the same shape as the input
    result = np.empty_like(data_array, dtype=float)

    # Calculate the median for each position considering the window size
    for i in range(len(data_array)):
        # Determine the start and end indices for the window
        start_index = max(0, i - window_size)
        end_index = min(len(data_array), i + window_size + 1)

        # Calculate the median for the current window
        result[i] = np.median(data_array[start_index:end_index])

    return result

def throughputs_to_avg_latency_and_width(self, ts_runs, samples=5_000):
    """
    Aggregates timestamped throughput samples from threads to a single averaged timestamped array of average latency,
    as wel as one for the window width

    Parameters:
    - ts_runs: List of
        - list of tuples:
            0: Timestamp
            1: Throughput from that timestamp until the next one
            2: Width from that timestamp until the next one

    Returns:
    - A list of timestamps
    - A list of average latency for each timestamp
    - A list of average window width for each timestamp
    - A list of the number of active threads at each instant
    """

    def count_nonzero(val):
        tot = 0
        for item in val:
            if item > 0.0001:
                tot += 1
        return tot

    nbr_runs = len(ts_runs)
    run_ts_samples = [thread for run in ts_runs for thread in run]

    # The interpolated throughputs
    thread_throughputs = [interp1d([pair[0] for pair in thread_ts], [pair[1] for pair in thread_ts], bounds_error=False, fill_value=0) for thread_ts in run_ts_samples]
    thread_widths = [interp1d([pair[0] for pair in thread_ts], [pair[2] for pair in thread_ts], bounds_error=False, fill_value=0) for thread_ts in run_ts_samples]
    # thread_activity = [interp1d([pair[0] for pair in thread_ts], [nonzero(pair[1]) for pair in thread_ts], bounds_error=False, fill_value=0) for thread_ts in ts_throughputs]

    t_start = min([thread[0][0] for thread in run_ts_samples])
    t_end = max([thread[-1][0] for thread in run_ts_samples])

    timestamps = np.linspace(t_start, t_end, samples)

    avg_latency = np.copy(timestamps)
    avg_width = np.copy(timestamps)
    actives = np.zeros(timestamps.shape)
    for (i, ts) in enumerate(timestamps):
        throughput_values = [f(ts).item() for f in thread_throughputs]
        width_values = [f(ts).item() for f in thread_widths]
        tot_through = sum(throughput_values)
        tot_width = sum(width_values)
        active = count_nonzero(throughput_values)
        actives[i] = active/nbr_runs

        if tot_through == 0:
            avg_latency[i] = 0
            avg_width[i] = 0
        else:
            avg_latency[i] = active/tot_through
            avg_width[i] = tot_width/active

    return timestamps, avg_latency, avg_width, actives


def get_root_path():
    return Path(__file__).parent.parent


def main(test_bench, old_folder):
    chdir(get_root_path())
    if old_folder is None:
        test_bench.compile(False)
        producer_samples, consumer_samples = zip(*[test_bench.parse_throughput(test_bench.run_test()) for _ in range(test_bench.runs)])

        producer_lat_ts, producer_avg_latency, producer_avg_width, producer_active = throughputs_to_avg_latency_and_width(test_bench, producer_samples)
        consumer_lat_ts, consumer_avg_latency, consumer_avg_width, consumer_active = throughputs_to_avg_latency_and_width(test_bench, consumer_samples)
        producer_smoothed_avg_latency = median_window(producer_avg_latency, 5)
        consumer_smoothed_avg_latency = median_window(consumer_avg_latency, 5)
        producer_smoothed_avg_width = median_window(producer_avg_width, 5)
        consumer_smoothed_avg_width = median_window(consumer_avg_width, 5)

        test_bench.save(producer_lat_ts, consumer_lat_ts, producer_smoothed_avg_latency, consumer_smoothed_avg_latency, producer_smoothed_avg_width, consumer_smoothed_avg_width, producer_active, consumer_active)
        test_bench.plot(producer_lat_ts, consumer_lat_ts, producer_smoothed_avg_latency, consumer_smoothed_avg_latency, producer_smoothed_avg_width, consumer_smoothed_avg_width, producer_active, consumer_active)
    else:
        path = Path(old_folder)
        old_bench = load_old_bench(path)
        if test_bench.title:
            old_bench.title = test_bench.title
        if test_bench.__dict__.get('static', False):
            old_bench.static = True
        with open(path / "performance.json", 'r') as f:
            perf_dict = json.load(f)
        old_bench.plot(
            perf_dict["producer_lat_ts"],
            perf_dict["consumer_lat_ts"],
            perf_dict["producer_latency"],
            perf_dict["consumer_latency"],
            perf_dict["producer_width"],
            perf_dict["consumer_width"],
            perf_dict["producer_active"],
            perf_dict["consumer_active"])

def load_old_bench(old_bench_path):
    with open(old_bench_path / "bench", 'rb') as f:
        old_bench = pickle.load(f)

    old_bench.save_plots = True
    old_bench.show = True
    old_bench.path = Path(f"{old_bench_path}-replot-{datetime.now().strftime('%H:%M:%S')}")
    return old_bench


def parse_args():
    # This became really ugly. Contemplating just rewriting in Rust to use the wonderful Clap crate :)
    parser = argparse.ArgumentParser(description='Benchmark throughput over time for one structure, for one run.')
    parser.add_argument('struct',
                        help='Data structure to benchmark, must have the test implemented')
    parser.add_argument('--name',
                        help='The folder within results to save the data in')
    parser.add_argument('--show', action='store_true',
                        help='Flag to show the generated graph')
    parser.add_argument('--nosave-plots', action='store_true',
                        help='Flag to not save the resulting plots')
    parser.add_argument('--nosave-data', action='store_true',
                        help='Flag to not save the whole output from tests (quite large amount of printouts)')
    parser.add_argument("--args", nargs="*", default=[],
                        help="Arguments to pass to the underlying benchmark cli call")
    parser.add_argument('--debug', action='store_true', #type=bool,
                        help='Flag to enable asserts, some of which can be very costly')
    parser.add_argument('--ops-per-ts', default=50, type=float,
                        help='How many operations should be done per taken timestamp (for both throughput and relaxation)')
    parser.add_argument('--window-size', default=0.025, type=float,
                        help='How wide should the MA window be for the plots?')
    parser.add_argument('--title', default="Over time benchmark",
                        help='What title to have for the plot')
    parser.add_argument('--old-folder',
                        help='An old folder to use for results (use settings from cmd line)')
    parser.add_argument('--runs', default=5, type=int,
                        help='how many runs to aggregate values from')
    parser.add_argument('--test', default='variable-workload',
                        help='which test to run')
    parser.add_argument('--static', action='store_true', #type=bool,
                        help='Force the width to always be constant (some edge case bug at producer activity edges)')

    args = parser.parse_args()

    datestr = datetime.now().strftime("%Y-%m-%d")
    path = get_root_path() / 'results'
    if args.name is None:
        path = path / f'{datestr}_{args.test}-{",".join(args.struct)}'
    else:
        path = path / f'{datestr}_{args.name}'

    if path.exists():
        timestr = datetime.now().strftime('%H:%M:%S')
        path = path.parent / f"{path.name}_{timestr}"
        # print(f'Path exists, so specifying datetime: {path}')

    bench = Bench(args.struct, args.args, path, args.show, not args.nosave_plots, not args.nosave_data, args.debug, args.test,
                  args.ops_per_ts, args.window_size, args.title, args.runs, args.static)

    return bench, args.old_folder

if __name__ == '__main__':
    main(*parse_args())


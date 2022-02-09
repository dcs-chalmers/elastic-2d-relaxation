import argparse
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
from matplotlib.patches import ConnectionPatch
import subprocess
import re
import pickle
import json

from os import chdir, environ
from pathlib import Path
from datetime import datetime
from itertools import chain
from collections import deque


class Bench:
    def __init__(self, struct, args, path, show, save_plots, save_data, debug, test, per_ts, window_size, title, extend):
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
        self.extend = extend

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
        # Basically run up to three times in case it fails

        program_path = get_root_path() / 'bin' / self.struct
        args = [program_path] + self.args
        test_out = None

        for i in range(3):
            try:
                test_out = subprocess.check_output(args).decode('utf8')
                null_returns = re.search(rf"Null_Count , (\d+)", test_out)
                if int(null_returns.group(1)) > 0:
                    print(f"null! {i+1}/3")
                    continue
                else:
                    break
            except Exception as e:
                print(f'failed run due to:\n{e}')
                pass
        else:
            print(f"The test cannot complete without null returns.\nTry increasing the number of initial items. Otherwise, our bitshift rng might be imbalanced for your computer, and it might need to be manually adjusted.\nError when running {args}\n")
            exit(1)

        timestamps, throughputs, changes = self.parse_timestamps(test_out)
        if timestamps:
            return timestamps, throughputs, changes, test_out
        else:
            exit(f"could not find timestamps in {test_out}")



    def parse_timestamps(self, output):
        nbr_threads = int(re.search(r"num_threads , (\d+)", output)[1])
        ops_per_timestamp = int(re.search(r"Updates per timestamp: (\d+)", output)[1])

        changes = [(match) for match in re.findall(rf"Initiating change to width (\d+) and depth (\d+) at timestamp (\d+)", output)]

        if "average period err" not in output:
            timestamps = []
            for thread in range(nbr_threads):
                stamps = [int(stamp) for stamp in re.findall(rf"\[{thread}\] timestamp \d+: (\d+) ns", output)]
                timestamps.append(stamps)
            throughputs = []
            for stamps in timestamps:
                throughputs.append([ops_per_timestamp/(after-before)*1e9 for (before, after) in zip(stamps[:-2], stamps[1:-1])])

            measured = throughputs
            timestamps = [stamps[1:-1] for stamps in timestamps]
        else:
            timestamps = [int(stamp) for stamp in re.findall(r"\[AGG\] timestamp \d+: (\d+) ns", output)]
            errors = [float(err) for err in       re.findall(r"\[AGG\] timestamp \d+: \d+ ns, (\d*\.?\d*) av", output)]
            measured = errors[1:-1]
            timestamps = timestamps[1:-1]

        return timestamps, measured, changes



    def save(self, timestamps1, throughputs, changes1, timestamps2, relaxations, changes2):
        if self.save_data:
            self.path.mkdir(parents=True)
            # Takes a while to save all this, so for a while we just skip it...
            with open(self.path / 'performance.json', 'w') as f:
              json.dump({'timestamps1': timestamps1, 'throughputs': throughputs, 'changes1': changes1, 'timestamps2': timestamps2, 'relaxations': relaxations, 'changes2': changes2}, f, indent=4)

            with open(self.path / 'bench', 'wb') as f:
                pickle.dump(self, f, -1)

            with open(self.path / 'bench_dict.txt', 'w') as f:
                f.write(json.dumps({k:str(v) for (k, v) in self.__dict__.items()}, indent=2))

    def plot(self, timestamps1, throughputs, changes1, timestamps2, relaxations, changes2):

        # Adjusted to fit article dimensions
        width = 800
        height = width*(3/4)*(3/4)

        # Create a new figure with the specified dimensions
        fig = plt.figure(figsize=(width / 100, height / 100), dpi=100)
    
        # Create a GridSpec object for layout
        gs = gridspec.GridSpec(2, 2, width_ratios=[3, 1], height_ratios=[1, 1])
    
        # Create subplots
        ax1 = plt.subplot(gs[:, 0])
        zoomax1up = plt.subplot(gs[0, 1])
        zoomax1down = plt.subplot(gs[1, 1])
        zoomax2up = zoomax1up.twinx()
        zoomax2down = zoomax1down.twinx()

        # Make ticks smaller for zoomed plots
        zoomax1up.tick_params(axis='x', labelsize=9)  
        zoomax1down.tick_params(axis='x', labelsize=9)  
        zoomax1up.tick_params(axis='y', labelsize=9)  
        zoomax2up.tick_params(axis='y', labelsize=9)  
        zoomax1up.yaxis.offsetText.set_fontsize(8)
        zoomax2up.yaxis.offsetText.set_fontsize(8)
        zoomax1down.tick_params(axis='y', labelsize=9)  
        zoomax2down.tick_params(axis='y', labelsize=9)  
        zoomax1down.yaxis.offsetText.set_fontsize(8)
        zoomax2down.yaxis.offsetText.set_fontsize(8)
        
        # Text to main plot
        ax1.set_title(self.title, fontsize=20)
        ax1.set_ylabel("Throughput", fontsize=18)
        ax1.set_xlabel("Time (s)", fontsize=20)

        # Set xlim for main plot before twinx
        ax1.set_xlim((0, max([round((float(time) - float(changes1[0][-1]))/1e9) for (_, _, time) in changes1])))

        ax2 = ax1.twinx()

        # Color ax1
        color_ax1 = 'navy'
        ax1.tick_params(axis='y', labelcolor=color_ax1, color=color_ax1)
        ax2.spines['left'].set_color(color_ax1)
        ax1.yaxis.label.set_color(color_ax1)

        # Color ax2
        color_ax2 = 'crimson'
        ax2.set_ylabel("Rank error", fontsize=18)
        ax2.tick_params(axis='y', labelcolor=color_ax2, color=color_ax2)
        ax2.spines['right'].set_color(color_ax2) # Have to use ax3 as it overwrites this spine when we twinx
        ax2.yaxis.label.set_color(color_ax2)
       
        # Color zoomed plots
        zoomax1up.tick_params(axis='y', labelcolor=color_ax1, color=color_ax1)
        zoomax2up.spines['left'].set_color(color_ax1)
        zoomax1up.yaxis.label.set_color(color_ax1)
        zoomax1down.tick_params(axis='y', labelcolor=color_ax1, color=color_ax1)
        zoomax2down.spines['left'].set_color(color_ax1)
        zoomax1down.yaxis.label.set_color(color_ax1)

        zoomax2up.tick_params(axis='y', labelcolor=color_ax2, color=color_ax2)
        zoomax2up.spines['right'].set_color(color_ax2) # Have to use ax3 as it overwrites this spine when we twinx
        zoomax2up.yaxis.label.set_color(color_ax2)
        zoomax2down.tick_params(axis='y', labelcolor=color_ax2, color=color_ax2)
        zoomax2down.spines['right'].set_color(color_ax2) # Have to use ax3 as it overwrites this spine when we twinx
        zoomax2down.yaxis.label.set_color(color_ax2)
        
        # Adjust throughput timestamps
        start_ns1 = int(changes1[0][-1])
        timestamps1 = [[(stamp - start_ns1)/1e9 for stamp in stamps] for stamps in timestamps1]
        changes1 = [(width, depth, (int(stamp) - start_ns1)/1e9) for (width, depth, stamp) in changes1]

        # Aggregate throughputs
        agg_stamps_1, agg_throughputs = agg_samples(timestamps1, throughputs, self.window_size)
        agg_throughputs = [throughput*len(timestamps1) for throughput in agg_throughputs] # sum instead of average

        # Plot throughput
        ax1.set_ylim([0, max(agg_throughputs)*1.15])
        ax1.plot(agg_stamps_1, agg_throughputs, alpha=0.8, color=color_ax1)

        # Aggregate and plot relaxation
        timestamps2 = [(stamp - int(changes2[0][-1]))/1e9/self.extend for stamp in timestamps2]
        timestamps2, relaxations = agg_samples([timestamps2], [relaxations], self.window_size) # Just use the sliding window part
        changes2 = [(width, depth, (int(stamp) - int(changes2[0][-1]))/1e9/self.extend) for (width, depth, stamp) in changes2]
        ax2.set_ylim([0, max(relaxations)*2])
        ax2.plot(timestamps2, relaxations, linestyle=":", alpha=0.8, color=color_ax2)

        # Add change lines and text (from norlmal run)
        for (i, ((width, depth, start_time), (end_width, _, end_time))) in enumerate(zip(changes1, changes1[1:])):
            if end_width != "0":
                ax1.axvline(x=end_time, alpha=0.2, color='black')
                
            midx = (end_time + start_time)/2
            if i in [1, 2, 7]:
                fontsize = 5.8
                text_ypos = ax1.get_ylim()[1]*0.91
            elif i == 8:
                fontsize = 7
                text_ypos = ax1.get_ylim()[1]*0.905
            else:
                fontsize = 8
                text_ypos = ax1.get_ylim()[1]*0.9
            ax1.text(midx, text_ypos, f"{width}W\n{depth}D", ha='center', fontsize=fontsize)

        # Add change lines for relaxation
        for (_, _, time) in changes2[1:-1]:
            ax1.axvline(x=time, linestyle=':', alpha=0.2, color='black')


        ## Zoomed plots
        
        # Plot the data for both
        zoomax1up.plot(agg_stamps_1, agg_throughputs, alpha=0.8, color=color_ax1)
        zoomax1down.plot(agg_stamps_1, agg_throughputs, alpha=0.8, color=color_ax1)
        zoomax2up.plot(timestamps2, relaxations, linestyle=":", alpha=0.8, color=color_ax2)
        zoomax2down.plot(timestamps2, relaxations, linestyle=":", alpha=0.8, color=color_ax2)

        # Add change lines for throughput
        for (_, _, time) in changes1[1:-1]:
            zoomax1up.axvline(x=time, alpha=0.2, color='black')
            zoomax1down.axvline(x=time, alpha=0.2, color='black')
        # Add change lines for relaxation
        for (_, _, time) in changes2[1:-1]:
            zoomax1up.axvline(x=time, linestyle=':', alpha=0.2, color='black')
            zoomax1down.axvline(x=time, linestyle=':', alpha=0.2, color='black')


        # Zoom region for upper plot
        up_offset = 0.04
        up_center = 0.5
        upxmin, upxmax = up_center - up_offset, up_center + up_offset
        zoomax1up.set_xlim([upxmin, upxmax])
        zoomax1up.set_ylim(ax1.get_ylim()[0], ax1.get_ylim()[1]*0.85)
        zoomax2up.set_ylim(ax2.get_ylim()[0], ax2.get_ylim()[1]*0.25)

        # Set x ticks for upper
        zoomax1up.set_xticks([up_center - 0.03, up_center, up_center + 0.03])
        zoomax1up.set_xticklabels(["0.47", "0.5", "0.53"])

        # Zoom region for lower plot
        downxmin, downxmax = 3.21, 3.29
        zoomax1down.set_xlim([downxmin, downxmax])
        zoomax1down.set_ylim(ax1.get_ylim())
        zoomax2down.set_ylim(ax2.get_ylim())

        # Set x ticks for lower
        zoomax1down.set_xticks([3.25 - 0.03, 3.25, 3.25 + 0.03])
        zoomax1down.set_xticklabels(["3.22", "3.25", "3.28"])
        
        # Connection to upper zoom
        upymax1 = zoomax1up.get_ylim()[1]

        # Connection to upper zoom
        downymax1 = zoomax1down.get_ylim()[1]

        # Text for changes in upper
        if "tack" in self.title:
            up_ytext = upymax1*0.65
        else:
            up_ytext = upymax1*0.69
        
        # Text has to be adjusted manually depending on results...
        # zoomax1up.text(up_center - 0.023, up_ytext, f"{changes1[2][0]}W\n{changes1[2][1]}D", ha='center', fontsize=10)
        # zoomax1up.text(up_center + 0.023, up_ytext, f"{changes1[3][0]}W\n{changes1[3][1]}D", ha='center', fontsize=10)
        
        # Text for changes in lower
        if "tack" in self.title:
            down_ytext = downymax1*0.55
        else:
            down_ytext = downymax1*0.56
        
        # zoomax1down.text(3.25 - 0.026, down_ytext, f"{changes1[7][0]}W\n{changes1[7][1]}D", ha='center', fontsize=10)
        # zoomax1down.text(3.25 + 0.026, down_ytext, f"{changes1[8][0]}W\n{changes1[8][1]}D", ha='center', fontsize=10)
        
        # Titles of zoomed plots
        zoomax1up.set_title("Zoom at t=0.5", fontsize=15)
        zoomax1down.set_title("Zoom at t=3.25", fontsize=15)
        

        
        plt.tight_layout()
        if self.save_plots:
            self.path.mkdir(parents=True, exist_ok=True)
            plt.savefig(self.path / f'{self.path.name}.pdf', format = 'pdf')

        if self.show:
            plt.show()

def agg_samples(timestamps, values, window_size):
    """
    Aggregate samples to compute a rolling average of throughput.

    Parameters:
    - timestamps: List of lists, each inner list represents a series of timestamp samples for a thread.
    - values: List of lists, each inner list represents a series of throughput samples for a thread.
    - window_size: Duration of the sliding window, for example 1ms.

    Returns:
    - A list of timestamps, and one of avg_throughput.
    """

    # Convert two lists of lists into a single flattened and sorted list of (timestamp, value) pairs
    flattened_data = sorted([(t, v) for ts, vs in zip(timestamps, values) for t, v in zip(ts, vs)])

    window = deque()
    throughput_sum = 0.0

    res_timestamps = []
    res_values = []

    for timestamp, throughput in flattened_data:

        # Add current point to the window
        window.append((timestamp, throughput))
        throughput_sum += throughput

        # Remove old points from the window (if needed)
        while window and (timestamp - window[0][0] > window_size):
            old_timestamp, old_throughput = window.popleft()
            throughput_sum -= old_throughput

        # Compute average and store it
        if timestamp - window_size > 0:
            # Don't plot before 0
            res_timestamps.append(timestamp - window_size/2)
            res_values.append(throughput_sum/len(window))

    return res_timestamps, res_values



def get_root_path():
    return Path(__file__).parent.parent


def main(test_bench, old_folder):
    chdir(get_root_path())
    if old_folder is None:
        test_bench.compile(False)
        timestamps1, throughputs, changes1, test_out1 = test_bench.run_test()
        test_bench.compile(True)
        test_bench.extend_duration()
        timestamps2, relaxations, changes2, test_out2 = test_bench.run_test()
        test_bench.reset_duration()
        # Conditionally saves and shows data based on arguments
        test_bench.save(timestamps1, throughputs, changes1, timestamps2, relaxations, changes2)
        test_bench.plot(timestamps1, throughputs, changes1, timestamps2, relaxations, changes2)
    else:
        path = Path(old_folder)
        old_bench = load_old_bench(path)
        with open(path / "performance.json", 'r') as f:
            perf_dict = json.load(f)
        old_bench.plot(perf_dict["timestamps1"], perf_dict["throughputs"], perf_dict["changes1"], perf_dict["timestamps2"], perf_dict["relaxations"], perf_dict["changes2"])

def load_old_bench(old_bench_path):
    # print(old_bench_path / 'bench')
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
    parser.add_argument('--test', default='throughput-over-time',
                        help='Which of the C tests to use, but should emit timestamps over time')
    parser.add_argument('--ops-per-ts', default=1000, type=float,
                        help='How many operations should be done per taken timestamp (for both throughput and relaxation)')
    parser.add_argument('--window-size', default=0.025, type=float,
                        help='How wide should the MA window be for the plots?')
    parser.add_argument('--title', default="Over time benchmark",
                        help='What title to have for the plot')
    parser.add_argument('--relax-extend', default=60, type=int,
                        help='The factor which the duration for the relaxation analysis should be extended. As it has way slower throughput')
    parser.add_argument('--old-folder',
                        help='An old folder to use for results (use settings from cmd line)')

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
                  args.ops_per_ts, args.window_size, args.title, args.relax_extend)

    return bench, args.old_folder

if __name__ == '__main__':
    main(*parse_args())


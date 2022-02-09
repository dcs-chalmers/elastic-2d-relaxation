import argparse
import numpy as np
import matplotlib.pyplot as plt
import subprocess
import re
import pickle
import json

from os import chdir, environ
from pathlib import Path
from datetime import datetime

STRICT_STRUCTS = ["queue-wf", "queue-ms", "stack-treiber", "stack-elimination"]

class Bench:
    def __init__(self, structs, args, w_ratio, var, start, to, step, path, track, runs, test, show, save, ndebug, errors, title, exp, sup_ll, sup_rl, inter_socket, hyperthreading, sup_legend, athena_points, include_start):
        # Yes, this should be refactored...
        self.structs = structs
        self.static_args = args
        self.width_ratio = w_ratio
        self.varying = var
        self.start = start
        self.to = to
        self.step_size = step
        self.path = path
        self.track = track
        self.runs = runs
        self.test = test
        self.show = show
        self.save = save
        self.ndebug = ndebug
        self.errors = errors
        self.title = title
        self.exp_steps = exp
        self.sup_left_label = sup_ll
        self.sup_right_label = sup_rl
        self.inter_socket = inter_socket
        self.hyperthreading = hyperthreading
        self.sup_legend = sup_legend
        self.athena_points = athena_points
        self.include_start = include_start

    def compile(self, relaxation_errors):
        # Compile the test for all of the structs, also check thah they exist
        my_env = environ.copy()
        my_env["TEST"] = self.test

        if self.inter_socket:
            my_env["MEMORY_SETUP"] = "numa"

        if self.hyperthreading:
            my_env["HYPERTHREAD"] = "1"

        if relaxation_errors:
            my_env["RELAXATION_ANALYSIS"] = "1"

        if self.ndebug:
            # Just 03 but with the ndebug flag
            my_env["VERSION"] = 'O4'

        for struct in self.structs:
            if relaxation_errors and struct in STRICT_STRUCTS:
                continue
            try:
                 subprocess.check_output(['make',  f'{struct}'], env=my_env)
            except Exception as e:
                exit(e)

    def evaluate(self):
        # Runs the test for the data structures as arguments specify
        # Returns:
        #     raw_data - numpy array of all runs, dimensions: [data structure, run number, step in variable arg]
        #     averages - numpy array of the average performance of tracked variable. Rows are the structs
        #     stds - same as above, but for standard deviation

        # For each struct run the tests many times on each setting and get matrix, then combine those to get results
        matrices = np.stack([self.run_tests(struct, self.track) for struct in self.structs])

        averages = matrices.mean(axis=1)
        stds = matrices.std(axis=1)

        if self.errors:

            self.compile(True)
            rel_err_matrices = np.stack([self.run_tests(struct, "mean_relaxation") for struct in self.structs])

            rel_err_averages = rel_err_matrices.mean(axis=1)
            rel_err_stds = rel_err_matrices.std(axis=1)

            matrices = np.stack([matrices, rel_err_matrices], axis=1)
            averages = np.stack([averages, rel_err_averages], axis=1)
            stds = np.stack([stds, rel_err_stds], axis=1)

        return matrices, averages, stds

    def run_tests(self, struct, track):
        # Runs all tests for a given data structure
        # Returns:
        #     Numpy mat of all test results, parameters vary with columns

        results = []

        program_path = get_root_path() / 'bin' / struct
        args = self.static_args.copy()

        for run in range(self.runs):
            run_results = []
            for var in self.var_points():
                args[f'-{self.varying}'] = var
                if self.width_ratio is not None:
                    args['-w'] = self.width_ratio * args['-n']

                arg_list = [program_path]
                for (k, v) in args.items():
                    arg_list.append(f'{k} {v}')

                run_results.append(self.run_test(arg_list, track))
            results.append(run_results)

        return np.array(results)

    def var_points(self):
        if self.athena_points:
            points = list(range(257))[::16]
            points[0] = 1
            return points
        elif not self.exp_steps and self.include_start:
            points = list(range(self.to, self.start-1, -self.step_size))
            if points[-1] != self.start:
                points.append(self.start)
            points.reverse()
            return points
        elif not self.exp_steps:
            return list(range(self.start, self.to + 1, self.step_size))
        else:
            acc = self.start
            points = []
            while acc <= self.to:
                points.append(acc)
                acc *= 2

            return points


    def run_test(self, args, track):
        if 'relaxation' in track and args[0].name.split("/")[-1] in STRICT_STRUCTS:
            return 0.0

        # Run a few times, restart if problem, such as null returns
        for i in range(6):
            try:
                # timeout = max(int(args["-d"])*3, 5000)
                test_out = subprocess.check_output(args, timeout=60).decode('utf8')
                tracked = re.search(rf"{track} , (\d+.?\d*)", test_out)
                null_returns = re.search(r"Null_Count , (\d+)", test_out)
                if  not args[0].name.endswith('multi-qu_ran') and not args[0].name.endswith('queue-1ra') and null_returns and int(null_returns.group(1)) > 0:
                    print(f"null! {i+1}/6")
                    continue
                if tracked:
                    return float(tracked.group(1))
                else:
                    print(f"could not parse {track} in \n{test_out}")

            except Exception as e:
                print(f'failed run with {e}')
                pass

        print(f"The test cannot complete without null returns.\nTry increasing the number of initial items. Otherwise, our bitshift rng might be imbalanced for your computer, and it might need to be manually adjusted.\nError when running {args}\n")
        exit(1)

    def save_data(self, raw_data):
        if self.save:
            self.path.mkdir(parents=True)
            np.save(self.path / 'raw_data', raw_data, allow_pickle=False)

            with open(self.path / 'bench', 'wb') as f:
                pickle.dump(self, f, -1)

            with open(self.path / 'bench_dict.txt', 'w') as f:
                f.write(json.dumps({k:str(v) for (k, v) in self.__dict__.items()}, indent=2))

    def plot(self, averages, stds):

        # # Adjusted to fit article dimensions
        width = 350
        height = width*0.7

        # Clearer to see during development
        # width = 500
        # height = width*(3/4)

        # Create a new figure with the specified dimensions
        fig = plt.figure(figsize=(width / 100, height / 100), dpi=100)
        ax1 = plt.gca()

        # Title
        plt.title(self.title, fontsize=12)

        # Y-label
        if not self.sup_left_label:
            if self.track == 'Mops':
                ax1.set_ylabel("Operations per ms")
            elif self.track == 'Ops':
                ax1.set_ylabel("Operations per second")
            else:
                ax1.set_ylabel(f"{self.track}")

        # Automatically include some x-ticks
        xticks = self.var_points()
        last = xticks[-1]
        xticks = xticks[:-1]
        while len(xticks) > 5:
            xticks = xticks[::2]
        xticks.append(last)
        plt.xticks(xticks)

        # Find x-label
        if self.varying == 'n':
            plt.xlabel("Threads", fontsize=12)
            if self.to == 256:
                plt.xticks([1, 64, 128, 192, 256])
        elif self.varying == 'w':
            plt.xlabel("Width")
        elif self.varying == 'l':
            plt.xlabel("Depth")
        elif self.varying == 'k':
            plt.xlabel("Rank Error Bound", fontsize=12) # Rank error bound?
        else:
            plt.xlabel(f"{self.varying}")

        # Both relaxation and throughput
        if averages.ndim == 3:
            # Create second y axis

            # Axes names
            ax2 = ax1.twinx()  # create a second y-axis that shares the same x-axis
            if not self.sup_left_label:
                ax1.set_ylabel("Throughput (solid line)", fontsize=10)
            if not self.sup_right_label:
                ax2.set_ylabel("Relaxation (dotted line)", fontsize=10)

            # Make ticks smaller
            ax1.tick_params(axis='x', labelsize=7)
            ax1.tick_params(axis='y', labelsize=7)
            ax2.tick_params(axis='y', labelsize=7)
            ax1.yaxis.offsetText.set_fontsize(7)
            ax2.yaxis.offsetText.set_fontsize(7)

            # Scale and limit axes
            if not self.exp_steps:
                ax1range = averages[:,0,:].max() - averages[:,0,:].min()
                ax2range = averages[:,1,:].max() - averages[:,1,:].min()
                offset1 = ax1range*0.25
                offset2 = ax2range*0.1
                ax1.set_ylim(max(averages[:,0,:].min() - ax1range/2, 0), averages[:,0,:].max() + offset1) # Upper 2/3 of the plot
                ax2.set_ylim(max(averages[:,1,:].min() - offset2, 0), averages[:,1,:].min() + ax2range*2 + offset2) # Lower 1/2 of the plot
            else:
                ax1.set_xscale("log")
                ax1.set_yscale("log")
                ax2.set_yscale("log")

                ax1range = averages[:,0,:].max() / averages[:,0,:].min()
                ax2range = averages[:,1,:].max() / averages[:,1,:][averages[:,1,:] > 0].min() # Filter out zero-relaxed values
                ax1.set_ylim(averages[:,0,:].min() / (ax1range**(2/3)), averages[:,0,:].max()*(ax1range**(0.09))) # Upper 2/3 of the plot
                ax2.set_ylim(averages[:,1,:][averages[:,1,:] > 0].min() / (ax2range**(0.09)), averages[:,1,:][averages[:,1,:] > 0].min()*ax2range**2) # Lower 1/2 of the plot


        color_list = [
            '#1f77b4',  # muted blue
            '#ff7f0e',  # safety orange
            '#2ca02c',  # cooked asparagus green
            '#d62728',  # brick red
            '#9467bd',  # muted purple
            '#8c564b',  # chestnut brown
            '#e377c2',  # raspberry yogurt pink
            '#7f7f7f',  # middle gray
            '#bcbd22',  # curry yellow-green
            '#17becf'   # blue-teal
        ]

        RENAME_MAP = {
            # Queues
            '2Dd-queue': '2D Static Original',
            '2Dd-queue_optimized': '2D Static',
            '2Dd-queue_elastic-law': '2D Elastic LaW',
            '2Dd-queue_elastic-lpw': '2D Elastic LpW',
            'queue-ms': 'MS',
            'queue-wf': 'WF',
            'queue-k-segment': 'k-Segment',

            # Stacks
            '2Dc-stack': '2D Static Original',
            '2Dc-stack_optimized': '2D Static',
            '2Dc-stack_elastic-lpw': '2D Elastic LpW',
            'treiber': 'Treiber',
            'stack-elimination': 'Elimination',
            'stack-k-segment': 'k-Segment',
        }

        for (row, struct) in enumerate(self.structs):
            color = color_list[row]
            if averages.ndim == 3:
                # Plot Throughput
                line = ax1.errorbar(self.var_points(), averages[row, 0, :], stds[row, 0, :], linestyle='-', color=color)
                line.set_label(RENAME_MAP.get(struct, struct))

                # Plot relaxation
                if struct not in STRICT_STRUCTS:
                    line = ax2.errorbar(self.var_points(), averages[row, 1, :], stds[row, 1, :], linestyle=':', color=color)
            else:
                line = ax1.errorbar(self.var_points(), averages[row, :], stds[row, :], linestyle='-', color=color)
                line.set_label(RENAME_MAP.get(struct, struct))


        handles, labels = ax1.get_legend_handles_labels()

        # Make the layout tight and nice before adding the legend outside of the figure
        plt.tight_layout(pad=0, rect=[0, 0, 1, 0.95])

        if not self.sup_legend:
            fig.legend(handles, labels, loc='lower center', bbox_to_anchor=(0.5, -0.00), ncol = 3, fontsize=6)
            # Adjust the layout to make room for the legend below the figure
            plt.subplots_adjust(bottom=0.32)  # Adjust the bottom parameter as needed to fit the legend

        if self.save:
            self.path.mkdir(parents=True, exist_ok=True)
            plt.savefig(self.path / f'{self.path.name}.pdf', format = 'pdf')

        if self.show:
            plt.show()


def get_root_path():
    return Path(__file__).parent.parent


def load_old_bench(old_bench_path, sup_ll, sup_rl, sup_legend, title):
    with open(old_bench_path / "bench", 'rb') as f:
        old_bench = pickle.load(f)

    old_bench.sup_left_label = sup_ll
    old_bench.sup_right_label = sup_rl

    old_bench.sup_legend = sup_legend
    if title:
        old_bench.title = title

    if 'athena_points' not in old_bench.__dict__:
        old_bench.athena_points = False

    old_bench.save = True
    old_bench.show = True
    old_bench.path = Path(f"{old_bench_path}-replot-{datetime.now().strftime('%H:%M:%S')}")
    return old_bench

def main(args):#test_bench, old_bench_path=None):
    chdir(get_root_path())
    # If old_bench_path is specified, load old bench instead of compiling and evaluating a new one
    if args.old_bench is not None:
        test_bench = load_old_bench(args.old_bench, args.sup_left_label, args.sup_right_label, args.sup_legend, args.title)
        raw_data = np.load(args.old_bench / 'raw_data.npy')
        if raw_data.ndim == 4:
            # A bit ugly now...
            raw = raw_data[:,0,:,:]
            raw_err_averages = raw.mean(axis=1)
            raw_err_stds = raw.std(axis=1)

            relaxation = raw_data[:,1,:,:]
            rel_err_averages = relaxation.mean(axis=1)
            rel_err_stds = relaxation.std(axis=1)

            averages = np.stack([raw_err_averages, rel_err_averages], axis=1)
            stds = np.stack([raw_err_stds, rel_err_stds], axis=1)
        else:
            averages = raw_data.mean(axis=1)
            stds = raw_data.std(axis=1)
    else:
        # Can be used from outside as well to reload a test bench
        test_bench = new_bench(args)
        test_bench.compile(False)
        raw_data, averages, stds = test_bench.evaluate()
        # Conditionally saves and shows data based on arguments
        test_bench.save_data(raw_data)
    test_bench.plot(averages, stds)


def new_bench(args):
    if args.width is not None:
        args.width_ratio = None

    static_args = {
        '-i': args.initial,
        '-w': args.width,
        '-l': args.depth,
        '-k': args.relaxation,
        '-p': args.put_rate,
        '-n': args.threads,
        '-d': args.duration,
        '-s': args.side_work,
        '-m': args.mode,
    }

    for (key, value) in static_args.copy().items():
        if value is None:
            del static_args[key]

    datestr = datetime.now().strftime("%Y-%m-%d")
    path = get_root_path() / 'results'
    if args.name is None:
        path = path / f'{datestr}_{args.test}-{",".join(args.structs)}'
    else:
        path = path / f'{datestr}_{args.name}'

    if path.exists() and not args.nosave:
        timestr = datetime.now().strftime('%H:%M:%S')
        path = path.parent / f"{path.name}_{timestr}"
        print(f'Path exists, so specifying datetime: {path}')

    bench = Bench(args.structs, static_args, args.width_ratio, args.varying, args.start,
        args.to, args.step_size, path, args.track, args.runs, args.test, args.show,
        not args.nosave, args.ndebug, args.errors, args.title, args.exp_steps,
        args.sup_left_label, args.sup_right_label, args.inter_socket, args.hyperthreading,
        args.sup_legend, args.athena_points, args.include_start)

    return bench

def parse_args():
    # This became really ugly. Contemplating just rewriting in Rust to use the wonderful Clap crate :)
    parser = argparse.ArgumentParser(description='Benchmark several tests against each other.')
    parser.add_argument('structs', nargs='*',
                        help='Adds a data structure to compare with')
    parser.add_argument('--name',
                        help='The folder within results to save the data in')
    parser.add_argument('--track', default='Ops',
                        help='Which metric to track from the raw outputs')
    parser.add_argument('--runs', default=5, type=int,
                        help='How many runs to take the average of')
    parser.add_argument('--test', default='default',
                        help='Which of the C tests to use')
    parser.add_argument('--show', action='store_true', #type=bool,
                        help='Flag to show the generated graph')
    parser.add_argument('--nosave', action='store_true', #type=bool,
                        help='Flag to not save the results')
    parser.add_argument('--ndebug', action='store_true', #type=bool,
                        help='Flag to turn off asserts')
    parser.add_argument('--varying', '-v', default='n',
                        help='What argument to vary between runs, overwrites set values')
    parser.add_argument('--start', '-f', default=1, type=int,
                        help='What to start the variable at')
    parser.add_argument('--to', '-t', default=8, type=int,
                        help='The max of the variable')
    parser.add_argument('--step_size', '-s', default=1, type=int,
                        help='How large linear steps to take, overwritten by --exp-steps')
    parser.add_argument('--exp_steps', action='store_true',
                        help='Take steps by doubling each time')


    # Now add the arguments passed into the actual program
    parser.add_argument('--width-ratio', type=int,
                        help='Sets the width as a ratio of number of threads, overwritten by --width')
    parser.add_argument('--width', '-w', type=int,
                        help='Fixed width for all runs')
    parser.add_argument('--depth', '-l', type=int,
                        help='Fixed depth for all runs')
    parser.add_argument('--relaxation', '-k', type=int,
                        help='Fixed upper relaxation bound. If width specified depth is first adjusted.')
    parser.add_argument('--mode', '-m', type=int,
                        help='Which relaxation mode to use.')
    parser.add_argument('--put_rate', '-p', default=50, type=int,
                        help='The percentage of operations to be insertions')
    parser.add_argument('--threads', '-n', type=int,
                        help='Fixed number of threads')
    parser.add_argument('--duration', '-d', default=500, type=int,
                        help='The duration (in ms) to run each test')
    parser.add_argument('--initial', default=2**17, type=int,
                        help='The initial number of elemets inserted before starting the test')
    parser.add_argument('--side_work', type=int,
                        help='How much side work to do between accesses')
    # TODO Add some extra arguments maybe? Some testr might want extra ones.

    parser.add_argument('--errors', action='store_true', #type=bool,
                        help='Flag to also measure the mean relaxation at each run')
    parser.add_argument('--title',
                        help='What title to hav for the plot')
    parser.add_argument('--old_bench', type=Path,  default=None,
                        help='Replots the figure of a specific path')
    parser.add_argument('--sup_left_label', action='store_true',
                        help="Don't include a y-label to the left")
    parser.add_argument('--sup_right_label', action='store_true',
                        help="Don't include a y-label to the right")
    parser.add_argument('--sup_legend', action='store_true',
                        help="Don't include a legend for the names")
    parser.add_argument('--inter_socket', action='store_true',
                        help="Compile to pin threads in intersocket-mode")
    parser.add_argument('--hyperthreading', action='store_true',
                        help="Compile to pin threads in hyperthreading-mode")
    parser.add_argument('--athena_points', action='store_true',
                        help="Use [1, 16, 32 ... 256] as the x-axis")
    parser.add_argument('--include_start', action='store_true',
                        help="Include the $start argument in x-axis, otherwise stepping backward from $from arg")
    args = parser.parse_args()

    return args

if __name__ == '__main__':
    main(parse_args())


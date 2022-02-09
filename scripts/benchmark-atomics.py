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


class Bench:
    def __init__(self, structs, args, var, start, to, step, path, track, runs, test, show, save, ndebug, title, exp, sup_ll, sup_rl, inter_socket):
        self.structs = structs
        self.static_args = args
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
        self.title = title
        self.exp_steps = exp
        self.sup_left_label = sup_ll
        self.sup_right_label = sup_rl
        self.inter_socket = inter_socket

    def compile(self):
        # Compile the test for all of the structs, also check thah they exist
        my_env = environ.copy()
        my_env["TEST"] = self.test

        if self.inter_socket:
            my_env["MEMORY_SETUP"] = "numa"

        if self.ndebug:
            # Just 03 but with the ndebug flag
            my_env["VERSION"] = 'O4'

        for struct in self.structs:
            try:
                 subprocess.check_output(['make', '-C',  f'src/{struct}'], env=my_env)
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

                arg_list = [program_path]
                for (k, v) in args.items():
                    arg_list.append(f'{k} {v} ')

                run_results.append(self.run_test(arg_list, track))
            results.append(run_results)

        return np.array(results)

    def var_points(self):
        if not self.exp_steps:
            return list(range(self.start, self.to + 1, self.step_size))
        else:
            acc = self.start
            points = []
            while acc <= self.to:
                points.append(acc)
                acc *= 2

            return points


    def run_test(self, args, track):
        # Basically run up to three times in case it fails
        for i in range(3):
            try:
                # timeout = max(int(args["-d"])*3, 5000)
                test_out = subprocess.check_output(args, timeout=60).decode('utf8')
                tracked = re.search(rf"{track} , (\d+.?\d*)", test_out)
                null_returns = re.search(r"Null_Count , (\d+)", test_out)
                if int(null_returns.group(1)) > 0:
                    print(f"null! {i+1}/3")
                    continue
                if tracked:
                    return float(tracked.group(1))
                else:
                    print(f"could not parse {track} in {test_out}")

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
        
        # Adjusted to fit article dimensions
        width = 250
        height = width*(3/4)

        # Create a new figure with the specified dimensions
        fig = plt.figure(figsize=(width / 100, height / 100), dpi=100)
        ax1 = plt.gca()

        # Title
        plt.title(self.title, fontsize=10)

        # Y-label
        if not self.sup_left_label:
            if self.track == 'Mops':
                ax1.set_ylabel("MOps/s")
            else:
                ax1.set_ylabel(f"{self.track}")

        # Find x-label
        if self.varying == 'n':
            plt.xlabel("Threads", fontsize=10)
        elif self.varying == 'w':
            plt.xlabel("Width")
        elif self.varying == 'l':
            plt.xlabel("Depth")
        elif self.varying == 'k':
            plt.xlabel("Relaxation Bound", fontsize=9)
        else:
            plt.xlabel(f"{self.varying}")

        # plt.xticks([0, 72, 72*2, 72*3, 72*4], [i*72 for i in range(5)])
        plt.xticks([i*9 for i in range(1,5)], [i*9 for i in range(1,5)])

        # Both relaxation and throughput
        if averages.ndim == 3:
            # Create second y axis

            # Axes names
            ax2 = ax1.twinx()  # create a second y-axis that shares the same x-axis
            if not self.sup_left_label:
                ax1.set_ylabel("Throughput (solid line)", fontsize=8)
            if not self.sup_right_label:
                ax2.set_ylabel("Relaxation (dotted line)", fontsize=8)

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
                ax2range = averages[:,1,:].max() / averages[:,1,:].min()
                ax1.set_ylim(averages[:,0,:].min() / (ax1range**(2/3)), averages[:,0,:].max()*(ax1range**(0.09))) # Upper 2/3 of the plot
                ax2.set_ylim(averages[:,1,:].min() / (ax2range**(0.09)), averages[:,1,:].min()*ax2range**2) # Lower 1/2 of the plot


        color_list = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd', '#8c564b']
        for (row, struct) in enumerate(self.structs):
            color = color_list[row]
            if averages.ndim == 3:
                # Plot Throughput
                line = ax1.errorbar(self.var_points()[::2], averages[row, 0, ::2], stds[row, 0, ::2], linestyle='-', color=color)
                if "elastic" in struct:
                    line.set_label("Elastic")
                elif "original" in struct:
                    line.set_label("Static")
                else:
                    print("Warning: Could not find if it is elastic or static struct")

                # Plot relaxation
                line = ax2.errorbar(self.var_points()[::2], averages[row, 1, ::2], stds[row, 1, ::2], linestyle=':', color=color)
            else:
                line = ax1.errorbar(self.var_points(), averages[row, :], stds[row, :], linestyle='-', color=color)
                ax1range = averages[:,:].max() / averages[:,:].min()
                ax1.set_ylim(averages[:,:].min() / (ax1range**(2/3)), averages[:,:].max()*(ax1range**(0.09))) # Upper 2/3 of the plot
                plt.yscale('log')
                line.set_label(struct)


        # Reverse the order of the legend to have static as the first line (could also just change order of colors and arguments...)
        handles, labels = ax1.get_legend_handles_labels()
        handles = handles[::-1]
        labels = labels[::-1]

        # Create an outside legend with names on the same line (ncol=2), for generating shared legend
        # ax1.legend(handles, labels, loc='upper left', bbox_to_anchor=(1, 1), ncol=2)
        # plt.tight_layout(rect=[0, 0, 0.85, 1])
        
        # Use shared legend instead
        ax1.legend(handles, labels, loc='lower left', fontsize=7)

        plt.tight_layout(pad=0.5)
        if self.save:
            self.path.mkdir(parents=True, exist_ok=True)
            plt.savefig(self.path / 'graph.pdf', format = 'pdf')

        if self.show:
            plt.show()


def get_root_path():
    return Path(__file__).parent.parent


def load_old_bench(old_bench_path, sup_ll, sup_rl):
    # print(old_bench_path / 'bench')
    with open(old_bench_path / "bench", 'rb') as f:
        old_bench = pickle.load(f)

    old_bench.sup_left_label = sup_ll
    old_bench.sup_right_label = sup_rl

    old_bench.save = True
    old_bench.show = True
    old_bench.path = Path(f"{old_bench_path}-replot-{datetime.now().strftime('%H:%M:%S')}")
    return old_bench

def main(args):#test_bench, old_bench_path=None):
    chdir(get_root_path())
    # If old_bench_path is specified, load old bench instead of compiling and evaluating a new one
    if args.old_bench is not None:
        test_bench = load_old_bench(args.old_bench, args.sup_left_label, args.sup_right_label)
        raw_data = np.load(args.old_bench / 'raw_data.npy')
        if raw_data.ndim == 4:
            # A bit ugly now...
            raw = raw_data[:,0,:,:]
            raw_err_averages = raw.mean(axis=1)
            raw_err_stds = raw.std(axis=1)
            
            # raw_data = np.load("/home/kare/dev/semantic-relaxation-2D-elastic/results/odysseus/2023-09-06_compare_queues_threads-hyperthreading/raw_data.npy")

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
        test_bench.compile()
        raw_data, averages, stds = test_bench.evaluate()
        # Conditionally saves and shows data based on arguments
        test_bench.save_data(raw_data)
    test_bench.plot(averages, stds)


def new_bench(args):
    static_args = {
        '-p': args.put_rate,
        '-n': args.threads,
        '-d': args.duration,
        '-s': args.side_work,
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

    bench = Bench(args.structs, static_args, args.varying, args.start,
        args.to, args.step_size, path, args.track, args.runs, args.test, args.show,
        not args.nosave, args.ndebug, args.title, args.exp_steps, 
        args.sup_left_label, args.sup_right_label, args.inter_socket)

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
                        help='TODO: Take steps by doubling each time')


    # Now add the arguments passed into the actual program
    parser.add_argument('--put_rate', '-p', default=50, type=int,
                        help='The percentage of operations to be insertions')
    parser.add_argument('--threads', '-n', default=8, type=int,
                        help='Fixed number of threads')
    parser.add_argument('--duration', '-d', default=500, type=int,
                        help='The duration (in ms) to run each test')
    parser.add_argument('--side_work', type=int,
                        help='How much side work to do between accesses')
    # TODO Add some extra arguments maybe? Some testr might want extra ones.

    parser.add_argument('--title',
                        help='What title to hav for the plot')
    parser.add_argument('--old_bench', type=Path,  default=None,
                        help='Replots the figure of a specific path')
    parser.add_argument('--sup_left_label', action='store_true',
                        help="Don't include a y-label to the left")
    parser.add_argument('--sup_right_label', action='store_true',
                        help="Don't include a y-label to the right")
    parser.add_argument('--inter_socket', action='store_true',
                        help="Compile to pin threads in intersocket-mode")


    args = parser.parse_args()

    return args

if __name__ == '__main__':
    main(parse_args())


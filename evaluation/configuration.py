import argparse, math, os, subprocess, shutil

# Create an argument parser
parser = argparse.ArgumentParser(description="Run evaluation for discrete isocontouring algorithms")

# Add the 'method' argument
parser.add_argument('--method', type=int, default=0,
                    help="Evaluation method: 0 - all, 1 - memory footprint, 2 - run-time, "
                         "3 - speed-up, 4 - non-manifold stats. Default: 0")
parser.add_argument("--iterations", type=int, default=10,
                    help="Number of iterations for each timed evaluation. Default: 10")

# Parse the command-line arguments
args = parser.parse_args()

# Access the 'method' argument
method = args.method

# Iterations
iterations = args.iterations

# Directories
evaluation_dir = os.path.dirname(os.path.abspath(__file__))
src_dir = os.path.dirname(evaluation_dir)
build_dir = os.path.join(src_dir, "build")  # Make sure this is the correct build directory
home_dir = os.path.expanduser("~")
datasets_dir = os.path.join(home_dir, "Data")  # Make sure this is the correct dataset directory

results_dir = os.path.join(evaluation_dir, "results")
# Make sure this is the correct configuration
# configuration = "testing"
configuration = f"frontier_tbb2023.0.0"
config_dir = os.path.join(results_dir, configuration)

data_dir = os.path.join(config_dir, "data")
data_memory_footprint_dir = os.path.join(data_dir, "memory_footprint")
data_runtime_dir = os.path.join(data_dir, "runtime")
data_speed_up_dir = os.path.join(data_dir, "speed_up")
data_non_manifold_stats_dir = os.path.join(data_dir, "non_manifold_stats")

figures_dir = os.path.join(config_dir, "figures")
fig_memory_footprint_dir = os.path.join(figures_dir, "memory_footprint")
fig_runtime_dir = os.path.join(figures_dir, "runtime")
fig_speed_up_dir = os.path.join(figures_dir, "speed_up")
fig_non_manifold_stats_dir = os.path.join(figures_dir, "non_manifold_stats")

# Executables
executable = os.path.join(build_dir, "vtk-discrete-isocontouring-evaluation")
time_executable = shutil.which("time")
memory_evaluator = f"{time_executable} -v"

# Datasets — ordered from smallest to biggest; each entry has name, path, and max_labels.
datasets = [
    {"name": "Brain", "path": f"{datasets_dir}/Brain.vti", "max_labels": 312},
    {"name": "Torso", "path": f"{datasets_dir}/Torso.vti", "max_labels": 92},
    {"name": "Synthetic", "path": f"{datasets_dir}/Synthetic.vti", "max_labels": 128},
    {"name": "Mouse", "path": f"{datasets_dir}/Mouse.vti", "max_labels": 687},
    {"name": "Pinky", "path": f"{datasets_dir}/Pinky.vti", "max_labels": 254},
]
biggest_datasets = datasets[-2:]

# Eight-spheres synthetic non-manifold validation dataset (no file on disk; generated internally).
eight_spheres_input = "eight-spheres"
eight_spheres_dims = 128
eight_spheres_num_steps = 50

# Algorithms
algorithms_names = {"--mc": "MC", "--fe": "FE", "--mmsn": "MMSN", "--fesn": "FESN", "--ossn": "OSSN"}
algorithms = ["--mc", "--fe", "--mmsn", "--fesn", "--ossn"]
sequential_algorithms = ["--mc", "--mmsn"]
parallel_algorithms = ["--fe", "--fesn", "--ossn"]
non_manifold_algorithms = ["--ossn"]
algorithms_joined = " ".join(algorithms)
parallel_algorithms_joined = " ".join(parallel_algorithms)
non_manifold_algorithms_joined = " ".join(non_manifold_algorithms)

# Number of threads
# max_number_of_threads_power_of_2 = int(math.log2(os.cpu_count()))  # Make sure this uses the correct number of threads
max_number_of_threads_power_of_2 = int(math.log2(128))  # Make sure this uses the correct number of threads
max_number_of_threads = int(2 ** max_number_of_threads_power_of_2)


def get_num_labels_settings(dataset):
    """Return the three label-count settings used across evaluations: 1, half, and max."""
    max_labels = dataset["max_labels"]
    return [
        ("1", 1),
        ("half", max(1, max_labels // 2)),
        ("max", max_labels),
    ]


def run_command(command, output_file):
    """Run a command and save the output to a file."""
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    with open(output_file, 'a') as f:
        subprocess.run(command, shell=True, stdout=f, stderr=f)

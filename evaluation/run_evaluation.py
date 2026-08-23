from configuration import *

if method == 0 or method == 1:
    # Memory footprint: all algorithms; 1 thread; 0 iterations; N_L in {1, max/2, max}.
    print("Get memory footprint information")
    os.makedirs(data_memory_footprint_dir, exist_ok=True)
    for dataset in datasets:
        for nl_label, n_labels in get_num_labels_settings(dataset):
            for algo in algorithms:
                run_command(
                    f"{memory_evaluator} {executable} -i {dataset['path']} -t 1 -l {n_labels} "
                    f"{algo} -n 0",
                    f"{data_memory_footprint_dir}/{dataset['name']}_nl_{nl_label}_"
                    f"{algorithms_names[algo]}.txt")

if method == 0 or method == 2:
    # Run-time: all algorithms at 1 thread; parallel algorithms at max threads; N_L in {1, max/2, max}.
    print("Get run-time information")
    os.makedirs(data_runtime_dir, exist_ok=True)
    for dataset in datasets:
        for nl_label, n_labels in get_num_labels_settings(dataset):
            run_command(
                f"{executable} -i {dataset['path']} -t 1 -l {n_labels} {algorithms_joined} -n {iterations}",
                f"{data_runtime_dir}/{dataset['name']}_nl_{nl_label}_1_threads.yaml")
            run_command(
                f"{executable} -i {dataset['path']} -t {max_number_of_threads} -l {n_labels} "
                f"{parallel_algorithms_joined} -n {iterations}",
                f"{data_runtime_dir}/{dataset['name']}_nl_{nl_label}_"
                f"{max_number_of_threads}_threads.yaml")

if method == 0 or method == 3:
    # Speed-up: parallel algorithms; N_L = max; threads in {1, 2, 4, ..., max}.
    print("Get speed-up information")
    os.makedirs(data_speed_up_dir, exist_ok=True)
    for dataset in biggest_datasets:
        max_labels = dataset["max_labels"]
        for power in range(0, max_number_of_threads_power_of_2 + 1):
            threads = int(math.pow(2, power))
            run_command(
                f"{executable} -i {dataset['path']} -t {threads} -l {max_labels} "
                f"{parallel_algorithms_joined} -n {iterations}",
                f"{data_speed_up_dir}/{dataset['name']}_nl_max_{threads}_threads.yaml")

if method == 0 or method == 4:
    # Non-manifold statistics: FESN and OSSN; 1 thread; 0 iterations; --report-non-manifold-stats.
    # Run for all real datasets at N_L in {1, max/2, max}, then for every step of the eight-spheres sequence.
    print("Get non-manifold statistics")
    os.makedirs(data_non_manifold_stats_dir, exist_ok=True)
    for dataset in datasets:
        for nl_label, n_labels in get_num_labels_settings(dataset):
            run_command(
                f"{executable} -i {dataset['path']} -t 1 -l {n_labels} --ossn "
                f"--report-non-manifold-stats -n 0",
                f"{data_non_manifold_stats_dir}/{dataset['name']}_nl_{nl_label}.yaml")

    for step in range(0, eight_spheres_num_steps + 1):
        run_command(
            f"{executable} -i {eight_spheres_input} --eight-spheres-dims {eight_spheres_dims} "
            f"--eight-spheres-num-steps {eight_spheres_num_steps} --eight-spheres-step {step} "
            f"-t 1 -l 0 --ossn --report-non-manifold-stats -n 0",
            f"{data_non_manifold_stats_dir}/EightSpheres_step_{step:03d}.yaml")

print(f"All evaluations completed. Check results in {results_dir}/.")

import re
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import yaml
from configuration import *

plt.rcParams['text.usetex'] = False  # use matplotlib mathtext

fig_width = 8
fig_height = 6.5
fig_height_small = 4.75

axis_label_fontsize = 14
legend_fontsize = 11


def load_gpl_palette(file_path):
    colors = []
    with open(file_path, 'r') as f:
        for line in f:
            if line.startswith('#') or not line.strip():
                continue
            parts = line.split()
            if len(parts) >= 4:
                r, g, b = int(parts[0]) / 255, int(parts[1]) / 255, int(parts[2]) / 255
                colors.append((r, g, b))
    return colors


palette = load_gpl_palette(f'{evaluation_dir}/Set1_5.gpl')
algorithm_colors = {algorithms_names[algo]: palette[i] for i, algo in enumerate(algorithms)}


# --------------------------------------------------------------------------------------------
# Parsing helpers
# --------------------------------------------------------------------------------------------

def get_run_time_info(filename):
    """Parse a run-time YAML result file. Returns {algorithm_name: avg_seconds_total}."""
    try:
        with open(filename, "r") as f:
            content = yaml.load(f, Loader=yaml.FullLoader)[0]
        result = {}
        for experiment in content["experiments"]:
            algo_name = experiment["algorithm-name"]
            trials = experiment.get("trials", [])
            if trials:
                result[algo_name] = sum(t["seconds-total"] for t in trials) / len(trials)
            else:
                result[algo_name] = np.nan
        return result
    except (yaml.YAMLError, IndexError, FileNotFoundError, KeyError) as e:
        print(f"Warning: could not parse {filename}: {e}")
        return None


def get_memory_footprint_info(filename):
    """Parse a memory footprint .txt file. Returns algorithm memory in GB
    (peak RSS minus the dataset baseline logged before the algorithm ran),
    or np.nan if the algorithm did not complete (no first-run-time recorded)."""
    try:
        with open(filename, "r") as f:
            content = f.read()
        if not re.search(r"first-run-time:", content):
            if re.search(r"out of memory, skipping", content):
                print(f"Warning: algorithm ran out of memory in {filename}")
            else:
                print(f"Warning: no first-run-time in {filename} (algorithm did not complete)")
            return np.nan
        dataset_memory_used_match = re.search(r"dataset-memory-used:\s*(\d+)", content)
        max_rss_match = re.search(r"Maximum resident set size \(kbytes\):\s*(\d+)", content)
        if dataset_memory_used_match and max_rss_match:
            dataset_memory_used = int(dataset_memory_used_match.group(1))
            max_rss = int(max_rss_match.group(1))
            return (max_rss - dataset_memory_used) / (1024 * 1024)
        print(f"Warning: could not find memory fields in {filename}")
        return np.nan
    except FileNotFoundError:
        print(f"Warning: file not found: {filename}")
        return np.nan


def get_non_manifold_stats_info(filename):
    """Parse a non-manifold stats YAML result file.
    Returns (num_manifold, num_solvable, num_unsolvable) or None."""
    try:
        with open(filename, "r") as f:
            content = yaml.load(f, Loader=yaml.FullLoader)[0]
        for experiment in content["experiments"]:
            if experiment["algorithm-name"] != "OSSN":
                continue
            stats = experiment.get("non-manifold-stats")
            if stats is None:
                return None
            return (stats["num-manifold"], stats["num-solvable-non-manifold"],
                    stats["num-unsolvable-non-manifold"])
        return None
    except (yaml.YAMLError, IndexError, FileNotFoundError, KeyError) as e:
        print(f"Warning: could not parse {filename}: {e}")
        return None


# --------------------------------------------------------------------------------------------
# Table helpers
# --------------------------------------------------------------------------------------------

def bold_min_per_row(df, fmt="{:.2f}"):
    """Return a DataFrame of strings with the minimum value per row bolded."""
    out = df.copy().astype(object)
    for idx in df.index:
        row = df.loc[idx]
        numeric_row = pd.to_numeric(row, errors="coerce")
        if numeric_row.isna().all():
            continue
        min_val = numeric_row.min()
        for col in df.columns:
            val = row[col]
            if pd.isna(val):
                out.loc[idx, col] = "---"
            elif np.isclose(pd.to_numeric(val), min_val):
                out.loc[idx, col] = f"\\textbf{{{fmt.format(val)}}}"
            else:
                out.loc[idx, col] = fmt.format(val)
    return out


def print_total_improvement_ratio(df):
    print()
    algorithm_ratios = {}
    for dataset in df.columns:
        min_val = df[dataset].min()
        for algorithm in df.index:
            ratio = df.loc[algorithm][dataset] / min_val
            algorithm_ratios.setdefault(algorithm, []).append(ratio)
    for algorithm, ratios in algorithm_ratios.items():
        print(f"Algorithm: {algorithm}, Min - Max Ratios: {np.min(ratios):.2f}x - {np.max(ratios):.2f}x")
    print()


def add_improvement_ratio_columns(df, metric_type='ratio'):
    result_df = df.copy()
    for dataset in df.columns:
        result_df[dataset] = result_df[dataset].round(2)
    for dataset in df.columns:
        if metric_type == 'ratio':
            ref_val = df[dataset].min(skipna=True)
            ratio_col = (df[dataset] / ref_val).round(2)
            col_name = f"{dataset}_ratio"
        else:  # speedup
            ref_val = df[dataset].max(skipna=True)
            ratio_col = (ref_val / df[dataset]).round(2)
            col_name = f"{dataset}_speedup"
        col_idx = result_df.columns.get_loc(dataset)
        result_df.insert(col_idx + 1, col_name, ratio_col)
    return result_df


# --------------------------------------------------------------------------------------------
# Chart helpers
# --------------------------------------------------------------------------------------------

def display_label(col):
    """Convert 'Brain_N_L_1' to 'Brain: $N_{L}=1$' for figure axis labels."""
    parts = col.split('_N_L_')
    if len(parts) == 2:
        dataset_name, nl_label = parts
        if nl_label in ('half', 'max'):
            return f"{dataset_name}: $N_{{L}}=\\mathrm{{{nl_label}}}$"
        return f"{dataset_name}: $N_{{L}}={nl_label}$"
    return col


def get_width_and_spacing(num_algorithms):
    max_num_algorithms = len(algorithms)
    ratio = max_num_algorithms / num_algorithms
    width = 0.2 * ratio
    spacing = 1.2
    return width, spacing


def create_line_chart(df, x_label, y_label, legend_title, legend_loc, figure_filename):
    fig, ax = plt.subplots(figsize=(fig_width, fig_height_small))
    for series in df.columns:
        ax.plot(df.index, df[series], marker='o', markersize=3, label=series)
    ax.set_xlabel(x_label, fontsize=axis_label_fontsize)
    ax.set_ylabel(y_label, fontsize=axis_label_fontsize)
    ax.legend(title=legend_title, loc=legend_loc, fontsize=legend_fontsize)
    ax.grid(True, linestyle='--')
    plt.tight_layout()
    plt.savefig(figure_filename, dpi=300, bbox_inches='tight')
    print(f"Wrote {figure_filename}")


def create_bar_chart(df, add_min_offset, x_label, y_label, legend_title, legend_loc, figure_filename,
                     x_max=None):
    num_datasets = len(df.columns)
    num_algorithms = len(df.index)
    width, spacing = get_width_and_spacing(num_algorithms)

    fig, ax = plt.subplots(figsize=(fig_width, fig_height))
    dataset_positions = np.arange(num_datasets) * spacing

    for j, algorithm in enumerate(df.index):
        values = df.loc[algorithm].values
        ax.barh(
            y=dataset_positions + ((num_algorithms - 1) - j - (num_algorithms - 1) / 2) * width,
            width=values, height=width, label=algorithm, color=algorithm_colors[algorithm])

    if x_max is not None:
        has_cropped_values = False
        for j, algorithm in enumerate(df.index):
            values = df.loc[algorithm].values
            for i, val in enumerate(values):
                if val > x_max:
                    has_cropped_values = True
                    y_pos = dataset_positions[i] + ((num_algorithms - 1) - j - (num_algorithms - 1) / 2) * width
                    ax.text(x_max * 0.99, y_pos, '+',
                            verticalalignment='center', horizontalalignment='right',
                            fontsize=14, fontweight='bold', color='black')
        if has_cropped_values:
            ax.set_xlim(right=x_max)

    for i, dataset in enumerate(df.columns):
        min_value = df[dataset].min()
        if add_min_offset:
            min_value *= 0.99
        ax.vlines(x=min_value,
                  ymin=dataset_positions[i] - width * num_algorithms / 2,
                  ymax=dataset_positions[i] + width * num_algorithms / 2,
                  color='grey', linestyle='--', linewidth=1)

    ax.set_yticks(dataset_positions)
    ax.set_yticklabels(df.columns)
    ax.set_xlabel(x_label, fontsize=axis_label_fontsize)
    ax.set_ylabel(y_label, fontsize=axis_label_fontsize)
    ax.legend(title=legend_title, loc=legend_loc, fontsize=legend_fontsize)
    ax.grid(axis='x', linestyle='--')
    plt.tight_layout()
    plt.savefig(figure_filename, dpi=300, bbox_inches='tight')
    print(f"Wrote {figure_filename}")


# --------------------------------------------------------------------------------------------
# Method 1: Memory Footprint
# --------------------------------------------------------------------------------------------

if method == 0 or method == 1:
    print("Generating memory footprint figures")
    os.makedirs(fig_memory_footprint_dir, exist_ok=True)

    memory_data = {}
    for dataset in datasets:
        dataset_name = dataset["name"]
        for nl_label, _ in get_num_labels_settings(dataset):
            col_name = f"{dataset_name}_N_L_{nl_label}"
            memory_data[col_name] = {}
            for algo in algorithms:
                algorithm_name = algorithms_names[algo]
                output_file = (f"{data_memory_footprint_dir}/"
                               f"{dataset_name}_nl_{nl_label}_{algorithm_name}.txt")
                memory_data[col_name][algorithm_name] = get_memory_footprint_info(output_file)

    df_memory = pd.DataFrame(memory_data)
    df_memory.index.name = 'Algorithm'
    df_memory_with_ratio = add_improvement_ratio_columns(df_memory)
    df_memory_with_ratio.to_csv(f"{fig_memory_footprint_dir}/memory_footprint.csv", float_format="%.2f")
    print(df_memory_with_ratio)
    print_total_improvement_ratio(df_memory)

    create_bar_chart(df_memory.rename(columns=display_label), True, 'Memory footprint (GB)',
                     'Datasets', 'Algorithms', 'lower right',
                     f"{fig_memory_footprint_dir}/memory_footprint.png", x_max=20)

    print("Generating memory footprint tables per N_L")

    for nl_label, _ in get_num_labels_settings(datasets[0]):
        memory_data_nl = {}
        for dataset in datasets:
            dataset_name = dataset["name"]
            column = {}
            for algo in algorithms:
                algorithm_name = algorithms_names[algo]
                output_file = (f"{data_memory_footprint_dir}/"
                               f"{dataset_name}_nl_{nl_label}_{algorithm_name}.txt")
                column[algorithm_name] = get_memory_footprint_info(output_file)
            memory_data_nl[dataset_name] = column

        df_memory_nl = pd.DataFrame(memory_data_nl).reindex([algorithms_names[algo] for algo in algorithms])
        df_memory_nl.index.name = 'Algorithm'
        df_memory_nl_with_ratio = add_improvement_ratio_columns(df_memory_nl)
        df_memory_nl_with_ratio.to_csv(f"{fig_memory_footprint_dir}/memory_footprint_nl_{nl_label}.csv",
                                       float_format="%.2f")
        print(df_memory_nl_with_ratio)
        print_total_improvement_ratio(df_memory_nl)

# --------------------------------------------------------------------------------------------
# Method 2: Run-time
# --------------------------------------------------------------------------------------------

if method == 0 or method == 2:
    print("Generating run-time figures")
    os.makedirs(fig_runtime_dir, exist_ok=True)

    for thread_label, algos_in_file in [("1", algorithms),
                                        (str(max_number_of_threads), parallel_algorithms)]:
        runtime_data = {}
        for dataset in datasets:
            dataset_name = dataset["name"]
            for nl_label, _ in get_num_labels_settings(dataset):
                col_name = f"{dataset_name}_N_L_{nl_label}"
                result_file = (f"{data_runtime_dir}/"
                               f"{dataset_name}_nl_{nl_label}_{thread_label}_threads.yaml")
                run_time = get_run_time_info(result_file)
                runtime_data[col_name] = {
                    algorithms_names[algo]: run_time.get(algorithms_names[algo]) if run_time else np.nan
                    for algo in algos_in_file
                }

        df_runtime = pd.DataFrame(runtime_data)
        df_runtime.index.name = 'Algorithm'
        df_runtime_with_ratio = add_improvement_ratio_columns(df_runtime)
        df_runtime_with_ratio.to_csv(f"{fig_runtime_dir}/runtime_{thread_label}_threads.csv",
                                     float_format="%.2f")
        print(df_runtime_with_ratio)
        print_total_improvement_ratio(df_runtime)

        create_bar_chart(df_runtime.rename(columns=display_label), True, 'Run-time (seconds)',
                         'Datasets', 'Algorithms', 'lower right',
                         f"{fig_runtime_dir}/runtime_{thread_label}_threads.png")

    print("Generating run-time tables per N_L and thread count")

    for thread_label, algos_in_file in [("1", algorithms),
                                        (str(max_number_of_threads), parallel_algorithms)]:
        row_labels = [algorithms_names[algo] for algo in algos_in_file]
        for nl_label, _ in get_num_labels_settings(datasets[0]):
            runtime_data = {}
            for dataset in datasets:
                dataset_name = dataset["name"]
                result_file = (f"{data_runtime_dir}/"
                               f"{dataset_name}_nl_{nl_label}_{thread_label}_threads.yaml")
                run_time = get_run_time_info(result_file)
                runtime_data[dataset_name] = {
                    algorithms_names[algo]: run_time.get(algorithms_names[algo]) if run_time else np.nan
                    for algo in algos_in_file
                }

            df_runtime_nl = pd.DataFrame(runtime_data).reindex(row_labels)
            df_runtime_nl.index.name = 'Algorithm'
            df_runtime_nl_with_ratio = add_improvement_ratio_columns(df_runtime_nl)
            df_runtime_nl_with_ratio.to_csv(
                f"{fig_runtime_dir}/runtime_nl_{nl_label}_{thread_label}_threads.csv",
                float_format="%.2f")
            print(df_runtime_nl_with_ratio)
            print_total_improvement_ratio(df_runtime_nl)

# --------------------------------------------------------------------------------------------
# Method 3: Speed-up
# --------------------------------------------------------------------------------------------

if method == 0 or method == 3:
    print("Generating speed-up figures")
    os.makedirs(fig_speed_up_dir, exist_ok=True)

    # Generate thread counts as powers of 2: [1, 2, 4, 8, ..., max_threads]
    thread_counts = [int(2 ** p) for p in range(0, max_number_of_threads_power_of_2 + 1)]

    for dataset in biggest_datasets:
        dataset_name = dataset["name"]

        # Initialize a dictionary to map parallel algorithms to their execution run-times
        times_per_algo = {algorithms_names[algo]: [] for algo in parallel_algorithms}

        # Read the execution run-times from YAML configuration files
        for threads in thread_counts:
            result_file = f"{data_speed_up_dir}/{dataset_name}_nl_max_{threads}_threads.yaml"
            run_time = get_run_time_info(result_file)
            for algo in parallel_algorithms:
                algorithm_name = algorithms_names[algo]
                # Safely get runtime or fallback to NaN if OOM/missing
                times_per_algo[algorithm_name].append(
                    run_time.get(algorithm_name, np.nan) if run_time else np.nan
                )

        # Build DataFrame with thread counts as the index
        df_speed_up = pd.DataFrame(times_per_algo, index=thread_counts)
        df_speed_up.index.name = "Threads"

        # Save raw data to CSV
        df_speed_up.to_csv(f"{fig_speed_up_dir}/{dataset_name}_speed_up.csv", float_format="%.3f")
        print(df_speed_up)

        # Create the figure
        fig, ax = plt.subplots(figsize=(fig_width, fig_height_small))

        # Speed-up is measured relative to the absolute best sequential baseline
        # across ALL algorithms at 1 thread (row 0 minimum)
        best_sequential_time = df_speed_up.iloc[0].min()

        for algo in parallel_algorithms:
            algorithm_name = algorithms_names[algo]
            times = np.array(times_per_algo[algorithm_name], dtype=float)

            # Calculate absolute speed-up values
            speed_up_values = best_sequential_time / times

            # Plot the solid line up to the second-to-last point (actual physical threads)
            ax.plot(
                thread_counts[:-1],
                speed_up_values[:-1],
                marker='o',
                label=algorithm_name,
                alpha=0.8,
                color=algorithm_colors[algorithm_name]
            )

            # Plot the last point with a dashed line (hyper-threading range)
            ax.plot(
                thread_counts[-2:],
                speed_up_values[-2:],
                marker='o',
                linestyle='--',
                color=algorithm_colors[algorithm_name]
            )

        # Configure axes scale, tick labels, and grids
        ax.set_xscale('log', base=2)
        ax.set_xticks(thread_counts)
        ax.set_xticklabels(thread_counts)

        # Add labels
        ax.set_xlabel('Threads', fontsize=axis_label_fontsize)
        ax.set_ylabel('Speed-up', fontsize=axis_label_fontsize)

        # Show legend and grid
        ax.legend(title='Algorithms', loc='upper left', fontsize=legend_fontsize)
        ax.grid(True, linestyle='--')

        # Tighten and save the figure
        plt.tight_layout()
        filename = f"{fig_speed_up_dir}/{dataset_name}_speed_up.png"
        plt.savefig(filename, dpi=300, bbox_inches='tight')
        print(f"Wrote {filename}")

# --------------------------------------------------------------------------------------------
# Method 4: Non-Manifold Statistics
# --------------------------------------------------------------------------------------------

if method == 0 or method == 4:
    print("Generating non-manifold statistics")
    os.makedirs(fig_non_manifold_stats_dir, exist_ok=True)

    # Real datasets: columns = dataset_N_L_label
    nm_data = {}
    for dataset in datasets:
        dataset_name = dataset["name"]
        for nl_label, _ in get_num_labels_settings(dataset):
            col_name = f"{dataset_name}_N_L_{nl_label}"
            result_file = f"{data_non_manifold_stats_dir}/{dataset_name}_nl_{nl_label}.yaml"
            stats = get_non_manifold_stats_info(result_file)
            if stats is None or sum(stats) == 0:
                nm_data[col_name] = {"Manifold (%)": np.nan,
                                     "Solvable NM (%)": np.nan,
                                     "Unsolvable NM (%)": np.nan}
                continue
            num_manifold, num_solvable, num_unsolvable = stats
            total = sum(stats)
            nm_data[col_name] = {
                "Manifold (%)": 100.0 * num_manifold / total,
                "Solvable Non-Manifold (%)": 100.0 * num_solvable / total,
                "Unsolvable Non-Manifold (%)": 100.0 * num_unsolvable / total,
            }

    df_nm = pd.DataFrame(nm_data)
    df_nm.index.name = "Metric"
    df_nm.to_csv(f"{fig_non_manifold_stats_dir}/non_manifold_stats.csv", float_format="%.3f")
    print(df_nm)

    # Eight-spheres: line chart of solvable % and unsolvable % over all steps.
    es_steps, es_solvable, es_unsolvable = [], [], []
    for step in range(0, eight_spheres_num_steps + 1):
        eight_spheres_file = f"{data_non_manifold_stats_dir}/EightSpheres_step_{step:03d}.yaml"
        stats = get_non_manifold_stats_info(eight_spheres_file)
        if stats is None or sum(stats) == 0:
            continue
        num_manifold, num_solvable, num_unsolvable = stats
        total = sum(stats)
        es_steps.append(step)
        es_solvable.append(100.0 * num_solvable / total)
        es_unsolvable.append(100.0 * num_unsolvable / total)

    df_es = pd.DataFrame({
        "Solvable Non-Manifold (%)": es_solvable,
        "Unsolvable Non-Manifold (%)": es_unsolvable,
    }, index=es_steps)
    df_es.index.name = "Step"
    df_es.to_csv(f"{fig_non_manifold_stats_dir}/eight_spheres_non_manifold_stats.csv",
                 float_format="%.3f")
    print(df_es)
    create_line_chart(df_es, 'Step', 'Percentage (%)', 'Metric', 'upper center',
                      f"{fig_non_manifold_stats_dir}/eight_spheres_non_manifold.png")

print(f"All figures generated. Check results in {figures_dir}/.")

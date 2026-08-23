# vtk-discrete-isocontouring-evaluation

## Introduction

Repository for the evaluation of discrete multi-label isocontouring algorithms.

The algorithms that are evaluated are the following:

1. MC: VTK's `vtkDiscreteMarchingCubes` (+ windowed sinc smoothing)
2. FE: VTK's `vtkDiscreteFlyingEdges3D` (+ windowed sinc smoothing)
3. MMSN: Frisken's Multi-Material SurfaceNets reference implementation (bundled, see Licensing
   below)
4. FESN: `vtkFlyingEdgesSurfaceNets`
5. OSSN: `vtkSurfaceNets3D`

## Compilation

To compile the executable on the frontier supercomputer, you can use the script `compile_frontier.sh`.
If you are compiling locally, you can get inspiration from the `compile_frontier.sh` script, and remove or change
what you do or do not need depending on your system. This build is CPU/TBB-only; no GPU backend is required.

## Executable

These algorithms can be used through the compiled executable named `vtk-discrete-isocontouring-evaluation`, with the
following options:

```
./vtk-discrete-isocontouring-evaluation -h
Discrete Isocontouring Evaluation
Usage: ./vtk-discrete-isocontouring-evaluation [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  -i,--input TEXT REQUIRED    Input file name, or the literal string "eight-spheres" to use the
                               synthetic non-manifold validation dataset.
  --eight-spheres-dims INT    Volume dimension (cubic) for the eight-spheres dataset (Default: 128)
  --eight-spheres-num-steps INT
                               Total number of steps in the eight-spheres convergence sequence
                               (Default: 50)
  --eight-spheres-step INT    Which step of the convergence sequence to generate, in [0,
                               num-steps]. -1 means fully converged, i.e. step == num-steps
                               (Default: -1)
  -l,--num-labels UINT        Number of labels to extract. 0 means all labels (Default: 0)
  -t,--threads UINT:UINT in [1 - 16]
                               Number of threads (Default: 1)
  -n,--trials UINT            Number of timed trials to average. 0 disables timing (Default: 1)
  -s,--smoothing BOOLEAN      Enable smoothing (Default: true)
  --mc                        Run discrete Marching Cubes
  --fe                        Run discrete Flying Edges
  --mmsn                      Run Frisken's Multi-Material SurfaceNets reference
  --fesn                      Run FlyingEdges SurfaceNets
  --ossn                      Run OuterSpace SurfaceNets
  --report-non-manifold-stats Report non-manifold classification statistics (manifold / solvable /
                               unsolvable) for OuterSpace SurfaceNets. Smoothing is not performed.
```

## Python Evaluation scripts

The ```vtk-discrete-isocontouring-evaluation``` executable can be used to evaluate the algorithms.
In the `evaluation` directory, you can find the following scripts:

1. `configuration.py` is used to define information regarding where data, executable(s) and
   results should be located. Be sure to check it out.
2. `run_evaluation.py` is used to run the ```vtk-discrete-isocontouring-evaluation``` executable. It will run the
   executable with the specified options and store the different kinds of results. For different segments of the
   evaluation, you can use the ``--method`` option to run part of the evaluation you want to run.
3. `generate_figures.py` is used to generate the figures and tables based on the results obtained from the
   evaluation. For different segments of the evaluation, you can use the ``--method`` option to generate figures for
   a specific part of the evaluation.

## Data

The datasets used for the evaluations of the algorithms can be downloaded from the following
[Google Drive folder](https://drive.google.com/drive/folders/1pJZ8NVlU3AFMIo2sqW4MCe6K68nGsX9Y?usp=sharing).

Be sure to update the `configuration.py` file with the correct path to the data.

## Results

The evaluation data is stored in different folders in the `evaluation/results` directory. The results that are
generated are:

1. memory-footprint: The memory footprint of the algorithms.
2. run-time: The run time of all algorithms with 1 thread and of all parallel algorithms using max number of
   threads.
3. speed-up: The speed-up of the parallel algorithms using max number of threads.
4. non-manifold-stats: Non-manifold classification statistics (manifold / solvable / unsolvable) for OuterSpace
   SurfaceNets.

## Licensing

`MMSurfaceNet`, `MMCellMap`, `MMCellFlag`, and `MMGeometryOBJ` are Sarah Frisken's open-source
Multi-Material SurfaceNets reference implementation, bundled here with permission for direct
comparison purposes. Please cite Frisken's work if reusing this code outside of reproducing this
repository's results.

//
// Arguments.h
// CLI options for the discrete isocontouring evaluation executable.
//

#ifndef ARGUMENTS_H
#define ARGUMENTS_H

#include <string>

struct Arguments
{
  // Input dataset. Either a file path, or the literal string "eight-spheres" to use the
  // synthetic non-manifold validation dataset generated internally (see EightSpheres.h).
  std::string InputFileName;

  // Number of labels to extract. 0 means "use all labels found in the dataset".
  unsigned int NumLabels = 0;

  // Threading.
  unsigned int NumberOfThreads = 1;

  // Number of timed trials to average. 0 means: run once (no timing loop), useful when only
  // memory footprint or non-manifold statistics are being collected.
  unsigned int NumberOfTrials = 1;

  bool Smoothing = true;

  // Algorithms to run.
  bool MC = false;
  bool FE = false;
  bool MMSN = false;
  bool FESN = false;
  bool OSSN = false;

  // If set, after running OuterSpaceSN, compute and report non-manifold classification
  // statistics (manifold / solvable non-manifold / unsolvable non-manifold) instead of
  // (or in addition to) timing. See NonManifoldStats.h.
  bool ReportNonManifoldStats = false;

  // Eight-spheres synthetic dataset parameters (only used when InputFileName == "eight-spheres").
  int EightSpheresDims = 128;
  int EightSpheresNumSteps = 50;
  int EightSpheresStep = -1; // -1 means "fully converged" (spheres meet at the center)

  /**
   * @brief Parse command line arguments.
   */
  void ParseArguments(int argc, char** argv);
};

#endif // ARGUMENTS_H

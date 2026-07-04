//
// Arguments.cxx
//

#include "Arguments.h"
#include "CLI/CLI.hpp"

#include <thread>

void Arguments::ParseArguments(int argc, char** argv)
{
  std::unique_ptr<CLI::App> app = std::make_unique<CLI::App>("Discrete Isocontouring Evaluation");

  app
    ->add_option("-i,--input", this->InputFileName,
      "Input file name, or the literal string \"eight-spheres\" to "
      "use the synthetic "
      "non-manifold validation dataset.")
    ->required();

  app->add_option("--eight-spheres-dims", this->EightSpheresDims,
    "Volume dimension (cubic) for the eight-spheres dataset (Default: 128)");
  app->add_option("--eight-spheres-num-steps", this->EightSpheresNumSteps,
    "Total number of steps in the eight-spheres convergence "
    "sequence (Default: 50)");
  app->add_option("--eight-spheres-step", this->EightSpheresStep,
    "Which step of the convergence sequence to generate, in [0, num-steps]. "
    "-1 means fully converged, i.e. step == num-steps (Default: -1)");

  app->add_option("-l,--num-labels", this->NumLabels,
    "Number of labels to extract. 0 means all labels (Default: 0)");

  app->add_option("-t,--threads", this->NumberOfThreads, "Number of threads (Default: 1)")
    ->check(CLI::Range(1u, std::thread::hardware_concurrency()));

  app->add_option("-n,--trials", this->NumberOfTrials,
    "Number of timed trials to average. 0 disables timing (Default: 1)");

  app->add_option("-s,--smoothing", this->Smoothing, "Enable smoothing (Default: true)");

  app->add_flag("--mc", this->MC, "Run discrete Marching Cubes");
  app->add_flag("--fe", this->FE, "Run discrete Flying Edges");
  app->add_flag("--mmsn", this->MMSN, "Run Frisken's Multi-Material SurfaceNets reference");
  app->add_flag("--fesn", this->FESN, "Run FlyingEdges SurfaceNets");
  app->add_flag("--ossn", this->OSSN, "Run OuterSpace SurfaceNets");

  app->add_flag("--report-non-manifold-stats", this->ReportNonManifoldStats,
    "Report non-manifold classification statistics (manifold / "
    "solvable / unsolvable) for "
    "OuterSpace SurfaceNets, Smoothing is not performed.");

  try
  {
    app->parse(argc, argv);
  }
  catch (const CLI::CallForHelp& e)
  {
    std::cout << app->help();
    exit(1);
  }
  catch (const CLI::CallForAllHelp& e)
  {
    std::cout << app->help();
    exit(1);
  }
  catch (const CLI::ParseError& e)
  {
    app->exit(e);
    exit(1);
  }
}

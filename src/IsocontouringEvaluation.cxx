// Evaluation driver for discrete multi-label isocontouring algorithms, comparing:
//   - MC:     vtkDiscreteMarchingCubes (+ windowed sinc smoothing)
//   - FE:     vtkDiscreteFlyingEdges3D (+ windowed sinc smoothing)
//   - MMSN:   Frisken's Multi-Material SurfaceNets reference implementation
//   - OSSN:   vtkSurfaceNets3D (this paper's contribution)

#include "vtkImageData.h"
#include "vtkNrrdReader.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkSMPTools.h"
#include "vtkSmartPointer.h"
#include "vtkStructuredPoints.h"
#include "vtkStructuredPointsReader.h"
#include "vtkTimerLog.h"
#include "vtkWindowedSincPolyDataFilter.h"
#include "vtkXMLImageDataReader.h"

#include <vtksys/SystemInformation.hxx>

#include "MMGeometryOBJ.h"
#include "MMSurfaceNet.h"
#include "vtkDiscreteFlyingEdges3D.h"
#include "vtkDiscreteMarchingCubes.h"
#include "vtkFlyingEdgesSurfaceNets.h"
#include "vtkSurfaceNets3D.h"

#include "Arguments.h"
#include "EightSpheres.h"
#include "NonManifoldStats.h"
#include "YamlWriter.h"
#include "vtkUnsignedShortArray.h"
#include "vtkVersionFull.h"

#include <functional>
#include <unordered_set>

namespace
{

// Reads a labeled volume from disk, dispatching on file extension. Mirrors RunSN.cxx/TestSN.cxx's
// ReadData(), extended with the eight-spheres synthetic case.
auto ReadOrGenerateDataset(const Arguments& args) -> vtkSmartPointer<vtkImageData>
{
  if (args.InputFileName == "eight-spheres")
  {
    return GenerateEightSpheres(
      args.EightSpheresDims, args.EightSpheresNumSteps, args.EightSpheresStep);
  }

  const std::string& filename = args.InputFileName;
  const std::string extension = vtksys::SystemTools::GetFilenameExtension(filename);

  vtkSmartPointer<vtkImageData> output;
  if (extension == ".vti")
  {
    vtkNew<vtkXMLImageDataReader> reader;
    reader->SetFileName(filename.c_str());
    reader->Update();
    output = reader->GetOutput();
  }
  else if (extension == ".vtk")
  {
    vtkNew<vtkStructuredPointsReader> reader;
    reader->SetFileName(filename.c_str());
    reader->Update();
    output = reader->GetOutput();
  }
  else if (extension == ".nrrd")
  {
    vtkNew<vtkNrrdReader> reader;
    reader->SetFileName(filename.c_str());
    reader->Update();
    output = reader->GetOutput();
  }
  else
  {
    std::cerr << "Unable to open file: " << filename << " (unrecognized extension)" << std::endl;
    exit(1);
  }
  // Convert scalar data to unsigned short, required by MMSurfaceNet.
  vtkNew<vtkUnsignedShortArray> outArray;
  outArray->ShallowCopy(output->GetPointData()->GetScalars());
  output->GetPointData()->SetScalars(outArray);
  return output;
}

// Determine the number of distinct non-background labels actually present in the volume, and
// the maximum label value (assumed contiguous in [1, maxLabel].
auto CountLabels(vtkUnsignedShortArray* scalars) -> unsigned short
{
  std::unordered_set<unsigned short> uniqueLabels;
  vtkIdType numTuples = scalars->GetNumberOfTuples();
  for (vtkIdType i = 0; i < numTuples; ++i)
  {
    auto label = scalars->GetValue(i);
    if (label != 0)
    {
      uniqueLabels.insert(label);
    }
  }
  return uniqueLabels.empty() ? 0 : *std::max_element(uniqueLabels.begin(), uniqueLabels.end());
}

constexpr int NumSmoothingIterations = 25;
constexpr double ConstraintScale = 2.0;

// Runs one warm-up trial (firstRun=true, where the trial logs output stats) then numTrials timed
// trials, each recorded as a list item with trial-index and seconds-total.
auto RunAllTrials(const std::string& algorithmName, const Arguments& args, vtkImageData* image,
  YamlWriter& log,
  const std::function<double(
    vtkImageData* image, const Arguments& args, YamlWriter& log, bool firstRun)>& runTrial) -> void
{
  log.StartListItem();
  log.AddDictionaryEntry("algorithm-name", algorithmName);
  try
  {
    log.AddDictionaryEntry("first-run-time", runTrial(image, args, log, true));

    if (args.NumberOfTrials > 0)
    {
      log.StartBlock("trials");
      for (unsigned int trial = 0; trial < args.NumberOfTrials; ++trial)
      {
        log.StartListItem();
        log.AddDictionaryEntry("trial-index", trial);
        log.AddDictionaryEntry("seconds-total", runTrial(image, args, log, false));
      }
      log.EndBlock();
    }
  }
  catch (const std::bad_alloc&)
  {
    std::cerr << algorithmName << ": out of memory, skipping." << std::endl;
  }
}

// --- MC ---------------------------------------------------------------------------------------
auto RunMCTrial(vtkImageData* image, const Arguments& args, YamlWriter& log, bool firstRun = false)
  -> double
{
  vtkNew<vtkTimerLog> timer;

  vtkNew<vtkDiscreteMarchingCubes> mc;
  mc->SetInputData(image);
  mc->GenerateValues(static_cast<int>(args.NumLabels), 1, args.NumLabels);
  timer->StartTimer();
  mc->Update();
  timer->StopTimer();
  const double surfaceExtractionTime = timer->GetElapsedTime();

  double surfaceSmoothingTime = 0;
  if (args.Smoothing)
  {
    vtkNew<vtkWindowedSincPolyDataFilter> smooth;
    smooth->SetInputConnection(mc->GetOutputPort());
    smooth->SetNumberOfIterations(NumSmoothingIterations);

    timer->StartTimer();
    smooth->Update();
    timer->StopTimer();
    surfaceSmoothingTime = timer->GetElapsedTime();
  }
  if (firstRun)
  {
    log.AddDictionaryEntry("num-output-points", mc->GetOutput()->GetNumberOfPoints());
    log.AddDictionaryEntry("num-output-cells", mc->GetOutput()->GetNumberOfCells());
  }
  else
  {
    log.AddDictionaryEntry("seconds-surface-extraction", surfaceExtractionTime);
    log.AddDictionaryEntry("seconds-surface-smoothing", surfaceSmoothingTime);
  }
  return surfaceExtractionTime + surfaceSmoothingTime;
}

// --- FE ---------------------------------------------------------------------------------------
auto RunFETrial(vtkImageData* image, const Arguments& args, YamlWriter& log, bool firstRun = false)
  -> double
{
  vtkNew<vtkTimerLog> timer;

  vtkNew<vtkDiscreteFlyingEdges3D> fe;
  fe->SetInputData(image);
  fe->GenerateValues(static_cast<int>(args.NumLabels), 1, args.NumLabels);
  timer->StartTimer();
  fe->Update();
  timer->StopTimer();
  const double surfaceExtractionTime = timer->GetElapsedTime();

  double surfaceSmoothingTime = 0;
  if (args.Smoothing)
  {
    vtkNew<vtkWindowedSincPolyDataFilter> smooth;
    smooth->SetInputConnection(fe->GetOutputPort());
    smooth->SetNumberOfIterations(NumSmoothingIterations);

    timer->StartTimer();
    smooth->Update();
    timer->StopTimer();
    surfaceSmoothingTime = timer->GetElapsedTime();
  }
  if (firstRun)
  {
    log.AddDictionaryEntry("num-output-points", fe->GetOutput()->GetNumberOfPoints());
    log.AddDictionaryEntry("num-output-cells", fe->GetOutput()->GetNumberOfCells());
  }
  else
  {
    log.AddDictionaryEntry("seconds-surface-extraction", surfaceExtractionTime);
    log.AddDictionaryEntry("seconds-surface-smoothing", surfaceSmoothingTime);
  }
  return surfaceExtractionTime + surfaceSmoothingTime;
}

// --- MMSN ---------------------------------------------------------------------------------------
auto RunMMSNTrial(
  vtkImageData* image, const Arguments& args, YamlWriter& log, bool firstRun = false) -> double
{
  vtkNew<vtkTimerLog> timer;

  auto* dims = image->GetDimensions();
  const auto* spacingD = image->GetSpacing();
  float spacing[3] = { static_cast<float>(spacingD[0]), static_cast<float>(spacingD[1]),
    static_cast<float>(spacingD[2]) };

  auto labelData = vtkUnsignedShortArray::FastDownCast(image->GetPointData()->GetScalars());

  timer->StartTimer();
  auto* sn = new MMSurfaceNet(labelData->GetPointer(0), dims, spacing);
  timer->StopTimer();
  double surfaceExtractionTime = timer->GetElapsedTime();

  double surfaceSmoothingTime = 0;
  if (args.Smoothing)
  {
    MMSurfaceNet::RelaxAttrs relax{};
    relax.numRelaxIterations = NumSmoothingIterations;
    relax.relaxFactor = 0.5f;
    relax.maxDistFromCellCenter = 1.0f;
    timer->StartTimer();
    sn->relax(relax);
    timer->StopTimer();
    surfaceSmoothingTime = timer->GetElapsedTime();
  }

  timer->StartTimer();
  MMGeometryOBJ geom(sn);
  vtkIdType totalPoints = 0;
  vtkIdType totalTris = 0;
  for (int label = 1; label <= args.NumLabels; ++label)
  {
    // always create the obj to ensure fair comparison with other algorithms
    MMGeometryOBJ::OBJData obj = geom.objData(label);
    totalPoints += static_cast<vtkIdType>(obj.vertexPositions.size());
    totalTris += static_cast<vtkIdType>(obj.triangles.size());
  }
  timer->StopTimer();
  surfaceExtractionTime += timer->GetElapsedTime();
  delete sn;

  if (firstRun)
  {
    log.AddDictionaryEntry("num-output-points", totalPoints);
    log.AddDictionaryEntry("num-output-cells", totalTris);
  }
  else
  {
    log.AddDictionaryEntry("seconds-surface-extraction", surfaceExtractionTime);
    log.AddDictionaryEntry("seconds-surface-smoothing", surfaceSmoothingTime);
  }
  return surfaceExtractionTime + surfaceSmoothingTime;
}

// --- FlyingEdges SurfaceNets ---------------------------------------------------------------------
auto RunFESNTrial(
  vtkImageData* image, const Arguments& args, YamlWriter& log, bool firstRun = false) -> double
{
  vtkNew<vtkTimerLog> timer;

  vtkNew<vtkFlyingEdgesSurfaceNets> sn;
  sn->SetInputData(image);
  sn->DataCachingOn();
  sn->GenerateValues(static_cast<int>(args.NumLabels), 1, args.NumLabels);
  sn->SmoothingOff();

  timer->StartTimer();
  sn->Update();
  timer->StopTimer();
  const double surfaceExtractionTime = timer->GetElapsedTime();

  if (firstRun)
  {
    if (args.ReportNonManifoldStats)
    {
      NonManifoldStats stats = ComputeNonManifoldStats(sn->GetOutput());
      log.StartBlock("non-manifold-stats");
      log.AddDictionaryEntry("num-manifold", stats.NumManifold);
      log.AddDictionaryEntry("num-solvable-non-manifold", stats.NumSolvableNonManifold);
      log.AddDictionaryEntry("num-unsolvable-non-manifold", stats.NumUnsolvableNonManifold);
      log.AddDictionaryEntry("total", stats.GetTotal());
      log.EndBlock();
    }
  }
  double surfaceSmoothingTime = 0;
  if (args.Smoothing)
  {
    sn->SetSmoothing(args.Smoothing);
    sn->SetNumberOfIterations(NumSmoothingIterations);
    sn->SetConstraintScale(ConstraintScale);

    timer->StartTimer();
    sn->Update();
    timer->StopTimer();
    surfaceSmoothingTime = timer->GetElapsedTime();
  }
  if (firstRun)
  {
    log.AddDictionaryEntry("num-output-points", sn->GetOutput()->GetNumberOfPoints());
    log.AddDictionaryEntry("num-output-cells", sn->GetOutput()->GetNumberOfCells());
  }
  else
  {
    log.AddDictionaryEntry("seconds-surface-extraction", surfaceExtractionTime);
    log.AddDictionaryEntry("seconds-surface-smoothing", surfaceSmoothingTime);
  }
  return surfaceExtractionTime + surfaceSmoothingTime;
}

// --- OuterSpace SurfaceNets ---------------------------------------------------------------------
auto RunOSSNTrial(
  vtkImageData* image, const Arguments& args, YamlWriter& log, bool firstRun = false) -> double
{
  vtkNew<vtkTimerLog> timer;

  vtkNew<vtkSurfaceNets3D> sn;
  sn->SetInputData(image);
  sn->DataCachingOn();
  sn->GenerateValues(static_cast<int>(args.NumLabels), 1, args.NumLabels);
  sn->SmoothingOff();

  timer->StartTimer();
  sn->Update();
  timer->StopTimer();
  const double surfaceExtractionTime = timer->GetElapsedTime();

  if (firstRun)
  {
    if (args.ReportNonManifoldStats)
    {
      NonManifoldStats stats = ComputeNonManifoldStats(sn->GetOutput());
      log.StartBlock("non-manifold-stats");
      log.AddDictionaryEntry("num-manifold", stats.NumManifold);
      log.AddDictionaryEntry("num-solvable-non-manifold", stats.NumSolvableNonManifold);
      log.AddDictionaryEntry("num-unsolvable-non-manifold", stats.NumUnsolvableNonManifold);
      log.AddDictionaryEntry("total", stats.GetTotal());
      log.EndBlock();
    }
  }
  double surfaceSmoothingTime = 0;
  if (args.Smoothing)
  {
    sn->SetSmoothing(args.Smoothing);
    sn->SetNumberOfIterations(NumSmoothingIterations);
    sn->SetConstraintScale(ConstraintScale);

    timer->StartTimer();
    sn->Update();
    timer->StopTimer();
    surfaceSmoothingTime = timer->GetElapsedTime();
  }
  if (firstRun)
  {
    log.AddDictionaryEntry("num-output-points", sn->GetOutput()->GetNumberOfPoints());
    log.AddDictionaryEntry("num-output-cells", sn->GetOutput()->GetNumberOfCells());
  }
  else
  {
    log.AddDictionaryEntry("seconds-surface-extraction", surfaceExtractionTime);
    log.AddDictionaryEntry("seconds-surface-smoothing", surfaceSmoothingTime);
  }
  return surfaceExtractionTime + surfaceSmoothingTime;
}

} // namespace

auto main(int argc, char** argv) -> int
{
  Arguments args;
  args.ParseArguments(argc, argv);

  vtksys::SystemInformation sysinfo;

  YamlWriter log;
  log.StartListItem();

  log.AddDictionaryEntry("vtk-version", VTK_VERSION_FULL);
  log.AddDictionaryEntry("hostname", sysinfo.GetHostname());
  std::time_t currentTime = std::time(nullptr);
  char timeString[256];
  std::strftime(timeString, 256, "%Y-%m-%dT%H:%M:%S%z", std::localtime(&currentTime));
  log.AddDictionaryEntry("date", timeString);

  vtkSMPTools::Initialize(static_cast<int>(args.NumberOfThreads));

  log.AddDictionaryEntry("input-file", args.InputFileName);
  log.AddDictionaryEntry("num-threads", args.NumberOfThreads);

  vtkSmartPointer<vtkImageData> image = ReadOrGenerateDataset(args);

  int maxLabelInData =
    CountLabels(vtkUnsignedShortArray::FastDownCast(image->GetPointData()->GetScalars()));
  args.NumLabels = (args.NumLabels == 0) ? maxLabelInData : static_cast<int>(args.NumLabels);

  log.AddDictionaryEntry("num-input-points", image->GetNumberOfPoints());
  log.AddDictionaryEntry("num-input-cells", image->GetNumberOfCells());
  log.AddDictionaryEntry("num-max-labels", maxLabelInData);
  log.AddDictionaryEntry("num-requested-labels", args.NumLabels);

  // Logged before running any algorithm, so that generate_figures.py can subtract this baseline
  // from the process's peak resident set size (reported externally by `/usr/bin/time -v`) to
  // isolate the algorithm's own memory footprint from the cost of holding the input dataset.
  const auto datasetMemoryUsed = sysinfo.GetProcMemoryUsed();
  log.AddDictionaryEntry("dataset-memory-used", datasetMemoryUsed);

  log.StartBlock("experiments");

  if (args.MC)
  {
    RunAllTrials("MC", args, image, log, RunMCTrial);
  }
  if (args.FE)
  {
    RunAllTrials("FE", args, image, log, RunFETrial);
  }
  if (args.FESN)
  {
    RunAllTrials("FESN", args, image, log, RunFESNTrial);
  }
  if (args.OSSN)
  {
    RunAllTrials("OSSN", args, image, log, RunOSSNTrial);
  }
  if (args.MMSN)
  {
    RunAllTrials("MMSN", args, image, log, RunMMSNTrial);
  }

  log.EndBlock();

  return 0;
}

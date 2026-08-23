#include "NonManifoldStats.h"
#include "vtkPointData.h"
#include "vtkPoints.h"
#include "vtkPolyData.h"
#include "vtkTypeInt8Array.h"

NonManifoldStats ComputeNonManifoldStats(vtkPolyData* surfaceNet)
{
  NonManifoldStats stats;
  auto points = vtk::DataArrayTupleRange<3>(
    vtkAOSDataArrayTemplate<float>::SafeDownCast(surfaceNet->GetPoints()->GetData()));
  auto tableIndex =
    vtkTypeInt8Array::FastDownCast(surfaceNet->GetPointData()->GetArray("NonManifoldTableIndices"));
  for (vtkIdType i = 0; i < surfaceNet->GetNumberOfPoints(); ++i)
  {
    const auto index = tableIndex->GetValue(i);
    if (index == -2)
    {
      stats.NumManifold++;
    }
    else if (index == -1)
    {
      stats.NumUnsolvableNonManifold++;
    }
    else if (index >= 0)
    {
      auto currentPt = points[i];
      // have this check to avoid counting duplicate points many times.
      if (i == 0 || currentPt != points[i - 1])
      {
        stats.NumSolvableNonManifold++;
      }
    }
  }
  return stats;
}

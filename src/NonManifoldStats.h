#ifndef NON_MANIFOLD_STATS_H
#define NON_MANIFOLD_STATS_H

#include "vtkImageData.h"
#include "vtkType.h"

class vtkPolyData;
struct NonManifoldStats
{
  vtkIdType NumManifold = 0;
  vtkIdType NumSolvableNonManifold = 0;
  vtkIdType NumUnsolvableNonManifold = 0;

  vtkIdType GetTotal() const
  {
    return this->NumManifold + this->NumSolvableNonManifold + this->NumUnsolvableNonManifold;
  }
};

/**
 * ompute non-manifold classification statistics for a surface net
 */
NonManifoldStats ComputeNonManifoldStats(vtkPolyData* surfaceNet);

#endif // NON_MANIFOLD_STATS_H

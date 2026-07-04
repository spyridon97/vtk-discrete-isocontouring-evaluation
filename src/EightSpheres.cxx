//
// EightSpheres.cxx
//

#include "EightSpheres.h"

#include "vtkDataArray.h"
#include "vtkPointData.h"
#include "vtkSmartPointer.h"
#include "vtkSphere.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
// Signs used to pick 1/4 or 3/4 along each axis for each sphere's starting center.
constexpr std::array<std::array<double, 3>, 8> CornerSigns = { { { -1, -1, -1 }, { 1, -1, -1 },
  { -1, 1, -1 }, { 1, 1, -1 }, { -1, -1, 1 }, { 1, -1, 1 }, { -1, 1, 1 }, { 1, 1, 1 } } };
}

vtkSmartPointer<vtkImageData> GenerateEightSpheres(int dims, int numSteps, int step)
{
  numSteps = std::max(numSteps, 1);
  if (step < 0)
  {
    step = numSteps; // fully converged
  }
  step = std::min(step, numSteps);

  // Voxel space: origin (0,0,0), spacing 1. Voxel index == world coordinate.
  // Radius is 49/2% of dims. Centers start at dims/4 or 3*dims/4 per axis (step 0)
  // and converge to dims/2 (step numSteps).
  const double radius = 0.49 * 0.5 * dims;
  const double r2 = radius * radius;
  vtkSmartPointer<vtkImageData> output = vtkSmartPointer<vtkImageData>::New();
  output->SetDimensions(dims, dims, dims);
  output->AllocateScalars(VTK_UNSIGNED_SHORT, 1);
  output->SetSpacing(1.0, 1.0, 1.0);
  output->SetOrigin(0.0, 0.0, 0.0);

  auto* scalars = static_cast<unsigned short*>(output->GetScalarPointer());
  std::fill_n(scalars, static_cast<vtkIdType>(dims) * dims * dims, static_cast<unsigned short>(0));

  // For each of the eight spheres, compute its center for the requested step and burn it into the
  // volume. Later spheres (higher label) overwrite earlier ones at overlapping voxels; the
  // precise tie-breaking near full convergence is not significant since the goal is to exercise the
  // non-manifold case where multiple materials meet, not to test a specific tie-breaking rule.
  for (int label = 0; label < 8; ++label)
  {
    const auto& signs = CornerSigns[label];

    // Starting center: dims/4 or 3*dims/4 per axis depending on which octant this sphere is in.
    const double start[3] = {
      (signs[0] < 0) ? dims / 4.0 : 3.0 * dims / 4.0,
      (signs[1] < 0) ? dims / 4.0 : 3.0 * dims / 4.0,
      (signs[2] < 0) ? dims / 4.0 : 3.0 * dims / 4.0,
    };

    // Unit direction from start toward the final center (dims/2, dims/2, dims/2).
    // Each component of (finalCenter - start) is ±dims/4, so the unnormalized direction is -signs.
    const double invSqrt3 = 1.0 / std::sqrt(3.0);
    const double dir[3] = { -signs[0] * invSqrt3, -signs[1] * invSqrt3, -signs[2] * invSqrt3 };

    // Total distance from start to final center along the diagonal; advance by one step per call.
    const double distPerStep = (dims / 4.0) * std::sqrt(3.0) / numSteps;
    const double dist = step * distPerStep;

    const double cx = start[0] + dist * dir[0];
    const double cy = start[1] + dist * dir[1];
    const double cz = start[2] + dist * dir[2];

    for (int k = 0; k < dims; ++k)
    {
      const double dz = k - cz;
      for (int j = 0; j < dims; ++j)
      {
        const double dy = j - cy;
        for (int i = 0; i < dims; ++i)
        {
          const double dx = i - cx;
          if (dx * dx + dy * dy + dz * dz <= r2)
          {
            scalars[static_cast<vtkIdType>(k) * dims * dims + j * dims + i] =
              static_cast<unsigned short>(label + 1);
          }
        }
      }
    }
  }

  return output;
}

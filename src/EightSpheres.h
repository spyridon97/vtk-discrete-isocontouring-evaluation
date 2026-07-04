//
// EightSpheres.h
//
// Generates the synthetic "eight spheres" non-manifold validation dataset used in the paper's
// evaluation: eight equal-radius spheres, one centered at each corner of a cubic volume, each
// assigned a distinct label. As the convergence sequence progresses, all eight sphere centers
// move linearly toward the center of the bounding box, growing the spheres (or just moving them,
// depending on configuration) until they meet at the volume's center -- producing a configuration
// where up to eight materials meet at a single point, the canonical non-manifold junction that
// vtkSurfaceNets3DNonManifoldCases is designed to detect and resolve.
//
// The function is parameterized by the number of steps in the convergence sequence (suggested
// range: 20-100) and the specific step to generate, so callers (evaluation code, tests) can
// request any single frame of the sequence without needing to know about steps themselves.
//

#ifndef EIGHT_SPHERES_H
#define EIGHT_SPHERES_H

#include "vtkImageData.h"
#include "vtkSmartPointer.h"

#include <array>

/**
 * @brief Generate the eight-spheres synthetic non-manifold validation dataset at a given step
 * of its convergence sequence.
 *
 * Eight spheres of equal radius are placed at the eight corners of a cubic bounding box, each
 * assigned a distinct label in [1,8]. At step 0, the spheres are positioned at their starting
 * corner locations and do not touch. At step == numSteps, all eight sphere centers have moved
 * to the center of the bounding box, so the eight labeled regions meet at a single point.
 * Intermediate steps linearly interpolate the sphere centers between their starting corner
 * positions and the bounding box center.
 *
 * @param dims      Cubic volume dimension (the output vtkImageData has dimensions dims^3).
 * @param numSteps  Total number of steps in the convergence sequence. The step parameter is
 *                  clamped to [0, numSteps]. Suggested range: 20-100.
 * @param step      Which step of the sequence to generate, in [0, numSteps]. A negative value
 *                  is interpreted as "fully converged", i.e. step == numSteps.
 * @return          A labeled vtkImageData volume of type unsigned short, with values
 *                  0 (background) through 8 (one per sphere).
 */
vtkSmartPointer<vtkImageData> GenerateEightSpheres(int dims = 128, int numSteps = 50, int step = -1);

#endif // EIGHT_SPHERES_H

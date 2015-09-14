//
// Created by Media Team on 8/12/15.
//

#include <cstdio>

#include <opencv2/opencv.hpp>

#include <vtkSmartPointer.h>
#include <vtkOBBTree.h>
#include <vtkPolyDataNormals.h>

#include "itk2vtk.h"

#ifndef VC_RAYTRACE_H
#define VC_RAYTRACE_H

namespace volcart {
  namespace meshing {
    // Using vtk's OBBTree to test a ray's intersection with the faces/cells/triangles in the mesh
    std::vector<cv::Vec6f> rayTrace(VC_MeshType::Pointer itkMesh, int aTraceDir, int &width, int &height, std::map<int, cv::Vec2d> &uvMap);
  }
}

#endif //VC_RAYTRACE_H
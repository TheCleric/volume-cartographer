//
// Created by Hannah Hatch on 8/15/16.
//
#ifdef VC_USE_VCGLIB

#include <iostream>
#include "common/vc_defines.h"
#include "meshing/QuadricEdgeCollapseDecimation.h"
#include "common/io/ply2itk.h"
#include "common/io/objWriter.h"
#include "common/shapes/Plane.h"
#include "common/shapes/Arch.h"
#include "common/shapes/Cone.h"
#include "common/shapes/Cube.h"
#include "common/shapes/Sphere.h"
#include "meshing/CalculateNormals.h"

int main(int argc, char*argv[])
{
    volcart::meshing::QuadricEdgeCollapseDecimation Resampler;
    volcart::io::objWriter writer;

    // Plane
    volcart::shapes::Plane plane(10,10);
    Resampler.setMesh(plane.itkMesh());
    Resampler.compute(plane.itkMesh()->GetNumberOfCells() / 2);

    VC_MeshType::Pointer Resample = Resampler.getMesh();
    volcart::meshing::CalculateNormals calcNorm( Resample );
    calcNorm.compute();
    Resample = calcNorm.getMesh();
    writer.setPath("QuadricEdgeCollapse_Plane.obj");
    writer.setMesh(Resample);
    writer.write();

    // Arch
    volcart::shapes::Arch arch(100,100);
    Resampler.setMesh(arch.itkMesh());
    Resampler.compute(arch.itkMesh()->GetNumberOfCells() / 2);

    Resample = Resampler.getMesh();
    calcNorm.setMesh(Resample);
    calcNorm.compute();
    Resample = calcNorm.getMesh();
    writer.setPath("QuadricEdgeCollapse_Arch.obj");
    writer.setMesh(Resample);
    writer.write();

    // Cone
    volcart::shapes::Cone cone(1000,1000);
    Resampler.setMesh(cone.itkMesh());
    Resampler.compute(cone.itkMesh()->GetNumberOfCells() / 2);

    Resample = Resampler.getMesh();
    calcNorm.setMesh(Resample);
    calcNorm.compute();
    Resample = calcNorm.getMesh();
    writer.setPath("QuadricEdgeCollapse_Cone.obj");
    writer.setMesh(Resample);
    writer.write();

    // Cube
    volcart::shapes::Cube cube;
    Resampler.setMesh(cube.itkMesh());
    Resampler.compute(cube.itkMesh()->GetNumberOfCells() / 2);

    Resample = Resampler.getMesh();
    calcNorm.setMesh(Resample);
    calcNorm.compute();
    Resample = calcNorm.getMesh();
    writer.setPath("QuadricEdgeCollapse_Cube.obj");
    writer.setMesh(Resample);
    writer.write();

    // Sphere
    volcart::shapes::Sphere sphere(30,3);
    Resampler.setMesh(sphere.itkMesh());
    Resampler.compute(sphere.itkMesh()->GetNumberOfCells() / 2);

    Resample = Resampler.getMesh();
    calcNorm.setMesh(Resample);
    calcNorm.compute();
    Resample = calcNorm.getMesh();
    writer.setPath("QuadricEdgeCollapse_Sphere.obj");
    writer.setMesh(Resample);
    writer.write();

    return EXIT_SUCCESS;
}
#endif
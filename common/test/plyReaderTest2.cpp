//
// Created by Hannah Hatch on 10/20/16.
//

#define BOOST_TEST_MODULE plyReader2

#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
#include "common/io/PlyReader2.h"
#include "common/io/plyWriter.h"
#include "common/shapes/Arch.h"
#include "common/shapes/Cone.h"
#include "common/shapes/Plane.h"
#include "common/shapes/Sphere.h"
#include "common/vc_defines.h"
#include "testing/parsingHelpers.h"
#include "testing/testingUtils.h"

struct ReadITKPlaneMeshFixture {
    ReadITKPlaneMeshFixture() {
        _in_PlaneMesh = _Plane.itkMesh();

        volcart::io::plyWriter writer( "PLYReader2_plane.ply",_in_PlaneMesh);
        writer.write();
    }

    volcart::ITKMesh::Pointer _in_PlaneMesh;
    volcart::ITKMesh::Pointer _read_PlaneMesh = volcart::ITKMesh::New();
    volcart::shapes::Plane _Plane;

};

struct ReadITKArchMeshFixture {
    ReadITKArchMeshFixture() {
        _in_ArchMesh = _Arch.itkMesh();

        volcart::io::plyWriter writer("PLYReader2_arch.ply",_in_ArchMesh);
        writer.write();

        volcart::testing::ParsingHelpers::parsePlyFile("PLYReader2_arch.ply", _SavedArchPoints, _SavedArchCells);
    }

    volcart::ITKMesh::Pointer _in_ArchMesh;
    volcart::ITKMesh::Pointer _read_ArchMesh;
    volcart::shapes::Arch _Arch;

    std::vector<volcart::Vertex> _SavedArchPoints;
    std::vector<volcart::Cell> _SavedArchCells;
};

struct ReadITKConeMeshFixture {
    ReadITKConeMeshFixture() {
        _in_ConeMesh = _Cone.itkMesh();

        volcart::io::plyWriter writer( "PLYReader2_cone.ply",_in_ConeMesh);
        writer.write();
    }

    volcart::ITKMesh::Pointer _in_ConeMesh;
    volcart::ITKMesh::Pointer _read_ConeMesh;
    volcart::shapes::Cone _Cone;

};

struct ReadITKSphereMeshFixture {
    ReadITKSphereMeshFixture() {
        _in_SphereMesh = _Sphere.itkMesh();

        volcart::io::plyWriter writer("PLYReader2_sphere.ply",_in_SphereMesh);
        writer.write();

    }

    volcart::ITKMesh::Pointer _in_SphereMesh;
    volcart::ITKMesh::Pointer _read_SphereMesh;
    volcart::shapes::Sphere _Sphere;

};

BOOST_FIXTURE_TEST_CASE(ReadPlaneMeshTest, ReadITKPlaneMeshFixture)
{
    volcart::io::PLYReader2("PLYReader2_plane.ply",_read_PlaneMesh);
    BOOST_CHECK_EQUAL(_in_PlaneMesh->GetNumberOfPoints(), _read_PlaneMesh->GetNumberOfPoints());
    BOOST_CHECK_EQUAL(_in_PlaneMesh->GetNumberOfCells(), _read_PlaneMesh->GetNumberOfCells());
    for(unsigned long pnt_id = 0; pnt_id < _in_PlaneMesh->GetNumberOfPoints(); pnt_id++){
        BOOST_CHECK_EQUAL(_in_PlaneMesh->GetPoint(pnt_id)[0], _read_PlaneMesh->GetPoint(pnt_id)[0]);
        BOOST_CHECK_EQUAL(_in_PlaneMesh->GetPoint(pnt_id)[1], _read_PlaneMesh->GetPoint(pnt_id)[1]);
        BOOST_CHECK_EQUAL(_in_PlaneMesh->GetPoint(pnt_id)[2], _read_PlaneMesh->GetPoint(pnt_id)[2]);

        volcart::ITKPixel in_normal;
        volcart::ITKPixel read_normal;

        _in_PlaneMesh->GetPointData(pnt_id, &in_normal);
        _read_PlaneMesh->GetPointData(pnt_id, &read_normal);

        BOOST_CHECK_EQUAL(in_normal[0], read_normal[0]);
        BOOST_CHECK_EQUAL(in_normal[1], read_normal[1]);
        BOOST_CHECK_EQUAL(in_normal[2], read_normal[2]);

    }

    for(unsigned long cell_id = 0; cell_id < _in_PlaneMesh->GetNumberOfCells(); cell_id++){
        volcart::ITKCell::CellAutoPointer in_C;
        _in_PlaneMesh->GetCell(cell_id, in_C);
        volcart::ITKCell::CellAutoPointer read_C;
        _read_PlaneMesh->GetCell(cell_id, read_C);
        BOOST_CHECK_EQUAL(in_C->GetPointIds()[0], read_C->GetPointIds()[0]);
        BOOST_CHECK_EQUAL(in_C->GetPointIds()[1], read_C->GetPointIds()[1]);
        BOOST_CHECK_EQUAL(in_C->GetPointIds()[2], read_C->GetPointIds()[2]);

    }
}
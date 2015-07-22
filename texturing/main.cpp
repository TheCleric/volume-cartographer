// main.cpp
// Abigail Coleman Feb. 2015

#include <iostream>
#include <fstream>

#include <opencv2/opencv.hpp>

#include "volumepkg.h"
#include "vc_defines.h"
#include "io/ply2itk.h"
#include "io/objWriter.h"

#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h>

#include "checkPtInTriangleUtil.h"
#include "texturingUtils.h"

#include <itkRGBPixel.h>

#include "UPointMapping.h"

class LineSegment {
public:
  LineSegment(cv::Vec3f left, cv::Vec3f right, cv::Vec3f left_normal, cv::Vec3f right_normal) {
    _left = left;
    _right = right;
    _left_normal = left_normal;
    _right_normal = right_normal;
  }

  bool aroundPlane(cv::Vec3f point, cv::Vec3f normal) {
    cv::Vec3f left_offset = _left - point;
    cv::Vec3f right_offset = _right - point;

    if (left_offset.dot(normal) * right_offset.dot(normal) < 0) {
      return true;
    }
    return false;
  }

  cv::Vec3f intersectWithPlane(cv::Vec3f point, cv::Vec3f normal) {
    cv::Vec3f l2r = (_right - _left);
    double scale_factor = (point - _left).dot(normal) / l2r.dot(normal);
    return scale_factor * l2r + _left;
  }

  // not the most sophitocated normal estimation
  cv::Vec3f normal() {
    return (_left_normal + _right_normal) / 2;
  }
  
private:
  cv::Vec3f _left;
  cv::Vec3f _right;
  cv::Vec3f _left_normal;
  cv::Vec3f _right_normal;
};



int main(int argc, char* argv[])
{
  if ( argc < 6 ) {
    std::cout << "Usage: vc_texture2 volpkg seg-id radius step-size texture-method sample-direction" << std::endl;
    std::cout << "Texture methods: " << std::endl;
    std::cout << "      0 = Intersection" << std::endl;
    std::cout << "      1 = Non-Maximum Suppression" << std::endl;
    std::cout << "      2 = Maximum" << std::endl;
    std::cout << "      3 = Minimum" << std::endl;
    std::cout << "      4 = Median w/ Averaging" << std::endl;
    std::cout << "      5 = Median" << std::endl;
    std::cout << "      6 = Mean" << std::endl;
    std::cout << std::endl;
    std::cout << "Sample Direction: " << std::endl;
    std::cout << "      0 = Omni" << std::endl;
    std::cout << "      1 = Positive" << std::endl;
    std::cout << "      2 = Negative" << std::endl;
    exit( -1 );
  }

  VolumePkg vpkg = VolumePkg( argv[ 1 ] );
  std::string segID = argv[ 2 ];
  if (segID == "") {
    std::cerr << "ERROR: Incorrect/missing segmentation ID!" << std::endl;
    exit(EXIT_FAILURE);
  }
  if ( vpkg.getVersion() < 2.0) {
    std::cerr << "ERROR: Volume package is version " << vpkg.getVersion() << " but this program requires a version >= 2.0."  << std::endl;
    exit(EXIT_FAILURE);
  }
  vpkg.setActiveSegmentation(segID);
  std::string meshName = vpkg.getMeshPath();
  double radius, minorRadius;
  radius = atof( argv[ 3 ] );
  if((minorRadius = radius / 3) < 1) minorRadius = 1;

  double step_size = atof( argv[ 4 ] );

  int aFindBetterTextureMethod = atoi( argv[ 5 ] );
  EFilterOption aFilterOption = ( EFilterOption )aFindBetterTextureMethod;

  int aSampleDir = atoi( argv[ 6 ] ); // sampleDirection (0=omni, 1=positive, 2=negative)
  EDirectionOption aDirectionOption = ( EDirectionOption )aSampleDir;

  // declare pointer to new Mesh object
  VC_MeshType::Pointer  mesh = VC_MeshType::New();

  int meshWidth = -1;
  int meshHeight = -1;

  // try to convert the ply to an ITK mesh
  if (!volcart::io::ply2itkmesh(meshName, mesh, meshWidth, meshHeight)){
    exit( -1 );
  };

  // Misc. vectors
  std::vector< cv::Vec3d > my3DPoints;    // 3D vector to hold 3D points
  std::vector< cv::Vec3d > my2DPoints;    // 3D vector to hold 2D points along with a 1 in the z coordinate
  cv::Vec3d my3DPoint;
  cv::Vec3d my2DPoint;

  // Homography matrix
  cv::Mat myH( 3, 3, CV_64F );

  // Matrix to store the output texture
  int textureW = meshWidth;
  int textureH = meshHeight;

  // pointID == point's position in 1D list of points
  // [meshX, meshY] == point's position if list was a 2D matrix
  // [u, v] == point's position in the output matrix
  unsigned long pointID, meshX, meshY;
  double u, v;

  // Load the slices from the volumepkg
  std::vector< cv::Mat > aImgVol;

  /*  This function is a hack to avoid a refactoring the texturing
      methods. See Issue #12 for more details. */
  // Setup
  int meshLowIndex = (int) mesh->GetPoint(0)[2];
  int meshHighIndex = meshLowIndex + meshHeight;
  int aNumSlices = vpkg.getNumberOfSlices();

  int bufferLowIndex = meshLowIndex - (int) radius;
  if (bufferLowIndex < 0) bufferLowIndex = 0;

  int bufferHighIndex = meshHighIndex + (int) radius;
  if (bufferHighIndex >= vpkg.getNumberOfSlices()) bufferHighIndex = vpkg.getNumberOfSlices();

  // Slices must be loaded into aImgVol at the correct index: slice 005 == aImgVol[5]
  // To avoid loading the whole volume, pad the beginning indices with 1x1 null mats
  cv::Mat nullMat = cv::Mat::zeros(1, 1, CV_16U);
  for ( int i = 0; i < bufferLowIndex; ++i ) {
    std::cout << "\rLoading null buffer slices: " << i + 1 << "/" << bufferLowIndex << std::flush;
    aImgVol.push_back( nullMat.clone() );
  }
  std::cout << std::endl;

  // Load the actual volume into a tempVol with a buffer of nRadius
  for ( int i = bufferLowIndex; i < bufferHighIndex; ++i ) {
    std::cout << "\rLoading real slices: " << i - bufferLowIndex + 1 << "/" << bufferHighIndex - bufferLowIndex << std::flush;
    aImgVol.push_back( vpkg.getSliceData( i ).clone() );
  }
  std::cout << std::endl;

  // Initialize iterators
  VC_CellIterator  cellIterator = mesh->GetCells()->Begin();
  VC_CellIterator  cellEnd      = mesh->GetCells()->End();
  VC_CellType *    cell;
  VC_CellType *    next_cell;
  VC_PointsInCellIterator pointsIterator;
  pcl::PointCloud<pcl::PointNormal>::Ptr new_cloud ( new pcl::PointCloud<pcl::PointNormal> );

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  // Get vertices and normals from mesh
  // Vector of vectors used to organize data
  // so we don't have to mess with itk
  std::vector<std::vector<cv::Vec3f> > points;
  std::vector<std::vector<cv::Vec3f> > normals;
  for (int i = 0; i < meshHeight; ++i) {
    std::vector<cv::Vec3f> point_row;
    std::vector<cv::Vec3f> normal_row;

    // look at a row of the mesh
    for (int k = 0; k < meshWidth - 1; ++k) {
      cell = cellIterator.Value();
      pointsIterator = cell->PointIdsBegin();
      unsigned long pointID = *pointsIterator;
      VC_PointType p = mesh->GetPoint(pointID);
      VC_PixelType n;
      mesh->GetPointData(pointID, &n);

      point_row.push_back(cv::Vec3f(p[0],p[1],p[2]));
      normal_row.push_back(cv::Vec3f(n[0],n[1],n[2]));

      // We only look at every other cell to avoid duplication
      cellIterator++;
      if (cellIterator == cellEnd)
        break;
      cellIterator++;
      if (cellIterator == cellEnd)
        break;
    }

    points.push_back(point_row);
    normals.push_back(normal_row);
    if (cellIterator == cellEnd)
      break;
  }

  // calculate running length of line segments for each row
  std::vector<cv::Vec3f> first_row = points[0];
  std::vector<double> diff_row;
  double sum = 0;
  diff_row.push_back(sum);
  for (int x = 0; x < first_row.size()-1; ++x) {

    // find the distance between neighboring points
    //  | b - a | = sqrt ((b - a) . (b - a))
    cv::Vec3f offset = first_row[x+1] - first_row[x];
    double len = sqrt(offset.dot(offset));

    sum += len;
    diff_row.push_back(sum);
  }

  std::cout << diff_row[diff_row.size() - 1] << std::endl;

  // resample first row
  std::vector<cv::Vec3f> resampled_first_row;
  for (int x = 0; x < (int)diff_row[diff_row.size() - 1] / step_size; ++x) {
    // we want to make sure this will work with a step size
    // of one before we get fancy
    double delta = x * step_size;

    // increase the index until the interpolated position is between
    // two existing points (by their accumulated distance)
    int lower_bound = 0;
    while (lower_bound < diff_row.size() && delta > diff_row[lower_bound]) {
      ++lower_bound;
    }
    int upper_bound = lower_bound + 1;

    // find out how far the new point is past the low index
    //  new_x - low_x
    // ---------------
    // high_x - low_x
    double scale_factor = (delta - diff_row[lower_bound]) / (diff_row[upper_bound] - diff_row[lower_bound]);

    // points from mesh that are around the interpolated point
    cv::Vec3f left = first_row[lower_bound];
    cv::Vec3f right = first_row[upper_bound];

    // find the new point
    // t*(p1 - p0) + p0
    cv::Vec3f CORRECT_POINT = scale_factor * (right - left) + left;
    resampled_first_row.push_back(CORRECT_POINT);

  }


  cv::Mat outputTexture = cv::Mat::zeros(textureH, resampled_first_row.size() +20, CV_16UC1);
  std::cout << "texture size is: " << textureH << "x(" << resampled_first_row.size() << "+20)" << std::endl;

#define NORMAL_AT(x) n = resampled_first_row[x+1] - resampled_first_row[x-1]  

  // for each row after the first one
  for (int row = 1; row < points.size(); ++row) {
    // for each point in the resampled first row
    for (int p = 0; p < resampled_first_row.size(); ++p) {
      cv::Vec3f reference_point = resampled_first_row[p];
      cv::Vec3f n;
      if (p == 0) {
        NORMAL_AT(p + 1);
      } else if (p == resampled_first_row.size() - 1) {
        NORMAL_AT(p - 1);
      } else {
        NORMAL_AT(p);
      }

      n(2) = 0;

      // find intersection of plane with row
      std::vector<LineSegment> potential;
      for (int i = 0; i < points[row].size() - 1; ++i) {
        LineSegment ls(points[row][i], points[row][i+1], normals[row][i], normals[row][i+1]);
        if (ls.aroundPlane(reference_point, n)) {
          potential.push_back(ls);
        }
      }

      int index = -1;
      double distance = 10000;
      for (int i = 0; i < potential.size(); ++i) {
        cv::Vec3f intersect = potential[i].intersectWithPlane(reference_point, n);
        double thisDistance = cv::norm(reference_point - intersect);
        if (thisDistance < distance) {
          distance = thisDistance;
          index = i;
        }
      }

      if (index != -1) {
        cv::Vec3f intersect = potential[index].intersectWithPlane(reference_point, n);
        cv::Vec3f NORMAL = potential[index].normal();
        double value = textureWithMethod( intersect,
                                          NORMAL,
                                          aImgVol,
                                          aFilterOption,
                                          radius,
                                          minorRadius,
                                          0.5,
                                          aDirectionOption);
        outputTexture.at<unsigned short>(row, p) = (unsigned short)value;
        pcl::PointNormal p;
        p.x = intersect[0];
        p.y = intersect[1];
        p.z = intersect[2];
        p.normal[0] = NORMAL[0];
        p.normal[1] = NORMAL[1];
        p.normal[2] = NORMAL[2];
        new_cloud->push_back(p);
      }
      
      
    }
  }

  // // total length of the first row of points
  // double real_chain_length = difference_array[0][difference_array[0].size() - 1];

  // // create an output texture with a little wiggle room
  // cv::Mat outputTexture = cv::Mat::zeros(textureH, (int)real_chain_length+20, CV_16UC1);


  // for (int i = 0; i < points.size(); ++i) {
  //   for (int x = 0; x < (int)real_chain_length; ++x) {
  //     // we want to make sure this will work with a step size
  //     // of one before we get fancy
  //     double delta = x;
  //     std::cout << "delta " << delta << std::endl;

  //     // increase the index until the interpolated position is between
  //     // two existing points (by their accumulated distance)
  //     int lower_bound = 0;
  //     while (lower_bound < difference_array[i].size() && delta > difference_array[i][lower_bound]) {
  //       ++lower_bound;
  //     }
  //     int upper_bound = lower_bound + 1;

  //     // find out how far the new point is past the low index
  //     //  new_x - low_x
  //     // ---------------
  //     // high_x - low_x
  //     double scale_factor = (delta - difference_array[i][lower_bound]) / (difference_array[i][upper_bound] - difference_array[i][lower_bound]);

  //     // points from mesh that are around the interpolated point
  //     cv::Vec3f left = points[i][lower_bound];
  //     cv::Vec3f right = points[i][upper_bound];

  //     // find the new point
  //     // t*(p1 - p0) + p0
  //     cv::Vec3f CORRECT_POINT = scale_factor * (right - left) + left;

  //     // normal vectors // we're just averaging the normals for now
  //     cv::Vec3f normal_left = normals[i][lower_bound];
  //     cv::Vec3f normal_right = normals[i][upper_bound];
  //     cv::Vec3f CORRECT_NORMAL = (normal_right + normal_left) / 2;

  //     pcl::PointNormal p;
  //     p.x = CORRECT_POINT[0];
  //     p.y = CORRECT_POINT[1];
  //     p.z = CORRECT_POINT[2];
  //     p.normal[0] = CORRECT_NORMAL[0];
  //     p.normal[1] = CORRECT_NORMAL[1];
  //     p.normal[2] = CORRECT_NORMAL[2];

  //     new_cloud->push_back(p);

  //     // texture the point like normal and add it to the output image
  //     double value = textureWithMethod( CORRECT_POINT,
  //                                       CORRECT_NORMAL,
  //                                       aImgVol,
  //                                       aFilterOption,
  //                                       radius,
  //                                       minorRadius,
  //                                       0.5,
  //                                       aDirectionOption);
  //     outputTexture.at<unsigned short>(i, x) = (unsigned short)value;
  //   }
  // }

  vpkg.saveTextureData(outputTexture);
  pcl::io::savePCDFileASCII("resampled.pcd", *new_cloud);

  return 0;
} // end main

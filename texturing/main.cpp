// main.cpp
// Abigail Coleman Feb. 2015

#include <iostream>
#include <fstream>

#include <opencv2/opencv.hpp>

#include "volumepkg.h"
#include "vc_defines.h"
#include "io/ply2itk.h"

#include "texturingUtils.h"
#include <itkRGBPixel.h>
#include "UPointMapping.h"

#define VC_INDEX_X 0
#define VC_INDEX_Y 1
#define VC_INDEX_Z 2

#define VC_DIRECTION_I cv::Vec3f(1,0,0)
#define VC_DIRECTION_J cv::Vec3f(0,1,0)
#define VC_DIRECTION_K cv::Vec3f(0,0,1)

namespace MIKE {
  class Triangle {
  public:
    std::vector<cv::Vec3f> corner;

    double minBy(int index) {
      return std::min(corner[0](index), std::min(corner[1](index), corner[2](index)));
    }
    double maxBy(int index) {
      return std::max(corner[0](index), std::max(corner[1](index), corner[2](index)));
    }

    bool pointInTriangle(cv::Vec3f point) {
      if (sameSide(point, corner[0], corner[1], corner[2]) &&
          sameSide(point, corner[1], corner[0], corner[2]) &&
          sameSide(point, corner[2], corner[0], corner[1])) {
        return true;
      }
      return false;
    }

    cv::Vec3f intersect(cv::Vec3f ray_origin, cv::Vec3f ray_direction) {
      cv::Vec3f normal = (corner[2] - corner[0]).cross(corner[1] - corner[0]);
      double scale_factor = (corner[0] - ray_origin).dot(normal) / ray_direction.dot(normal);
      cv::Vec3f point = scale_factor * ray_direction + ray_origin;
      return point;
    }

    cv::Vec3f normal() {
      cv::Vec3f a2b = corner[1] - corner[0];
      cv::Vec3f a2c = corner[2] - corner[0];
      return a2b.cross(a2c);
    }

  private:

    // a Barycentric method will probably be faster but this works for the time being.
    bool sameSide(cv::Vec3f point0, cv::Vec3f point1, cv::Vec3f alpha, cv::Vec3f beta) {
      cv::Vec3f a2b = beta - alpha;
      cv::Vec3f a2_point0 = point0 - alpha;
      cv::Vec3f a2_point1 = point1 - alpha;
      cv::Vec3f cp0 = a2b.cross(a2_point0);
      cv::Vec3f cp1 = a2b.cross(a2_point1);

      if (cp0.dot(cp1) >= 0) {
        return true;
      }
      return false;
    }
  };

  class TriangleStore {
  public:
    TriangleStore(double e = 0.1) {
      epsilon_ = e;

      upper_bound_x_ = 0;
      lower_bound_x_ = std::numeric_limits<double>::max();

      upper_bound_y_ = 0;
      lower_bound_y_ = std::numeric_limits<double>::max();

      upper_bound_z_ = 0;
      lower_bound_z_ = std::numeric_limits<double>::max();

    }

    void push_back(Triangle t) {
      double minZ = t.minBy(VC_INDEX_Z);
      double maxZ = t.maxBy(VC_INDEX_Z);

      minZ -= epsilon_;
      maxZ += epsilon_;

      for (int i = std::floor(minZ); i < std::ceil(maxZ); ++i) {
        bin_[i].push_back(t);
      }

      // record upper and lower bounds of triangle Xs and Ys
      if (t.minBy(VC_INDEX_X) < lower_bound_x_) {
        lower_bound_x_ = t.minBy(VC_INDEX_X);
      }
      if (t.maxBy(VC_INDEX_X) > upper_bound_x_) {
        upper_bound_x_ = t.maxBy(VC_INDEX_X);
      }
      if (t.minBy(VC_INDEX_Y) < lower_bound_y_) {
        lower_bound_y_ = t.minBy(VC_INDEX_Y);
      }
      if (t.maxBy(VC_INDEX_Y) > upper_bound_y_) {
        upper_bound_y_ = t.maxBy(VC_INDEX_Y);
      }
      if (minZ < lower_bound_z_) {
        lower_bound_z_ = minZ;
      }
      if (maxZ > upper_bound_z_) {
        upper_bound_z_ = maxZ;
      }


    }

    void info() {
      std::cout << bin_.size() << " bins used" << std::endl;
      std::cout << "x bounds: [" << lower_bound_x_ << " .. " << upper_bound_x_ << "]" << std::endl;
      std::cout << "y bounds: [" << lower_bound_y_ << " .. " << upper_bound_y_ << "]" << std::endl;

    }
    double epsilon_;
    double upper_bound_x_;
    double lower_bound_x_;

    double upper_bound_y_;
    double lower_bound_y_;

    double upper_bound_z_;
    double lower_bound_z_;

    std::map<int,std::vector<Triangle> > bin_;
  };


}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
  if ( argc < 6 ) {
    std::cout << "Usage: vc_texture2 volpkg seg-id radius texture-method sample-direction" << std::endl;
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

  int aFindBetterTextureMethod = atoi( argv[ 4 ] );
  EFilterOption aFilterOption = ( EFilterOption )aFindBetterTextureMethod;

  int aSampleDir = atoi( argv[ 5 ] ); // sampleDirection (0=omni, 1=positive, 2=negative)
  EDirectionOption aDirectionOption = ( EDirectionOption )aSampleDir;

  // declare pointer to new Mesh object
  VC_MeshType::Pointer mesh = VC_MeshType::New();
  int meshWidth = -1;
  int meshHeight = -1;

  if (!volcart::io::ply2itkmesh(meshName, mesh, meshWidth, meshHeight)) {
    exit(-1);
  }

  // Matrix to store the output texture
  int textureW = meshWidth;
  int textureH = meshHeight;
  //  cv::Mat outputTexture = cv::Mat::zeros( textureH, textureW, CV_16UC1 );

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
  if (bufferHighIndex >= vpkg.getNumberOfSlices()) {
    bufferHighIndex = vpkg.getNumberOfSlices();
  }

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

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  MIKE::TriangleStore storage;

  VC_CellIterator cellIterator = mesh->GetCells()->Begin();
  while(cellIterator != mesh->GetCells()->End()) {
    VC_CellType* cell = cellIterator.Value();
    VC_PointsInCellIterator pointsIterator = cell->PointIdsBegin();

    MIKE::Triangle tri;
    while (pointsIterator != cell->PointIdsEnd()) {
      pointID = *pointsIterator;
      VC_PointType p = mesh->GetPoint(pointID);
      tri.corner.push_back(cv::Vec3f(p[0], p[1], p[2]));
      ++pointsIterator;
    }

    storage.push_back(tri);
    ++cellIterator;
  }

  storage.info();

  // now we have the triangles
  // do ray tracing

  FILE* file_points;
  file_points = fopen("points.txt","r");
  if (file_points == NULL) {
    std::cout << "couldn't open points.txt" << std::endl;
    exit(EXIT_FAILURE);
  }
  FILE* file_normals;
  file_normals = fopen("normals.txt","r");
  if(file_normals == NULL) {
    std::cout << "couldn't open normals.txt" << std::endl;
    exit(EXIT_FAILURE);
  }



  struct {
    std::vector<cv::Vec3f> origin;
    std::vector<cv::Vec3f> direction;
  } rays;

  while (!feof(file_points) && !feof(file_normals)) {
    float a, b, c;

    fscanf(file_points, "[%f, %f, %f]\n", &a, &b, &c);
    rays.origin.push_back(cv::Vec3f(a,b,c));

    fscanf(file_normals, "[%f, %f, %f]\n", &a, &b, &c);
    rays.direction.push_back(cv::Vec3f(a,b,c).cross(VC_DIRECTION_K));
  }

  std::cout << rays.origin.size() << std::endl;
  std::cout << rays.direction.size() << std::endl;

  std::cout << "storage " << storage.upper_bound_z_ << "\t" << storage.lower_bound_z_ << std::endl;

  cv::Mat outputTexture = cv::Mat::zeros( (int)storage.upper_bound_z_ + 20, rays.origin.size() + 20, CV_16UC1 );

  for (int z = (int)storage.lower_bound_z_; z < (int)storage.upper_bound_z_; ++z) {
    std::vector<MIKE::Triangle> triangle_row = storage.bin_[z];
    for (int r = 0; r < rays.origin.size(); ++r) {
      cv::Vec3f origin = rays.origin[r];
      origin(VC_INDEX_Z) = z;
      cv::Vec3f direction = rays.direction[r];

      // should do something to deal with multiple intersections
      for (int t = 0; t < triangle_row.size(); ++t) {
        MIKE::Triangle tri = triangle_row[t];
        cv::Vec3f p = tri.intersect(origin,direction);

        if (tri.pointInTriangle(p)) {
          double color = textureWithMethod( p,
                                            tri.normal(),
                                            aImgVol,
                                            aFilterOption,
                                            radius,
                                            minorRadius,
                                            0.5,
                                            aDirectionOption);
          outputTexture.at<unsigned short>(z, r) = (unsigned short)color;
          break;
        }
      }

    }
  }

  vpkg.saveTextureData(outputTexture);
  return 0;
}


// VC_PixelType normal;

// mesh->GetPointData( pointID, &normal );
// meshX = pointID % meshWidth;
// meshY = (pointID - meshX) / meshWidth;

// u = (double)textureW * (double)meshX / (double)meshWidth;
// v = (double)textureH * (double)meshY / (double)meshHeight;

// double color = textureWithMethod(cv::Vec3f(p[0], p[1], p[2]),
//                                  cv::Vec3f(normal[0], normal[1], normal[2]),
//                                  aImgVol,
//                                  aFilterOption,
//                                  radius,
//                                  minorRadius,
//                                  0.5,
//                                  aDirectionOption);

// outputTexture.at<unsigned short>(v, u) = (unsigned short)color;

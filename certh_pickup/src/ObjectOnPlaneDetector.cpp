#include "ObjectOnPlaneDetector.h"

#include <pcl/io/pcd_io.h>

#include <pcl/sample_consensus/method_types.h>
#include <pcl/sample_consensus/model_types.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/octree/octree_impl.h>
#include <pcl/octree/octree_pointcloud_density.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/filters/project_inliers.h>
#include <pcl/common/pca.h>

#include <highgui.h>
#include "cvblob/cvBlob/cvblob.h"


using namespace std ;

ObjectOnPlaneDetector::ObjectOnPlaneDetector(const CloudType &cloud): in_cloud_(cloud)
{

}

ObjectOnPlaneDetector::ObjectOnPlaneDetector(const cv::Mat &clr_im, const cv::Mat &depth_im, double fx, double fy, double cx, double cy) {
    in_cloud_ = cvMatToCloud(clr_im, depth_im, fx, fy, cx, cy) ;

    pcl::io::savePCDFileBinary("/tmp/cloud.pcd", in_cloud_) ;
}


CloudType ObjectOnPlaneDetector::cvMatToCloud(const cv::Mat &clr_im, const cv::Mat &depth_im, double fx, double fy, double center_x, double center_y)
{
    float constant_x =  0.001/fx ;
    float constant_y =  0.001/fy ;
    float bad_point = std::numeric_limits<float>::quiet_NaN ();

    int w = clr_im.cols, h = clr_im.rows ;

    cv::Mat_<ushort> dim(depth_im) ;
    cv::Mat_<cv::Vec3b> cim(clr_im) ;

    CloudType cloud(w, h) ;

    for( int i=0 ; i<h ; i++ )
        for(int j=0 ; j<w ; j++)
        {
            pcl::PointXYZRGB& pt = cloud.at(j, i) ;
            ushort depth = dim[i][j] ;
            cv::Vec3b clr = cim[i][j] ;

            if ( depth == 0 ) {
                pt.x = pt.y = pt.z = bad_point;
            }
            else
            {
                pt.x = (j - center_x) * depth * constant_x;
                pt.y = (i - center_y) * depth * constant_y;
                pt.z = depth/1000.0 ;
            }


            pt.r = clr[0] ;
            pt.g = clr[1] ;
            pt.b = clr[2] ;
        }


    return cloud ;
}

static double calculateCloudDensity(const CloudType &cloud, double voxelSize = 0.01)
{
    CloudType::Ptr in_cloud(new CloudType(cloud)) ;

    pcl::octree::OctreePointCloudDensity<PointType> oc(0.01) ;
    oc.setInputCloud(in_cloud);
    oc.addPointsFromInputCloud ();

    pcl::octree::OctreePointCloudDensity<PointType>::LeafNodeIterator it(oc) ;

    int nVoxels = 0 ;
    double avgDensity = 0.0 ;

    ++it ;
    while ( *it )
    {
        pcl::octree::OctreePointCloudDensityLeaf<int> *leaf = (pcl::octree::OctreePointCloudDensityLeaf<int> *)(*it) ;
        int c = leaf->getPointCounter() ;

        avgDensity += voxelSize*voxelSize*voxelSize/c ;
        nVoxels ++ ;
        ++it ;
    }

    avgDensity /= nVoxels ;

    return pow(avgDensity, 1/3.0) ;

}

bool ObjectOnPlaneDetector::findPlane(Eigen::Vector3d &n, double &d, cv::Mat &mask)
{
    int w = in_cloud_.width, h = in_cloud_.height ;

    CloudType::Ptr cloud_plane(new CloudType) ;
    CloudType::Ptr cloud_rest(new CloudType) ;
    CloudType::Ptr cloud_object(new CloudType) ;
    CloudType::Ptr cloud_projected(new CloudType) ;
    CloudType::Ptr cloud_in (new CloudType(in_cloud_));

    // Create model coefficients and inliners

    vector<pcl::ModelCoefficients::Ptr> coefficients ;
    vector<Eigen::Vector4f> centers ;

    // Segmentation object and configuration

    pcl::SACSegmentation<pcl::PointXYZRGB> seg;
    seg.setOptimizeCoefficients (true);
    seg.setProbability(0.99);
    seg.setModelType (pcl::SACMODEL_PLANE);
    seg.setMethodType (pcl::SAC_RANSAC);
    seg.setDistanceThreshold (0.01);
    seg.setMaxIterations(100);

    pcl::ExtractIndices<PointType> extract;

    pcl::ModelCoefficients::Ptr coeff (new pcl::ModelCoefficients);
    pcl::PointIndices::Ptr inliers_plane (new pcl::PointIndices);

    // Add input cloud and segment
    seg.setInputCloud (cloud_in);
    seg.segment (*inliers_plane, *coeff);

    if ( inliers_plane->indices.size() == 0 ) return false ;

    // Extract indices

    extract.setInputCloud (cloud_in);
    extract.setIndices (inliers_plane);
    extract.setNegative (false);
    extract.filter (*cloud_plane);

    Eigen::Vector4f center ;
    pcl::compute3DCentroid (*cloud_in, *inliers_plane, center);
    /*

    pcl::PCA< PointType > pca;
    CloudType proj;

    pca.setInputCloud (cloud_plane);
    pca.project (*cloud_plane, proj);

    PointType proj_min;
    PointType proj_max;
    pcl::getMinMax3D (proj, proj_min, proj_max);

    double area = (proj_max.x - proj_min.x)*(proj_max.y - proj_min.y) ;

    Eigen::Vector4f center = pca.getMean() ;
*/
    extract.setNegative (true);
    extract.filter (*cloud_rest);

  //  p = Eigen::Vector3d(center.x(), center.y(), center.z()) ;
    n = Eigen::Vector3d(coeff->values[0], coeff->values[1], coeff->values[2]) ;
    d = coeff->values[3] ;

    mask = cv::Mat::zeros(h, w, CV_8UC1) ;
    for(int i=0 ; i<inliers_plane->indices.size() ; i++ )
    {
        int index_ = inliers_plane->indices[i] ;
        int row = index_ / w;
        int col = index_ % w ;

        mask.at<uchar>(row, col) = 255 ;
    }

    return true ;
}

cv::Mat ObjectOnPlaneDetector::findObjectMask(const Eigen::Vector3d &n, double d, double thresh, vector<cv::Point> &hull)
{
    int w = in_cloud_.width, h = in_cloud_.height ;

    cv::Mat mask = cv::Mat::zeros(h, w, CV_8UC1) ;

    for( int i=0 ; i<h ; i++ )
        for(int j=0 ; j<w ; j++ )
        {
            PointType &p = in_cloud_.at(j, i) ;

            if ( !pcl::isFiniteFast(p) ) {
                mask.at<uchar>(i, j) = 0 ;
                continue ;
            }

            Eigen::Vector3d p_(p.x, p.y, p.z) ;

            double dist = p_.dot(n) + d ;

            if ( dist < -thresh ) mask.at<uchar>(i, j) = 255 ;
    }

    cv::imwrite("/tmp/mask0.png", mask) ;
    cv::Mat fgMask = getForegroundMask(mask, hull, 100) ;

    return fgMask ;

}


cv::Mat ObjectOnPlaneDetector::getForegroundMask(const cv::Mat &mask_ref, vector<cv::Point> &hull, int minArea)
{
    int w = mask_ref.cols, h = mask_ref.rows ;
    cv::Mat planeMask = cv::Mat::zeros(h, w, CV_8UC1) ;

    // find largest blob in image

    cv::Mat_<uchar> mask_erode ;

    cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
    cv::erode(mask_ref, mask_erode, element) ;

    cvb::CvBlobs blobs ;
    IplImage mask_erode_ipl = mask_erode ;

    IplImage *labelImg = cvCreateImage(mask_erode.size(),IPL_DEPTH_LABEL,1);
    cvb::cvLabel(&mask_erode_ipl, labelImg, blobs) ;

    cvb::cvFilterByArea(blobs, minArea, w*h);

    if ( blobs.size() == 0 ) return planeMask ;

    cv::Mat labels(labelImg) ;

    cvb::CvLabel gb = cvb::cvGreaterBlob(blobs) ;
    cvb::cvFilterByLabel(blobs, gb) ;

    cvb::CvBlobs::const_iterator it = blobs.begin() ;

    cvb::CvBlob *blob = (*it).second ;

    for(int y=blob->miny ; y<=blob->maxy ; y++)
        for(int x=blob->minx ; x<=blob->maxx ; x++)
        {
            if ( labels.at<cvb::CvLabel>(y, x) == gb )
                planeMask.at<uchar>(y, x) = 255 ;
        }

    cv::imwrite("/tmp/plane.png", planeMask) ;

    // get the polygon of the blob and create a mask with the internal points

    cvb::CvContourPolygon *poly = cvb::cvConvertChainCodesToPolygon(&blob->contour) ;

    vector<cv::Point> contour ;

    for(int i=0 ; i<poly->size() ; i++ )
        contour.push_back(cv::Point((*poly)[i].x, (*poly)[i].y)) ;

    /// Find the convex hull object for each contour

    cv::convexHull( cv::Mat(contour), hull, false );

    /*

    cv::Mat planeMaskFilled = cv::Mat::zeros(h, w, CV_8UC1) ;
    cv::fillPoly(planeMaskFilled, contours, cv::Scalar(255)) ;

    // find points in the mask that are not background

    cv::Mat mask ;

    cv::bitwise_xor(planeMask, planeMaskFilled, mask) ;
*/
    cvReleaseImage(&labelImg);

    return planeMask ;

}

cv::Mat ObjectOnPlaneDetector::refineSegmentation(const cv::Mat &clr, const cv::Mat &fgMask, vector<cv::Point> &hull)
{
    int w = clr.cols, h = clr.rows ;

    cv::Mat result(h, w, CV_8UC1) ;

    for(int i=0 ; i<h ; i++)
        for(int j=0 ; j<w ; j++)
            result.at<uchar>(i, j) = fgMask.at<uchar>(i, j) ? cv::GC_PR_FGD : cv::GC_PR_BGD ;

    cv::Rect rectangle ;

    cv::Mat bgModel,fgModel; // the models (internally used)

    cv::grabCut(clr, result, rectangle, bgModel, fgModel, 1, cv::GC_INIT_WITH_MASK);

    cv::grabCut(clr, result, rectangle, bgModel, fgModel, 1, cv::GC_EVAL);


    // Get the pixels marked as likely foreground
    cv::compare(result,cv::GC_PR_FGD, result, cv::CMP_EQ);

    cv::Mat fgMask_ = getForegroundMask(result, hull, 100) ;

    // Generate output image
    cv::Mat foreground(clr.size(),CV_8UC3,cv::Scalar(255,255,255));
    clr.copyTo(foreground, fgMask_); // bg pixels not copied

    cv::imwrite("/tmp/seg.png", foreground) ;



    return fgMask_ ;
}
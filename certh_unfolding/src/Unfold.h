#ifndef __ROBOT_HELPERS_UNFOLDING_H__
#define __ROBOT_HELPERS_UNFOLDING_H__

#include <robot_helpers/Utils.h>
#include <robot_helpers/Geometry.h>
#include <geometric_shapes/shape_operations.h>
#include <planning_environment/util/construct_object.h>
#include <geometry_msgs/Pose.h>
#include <arm_navigation_msgs/CollisionObject.h>
#include <arm_navigation_msgs/GetPlanningScene.h>
#include <clopema_motoros/SetGripperState.h>
#include <visualization_msgs/MarkerArray.h>
#include <planning_environment/models/collision_models.h>
#include <tf/transform_listener.h>
#include <tf_conversions/tf_eigen.h>

#include <cv_bridge/cv_bridge.h>

#include <pcl/point_cloud.h>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/ros/conversions.h>


#include <nodelet/nodelet.h>
#include <nodelet/loader.h>
#include <iostream>
#include <fstream>
#include <ros/ros.h>
#include <camera_helpers/OpenNICapture.h>
#include <camera_helpers/OpenNIServiceClient.h>


#include <cv.h>
#include <Eigen/Core>
#include <Eigen/Eigenvalues> // for cwise access

using namespace std;
using namespace robot_helpers;



class Unfold {



public:
    Unfold(const string &armName);
    Unfold();
    virtual ~Unfold();

private:


    string holdingArm;
    string movingArm;
    int clothType; //0 shirt ,1 trousers, 2 shorts1, 3 shorts2, 4 T-shirt1, 5 T-shirt2

public:
    string camera ;

    void setClothType( int type);
    string getHoldingArm();
    string getMovingArm();

    inline Eigen::Matrix3d vertical(){
        Eigen::Matrix3d vertical;
        if(holdingArm == "r1"){
            vertical << 0, -1, 0,-1, 0, 0, 0, 0, -1;
        }
        else{
            vertical << 0, 1, 0, 1, 0, 0, 0, 0, -1;
        }
        return vertical;
    }

    inline Eigen::Matrix3d horizontal(){
        Eigen::Matrix3d horizontal;
        if(holdingArm == "r1"){
            horizontal << 0, 0, -1,
                    -1, 0, 0,
                    0, 1, 0 ;
        }
        else{
            horizontal << 0, 0, 1, 1, 0, 0, 0, 1, 0 ;
        }
        return horizontal;
    }

    inline Eigen::Matrix3d diagonalDown(){
        Eigen::Matrix3d diagonalDown;
        if(holdingArm == "r1"){
            diagonalDown << 0, -1, -1, -1, 0, 0, 0, 1, -1 ;
        }
        else{
            diagonalDown <<  0, 1, 1, 1, 0, 0, 0, 1, -1 ;
        }
        return diagonalDown;
    }

    inline Eigen::Matrix3d diagonalUp(){
        Eigen::Matrix3d diagonalUp;
        if(holdingArm == "r1"){
            diagonalUp << 0, 0.5f, -sqrt(3)/2.0f, -1, 0, 0, 0, sqrt(3)/2.0f, 0.5f ;
        }
        else{
            diagonalUp <<  0, -0.5f, sqrt(3)/2.0f, 1, 0, 0, 0, sqrt(3)/2.0f, 0.5f ;
        }
        return diagonalUp;
    }

    inline geometry_msgs::Pose holdingArmPose(){
        geometry_msgs::Pose pose;
        pose.orientation = rotationMatrix3ToQuaternion(vertical());
        if (holdingArm == "r1"){
            pose.position.x = -0.12;
            pose.position.y = -0.83;
            pose.position.z = 1.6;
        }
        else{
            pose.position.x = -0.23;
            pose.position.y = -0.8;
            pose.position.z = 1.58;
        }
        return pose;
    }



    inline geometry_msgs::Pose movingArmPose(){
        geometry_msgs::Pose pose;
        pose.orientation = rotationMatrix3ToQuaternion(horizontal());
        if (holdingArm == "r1"){
            pose.position.x = 0.7;
            pose.position.y = -0.8;
            pose.position.z = 1.1;
        }
        else{
            pose.position.x = -0.7 ;
            pose.position.y = -0.8;
            pose.position.z = 1.1;
        }
        return pose;
    }


//    inline geometry_msgs::Pose movingArmPose(){
//        geometry_msgs::Pose pose;
//        pose.orientation = rotationMatrix3ToQuaternion(vertical());
//        if (holdingArm == "r1"){
//            pose.position.x = 0.7 ;
//            pose.position.y = -1;
//            pose.position.z = 1.3;
//        }
//        else{
//            pose.position.x = -0.7 ;
//            pose.position.y = -0.5;
//            pose.position.z = 1.5;
//        }
//        return pose;
//    }

    void switchArms();
    void setHoldingArm(const string &armName);

    int setGripperStates(const string &armName  , bool open);
    int setGrippersStates( bool open);


    void rotateHoldingGripper(float angle );

    Eigen::Matrix4d findLowestPointOrientation(Eigen::Vector4d vector );
    Eigen::Matrix4d findGraspingPointOrientation(Eigen::Vector4d vector, bool orientUp = false);

    float findBias(Eigen::Vector4d vector);
    bool findLowestPoint(const pcl::PointCloud<pcl::PointXYZ> &depth, const Eigen::Vector3d &orig, const Eigen::Vector3d &base, float apperture, Eigen::Vector3d &p, Eigen::Vector3d &n);
    void findMeanShiftPoint(const pcl::PointCloud<pcl::PointXYZ> &depth, int x0, int y0, int &x1, int &y1, double radius, double variance = 1.0e-3, int maxIter = 10);

    bool pointInsideCone(const Eigen::Vector3d &x, const Eigen::Vector3d &apex, const Eigen::Vector3d &base, float aperture);
    Eigen::Vector3d computeNormal(const pcl::PointCloud<pcl::PointXYZ> &pc, int x, int y);
    void robustPlane3DFit(vector<Eigen::Vector3d> &x, Eigen::Vector3d  &c, Eigen::Vector3d &u);

    bool parkArmsForGrasping() ;

    bool graspLowestPoint(bool lastMove = false , bool allwaysDrop = false );
    bool graspPoint(const  pcl::PointCloud<pcl::PointXYZ> &pc,  int x, int y , bool lastMove = false, bool orientLeft = true, bool orientUp = false, bool alwaysDrop = false );
    bool flipCloth(bool allwaysDrop = false);
    bool moveToCheckGrasping() ;

    int moveArmsFlipCloth( float radious, geometry_msgs::Pose pose1 , geometry_msgs::Pose pose2, const string &arm1Name,  const std::string &arm2Name = "r1" );
    bool releaseCloth( const string &armName );
    bool showUnfolding();

    bool confirmGrasping();

    bool grabFromXtion(cv::Mat &rgb, cv::Mat &depth, pcl::PointCloud<pcl::PointXYZ> &pc );
    bool grabFromXtion(cv::Mat &rgb, cv::Mat &depth, pcl::PointCloud<pcl::PointXYZ> &pc, cv::Rect & r);

    bool findMiddlePoint(const pcl::PointCloud<pcl::PointXYZ> &depth, int &x , int &y) ;
    bool graspMiddle() ;


    //------- DIMITRA--------

public:

    struct grabRGBD {

        cv::Mat rgb ;
        cv::Mat depth ;
        Eigen::Matrix4d gripperOrientation;

    };

    std::vector <grabRGBD> grabRGBD360() ;
//    std::vector <grabRGBD> rotateAndGrab() ;

//    void startCapture() ;
//    void stopCapture() ;
//    void doCapture() ;


};

void printPose(geometry_msgs::Pose p);



#endif

#include "Unfold.h"
#include <opencv2/highgui/highgui.hpp>
#include "clopema_motoros/WriteIO.h"
#include <cv.h>
//#include <thread>
#include <Eigen/Core>
#include <Eigen/Eigenvalues> // for cwise access


#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/kdtree/kdtree_flann.h>

#include <boost/signals2.hpp>

using namespace std;
using namespace robot_helpers;
using namespace camera_helpers ;

Unfold::Unfold(const string &armName){

   setHoldingArm(armName);

   camera = "xtion3" ;
   openni::connect( camera ) ;

   ros::Duration(1).sleep() ;
   clothType = -1;
}

Unfold::Unfold(){

   camera = "xtion3" ;
   openni::connect( camera ) ;

   ros::Duration(1).sleep() ;
   clothType = -1;
}

Unfold::~Unfold() {
    openni::disconnect(camera ) ;
}

//Sets the cloth type
void Unfold::setClothType(int type){

    clothType = type;
    return;
}
//Returns the name of the holding arm;
string Unfold::getHoldingArm(){

    return holdingArm;
}
//Returns the name of the moving arm
string Unfold::getMovingArm(){

    return movingArm;
}

//Opens or closes a gripper
int Unfold::setGripperStates(const string &armName  , bool open){
    ros::service::waitForService("/" + armName + "_gripper/set_open");
    clopema_motoros::SetGripperState sopen;
    sopen.request.open=open;
    ros::service::call("/" + armName + "_gripper/set_open", sopen);
}

//Opens or closes the grippers
int Unfold::setGrippersStates( bool open){

    ros::service::waitForService("/r1_gripper/set_open");
    ros::service::waitForService("/r2_gripper/set_open");
    clopema_motoros::SetGripperState sopen;
    sopen.request.open=open;
    ros::service::call("/r1_gripper/set_open", sopen);
    ros::service::call("/r2_gripper/set_open", sopen);


    return 0;
}



////Calculates the quaternion of a 4d rotatation matrix
//geometry_msgs::Quaternion Unfold::rotationMatrix4ToQuaternion(Eigen::Matrix4d matrix){

//    float roll , pitch, yaw;

//    roll= atan2f(matrix(2, 1),matrix(2, 2) );
//    pitch= atan2f(-matrix(2,0),sqrt(pow(matrix(2, 2),2)+pow(matrix(2, 1),2)));
//    yaw= atan2f(matrix(1, 0),matrix(0, 0));
//    return tf::createQuaternionMsgFromRollPitchYaw(roll, pitch, yaw );
//}

////Calculates the quaternion of a 3d rotatation matrix
//geometry_msgs::Quaternion Unfold::rotationMatrix3ToQuaternion(Eigen::Matrix3d matrix){

//    float roll , pitch, yaw;

//    roll= atan2f(matrix(2, 1),matrix(2, 2) );
//    pitch= atan2f(-matrix(2,0),sqrt(pow(matrix(2, 2),2)+pow(matrix(2, 1),2)));
//    yaw= atan2f(matrix(1, 0),matrix(0, 0));
//    return tf::createQuaternionMsgFromRollPitchYaw(roll, pitch, yaw );
//}


//Rotates the holding gripper to a given angle

void Unfold::rotateHoldingGripper(float angle){

    //Create plan
    clopema_arm_navigation::ClopemaMotionPlan mp;
    MoveRobot cmove;
    cmove.setServoMode(false);
    mp.request.motion_plan_req.group_name = holdingArm + "_arm";
    mp.request.motion_plan_req.allowed_planning_time = ros::Duration(5.0);

    //Set start state
    getRobotState(mp.request.motion_plan_req.start_state);

    arm_navigation_msgs::SimplePoseConstraint desired_pose;

    desired_pose.header.frame_id = holdingArm + "_ee";
    desired_pose.header.stamp = ros::Time::now();
    desired_pose.link_name = holdingArm + "_ee";

    desired_pose.pose.position.x = 0;
    desired_pose.pose.position.y = 0;
    desired_pose.pose.position.z = 0;
    desired_pose.pose.orientation = tf::createQuaternionMsgFromRollPitchYaw(0, 0, angle );


   // cout<< "\n going to --- >  "<< desired_pose.pose.position.x<< " "  << desired_pose.pose.position.y<<"  " << desired_pose.pose.position.z <<"\n";

    desired_pose.absolute_position_tolerance.x = 0.002;
    desired_pose.absolute_position_tolerance.y = 0.002;
    desired_pose.absolute_position_tolerance.z = 0.002;
    desired_pose.absolute_roll_tolerance = 0.004;
    desired_pose.absolute_pitch_tolerance = 0.004;
    desired_pose.absolute_yaw_tolerance = 0.004;
    poseToClopemaMotionPlan(mp, desired_pose);

    ROS_INFO("Planning");
    if (!plan(mp))
        return ;

    ROS_INFO("Executing");
    control_msgs::FollowJointTrajectoryGoal goal;
    goal.trajectory = mp.response.joint_trajectory;
    cmove.doGoal(goal);

}

// Switch the name of the holding and moving arm
void Unfold::switchArms(){

    string tmp=holdingArm;

    holdingArm=movingArm;
    movingArm=tmp;

}

// Set the name of the holding arm
void Unfold::setHoldingArm(const string &armName){

    holdingArm = armName;
    if(armName == "r1")
        movingArm = "r2";    
    else
        movingArm = "r1";

}


//finds the orientation (rotation matrix) of the lowest point of the hanging cloth
Eigen::Matrix4d Unfold::findLowestPointOrientation(Eigen::Vector4d vector ){


    Eigen::Matrix4d rotMat;
    Eigen::Vector3d N;


    N << -vector.x(), -vector.y(), -vector.z();
    if(holdingArm == "r2")
        N << vector.x(), vector.y(), vector.z();
    N.normalize();

    Eigen::Vector3d A(sqrt(3.0)/2.f,0,-0.5);
    if(holdingArm == "r2")
         A << -sqrt(3.0)/2.f,0,-0.5;

    Eigen::Vector3d B=A-(A.dot(N))*N;
    B.normalize();
    Eigen::Vector3d C=N.cross(B);

    C.normalize();


    rotMat(0, 0)=N.x();
    rotMat(1, 0)=N.y();
    rotMat(2, 0)=N.z();

    rotMat(0, 1)=C.x();
    rotMat(1, 1)=C.y();
    rotMat(2, 1)=C.z();

    rotMat(0, 2)=-B.x();
    rotMat(1, 2)=-B.y();
    rotMat(2, 2)=-B.z();

    return rotMat;
}

// find the orientation (rotation matrix) of a given point of the hanging cloth
Eigen::Matrix4d Unfold::findGraspingPointOrientation(Eigen::Vector4d vector, bool orientUp ){

    Eigen::Matrix4d rotMat;
    Eigen::Vector3d N;
    cout<< "Fit vector is" << N << endl;


    N << -vector.x(), -vector.y(), -vector.z();
    if(holdingArm == "r2")
        N << vector.x(), vector.y(), vector.z();
    N.normalize();

    Eigen::Vector3d A;
    if(orientUp){
        A << sqrt(3)/2.0f,0,-0.5;
        if(holdingArm == "r2")
            A << -sqrt(3)/2.0f,0,-0.5;
    }
    else{
        A << 1,0,0;
        if(holdingArm == "r2")
             A << -1,0,0;
    }

    Eigen::Vector3d B=A-(A.dot(N))*N;
    B.normalize();
    Eigen::Vector3d C=N.cross(B);

    C.normalize();


    rotMat(0, 0)=N.x();
    rotMat(1, 0)=N.y();
    rotMat(2, 0)=N.z();

    rotMat(0, 1)=C.x();
    rotMat(1, 1)=C.y();
    rotMat(2, 1)=C.z();

    rotMat(0, 2)=-B.x();
    rotMat(1, 2)=-B.y();
    rotMat(2, 2)=-B.z();


    return rotMat;
}

// Finds the orientation bias of a given point from the Y axis
float Unfold::findBias(Eigen::Vector4d vector){

    float theta=atan2f(vector.x(),abs(vector.y()));

    return theta;
}

void mouse_callback( int event, int x, int y, int flags, void* param){
    if(event == CV_EVENT_LBUTTONDOWN){
        cout << x << " " << y << endl;
    }
}


void Unfold::robustPlane3DFit(vector<Eigen::Vector3d> &x, Eigen::Vector3d  &c, Eigen::Vector3d &u)
{
    int i, k, N = x.size() ;
    Eigen::Vector3d u1, u2, u3 ;
    const int NITER = 1 ;

    double *weight = new double [N] ;
    double *res = new double [N] ;

    for( i=0 ; i<N ; i++ ) weight[i] = 1.0 ;

    double wsum = N ;

    for( k=0 ; k<NITER ; k++ )
    {

        c = Eigen::Vector3d::Zero() ;
        Eigen::Matrix3d cov = Eigen::Matrix3d::Zero();

        for( i=0 ; i<N ; i++ )
        {
            const Eigen::Vector3d &P = x[i] ;
            double w = weight[i] ;

            c += w * P ;
        }

        c /= wsum ;

        for( i=0 ; i<N ; i++ )
        {
            const Eigen::Vector3d &P = x[i] ;
            double w = weight[i] ;

            cov += w *  (P - c) * (P - c).adjoint();

        }

        cov *= 1.0/wsum ;

        Eigen::Matrix3d U ;
        Eigen::Vector3d L ;

        Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> eigensolver(cov);
        L = eigensolver.eigenvalues() ;
        U = eigensolver.eigenvectors() ;

        // Recompute weights

        u1 = U.col(0) ;
        u2 = U.col(1) ;
        u3 = U.col(2) ;

        for( i=0 ; i<N ; i++ )
        {
            const Eigen::Vector3d &P = x[i] ;

        //    double r = (P -c).dot(P-c) ;
        //    double ss = (P-c).dot(u1);

         //   r -= ss * ss ;

          //  r = sqrt(r) ;

            double r = fabs((P-c).dot(u1));

            weight[i] = r ;
            res[i] = r ;
        }

        // Estimate residual variance
        double sigma ;

        sort(weight, weight + N) ;

        sigma = weight[N/2]/0.6745 ;
        cout << "FIT QUALITY: " << sigma << endl;
        // Update weights using Hubers scheme

        wsum = 0.0 ;

        for( i=0 ; i<N ; i++ )
        {

            double r = fabs(res[i]) ;

            if ( r <= sigma )  weight[i] = 1.0 ;
            else if ( r > sigma && r <= 3.0*sigma ) weight[i] = sigma/r ;
            else weight[i] = 0.0 ;

            wsum += weight[i] ;
        }
    }

    u = u1 ;

    delete weight ;
    delete res ;
}

//Computes the normal vector of a given point in a point cloud
 Eigen::Vector3d Unfold::computeNormal(const pcl::PointCloud<pcl::PointXYZ> &pc, int x, int y)
{
    const int nrmMaskSize = 6 ;
    int w = pc.width, h = pc.height ;

    vector<Eigen::Vector3d> pts ;

    for(int i = y - nrmMaskSize ; i<= y + nrmMaskSize ; i++  )
        for(int j = x - nrmMaskSize ; j<= x + nrmMaskSize ; j++  )
        {
            if ( i < 0 || j < 0 || i > h-1 || j > w-1 ) continue ;

            pcl::PointXYZ val = pc.at(j, i) ;

            if ( !pcl_isfinite(val.z) ) continue ;

            pts.push_back(Eigen::Vector3d(val.x, val.y, val.z)) ;
        }

    if ( pts.size() < 3 ) return Eigen::Vector3d() ;

    Eigen::Vector3d u, c ;
    robustPlane3DFit(pts, c, u);

    if ( u(2) > 0 ) u = -u ;

    return u ;

}

//Calculates whether or not a point is inside a given cone
 bool Unfold::pointInsideCone(const Eigen::Vector3d &x, const Eigen::Vector3d &apex, const Eigen::Vector3d &base, float aperture)
{
    // This is for our convenience
    float halfAperture = aperture/2.f;

    // Vector pointing to X point from apex
    Eigen::Vector3d apexToXVect = apex - x ;
    apexToXVect.normalize() ;

    // Vector pointing from apex to circle-center point.
    Eigen::Vector3d axisVect = apex - base;
    axisVect.normalize() ;

    // determine angle between apexToXVect and axis.

    double d = apexToXVect.dot(axisVect) ;

    bool isInInfiniteCone = fabs(d) > cos(halfAperture) ;

    if(!isInInfiniteCone) return false;

    // X is contained in cone only if projection of apexToXVect to axis
    // is shorter than axis.
    // We'll use dotProd() to figure projection length.

    bool isUnderRoundCap = apexToXVect.dot(axisVect)  < 1 ;

    return isUnderRoundCap;
}


void Unfold::findMeanShiftPoint(const pcl::PointCloud<pcl::PointXYZ> &depth, int x0, int y0, int &x1, int &y1, double radius, double variance, int maxIter)
{
   pcl::KdTreeFLANN<pcl::PointXYZ> kdtree;

    boost::shared_ptr <pcl::PointCloud<pcl::PointXYZ> > cloud(new pcl::PointCloud<pcl::PointXYZ>(depth)) ;

    kdtree.setInputCloud (cloud);

    int w = depth.width, h = depth.height ;

    pcl::PointXYZ p = depth.at(x0, y0) ;
    Eigen::Vector3d center(p.x, p.y, p.z) ;

    int iter = 0 ;
    double centerDist ;

    do
    {

        Eigen::Vector3d newCenter(0, 0, 0) ;

        vector<int> pointIdxRadiusSearch;
        vector<float> pointRadiusSquaredDistance;

        pcl::PointXYZ center_(center.x(), center.y(), center.z()) ;
        kdtree.radiusSearch (center_, radius, pointIdxRadiusSearch, pointRadiusSquaredDistance) ;

        int n = pointIdxRadiusSearch.size() ;

        double denom = 0.0 ;

        for(int i=0 ; i<n ; i++ )
        {
            pcl::PointXYZ p = depth.at(pointIdxRadiusSearch[i]) ;
            Eigen::Vector3d pc(p.x, p.y, p.z) ;

            double ep = exp(-pointRadiusSquaredDistance[i]/ (2.0 * variance));

            denom += ep ;

            newCenter += ep * pc ;
        }

        newCenter /= denom ;

        centerDist = (newCenter - center).norm() ;

        center = newCenter ;

        ++iter ;

        //cout << centerDist << endl ;

    } while ( centerDist > variance && iter < maxIter ) ;

    vector<int> pointIdxNNSearch;
    vector<float> pointNNSquaredDistance;

    pcl::PointXYZ center_(center.x(), center.y(), center.z()) ;
    if ( kdtree.nearestKSearch(center_, 1, pointIdxNNSearch, pointNNSquaredDistance) > 0 )
    {
        int idx = pointIdxNNSearch[0] ;

        y1 = idx / w ;
        x1 =  idx - y1 * w ;

    }
}

//Finds the lowest point of the hanging clothe , returns the coords and the normal vector (point and orientation)
bool Unfold::findLowestPoint(const pcl::PointCloud<pcl::PointXYZ> &depth, const Eigen::Vector3d &orig, const Eigen::Vector3d &base, float apperture, Eigen::Vector3d &p, Eigen::Vector3d &n)
{

    int w = depth.width, h = depth.height ;

    int max_y = -1 ;
    int best_j = -1, best_i = -1 ;

    bool found = false ;

    double maxv = -DBL_MAX ;

    for(int j=0 ; j<w ; j++ )
    {

        for(int i=0 ; i<h ; i++)
        {
            pcl::PointXYZ val = depth.at(j, i) ;

            if ( !pcl_isfinite(val.z) ) continue ;

            Eigen::Vector3d p(val.x, val.y, val.z) ;

            // test whether the point lies within the cone

            if ( !pointInsideCone(p, orig, base, apperture) ) continue ;

            // project the point on the cone axis to determine how low it is.

            double s = (p - orig).dot(base - orig) ;

            if ( s > maxv )
            {
                maxv = s ;
                best_j = j ;
                best_i = i ;
                found = true ;
            }

        }
    }

    if ( !found ) return false ;

    pcl::PointXYZ p_ = depth.at(best_j, best_i) ;

    int ox, oy ;

    // find densest point cluster in a 3cm sphere around the detected point
    findMeanShiftPoint(depth, best_j, best_i, ox, oy, 0.03) ;

    // compute normal vector around this point
    n = computeNormal(depth, ox, oy) ;
    p_ = depth.at(ox, oy) ;
    p = Eigen::Vector3d(p_.x, p_.y, p_.z) ;
}




//bool Unfold::findLowestPoint(const pcl::PointCloud<pcl::PointXYZ> &depth, const Eigen::Vector3d &orig, const Eigen::Vector3d &base, float apperture, Eigen::Vector3d &p, Eigen::Vector3d &n, cv::Mat depthMap){

//    int w = depth.width, h = depth.height ;

//    int best_i=-1, best_j=-1;
//    float best_x=-1, best_y=-1, best_z=-1;


//    bool found = false ;
//    float minx = 10 ;
//    for(int j=66 ; j<626 ; j++ )
//    {


//        for(int i=100 ; i<350 ; i++)
//        {
//            pcl::PointXYZ val = depth.at(j, i) ;

//            if ( val.z<1 || val.z>1.5 ) continue ;

//            if ( minx>val.x )
//            {
//                minx = val.x ;
//                best_j = j ;
//                best_i = i ;

//                found = true ;
//            }

//        }
//    }



//    for(int i=0; i<depthMap.rows; ++i)
//        for(int j=0; j<depthMap.cols; ++j)
//            if( (depthMap.at<unsigned short>(i, j) < 900) || (depthMap.at<unsigned short>(i, j) > 1400) )
//                depthMap.at<unsigned short>(i, j) = 0;

//    cv::TermCriteria criteria(1, 10, 0.1);
//    cv::Rect rect1(best_j-10, best_i-10, 20, 20);
//    cv::meanShift(depthMap, rect1, criteria);
//    best_j = rect1.x + rect1.width/2;
//    best_i = rect1.y + rect1.height/2;
////    cout<< "best2= (" << best_j <<" , "<< best_i <<")"<<endl;

//    pcl::PointXYZ p_ = depth.at(best_j, best_i ) ;
//    n = computeNormal(depth, best_j, best_i ) ;
//    p = Eigen::Vector3d(p_.x, p_.y, p_.z) ;

//    if ( !found ) return false ;

//}


//Prints the coords of a given pose
void printPose(geometry_msgs::Pose p){

    cout << "pose = (" << p.position.x << ", "<< p.position.y << ", "<< p.position.z << " )" <<endl;

}


//int Unfold::graspLowestPoint(bool lastMove){


//    moveArms(movingArmPose(), holdingArmPose(), movingArm, holdingArm );

//    setGripperStates(movingArm , true);

//    //starting position

//    bool grasp = false;
//    bool firstTime = true;
//    geometry_msgs::Pose desPose;
//    vector <geometry_msgs::Pose> poses;
//    Eigen::Vector4d targetP;
//    Eigen::Matrix4d rotMat;

//    while(!grasp){

//        cv::Mat rgb, depth;
//        pcl::PointCloud<pcl::PointXYZ> pc;
//        cv::Mat R = cv::Mat(4, 4, CV_32FC1, cv::Scalar::all(0));
//        ros::Time ts(0);
//        image_geometry::PinholeCameraModel cm;

//        //grabbing image and making the fit of lowest point
//        Eigen::Matrix4d calib = getTranformationMatrix("xtion3_rgb_optical_frame");

//        tf::StampedTransform st;
//        Eigen::Vector3d top , bottom, p, n;
//        float angle=0.5;

//        st= getTranformation(holdingArm + "_ee", "xtion3_rgb_optical_frame");

//        top.x()=st.getOrigin().x();
//        top.y()=st.getOrigin().y();
//        top.z()=st.getOrigin().z();

//        bottom = top;
//        bottom.x()-=1;

//        grabber->grab(rgb, depth, pc, ts, cm);
//        if(findLowestPoint(pc, top, bottom, angle, p, n)== false)
////            cout<< "Cant find lowest point"<< endl;

//        publishLowestPointMarker(marker_pub,p ,n );

//        Eigen::Vector4d tar(p.x(), p.y(), p.z(), 1);
//        targetP = calib * tar;

//        Eigen::Vector4d norm (n.x(), n.y(), n.z(), 0);
//        Eigen::Vector4d targetN;
//        targetN = calib * norm.normalized();
//        targetN.normalized();

//        rotMat = findLowestPointOrientation(targetN);


//        desPose.orientation = rotationMatrix4ToQuaternion(rotMat);
//        desPose.position.x = targetP.x() + rotMat(0, 0) * 0.02 - rotMat(0, 2) * 0.04;
//        desPose.position.y = targetP.y() + rotMat(1, 0) * 0.02 - rotMat(1, 2) * 0.04;
//        desPose.position.z = targetP.z() + rotMat(2, 0) * 0.02 - rotMat(2, 2) * 0.04;

//        st= getTranformation(holdingArm + "_ee");

//       // grasp lowest point
//        addConeToCollisionModel(holdingArm, st.getOrigin().z() - desPose.position.z-0.1 , 0.2 );

//        if (firstTime){
//        rotateGripper(-findBias(targetN), holdingArm);
//        firstTime = false;
//        continue;
//        }

//        if(moveArm(desPose, movingArm)==-1){
//            cout<< "Cant reach lowest Point" << endl;
//            resetCollisionModel();
//            cout<< "BIAS : " <<findBias(targetN)<<endl;
//            rotateGripper(-findBias(targetN), holdingArm);
//            continue;
//        }
//        resetCollisionModel();
//        break;

//    }

//    desPose.position.x = targetP.x() + rotMat(0, 2) * 0.03 + rotMat(0, 0) * 0.02;
//    desPose.position.y = targetP.y() + rotMat(1, 2) * 0.03 + rotMat(1, 0) * 0.02;
//    desPose.position.z = targetP.z() + rotMat(2, 2) * 0.03 + rotMat(2, 0) * 0.02;

//    for (unsigned int i =0 ; i < 4 ;  i++){
//        if(moveArm(desPose, movingArm)==-1){
//            cout<< "Cant grasp lowest Point .. " << i << "TIMES" <<endl;
//            if (i == 3){
//                cout<< "ABORDING ..." << endl;
//                return -1;
//            }
//        }else
//            break;
//    }

//    setGripperStates( movingArm, false);

//    //MOVING SIDEWAYS

//    geometry_msgs::Pose desPos1, desPos2;
//    float radious=getArmsDistance();

//    desPos1 = getArmPose("r1");
//    desPos2 = getArmPose("r2");

//    if(holdingArm == "r1"){
//        desPos2.position.x-=radious/2.0;
//        desPos1.position.x-=radious/2.0;
//    }
//    else{
//        desPos1.position.x+=radious/2.0;
//        desPos2.position.x+=radious/2.0;
//    }

//    ros::Duration(0.3).sleep();

//    for (unsigned int i =0 ; i < 4 ;  i++){
//        if (moveArmsNoTearing(desPos1, desPos2) == -1 ){
//            cout << "cant move sideways ..." << endl;
//            if (i == 3){
//                cout<< "ABORDING ..." << endl;
//                return -1;
//            }
//        }
//        else
//            break;

//    }
//     //MOVING UP

//    desPose.orientation = rotationMatrix3ToQuaternion(diagonalDown());
//    ros::Duration(0.3).sleep();
//    for (unsigned int i =0 ; i < 4 ;  i++){
//        if (moveArmBetweenSpheres(movingArm, true,  desPose) == -1) {
//            cout << "cant move up ..." << endl;
//            if (i == 3){
//                cout<< "ABORDING ..." << endl;
//                return -1;
//            }
//        }
//        else
//            break;
//    }

//    if ( lastMove == true )
//        return -1;


//    desPos1 = getArmPose(holdingArm);
//    desPos2 = getArmPose(movingArm);

//    //MOVING DOWN



//    if(holdingArm == "r1"){
//        desPos2.position.x-=radious/2.0f;

//        desPos2.orientation = rotationMatrix3ToQuaternion(diagonalDown());
//    }
//    else{
//        desPos2.position.x+=radious/2.0f;

//        desPos2.orientation = rotationMatrix3ToQuaternion(diagonalDown());
//    }
//        desPos1.position.z-=0.7*radious;

//    ros::Duration(0.3).sleep();

//    for (unsigned int i =0 ; i < 4 ;  i++){
//        if (moveArmsNoTearing(desPos2,desPos1, movingArm, holdingArm) == -1) {
//            cout << "cant move down ..." << endl;
//            if (i == 3){
//                cout<< "ABORDING ..." << endl;
//                setGripperStates(holdingArm, true);
//            }
//        }
//        else
//            break;
//    }



//    setGripperStates(holdingArm, true);

//    switchArms();

//    moveArms(movingArmPose(), holdingArmPose(), movingArm, holdingArm );

//return 0;

//}

//Finds and grasps the lowest point of a hanging cloth, flips the cloth and releases the moving arm


bool Unfold::parkArmsForGrasping(){

    moveArms(movingArmPose(), holdingArmPose(), movingArm, holdingArm );
    return true;
}

bool Unfold::graspLowestPoint(bool lastMove, bool allwaysDrop){

    cout<< movingArmPose()<< endl;
    cout<< holdingArmPose() << endl;
    cout << movingArm <<" "<< holdingArm<< endl;
    moveArms(movingArmPose(), holdingArmPose(), movingArm, holdingArm );

    setGripperStates(movingArm , true);

    //starting position

    bool grasp = false;
    bool firstTime = true;
    geometry_msgs::Pose desPose;
    vector <geometry_msgs::Pose> poses;
    Eigen::Vector4d targetP;
    Eigen::Matrix4d rotMat;
    int counter = 0;

    while(!grasp){

        counter++;
        cv::Mat rgb, depth;
        pcl::PointCloud<pcl::PointXYZ> pc;
        cv::Mat R = cv::Mat(4, 4, CV_32FC1, cv::Scalar::all(0));
        ros::Time ts(0);
        image_geometry::PinholeCameraModel cm;

        //grabbing image and making the fit of lowest point
        Eigen::Matrix4d calib = getTranformationMatrix("xtion3_rgb_optical_frame");

        tf::StampedTransform st;
        Eigen::Vector3d top , bottom, p, n;
        float angle=0.5;

        st= getTranformation(holdingArm + "_ee", "xtion3_rgb_optical_frame");

        top.x()=st.getOrigin().x();
        top.y()=st.getOrigin().y();
        top.z()=st.getOrigin().z();

        bottom = top;
        bottom.x()-=1;

        openni::connect( camera ) ;
        while( !openni::grab(camera, rgb, depth, pc, ts, cm) ) ;
        findLowestPoint(pc, top, bottom, angle, p, n);

     //   publishLowestPointMarker(marker_pub, p ,n );

        Eigen::Vector4d tar(p.x(), p.y(), p.z(), 1);       
        targetP = calib * tar ;

        Eigen::Vector4d norm (n.x(), n.y(), n.z(), 0);
        Eigen::Vector4d targetN;
        targetN = calib * norm.normalized();
        targetN.normalized();

        rotMat = findLowestPointOrientation(targetN);


        desPose.orientation = rotationMatrix4ToQuaternion(rotMat);
        desPose.position.x = targetP.x() + rotMat(0, 0) * 0.02 - rotMat(0, 2) * 0.04;
        desPose.position.y = targetP.y() + rotMat(1, 0) * 0.02 - rotMat(1, 2) * 0.04;
        desPose.position.z = targetP.z() + rotMat(2, 0) * 0.02 - rotMat(2, 2) * 0.04;


        cout << endl << "Lowest Point: " << targetP << endl;
        if(targetP.z() < 0.56 || counter > 4){
            while(!graspMiddle())
                rotateHoldingGripper(30.0f/180.0f * M_PI);
            counter = 0 ;
            continue;
        }
        st= getTranformation(holdingArm + "_ee");

       // grasp lowest point
        addConeToCollisionModel(holdingArm, st.getOrigin().z() - desPose.position.z-0.2 , 0.2 );

        if (firstTime){
        rotateGripper(-findBias(targetN), holdingArm);
        firstTime = false;
        continue;
        }

        if(moveArm(desPose, movingArm)==-1){
            cout<< "Cant reach lowest Point" << endl;
            resetCollisionModel();
            cout<< "BIAS : " <<findBias(targetN)<<endl;
            rotateGripper(-findBias(targetN), holdingArm);
            continue;
        }
        resetCollisionModel();
        break;

    }

    desPose.position.x = targetP.x() + rotMat(0, 2) * 0.03 + rotMat(0, 0) * 0.02;
    desPose.position.y = targetP.y() + rotMat(1, 2) * 0.03 + rotMat(1, 0) * 0.02;
    desPose.position.z = targetP.z() + rotMat(2, 2) * 0.03 + rotMat(2, 0) * 0.02;

    for (unsigned int i =0 ; i < 4 ;  i++){
        if(moveArm(desPose, movingArm)==-1){
            cout<< "Cant grasp lowest Point .. " << i << "TIMES" <<endl;
            if (i == 3){
                cout<< "ABORDING: Inverse Kinematics not found!" << endl;
                return false;
            }
        }else
            break;
    }

    setGripperStates( movingArm, false);

    if(lastMove == true){
        moveToCheckGrasping() ;
        if(!confirmGrasping() ){
            moveArm(movingArmPose(), movingArm) ;
            setGripperStates(movingArm, true);
            return false ;
        }
        showUnfolding();
        return true ;
    }

    if( !flipCloth( allwaysDrop) ){
        cout << "CANT FLIP CLOTH"<< endl;
        moveArm(movingArmPose(), movingArm) ;
        setGripperStates(movingArm, true);
        return false;
    }

    moveArms(movingArmPose(), holdingArmPose(), movingArm, holdingArm );

    return true;

}

//Finds and grasps the given point of a hanging cloth, flips it and releases the moving arm
bool Unfold::graspPoint(const  pcl::PointCloud<pcl::PointXYZ> &pc,  int x, int y , bool lastMove, bool orientLeft, bool orientUp , bool alwaysDrop ){

    float deep = 0.03 ;
    float far = 0.1 ;

    Eigen::Vector3d n;
    pcl::PointXYZ val = pc.at(x, y) ;
    Eigen::Vector3d p(val.x, val.y, val.z);
    geometry_msgs::Pose desPose;
    vector <geometry_msgs::Pose> poses;
    Eigen::Matrix4d rotMat;
    Eigen::Vector4d targetP;
    tf::Transform ts;
   // setGripperStates(movingArm , true);
    int ox , oy;
   // findMeanShiftPoint(pc, x, y, ox, oy, 0.05) ;
    ox = x; oy = y;
    n = computeNormal(pc, ox, oy) ;
    x= ox;
    y= oy;
    Eigen::Matrix4d calib = getTranformationMatrix("xtion3_rgb_optical_frame");

    //publishLowestPointMarker(marker_pub,p ,n );

    Eigen::Vector4d tar(p.x(), p.y(), p.z(), 1);
    targetP = calib * tar;
   // publishPointMarker(marker_pub, targetP, 1);
    Eigen::Vector4d norm (n.x(), n.y(), n.z(), 0);
    Eigen::Vector4d targetN;
    targetN = calib * norm.normalized();
    targetN.normalized();

    rotMat = findGraspingPointOrientation(targetN, orientUp);
    ts = getTranformation(holdingArm + "_ee" );

    if ((orientLeft && (holdingArm == "r2")) || (!orientLeft && (holdingArm == "r1"))){
        Eigen::Vector2d vect;
        vect << targetP.x() - ts.getOrigin().x(),  targetP.y() - ts.getOrigin().y();
        double theta = acos(-targetN.y() / sqrt((targetN.x()*targetN.x() + targetN.y()*targetN.y())));
        if(targetN.x() < 0)
            theta = -theta;
        Eigen::Matrix2d rot;
        rot << cos(theta) , sin(theta), -sin(theta), cos(theta);
        vect = rot * vect;
        targetP << ts.getOrigin().x() + vect.x(), ts.getOrigin().y() + vect.y(), targetP.z(), 1;        

        Eigen::Matrix3d orient;
        if(orientUp)
            orient = diagonalUp();
        else
            orient = horizontal();

        desPose.orientation = rotationMatrix3ToQuaternion(orient);

        desPose.position.x = targetP.x() + orient(0, 0) * 0.02 - orient(0, 2) * far;
        desPose.position.y = targetP.y() + orient(1, 0) * 0.02 - orient(1, 2) * far;
        desPose.position.z = targetP.z() + orient(2, 0) * 0.02 - orient(2, 2) * far;
        poses.push_back(desPose);

        desPose.position.x = targetP.x() + orient(0, 2) * deep + orient(0, 0) * 0.02;
        desPose.position.y = targetP.y() + orient(1, 2) * deep + orient(1, 0) * 0.02;
        desPose.position.z = targetP.z() + orient(2, 2) * deep + orient(2, 0) * 0.02;
        poses.push_back(desPose);
        rotateHoldingGripper(theta);
        ros::Duration(2).sleep();

    }else{

        desPose.orientation = rotationMatrix4ToQuaternion(rotMat);

        desPose.position.x = targetP.x() + rotMat(0, 0) * 0.02 - rotMat(0, 2) * far;
        desPose.position.y = targetP.y() + rotMat(1, 0) * 0.02 - rotMat(1, 2) * far;
        desPose.position.z = targetP.z() + rotMat(2, 0) * 0.02 - rotMat(2, 2) * far;
        poses.push_back(desPose);

        desPose.position.x = targetP.x() + rotMat(0, 2) * deep + rotMat(0, 0) * 0.02;
        desPose.position.y = targetP.y() + rotMat(1, 2) * deep + rotMat(1, 0) * 0.02;
        desPose.position.z = targetP.z() + rotMat(2, 2) * deep + rotMat(2, 0) * 0.02;
        poses.push_back(desPose);
    }

    if(moveArmThrough(poses, movingArm) == -1){

        poses.clear();

        double theta = acos(targetN.y() / sqrt((targetN.x()*targetN.x() + targetN.y()*targetN.y())));
        if(targetN.x() > 0)
            theta = -theta;

        rotateHoldingGripper(theta);
        ros::Duration(2).sleep();
        Eigen::Vector2d vect;
        vect << targetP.x() - ts.getOrigin().x(),  targetP.y() - ts.getOrigin().y();
        Eigen::Matrix2d rot;
        rot << cos(theta) , sin(theta), -sin(theta), cos(theta);
        vect = rot * vect;
        targetP << ts.getOrigin().x() + vect.x(), ts.getOrigin().y() + vect.y(), targetP.z(), 1;
       // publishPointMarker(marker_pub, targetP, 2);

        Eigen::Matrix3d orient;
        if(orientUp)
            orient = diagonalUp();
        else
            orient = horizontal();

        desPose.orientation = rotationMatrix3ToQuaternion(orient);

        desPose.position.x = targetP.x() + orient(0, 0) * 0.02 - orient(0, 2) * far;
        desPose.position.y = targetP.y() + orient(1, 0) * 0.02 - orient(1, 2) * far;
        desPose.position.z = targetP.z() + orient(2, 0) * 0.02 - orient(2, 2) * far;
        poses.push_back(desPose);

        desPose.position.x = targetP.x() + orient(0, 2) * deep + orient(0, 0) * 0.02;
        desPose.position.y = targetP.y() + orient(1, 2) * deep + orient(1, 0) * 0.02;
        desPose.position.z = targetP.z() + orient(2, 2) * deep + orient(2, 0) * 0.02;
        poses.push_back(desPose);

         // GRASPING POINT
        for (unsigned int i =0 ; i < 4 ;  i++){
            if(moveArmThrough(poses, movingArm) == -1){
                cout<< "Cant grasp Point .. " << i << "TIMES" <<endl;
                if (i == 3){
                    cout<< "ABORDING: Motion Planning Failed!" << endl;
                    moveArm(movingArmPose(), movingArm) ;
                    setGripperStates(movingArm, true);
                    return false;
                }
            }else
                break;
        }

    }

    setGripperStates(movingArm , false);

    if(lastMove == true){
        moveToCheckGrasping() ;
        if(!confirmGrasping() ){
            setGripperStates(movingArm, true);
            moveArm(movingArmPose(), movingArm) ;           
            return false ;
        }
        showUnfolding();
        return true ;
    }

    if( !flipCloth( alwaysDrop) ){
        cout << "CANT FLIP CLOTH"<< endl;
        moveArm(movingArmPose(), movingArm) ;
        setGripperStates(movingArm, true);
        return false;
    }

    setGripperStates(movingArm, true);
    moveArms(movingArmPose(), holdingArmPose(), movingArm, holdingArm );
    return true;
}

//int Unfold::graspPoint(const  pcl::PointCloud<pcl::PointXYZ> &pc,  int x, int y , bool lastMove, bool orientLeft, bool orientUp  ){


//    Eigen::Vector3d n;
//    pcl::PointXYZ val = pc.at(x, y) ;
//    Eigen::Vector3d p(val.x, val.y, val.z);
//    geometry_msgs::Pose desPose;
//    vector <geometry_msgs::Pose> poses;
//    Eigen::Matrix4d rotMat;
//    Eigen::Vector4d targetP;
//    tf::Transform ts;
//    setGripperStates(movingArm , true);
//    int ox , oy;
//   findMeanShiftPoint(pc, x, y, ox, oy, 0.05) ;
//    n = computeNormal(pc, ox, oy) ;
//    x= ox;
//    y= oy;
//    Eigen::Matrix4d calib = getTranformationMatrix("xtion3_rgb_optical_frame");

//    publishLowestPointMarker(marker_pub,p ,n );

//    Eigen::Vector4d tar(p.x(), p.y(), p.z(), 1);
//    targetP = calib * tar;
//    publishPointMarker(marker_pub, targetP, 1);
//    Eigen::Vector4d norm (n.x(), n.y(), n.z(), 0);
//    Eigen::Vector4d targetN;
//    targetN = calib * norm.normalized();
//    targetN.normalized();

//    rotMat = findGraspingPointOrientation(targetN, orientUp);
//    ts = getTranformation(holdingArm + "_ee" );

//    if ((orientLeft && (holdingArm == "r2")) || (!orientLeft && (holdingArm == "r1"))){
//        Eigen::Vector2d vect;
//        vect << targetP.x() - ts.getOrigin().x(),  targetP.y() - ts.getOrigin().y();
//        double theta = acos(-targetN.y() / sqrt((targetN.x()*targetN.x() + targetN.y()*targetN.y())));
//        if(targetN.x() < 0)
//            theta = -theta;
//        Eigen::Matrix2d rot;
//        rot << cos(theta) , sin(theta), -sin(theta), cos(theta);
//        vect = rot * vect;
//        targetP << ts.getOrigin().x() + vect.x(), ts.getOrigin().y() + vect.y(), targetP.z(), 1;

//        Eigen::Matrix3d orient;
//        if(orientUp)
//            orient = diagonalUp();
//        else
//            orient = horizontal();

//        desPose.orientation = rotationMatrix3ToQuaternion(orient);

//        desPose.position.x = targetP.x() + orient(0, 0) * 0.02 - orient(0, 2) * 0.04;
//        desPose.position.y = targetP.y() + orient(1, 0) * 0.02 - orient(1, 2) * 0.04;
//        desPose.position.z = targetP.z() + orient(2, 0) * 0.02 - orient(2, 2) * 0.04;
//        poses.push_back(desPose);

//        desPose.position.x = targetP.x() + orient(0, 2) * 0.03 + orient(0, 0) * 0.02;
//        desPose.position.y = targetP.y() + orient(1, 2) * 0.03 + orient(1, 0) * 0.02;
//        desPose.position.z = targetP.z() + orient(2, 2) * 0.03 + orient(2, 0) * 0.02;
//        poses.push_back(desPose);
//        rotateHoldingGripper(theta);
//        ros::Duration(2).sleep();

//    }else{

//        desPose.orientation = rotationMatrix4ToQuaternion(rotMat);

//        desPose.position.x = targetP.x() + rotMat(0, 0) * 0.02 - rotMat(0, 2) * 0.04;
//        desPose.position.y = targetP.y() + rotMat(1, 0) * 0.02 - rotMat(1, 2) * 0.04;
//        desPose.position.z = targetP.z() + rotMat(2, 0) * 0.02 - rotMat(2, 2) * 0.04;
//        poses.push_back(desPose);

//        desPose.position.x = targetP.x() + rotMat(0, 2) * 0.03 + rotMat(0, 0) * 0.02;
//        desPose.position.y = targetP.y() + rotMat(1, 2) * 0.03 + rotMat(1, 0) * 0.02;
//        desPose.position.z = targetP.z() + rotMat(2, 2) * 0.03 + rotMat(2, 0) * 0.02;
//        poses.push_back(desPose);
//    }

//    if(moveArmThrough(poses, movingArm) == -1){

//        poses.clear();
//       // cout << "fixing rotation...." << endl;
//        //float theta = findBias(targetN);
//        double theta = acos(targetN.y() / sqrt((targetN.x()*targetN.x() + targetN.y()*targetN.y())));
//        if(targetN.x() > 0)
//            theta = -theta;
//     //   cout<< " theta = " << theta/M_PI *180<<endl;

//        rotateHoldingGripper(theta);
//        ros::Duration(2).sleep();
//        Eigen::Vector2d vect;
//        vect << targetP.x() - ts.getOrigin().x(),  targetP.y() - ts.getOrigin().y();
//        Eigen::Matrix2d rot;
//        rot << cos(theta) , sin(theta), -sin(theta), cos(theta);
//        vect = rot * vect;
//        targetP << ts.getOrigin().x() + vect.x(), ts.getOrigin().y() + vect.y(), targetP.z(), 1;
//        publishPointMarker(marker_pub, targetP, 2);
//       // cout<< targetP << endl;

//        Eigen::Matrix3d orient;
//        if(orientUp)
//            orient = diagonalUp();
//        else
//            orient = horizontal();

//        desPose.orientation = rotationMatrix3ToQuaternion(orient);

//        desPose.position.x = targetP.x() + orient(0, 0) * 0.02 - orient(0, 2) * 0.04;
//        desPose.position.y = targetP.y() + orient(1, 0) * 0.02 - orient(1, 2) * 0.04;
//        desPose.position.z = targetP.z() + orient(2, 0) * 0.02 - orient(2, 2) * 0.04;
//        poses.push_back(desPose);

//        desPose.position.x = targetP.x() + orient(0, 2) * 0.03 + orient(0, 0) * 0.02;
//        desPose.position.y = targetP.y() + orient(1, 2) * 0.03 + orient(1, 0) * 0.02;
//        desPose.position.z = targetP.z() + orient(2, 2) * 0.03 + orient(2, 0) * 0.02;
//        poses.push_back(desPose);

//         // GRASPING POINT
//        for (unsigned int i =0 ; i < 4 ;  i++){
//            if(moveArmThrough(poses, movingArm) == -1){
//                cout<< "Cant grasp Point .. " << i << "TIMES" <<endl;
//                if (i == 3){
//                    cout<< "ABORDING ..." << endl;
//                    return -1;
//                }
//            }else
//                break;
//        }

//    }

//    setGripperStates(movingArm , false);

//    geometry_msgs::Pose desPos1, desPos2;

//   float radious=getArmsDistance();

//    desPos1 = getArmPose("r1");
//    desPos2 = getArmPose("r2");

//    if(holdingArm == "r1"){
//        desPos2.position.x-=radious/2.0;
//        desPos1.position.x-=radious/2.0;
//    }
//    else{
//        desPos1.position.x+=radious/2.0;
//        desPos2.position.x+=radious/2.0;
//    }

//    ros::Duration(0.3).sleep();

// // MOVING SIDEWAYS

//    for (unsigned int i =0 ; i < 4 ;  i++){
//        if (moveArmsNoTearing(desPos1, desPos2) == -1 ){
//            cout << "cant move sideways ..." << endl;
//            if (i == 3){
//                cout<< "ABORDING ..." << endl;
//                moveArms(desPos1, desPos2 );
//            }
//        }
//        else
//            break;

//    }

//    // MOVING UP

//    desPose.orientation = rotationMatrix3ToQuaternion(diagonalDown());
//    for (unsigned int i =0 ; i < 4 ;  i++){
//        if(moveArmBetweenSpheres(movingArm, true ,desPose) == -1){
//            cout<< "Cant move up .. " << i << "TIMES" <<endl;
//            if (i == 3){
//                cout<< "ABORDING ..." << endl;
//                return -1;
//            }
//        }else
//            break;
//    }
//    if ( lastMove == true )
//        return -1;

//    radious=getArmsDistance();

//    desPos1 = getArmPose("r1");
//    desPos2 = getArmPose("r2");

//    if(holdingArm == "r1"){
//        desPos2.position.x-=radious/2.0f;

//        desPos2.orientation = rotationMatrix3ToQuaternion(diagonalDown());
//    }
//    else{
//        desPos2.position.x+=radious/2.0f;

//        desPos2.orientation = rotationMatrix3ToQuaternion(diagonalDown());
//    }
//        desPos1.position.z-=0.7*radious;

//    ros::Duration(0.3).sleep();
//    // MOVING DOWN

//    for (unsigned int i =0 ; i < 4 ;  i++){
//        if (moveArmsNoTearing(desPos2,desPos1, movingArm, holdingArm) == -1) {
//            cout << "cant move down ..." << endl;
//            if (i == 3){
//                cout<< "ABORDING ..." << endl;
//                 setGripperStates(holdingArm, true);
//            }
//        }
//        else
//            break;
//    }

////    desPose.orientation = rotationMatrix3ToQuaternion(horizontal());



////    desPos1 = getArmPose("r1");
////    desPos2 = getArmPose("r2");


////    cout<< "holding arm is = " << holdingArm << endl;

////    if(holdingArm == "r1"){
////        desPos2.position.x-=radious/3.0f;
////        desPos1.position.z-=2.23606 *radious/3.f;
////    }
////    else{
////        desPos1.position.x+=radious/3.0f;
////        desPos2.position.z-=2.23606 *radious/3.f;;
////    }

////    desPos1.orientation = rotationMatrix3ToQuaternion(horizontal());


////    for (unsigned int i =0 ; i < 4 ;  i++){
////        if (moveArmsNoTearing(desPos1, desPos2, movingArm, holdingArm) == -1) {
////            cout << "cant move up ..." << endl;
////            if (i == 3){
////                cout<< "ABORDING ..." << endl;
////                return -1;
////            }
////        }
////        else
////            break;
////    }

//    setGripperStates(holdingArm, true);

//    switchArms();

//    moveArms(movingArmPose(), holdingArmPose(), movingArm, holdingArm );


//}


bool Unfold::confirmGrasping(){


    cv::Mat rgb, depth ;
    pcl::PointCloud <pcl::PointXYZ> pc ;
    openni::connect( camera ) ;
    ros::Duration(0.3).sleep();
    grabFromXtion(rgb, depth, pc ) ;
    string rgbFileName = "/tmp/rgb_cap_.png" ;
    string depthFileName = "/tmp/depth_cap_.png" ;
    cv::imwrite(rgbFileName, rgb) ;

    geometry_msgs::Point point ;
    point = getArmPose(movingArm, "xtion3_rgb_optical_frame").position ;

    int x, y ; //640 ,480

    x = (525.0 * (float) point.x)/(float) point.z + 320.0 ;
    y = (525.0 * (float) point.y)/(float) point.z + 240.0 ;

    if (movingArm == "r1")
        y -= 10 ;
    else
        y += 10 ;

    for (int i = -4; i<5 ; i++){
        for ( int j = -20 ; j<5 ; j++){

            rgb.at<cv::Vec3b>(y+i,x+j)[0] = 1;
            rgb.at<cv::Vec3b>(y+i,x+j)[1] = 1;
            rgb.at<cv::Vec3b>(y+i,x+j)[2] = 200;

        }
    }
    rgbFileName = "/tmp/rgb_cap_confirm.png" ;
    depthFileName = "/tmp/depth_cap_confirm.png" ;

    cv::imwrite(rgbFileName, rgb) ;
    cv::imwrite(depthFileName, depth) ;

    float sum =0;
    pcl::PointXYZ val ;
    unsigned int count = 0 ;


    for (int i = -4 ; i<5 ; i++){
        for ( int j = -10 ; j<10 ; j++){

            val = pc.at(x+j,y+i) ;

            if((val.z > 0.7) && (val.z < 2.0) ){

                sum +=(float) val.z ;
                count++ ;

            }
        }
    }

    cout << "COUNT = " << count<< endl;

    if (count < 5) return false;

    float mean = sum/(float) count ;

    cout << "MEAN = " << mean << "GRIPPER POINT = " << point.z << endl;

    if ( abs( mean - point.z ) > 0.15 )
        return false ;

    return true ;

}

//Grabs the rgb , depth map and point cloud from xtion

bool Unfold::grabFromXtion(cv::Mat &rgb, cv::Mat &depth, pcl::PointCloud<pcl::PointXYZ> &pc ){

    openni::connect( camera ) ;

    ros::Duration(0.3).sleep();
    ros::Time ts(0);
    image_geometry::PinholeCameraModel cm;

    if(!openni::grab( camera, rgb, depth, pc, ts, cm)){
        cout<<"cant grab!  " << endl;
        return false;
    }

    return true;
}

//Grabs the rgb , depth map and point cloud from xtion, returning a rectangular depending on the holding arm

bool Unfold::grabFromXtion(cv::Mat &rgb, cv::Mat &depth, pcl::PointCloud<pcl::PointXYZ> &pc, cv::Rect & r ){

    openni::connect( camera ) ;

    ros::Duration(0.3).sleep();
    ros::Time ts(0);
    image_geometry::PinholeCameraModel cm;


    if(!openni::grab(camera, rgb, depth, pc, ts, cm)){
        cout<<"cant grab!  " << endl;
        return false;
    }
    pcl::PointCloud<pcl::PointXYZ> pc_avg;
    pcl::copyPointCloud<pcl::PointXYZ>(pc, pc_avg);
    for(int x = 0; x<640; ++x){
        for(int y = 0; y<480; ++y){
            pc_avg.at(x, y).x /= 10;
            pc_avg.at(x, y).y /= 10;
            pc_avg.at(x, y).z /= 10;
        }
    }

    for(int i=0; i<9; ++i){
        if(!openni::grab(camera, rgb, depth, pc, ts, cm)){
            cout<<"cant grab!  " << endl;
            return false;
        }
        for(int x = 0; x<640; ++x){
            for(int y = 0; y<480; ++y){
                pc_avg.at(x, y).x += pc.at(x,y).x/10;
                pc_avg.at(x, y).y += pc.at(x,y).y/10;
                pc_avg.at(x, y).z += pc.at(x,y).z/10;
            }
        }
    }

    pcl::copyPointCloud<pcl::PointXYZ>(pc_avg, pc);


    if(holdingArm == "r1")
        r = cv::Rect (10, 145 , 530, 250);
    else
        r = cv::Rect (10, 195, 530, 250);
    return true;
}


bool Unfold::moveToCheckGrasping(){

    float radious = getArmsDistance() ;
    float offset ;

    if (radious > 0.3)
        offset = 0.3 ;
    else
        offset = radious ;

    geometry_msgs::Pose desPoseUp , desPoseDown ;
    desPoseUp = getArmPose(holdingArm) ;
    desPoseDown.position = desPoseUp.position ;
    desPoseDown.position.z -= radious - offset;

    if(movingArm == "r1")
        desPoseDown.position.x -=  offset ;
    else
        desPoseDown.position.x += offset ;

    desPoseDown.orientation = rotationMatrix3ToQuaternion(horizontal());

    if ( moveArmConstrains(desPoseDown, movingArm, radious+0.02 ) == 0 )
        return true;
    return false ;

}



//Flips the cloth
bool Unfold::flipCloth( bool allwaysDrop){

    geometry_msgs::Pose desPoseDown, desPoseUp;
    float radious = getArmsDistance();
    Eigen::Matrix3d orient;
    desPoseDown = getArmPose(movingArm);
    desPoseUp = getArmPose(holdingArm);

    moveToCheckGrasping() ;
    bool grasp = confirmGrasping() ;
    cout << "grasping = " << grasp  << endl ;
    if(!grasp){
        setGripperStates(movingArm, true) ;
        return false;
    }
////////////////////NEW///////////////////

    if( (clothType == 0) || (clothType == 2) || (clothType == 3)  || (clothType == 4) ) {

        desPoseDown.position = desPoseUp.position ;
        desPoseDown.position.z += 2.0*radious/3.0 - radious ;

        if(holdingArm == "r2"){
            desPoseDown.position.x -=2.0*radious/3.0;
        }
        else{
            desPoseDown.position.x += 2.0*radious/3.0;
        }
        desPoseDown.orientation = rotationMatrix3ToQuaternion(horizontal());
        desPoseUp = getArmPose(holdingArm);
        desPoseUp.position.z -= radious/3.0;
        moveArmsNoTearing(desPoseDown, desPoseUp, movingArm, holdingArm,radious+0.02);//(desPoseDown, movingArm, radious+0.02 );
        setGripperStates(holdingArm, true);
        switchArms();
        return true;
    }
    if (clothType == 5){

        setGripperStates(holdingArm, true);
        switchArms();
        return true;

    }
    if(clothType == 1){

        desPoseDown.position = desPoseUp.position ;
        desPoseDown.position.z += 2.0*radious/3.0 -radious ;

        if(holdingArm == "r2"){
            desPoseDown.position.x -=radious/2.0;
        }
        else{
            desPoseDown.position.x += radious/2.0;
        }
        desPoseDown.orientation = rotationMatrix3ToQuaternion(horizontal());
        desPoseUp = getArmPose(holdingArm);
        desPoseUp.position.z -= radious/3.0;
        moveArmsNoTearing(desPoseDown, desPoseUp, movingArm, holdingArm,radious+0.02);//(desPoseDown, movingArm, radious+0.02 );
        setGripperStates(holdingArm, true);
        switchArms();
        return true;

    }
    else{

        desPoseDown = getArmPose(holdingArm) ;

        //getting the orientation
        if(holdingArm == "r2")
            orient << 0, 0, -1, -1, 0, 0, 0, 1,0;
        else
            orient << 0, 0, 1, 1, 0, 0, 0, 1, 0;

        desPoseDown.orientation = rotationMatrix3ToQuaternion(orient);
        desPoseUp.orientation = holdingArmPose().orientation;

        //getting the positions
        switchArms();
        desPoseUp.position = holdingArmPose().position;
        desPoseDown.position = desPoseUp.position;
        desPoseDown.position.z -= getArmsDistance();
        if (allwaysDrop ==  true){
            setGripperStates(movingArm,true);
            return true ;

        }
            if( radious > 0.5){
            if(radious < 0.65)
               addSphereToCollisionModel(movingArm, 0.2);
            else
                addSphereToCollisionModel(movingArm, 0.3);

                cout<< "Collision Sphere added" << endl;

            if ( moveArmsFlipCloth(  radious + 0.1, desPoseUp, desPoseDown, holdingArm, movingArm) == -1){
                resetCollisionModel();

                setGripperStates(movingArm,true);
                return true;
            }
        }else{
            cout<< "just dropping the cloth" << endl;

            setGripperStates(movingArm, true);
            return true;
        }
        resetCollisionModel();
        if (!releaseCloth( movingArm )){
            setGripperStates(movingArm, true);
            return false;
        }
        return true;
    }

    return false ;
/////////////////////////////END///////////////////
//    desPoseDown.position.z += 0.15;
//    if(holdingArm == "r2"){
//        desPoseDown.position.x -=0.15;
//    }
//    else{
//        desPoseDown.position.x += 0.15;
//    }
//    desPoseDown.orientation = rotationMatrix3ToQuaternion(horizontal());

//    moveArmConstrains(desPoseDown, movingArm, radious+0.02 );

//    //getting the orientation
//    if(holdingArm == "r2")
//        orient << 0, 0, -1, -1, 0, 0, 0, 1,0;
//    else
//        orient << 0, 0, 1, 1, 0, 0, 0, 1, 0;

//    desPoseDown.orientation = rotationMatrix3ToQuaternion(orient);
//    desPoseUp.orientation = holdingArmPose().orientation;

//    //getting the positions
//    switchArms();
//    desPoseUp.position = holdingArmPose().position;
//    desPoseDown.position = desPoseUp.position;
//    desPoseDown.position.z -= getArmsDistance();

//    if( radious > 0.5){
//        if(radious < 0.65)
//           addSphereToCollisionModel(movingArm, 0.2);
//        else
//            addSphereToCollisionModel(movingArm, 0.3);

//            cout<< "Collision Sphere added" << endl;

//        if ( moveArmsFlipCloth( marker_pub, radious + 0.1, desPoseUp, desPoseDown, holdingArm, movingArm) == -1){
//            resetCollisionModel();
//            return false;
//        }
//    }else{
//        cout<< "just dropping the cloth" << endl;
//        setGripperStates(movingArm, true);
//        return true;
//    }
//    resetCollisionModel();
//    if (!releaseCloth( movingArm ))
//        return false;


//    return true;

}

//Sets the constrains in order to flip the cloth and then moving the arms
int Unfold::moveArmsFlipCloth(  float radious , geometry_msgs::Pose pose1, geometry_msgs::Pose pose2, const string &arm1Name, const string &arm2Name){

    MoveRobot cmove;
    cmove.setServoMode(false);
     //Create plan
    clopema_arm_navigation::ClopemaMotionPlan mp;
    mp.request.motion_plan_req.group_name = "arms";
    mp.request.motion_plan_req.allowed_planning_time = ros::Duration(10.0);

    //Set start state
    if (!getRobotState(mp.request.motion_plan_req.start_state))
        return -1;

    arm_navigation_msgs::SimplePoseConstraint desired_pose;
    desired_pose.header.frame_id = "base_link";
    desired_pose.header.stamp = ros::Time::now();
    desired_pose.link_name = arm2Name + "_ee";
    desired_pose.pose=pose2;

    desired_pose.absolute_position_tolerance.x = 0.02;
    desired_pose.absolute_position_tolerance.y = 0.02;
    desired_pose.absolute_position_tolerance.z = 0.02;
    desired_pose.absolute_roll_tolerance = 0.04;
    desired_pose.absolute_pitch_tolerance = 0.04;
    desired_pose.absolute_yaw_tolerance = 0.04;

    geometry_msgs::Point p1 = getArmPose(arm1Name).position;
    geometry_msgs::Point p2 = getArmPose(arm2Name).position;
    p1.x= (p1.x + p2.x)/2.0;
    p1.y= (p1.y + p2.y)/2.0;
    p1.z= (p1.z + p2.z)/2.0;

    //adding a sphere constrain in order not to tear the cloth
    mp.request.motion_plan_req.path_constraints.position_constraints.resize(3);
    mp.request.motion_plan_req.path_constraints.position_constraints[0].header.frame_id = arm1Name + "_ee";
    mp.request.motion_plan_req.path_constraints.position_constraints[0].header.stamp = ros::Time::now();
    mp.request.motion_plan_req.path_constraints.position_constraints[0].link_name = arm2Name + "_ee";
    mp.request.motion_plan_req.path_constraints.position_constraints[0].position.x = 0;
    mp.request.motion_plan_req.path_constraints.position_constraints[0].position.y = 0;
    mp.request.motion_plan_req.path_constraints.position_constraints[0].position.z = 0;
    mp.request.motion_plan_req.path_constraints.position_constraints[0].constraint_region_shape.type = arm_navigation_msgs::Shape::SPHERE;
    mp.request.motion_plan_req.path_constraints.position_constraints[0].constraint_region_shape.dimensions.push_back(radious); //radius
    mp.request.motion_plan_req.path_constraints.position_constraints[0].constraint_region_shape.dimensions.push_back(radious);
    mp.request.motion_plan_req.path_constraints.position_constraints[0].constraint_region_shape.dimensions.push_back(radious);
    mp.request.motion_plan_req.path_constraints.position_constraints[0].constraint_region_orientation.x = 0.0;
    mp.request.motion_plan_req.path_constraints.position_constraints[0].constraint_region_orientation.y = 0.0;
    mp.request.motion_plan_req.path_constraints.position_constraints[0].constraint_region_orientation.z = 0.0;
    mp.request.motion_plan_req.path_constraints.position_constraints[0].constraint_region_orientation.w = 1.0;
    mp.request.motion_plan_req.path_constraints.position_constraints[0].weight = 1.0;

    //adding a box constrain for 1st arm in order not to twist the cloth
    mp.request.motion_plan_req.path_constraints.position_constraints[1].header.frame_id = "base_link";
    mp.request.motion_plan_req.path_constraints.position_constraints[1].header.stamp = ros::Time::now();
    mp.request.motion_plan_req.path_constraints.position_constraints[1].link_name = arm1Name +"_ee";
    mp.request.motion_plan_req.path_constraints.position_constraints[1].position.x = p1.x;
    mp.request.motion_plan_req.path_constraints.position_constraints[1].position.y = p1.y;
    mp.request.motion_plan_req.path_constraints.position_constraints[1].position.z = p1.z;
    mp.request.motion_plan_req.path_constraints.position_constraints[1].constraint_region_shape.type = arm_navigation_msgs::Shape::BOX;
    mp.request.motion_plan_req.path_constraints.position_constraints[1].constraint_region_shape.dimensions.push_back(1.5);
    mp.request.motion_plan_req.path_constraints.position_constraints[1].constraint_region_shape.dimensions.push_back(0.6);
    mp.request.motion_plan_req.path_constraints.position_constraints[1].constraint_region_shape.dimensions.push_back(1.5);
    mp.request.motion_plan_req.path_constraints.position_constraints[1].constraint_region_orientation.x = 0.0;
    mp.request.motion_plan_req.path_constraints.position_constraints[1].constraint_region_orientation.y = 0.0;
    mp.request.motion_plan_req.path_constraints.position_constraints[1].constraint_region_orientation.z = 0.0;
    mp.request.motion_plan_req.path_constraints.position_constraints[1].constraint_region_orientation.w = 1.0;
    mp.request.motion_plan_req.path_constraints.position_constraints[1].weight = 1.0;

    //adding a box constrain for 2nd arm in order not to twist the cloth
    mp.request.motion_plan_req.path_constraints.position_constraints[2].header.frame_id = "base_link";
    mp.request.motion_plan_req.path_constraints.position_constraints[2].header.stamp = ros::Time::now();
    mp.request.motion_plan_req.path_constraints.position_constraints[2].link_name = arm2Name +"_ee";
    mp.request.motion_plan_req.path_constraints.position_constraints[2].position.x = p1.x;
    mp.request.motion_plan_req.path_constraints.position_constraints[2].position.y = p1.y;
    mp.request.motion_plan_req.path_constraints.position_constraints[2].position.z = p1.z;
    mp.request.motion_plan_req.path_constraints.position_constraints[2].constraint_region_shape.type = arm_navigation_msgs::Shape::BOX;
    mp.request.motion_plan_req.path_constraints.position_constraints[2].constraint_region_shape.dimensions.push_back(1.5);
    mp.request.motion_plan_req.path_constraints.position_constraints[2].constraint_region_shape.dimensions.push_back(0.6);
    mp.request.motion_plan_req.path_constraints.position_constraints[2].constraint_region_shape.dimensions.push_back(1.5);
    mp.request.motion_plan_req.path_constraints.position_constraints[2].constraint_region_orientation.x = 0.0;
    mp.request.motion_plan_req.path_constraints.position_constraints[2].constraint_region_orientation.y = 0.0;
    mp.request.motion_plan_req.path_constraints.position_constraints[2].constraint_region_orientation.z = 0.0;
    mp.request.motion_plan_req.path_constraints.position_constraints[2].constraint_region_orientation.w = 1.0;
    mp.request.motion_plan_req.path_constraints.position_constraints[2].weight = 1.0;


    // PUBLISH MARKERS
//    visualization_msgs::Marker marker1;
//    marker1.header.frame_id = "base_link";
//    marker1.header.stamp = ros::Time::now();
//    marker1.ns = "BOX";
//    marker1.id = 0;
//    marker1.type = visualization_msgs::Marker::CUBE;
//    marker1.action = visualization_msgs::Marker::ADD;
//    marker1.pose.position.x = mp.request.motion_plan_req.path_constraints.position_constraints.at(1).position.x;
//    marker1.pose.position.y = mp.request.motion_plan_req.path_constraints.position_constraints.at(1).position.y;
//    marker1.pose.position.z = mp.request.motion_plan_req.path_constraints.position_constraints.at(1).position.z;
//    marker1.pose.orientation.x = mp.request.motion_plan_req.path_constraints.position_constraints.at(1).constraint_region_orientation.x;
//    marker1.pose.orientation.y = mp.request.motion_plan_req.path_constraints.position_constraints.at(1).constraint_region_orientation.y;
//    marker1.pose.orientation.z = mp.request.motion_plan_req.path_constraints.position_constraints.at(1).constraint_region_orientation.z;
//    marker1.pose.orientation.w = mp.request.motion_plan_req.path_constraints.position_constraints.at(1).constraint_region_orientation.w;
//    marker1.scale.x = mp.request.motion_plan_req.path_constraints.position_constraints.at(1).constraint_region_shape.dimensions.at(0);
//    marker1.scale.y = mp.request.motion_plan_req.path_constraints.position_constraints.at(1).constraint_region_shape.dimensions.at(1);
//    marker1.scale.z = mp.request.motion_plan_req.path_constraints.position_constraints.at(1).constraint_region_shape.dimensions.at(2);

//    marker1.color.r = 0.0f;
//    marker1.color.g = 1.0f;
//    marker1.color.b = 0.0f;
//    marker1.color.a = 0.5;

//    marker1.lifetime = ros::Duration(5);

//    ros::Duration(0.3).sleep();
//    vis_pub.publish(marker1);
//    ros::Duration(0.3).sleep();
    //////END PUBLISHING

    arm_navigation_msgs::PositionConstraint position_constraint;
    arm_navigation_msgs::OrientationConstraint orientation_constraint;
    arm_navigation_msgs::poseConstraintToPositionOrientationConstraints(desired_pose, position_constraint, orientation_constraint);
    mp.request.motion_plan_req.goal_constraints.orientation_constraints.push_back(orientation_constraint);
    mp.request.motion_plan_req.goal_constraints.position_constraints.push_back(position_constraint);

    //add r1 goal constraints
    desired_pose.link_name = arm1Name + "_ee";
    desired_pose.pose=pose1;
    arm_navigation_msgs::poseConstraintToPositionOrientationConstraints(desired_pose, position_constraint, orientation_constraint);
    mp.request.motion_plan_req.goal_constraints.orientation_constraints.push_back(orientation_constraint);
    mp.request.motion_plan_req.goal_constraints.position_constraints.push_back(position_constraint);

    ROS_INFO("Planning");
    if (!plan(mp))
        return -1;

    ROS_INFO("Executing");
    control_msgs::FollowJointTrajectoryGoal goal;
    goal.trajectory = mp.response.joint_trajectory;
    cmove.doGoal(goal);

    return 0;
}

//Releasing the cloth in order not to hook with the cloth
bool Unfold::releaseCloth( const string &armName ){

    geometry_msgs::Pose pose = getArmPose(armName);
    setGripperStates(armName , true);
    float dx, dz=0.1;
    if (armName == "r1")
        dx = -0.3;
    else
        dx = 0.3;
    pose.position.x += dx;
    pose.position.z -= dz;
    if ( moveArmConstrains(pose, armName, getArmsDistance()+abs(dx)+0.02) == -1)
        return false;

    return true;
}

//Gets the final position showing the unfolding outcome
bool Unfold::showUnfolding(){

    geometry_msgs::Pose poseH, poseM;
    float radious = getArmsDistance()-0.03;
    Eigen::Matrix3d orient;
    if (holdingArm == "r1"){
        poseH.position.x = -radious/2.0;
        poseH.position.y = -1.1;
        poseH.position.z = 1.4;

        poseM.position = poseH.position;
        poseM.position.x = radious/2.0;

        orient << 0, -1, 1, -1, 0,0,0,-1,-1;
    }else{
        poseM.position.x = -radious/2.0;
        poseM.position.y = -1.1;
        poseM.position.z = 1.4;

        poseH.position = poseM.position;
        poseH.position.x = radious/2.0;
        orient << 0, 1, -1, 1, 0,0,0,-1,-1;
    }


    poseM.orientation = rotationMatrix3ToQuaternion(diagonalDown());
    poseH.orientation = rotationMatrix3ToQuaternion(orient);


    if(moveArmsNoTearing(poseH, poseM, holdingArm, movingArm ) == -1)
        return false;

    return true;

}

//-------------------------DIMITRA ----------------------------



std::vector <Unfold::grabRGBD> Unfold::grabRGBD360() {

    moveArms(holdingArmPose(), movingArmPose(), holdingArm, movingArm ) ;
    openni::connect( camera ) ;

    std::vector <grabRGBD> images ;
    cv::Mat rgb, depth ;
    pcl::PointCloud <pcl::PointXYZ> pc ;
    grabRGBD image ;
    ros::Time ts(0);
    image_geometry::PinholeCameraModel cm;

    for (unsigned int i = 0 ; i < 8 ; i++){

        rotateGripper( 0.09 , holdingArm) ;

        openni::grab(camera, rgb, depth, pc, ts, cm);

        image.rgb = rgb ;
        image.depth = depth ;
        image.gripperOrientation =  getTranformationMatrix( holdingArm + "_ee", "xtion3_rgb_optical_frame" ) ;
        cout<< image.gripperOrientation << endl;
        images.push_back(image);
     }

    return images ;
}

bool Unfold::graspMiddle(){


    openni::connect( camera ) ;
    //moveArms(movingArmPose(), holdingArmPose(), movingArm, holdingArm );

    setGripperStates(movingArm , true);

    cv::Mat rgb, depth ;
    pcl::PointCloud <pcl::PointXYZ> pc ;
    cv::Rect r ;
    grabFromXtion(rgb, depth, pc, r) ;
    int x,y;
    findMiddlePoint(pc,x, y );
    bool orientLeft ;
    if (holdingArm == "r2")
        orientLeft = false;
    else
        orientLeft = true ;

    cv::rectangle(rgb, cv::Rect(x-2, y-2, 5, 5), cv::Scalar(255, 0, 255), 2) ;
    cv::imwrite("/tmp/middle.png", rgb) ;

    return graspPoint(pc, x, y, false, orientLeft,false,  true);


}

bool Unfold::findMiddlePoint(const pcl::PointCloud<pcl::PointXYZ> &depth, int &x , int &y)
{

    int w = depth.width, h = depth.height ;

    int max_y = -1 ;
    int best_j = -1, best_i = -1 ;

    bool found = false ;

    double maxv = -DBL_MAX ;
    int start, end, step ;

    if(holdingArm == "r1"){
        start = 0 ;
        end = 480 ;
        step = 1 ;
    }
    else{
        start = 479 ;
        end = -1 ;
        step = -1;
    }

    int j= int(640.0f/(2.5f)) ;
    int i;
    for( i=start ; i != end ; i+=step)
    {

        pcl::PointXYZ val = depth.at(j, i) ;

        if ( !pcl_isfinite(val.z) ) continue ;

        Eigen::Vector3d p(val.x, val.y, val.z) ;

        // test whether the point lies within the cone

        if (  val.z > 1 && val.z < 1.5){
            found = true ;
            break ;
        }

    }

    if ( !found ) return false ;

    pcl::PointXYZ p_ = depth.at(j,i) ;

    int ox = j, oy = i + 10*step ;

    // find densest point cluster in a 3cm sphere around the detected point
    //findMeanShiftPoint(depth, j, i, ox, oy, 0.03) ;

    x= ox ;
    y= oy ;

    return true ;

}


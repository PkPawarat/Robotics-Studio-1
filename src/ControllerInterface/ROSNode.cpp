
#include "library/ROSNode.h"
// Keep only the headers needed
#include "ros/ros.h"


ROSNode::ROSNode(ros::NodeHandle nh) : nh_(nh){
    odom = nh_.subscribe("/odom", 1, &ROSNode::odomCallBack, this);
    laser_scan = nh_.subscribe("/scan", 1, &ROSNode::laserScanCallBack, this);
    camera = nh_.subscribe("/camera/rgb/image_raw", 1, &ROSNode::cameraCallBack, this);
    camera_depth = nh_.subscribe("/camera/depth/points", 1, &ROSNode::pointCloudCallBack, this);
// 
    pub_vel = nh_.advertise<geometry_msgs::Twist>("/cmd_vel", 3, false);
    pub_goal = nh_.advertise<geometry_msgs::PoseStamped>("/move_base_simple/goal", 3, false);
    // std::this_thread::sleep_for(std::chrono::seconds(5));
    // pub_goal = nh_.advertise<geometry_msgs::PoseStamped>("/move_base_simple/goal", 3, false);
    thread_ = std::thread(&ROSNode::simulate, this);
    thread_.detach();
    // camera = nh_.subscribe("/camera/rgb/image_raw", 1000, &ROSNode::cameraCallBack, this);
    // lidarSensor = nh_.subscribe("/sensor", 1000, &ROSNode::lidarCallBack, this);
    // pub_vel = nh_.advertise<std_msgs::Float64>("/cmd_vel", 3, false);

    
}



void ROSNode::simulate()
{
    while (ros::ok()) {
    // Simulate the environment using ROS
    std::cout << "Simulating environment using ROS..." << std::endl;
    // Add ROS simulation logic here
    std::this_thread::sleep_for(std::chrono::seconds(2));
    ROS_INFO_STREAM(bot_odom);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    Sensor sensor;
    sensor.detectShelf(returnReducedPointCloud());

    ROS_INFO_STREAM(returnReducedPointCloud().size());
    ROS_INFO_STREAM(returnLaserScan().size());
    
    std::cout << "Initializing next loop..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // if (pcl_points.size() > 0){
    //     ROS_INFO_STREAM('true');
    // }else{
    //     ROS_INFO_STREAM('false');
    // }
    // ROS_INFO_STREAM(point_cloud.width);
    }
    
}

void ROSNode::odomCallBack(const nav_msgs::OdometryConstPtr &msg)
{
    robotMtx_.lock();
    bot_odom = *msg;
    robotMtx_.unlock();
}

void ROSNode::laserScanCallBack(const sensor_msgs::LaserScanConstPtr &msg)
{
    robotMtx_.lock();
    bot_laser_scan = *msg;
    robotMtx_.unlock();
}

void ROSNode::cameraCallBack(const sensor_msgs::ImageConstPtr &msg)
{ // Type: sensor_msgs/Image
    robotMtx_.lock();
    image_ = *msg;
    robotMtx_.unlock();
}

void ROSNode::pointCloudCallBack(const sensor_msgs::PointCloud2ConstPtr &msg)
{ // Type: sensor_msgs/Image
    robotMtx_.lock();
    point_cloud = *msg;
    robotMtx_.unlock();
}

nav_msgs::Odometry ROSNode::returnOdom()
{
    return bot_odom;
}

std::vector<geometry_msgs::Point> ROSNode::returnLaserScan()
{
    std::vector<geometry_msgs::Point> filteredLaserScan_;
    temp_scan = bot_laser_scan;
    for (int i = bot_laser_scan.ranges.size() - 1; i >= 0; i--)
    {
        auto &scan = bot_laser_scan.ranges.at(i);

        if ((std::isfinite(scan) && scan != std::numeric_limits<double>::infinity() && scan != std::numeric_limits<double>::quiet_NaN() && scan < bot_laser_scan.range_max))
        {
            // delete
            filteredLaserScan_.push_back(polarToCart(i));
        }
    }

    return filteredLaserScan_;
}

sensor_msgs::Image ROSNode::returnImage()
{
    return image_;
}



std::vector<geometry_msgs::Point32>  ROSNode::returnPointCloud(){

    sensor_msgs::PointCloud out_pointcloud;
    sensor_msgs::convertPointCloud2ToPointCloud(point_cloud, out_pointcloud);
    std::vector<geometry_msgs::Point32> temp_;

    for(int i = 0 ; i < out_pointcloud.points.size(); ++i){
        geometry_msgs::Point32 point;

        //Dooo something here
        point.x = out_pointcloud.points[i].x;
        point.y = out_pointcloud.points[i].y;
        point.z = out_pointcloud.points[i].z;

        temp_.push_back(point);
        
    }
    pcl_points = temp_; //Not necessary but keeps a local record
    return temp_;
}


std::vector<geometry_msgs::Point32>  ROSNode::returnReducedPointCloud(){
    std::vector<geometry_msgs::Point32> temp_;
    temp_ = returnPointCloud();

    std::vector<geometry_msgs::Point32> filtered_temp_;

    float anglerange = 15*M_PI/180;


    for(int i = 0; i < temp_.size(); ++i){
        float distance_ = calculateDistance(temp_.at(i).x, temp_.at(i).y, temp_.at(i).z);
        if(distance_ < 1 ){ //&& temp_.at(i).y > 0
            float angle = atan2(temp_.at(i).x, temp_.at(i).y);

            if(std::abs(angle) < anglerange) {
                filtered_temp_.push_back(temp_.at(i));
            }
            //filtered_temp_.push_back(temp_.at(i));
        }
    }
    return filtered_temp_;
}

std::vector<geometry_msgs::Point32> ROSNode::filterRangeBounds() {
    std::vector<geometry_msgs::Point32> temp_;
    std::vector<geometry_msgs::Point32> filtered_temp_;
    temp_ = returnReducedPointCloud();

    //set 15 degree range bounds to keep vision small
    float anglerange = 15*M_PI/180;
    
    //calculate angle of each point relative to robots orientation
    for (int i = 0; i < temp_.size(); i++ ) {
        float angle = atan2(temp_.at(i).x, temp_.at(i).y);

        if(std::abs(angle) < anglerange) {
            filtered_temp_.push_back(temp_.at(i));
        }
    }

    return filtered_temp_;




}

void ROSNode::sendCmd(double linear_x, double linear_y, double linear_z, double angular_x, double angular_y, double angular_z)
{
    bot_vel.linear.x = linear_x;
    bot_vel.linear.y = linear_y;
    bot_vel.linear.z = linear_z;
    bot_vel.angular.x = angular_x;
    bot_vel.angular.y = angular_y;
    bot_vel.angular.z = angular_z;
    pub_vel.publish(bot_vel);
}

geometry_msgs::Point ROSNode::polarToCart(unsigned int index)
{
    float angle = temp_scan.angle_min + (temp_scan.angle_increment * index); // + angle_range/2;
    float range = temp_scan.ranges.at(index);
    geometry_msgs::Point cart;

    cart.x = static_cast<double>(range * cos(angle));
    cart.y = static_cast<double>(range * sin(angle));

    // ROS_INFO_STREAM(cart.x);
    // ROS_INFO_STREAM(cart.y);

    return cart;
}

float ROSNode::calculateDistance(float x, float y, float z) {
    float distance = std::sqrt(std::pow(x, 2) +
                               std::pow(y, 2) +
                               std::pow(z, 2));
    return distance;
}
// int main(int argc, char **argv) {
//     ros::init(argc, argv, "object_detection_node");
//     ros::NodeHandle nh_;
//     ROSNode rosnode(nh_);
//     ros::spin();
//     return 0;
// }

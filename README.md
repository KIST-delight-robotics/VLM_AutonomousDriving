# Autonomous Navigation

코드 뒤에 _success 파일은 최종적으로 성공했던 파일

## 1. RealSense Camera Node

A node that outputs RGB and Depth streams from the RealSense camera and performs depth–color alignment.

Execution command:

ros2 launch realsense2_camera rs_launch.py depth_module.depth_profile:=640x480x30 rgb_camera.color_profile:=640x480x30 enable_depth:=true enable_color:=true pointcloud.enable:=false align_depth.enable:=true

---

## 2. Segmentation Node (jetson_quantization-master)

Runs a TensorRT-optimized segmentation model to generate real-time class masks.

Execution command:

cd file_path

python3 demo_trt.py

---

## 3. GPU PointCloud Generation Node (pointcloud_xyzrgb)

A package that generates XYZRGB pointclouds using CUDA based on depth images and segmentation results.

Execution command:

ros2 run pointcloud_xyzrgb pointcloud_gpu_node

rviz -> frame id : camera_color_optical_frame
	add topic : "/camera/depth_registered/points"
	
File : 
/Interaction_AutonomousNavigation/Autonomous_Navigation/src/pointcloud_xyzrgb/pointcloud_xyzrgb/pointcloud_gpu_xyzrgb.py
/Interaction_AutonomousNavigation/Autonomous_Navigation/src/pointcloud_xyzrgb/pointcloud_xyzrgb/point_cloud_xyzrgb.py
	
---

## 4. BEV Occupancy Grid Node (bev_cuda)

A package that generates a CUDA-based Bird’s-Eye-View (top-view) occupancy grid from pointcloud input.

Execution command:

ros2 launch bev_cuda bev.launch.py

File : 
/Interaction_AutonomousNavigation/Autonomous_Navigation/src/bev_cuda/bev_cuda/bev_node.py

---

## 5. GPS-based Global Planner (gps_nav)

A global planner package that calculates global target positions using RTK-GPS data and robot odometry, and publishes /dxdy.

Execution command:

cd ~/ros2_ws/src/gps_nav/gps_nav

ros2 run gps_nav planner_server

File : 
/Interaction_AutonomousNavigation/Autonomous_Navigation/src/gps_nav/gps_nav/planner_server11_success.py
/Interaction_AutonomousNavigation/Autonomous_Navigation/src/gps_nav/gps_nav/nav_utils_success.py

---

## 6. DWA Local Planner (dwa_nav)

A local planner that computes obstacle-avoidance paths based on the BEV occupancy grid and generates velocity commands (/cmd).

Execution command:

ros2 run dwa_nav dwa_node

File :
/Interaction_AutonomousNavigation/Autonomous_Navigation/src/dwa_nav/dwa_nav/distmap_def.py
/Interaction_AutonomousNavigation/Autonomous_Navigation/src/dwa_nav/dwa_nav/dwa_node_success.py

---

## 7. Go2 Control Bridge Node (unitree_ros2_example)

A bridge package that converts /cmd messages from the DWA or GPS planner into Unitree Go2 control commands and sends them to the robot.

Execution command:

ros2 run unitree_ros2_example dwa2go2_node

File : 
/Interaction_AutonomousNavigation/Autonomous_Navigation/src/unitree_ros2/example/src/src/go2/dwa2go2_node.cpp

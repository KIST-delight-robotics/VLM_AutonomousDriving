/**********************************************************************
 * Go2 Sport Client Teleop (Keyboard Control)
 * - 키보드 입력으로 이동/정지/자세 제어
 * - ROS2 Humble, Unitree Go2 SDK 기반
 ***********************************************************************/

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <termios.h>
#include <unistd.h>

#include "rclcpp/rclcpp.hpp"
#include "common/ros2_sport_client.h"
#include "unitree_go/msg/sport_mode_state.hpp"

#define TOPIC_HIGHSTATE "lf/sportmodestate"

enum TestMode {
  NORMAL_STAND = 0,
  BALANCE_STAND,
  VELOCITY_MOVE,
  STAND_DOWN,
  STAND_UP,
  DAMP,
  RECOVERY_STAND,
  SIT,
  RISE_SIT,
  MOVE,
  STOP_MOVE,
};

// --- Non-blocking keyboard 입력용 함수 ---
int getch()
{
  struct termios oldt, newt;
  int ch;
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  ch = getchar();
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  return ch;
}

class Go2SportTeleopNode : public rclcpp::Node {
public:
  Go2SportTeleopNode() : Node("go2_sport_teleop_node"), sport_client_(this) {
    state_sub_ = this->create_subscription<unitree_go::msg::SportModeState>(
        TOPIC_HIGHSTATE, 10,
        std::bind(&Go2SportTeleopNode::StateCallback, this, std::placeholders::_1));

    RCLCPP_INFO(this->get_logger(),
                "Go2 Teleop Ready. Controls:\n"
                "  w/s : 전진/후진\n"
                "  a/d : 좌측/우측 이동\n"
                "  q/e : 좌회전/우회전\n"
                "  space: 정지\n"
                "  1: Normal Stand, 2: Balance Stand, z: Sit, x: RiseSit\n"
                "  Ctrl+C to quit.");
    teleop_thread_ = std::thread(&Go2SportTeleopNode::TeleopLoop, this);
  }

private:
  void StateCallback(const unitree_go::msg::SportModeState::SharedPtr state) {
    // 위치와 자세 출력
    RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
                         "Pos(%.2f, %.2f, %.2f), Yaw=%.2f",
                         state->position[0], state->position[1], state->position[2],
                         state->imu_state.rpy[2]);
  }

  void TeleopLoop() {
    double vx = 0.0, vy = 0.0, wz = 0.0;

    while (rclcpp::ok()) {
      int c = getch();

      if (c == 'w') { vx = 0.3; vy = 0.0; wz = 0.0; }
      else if (c == 's') { vx = -0.3; vy = 0.0; wz = 0.0; }
      else if (c == 'a') { vx = 0.0; vy = 0.2; wz = 0.0; }
      else if (c == 'd') { vx = 0.0; vy = -0.2; wz = 0.0; }
      else if (c == 'q') { vx = 0.0; vy = 0.0; wz = 0.3; }
      else if (c == 'e') { vx = 0.0; vy = 0.0; wz = -0.3; }
      else if (c == ' ') { vx = vy = wz = 0.0; sport_client_.StopMove(req_); continue; }
      else if (c == '1') { sport_client_.StandUp(req_); continue; }
      else if (c == '2') { sport_client_.BalanceStand(req_); continue; }
      else if (c == 'z') { sport_client_.Sit(req_); continue; }
      else if (c == 'x') { sport_client_.RiseSit(req_); continue; }
      else { continue; }

      // Move 명령 보내기
      sport_client_.Move(req_, vx, vy, wz);
    }
  }

  SportClient sport_client_;
  unitree_api::msg::Request req_;
  rclcpp::Subscription<unitree_go::msg::SportModeState>::SharedPtr state_sub_;
  std::thread teleop_thread_;
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<Go2SportTeleopNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}

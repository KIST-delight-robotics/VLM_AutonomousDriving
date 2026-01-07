/**********************************************************************
 * Go2 Sport Client Teleop (Keyboard Control) - Incremental speed control
 * - 기본 vx=1.0으로 출발
 * - 키보드 입력으로 vx를 0.1씩 증감
 ***********************************************************************/

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <termios.h>
#include <unistd.h>
#include <algorithm>  // std::clamp

#include "rclcpp/rclcpp.hpp"
#include "common/ros2_sport_client.h"
#include "unitree_go/msg/sport_mode_state.hpp"

#define TOPIC_HIGHSTATE "/lf/sportmodestate"
#include <atomic>
#include <mutex>

// --- Non-blocking keyboard 입력용 함수 (주의: getchar()는 사실상 blocking임) ---
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
                "Go2 Teleop Ready (Incremental vx).\n"
                "  w/s : vx += 0.1 / vx -= 0.1\n"
                "  a/d : vy += 0.1 / vy -= 0.1\n"
                "  q/e : wz += 0.1 / wz -= 0.1\n"
                "  space: StopMove (and reset v=0)\n"
                "  1: StandUp, 2: BalanceStand, z: StandDown, x: RiseSit\n"
                "  r: reset (vx=1.0, vy=0, wz=0)\n"
                "  Ctrl+C to quit.");

    teleop_thread_ = std::thread(&Go2SportTeleopNode::TeleopLoop, this);
  }

private:
  void StateCallback(const unitree_go::msg::SportModeState::SharedPtr state) {
    RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
                         "Pos(%.2f, %.2f, %.2f), Yaw=%.2f",
                         state->position[0], state->position[1], state->position[2],
                         state->imu_state.rpy[2]);
  }

  static double clamp(double v, double lo, double hi) {
    return std::max(lo, std::min(v, hi));
  }

  void TeleopLoop() {
    // ---- 기본값: x방향 1.0으로 출발 ----
    double vx = 0.0;
    double vy = 0.0;
    double wz = 0.0;

    // ---- 스텝/제한 ----
    const double step_vx = 0.1;
    const double step_vy = 0.1;
    const double step_wz = 0.1;

    // 안전하게 범위 제한 (필요하면 너 환경에 맞게 조정)
    const double vx_min = 0.0, vx_max = 1.5;
    const double vy_min = -0.8, vy_max = 0.8;
    const double wz_min = -1.0, wz_max = 1.0;

    // 시작하자마자 "출발"
    sport_client_.Move(req_, vx, vy, wz);
    RCLCPP_INFO(this->get_logger(), "Start Move: vx=%.2f vy=%.2f wz=%.2f", vx, vy, wz);

    while (rclcpp::ok()) {
      int c = getch();

      bool send_move = false;

      if (c == 'w') { vx += step_vx; send_move = true; }
      else if (c == 's') { vx -= step_vx; send_move = true; }
      else if (c == 'a') { vy += step_vy; send_move = true; }
      else if (c == 'd') { vy -= step_vy; send_move = true; }
      else if (c == 'q') { wz += step_wz; send_move = true; }
      else if (c == 'e') { wz -= step_wz; send_move = true; }

      else if (c == ' ') {
        // 정지 + 값 리셋(원치 않으면 리셋 줄만 지우면 됨)
        sport_client_.StopMove(req_);
        vx = 0.0; vy = 0.0; wz = 0.0;
        RCLCPP_INFO(this->get_logger(), "StopMove. Reset v=0.");
        continue;
      }

      else if (c == '1') { sport_client_.StandUp(req_); continue; }
      else if (c == '2') { sport_client_.BalanceStand(req_); continue; }
      else if (c == 'z') { sport_client_.StandDown(req_); continue; }
      else if (c == 'x') { sport_client_.RiseSit(req_); continue; }

      else if (c == 'r') {
        vx = 1.0; vy = 0.0; wz = 0.0;
        send_move = true;
      }
      else {
        continue;
      }

      if (send_move) {
        vx = clamp(vx, vx_min, vx_max);
        vy = clamp(vy, vy_min, vy_max);
        wz = clamp(wz, wz_min, wz_max);

        sport_client_.Move(req_, vx, vy, wz);
        RCLCPP_INFO(this->get_logger(), "Move: vx=%.2f vy=%.2f wz=%.2f", vx, vy, wz);
      }
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

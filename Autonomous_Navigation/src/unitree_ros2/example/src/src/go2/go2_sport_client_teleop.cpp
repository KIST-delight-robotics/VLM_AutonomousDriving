/**********************************************************************
 * Go2 Sport Client Teleop (Keyboard Control) - Incremental speed control
 *
 * - Ctrl+C 종료가 깔끔하도록: non-blocking key input(select)
 * - 무선 컨트롤러로 속도 명령 내릴 예정:
 *     -> Move 송신은 "주석(매크로)" 처리 (ENABLE_MOVE_CMD)
 * - space는 안전용 StopMove로 유지 (ENABLE_STOP_CMD)
 ***********************************************************************/

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <mutex>
#include <algorithm>

#include <termios.h>
#include <unistd.h>
#include <sys/select.h>

#include "rclcpp/rclcpp.hpp"
#include "common/ros2_sport_client.h"
#include "unitree_go/msg/sport_mode_state.hpp"

#define TOPIC_HIGHSTATE "/lf/sportmodestate"

// =================== 송신 ON/OFF 스위치 ===================
// 무선 컨트롤러로 속도 명령을 내릴 거면 0으로 두면 됨
#define ENABLE_MOVE_CMD 0     // 1: Move(req_, vx, vy, wz) 송신 / 0: 비활성(주석 처리 효과)
#define ENABLE_STOP_CMD 1     // 1: StopMove(req_) 송신 / 0: 비활성

// =================== Non-blocking keyboard input ===================
// timeout_ms 동안 입력 기다렸다가 없으면 -1 반환
static int getch_nonblocking(int timeout_ms = 50)
{
  fd_set set;
  FD_ZERO(&set);
  FD_SET(STDIN_FILENO, &set);

  struct timeval timeout;
  timeout.tv_sec = timeout_ms / 1000;
  timeout.tv_usec = (timeout_ms % 1000) * 1000;

  int rv = select(STDIN_FILENO + 1, &set, nullptr, nullptr, &timeout);
  if (rv <= 0) return -1; // timeout or error

  struct termios oldt, newt;
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);

  int ch = getchar();

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  return ch;
}

class Go2SportTeleopNode : public rclcpp::Node {
public:
  Go2SportTeleopNode()
  : Node("go2_sport_teleop_node"),
    sport_client_(this)
  {
    state_sub_ = this->create_subscription<unitree_go::msg::SportModeState>(
      TOPIC_HIGHSTATE, 10,
      std::bind(&Go2SportTeleopNode::StateCallback, this, std::placeholders::_1)
    );

    // 20Hz 타이머: Move 지속 송신
    timer_ = this->create_wall_timer(
      std::chrono::milliseconds(50),
      std::bind(&Go2SportTeleopNode::SendMove, this)
    );

    {
      std::lock_guard<std::mutex> lk(cmd_mtx_);
      vx_ = 1.0; vy_ = 0.0; wz_ = 0.0;   // 기본 vx=1.0 출발(요구사항)
      stopped_ = false;
    }

    RCLCPP_INFO(this->get_logger(),
      "Go2 Teleop Ready (Incremental).\n"
      "  w/s : vx += 0.1 / vx -= 0.1\n"
      "  a/d : vy += 0.1 / vy -= 0.1\n"
      "  q/e : wz += 0.1 / wz -= 0.1\n"
      "  space: StopMove (and reset v=0)\n"
      "  u: StandUp, 2: BalanceStand, z: StandDown, x: RiseSit\n"
      "  r: reset (vx=1.0, vy=0, wz=0)\n"
      "  Ctrl+C to quit.\n"
      "  ENABLE_MOVE_CMD=%d, ENABLE_STOP_CMD=%d",
      ENABLE_MOVE_CMD, ENABLE_STOP_CMD
    );

    teleop_thread_ = std::thread(&Go2SportTeleopNode::TeleopLoop, this);
  }

  ~Go2SportTeleopNode() override {
    // Ctrl+C 시 getch가 non-blocking이라 thread가 자연 종료 가능
    if (teleop_thread_.joinable()) teleop_thread_.join();
  }

private:
  void StateCallback(const unitree_go::msg::SportModeState::SharedPtr state) {
    RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
      "Pos(%.2f, %.2f, %.2f), Yaw=%.2f",
      state->position[0], state->position[1], state->position[2],
      state->imu_state.rpy[2]
    );
  }

  static double clamp(double v, double lo, double hi) {
    return std::max(lo, std::min(v, hi));
  }

  // 타이머 콜백: 현재 목표 속도를 계속 송신
  void SendMove() {
    std::lock_guard<std::mutex> lk(cmd_mtx_);
    if (stopped_) return;

#if ENABLE_MOVE_CMD
    sport_client_.Move(req_, vx_, vy_, wz_);
#endif
  }

  void TeleopLoop() {
    // ---- 스텝/제한 ----
    const double step_vx = 0.1, step_vy = 0.1, step_wz = 0.1;

    // 안전 범위 (환경에 맞게 조정)
    const double vx_min = 0.0, vx_max = 1.5;
    const double vy_min = -0.8, vy_max = 0.8;
    const double wz_min = -1.0, wz_max = 1.0;

    while (rclcpp::ok()) {
      int c = getch_nonblocking(50);
      if (c < 0) continue;  // 입력 없으면 루프 계속 → Ctrl+C에 잘 반응

      bool update_cmd = false;

      // --- 안전하게: lock은 값 업데이트에만 사용 ---
      // (SportClient 호출은 lock 밖에서 하는 게 안정적)
      // 필요 시 호출할 명령을 밖에서 실행하도록 flag로 빼둠.
      enum class Action { NONE, STANDUP, BALANCE, STANDDOWN, RISESIT, STOPMOVE };
      Action action = Action::NONE;

      double vx_local, vy_local, wz_local;
      bool stopped_local;

      {
        std::lock_guard<std::mutex> lk(cmd_mtx_);

        if (c == 'w') { vx_ += step_vx; update_cmd = true; }
        else if (c == 's') { vx_ -= step_vx; update_cmd = true; }
        else if (c == 'a') { vy_ += step_vy; update_cmd = true; }
        else if (c == 'd') { vy_ -= step_vy; update_cmd = true; }
        else if (c == 'q') { wz_ += step_wz; update_cmd = true; }
        else if (c == 'e') { wz_ -= step_wz; update_cmd = true; }

        else if (c == ' ') {
          vx_ = 0.0; vy_ = 0.0; wz_ = 0.0;
          stopped_ = true;
          action = Action::STOPMOVE;
        }
        else if (c == 'u') { action = Action::STANDUP; }
        else if (c == '2') { action = Action::BALANCE; }
        else if (c == 'z') { action = Action::STANDDOWN; }
        else if (c == 'x') { action = Action::RISESIT; }

        else if (c == 'r') {
          vx_ = 1.0; vy_ = 0.0; wz_ = 0.0;
          update_cmd = true;
        } else {
          // 알 수 없는 키
          continue;
        }

        if (update_cmd) {
          vx_ = clamp(vx_, vx_min, vx_max);
          vy_ = clamp(vy_, vy_min, vy_max);
          wz_ = clamp(wz_, wz_min, wz_max);
          stopped_ = false; // 움직임 재개
        }

        // lock 밖에서 출력/호출하려고 스냅샷 떠둠
        vx_local = vx_; vy_local = vy_; wz_local = wz_;
        stopped_local = stopped_;
      }

      // --- lock 밖에서 SportClient 호출 ---
      if (action == Action::STOPMOVE) {
#if ENABLE_STOP_CMD
        sport_client_.StopMove(req_);
#endif
        RCLCPP_INFO(this->get_logger(), "StopMove. Reset v=0. (stopped=%d)", (int)stopped_local);
        continue;
      }
      if (action == Action::STANDUP) { sport_client_.StandUp(req_); continue; }
      if (action == Action::BALANCE) { sport_client_.BalanceStand(req_); continue; }
      if (action == Action::STANDDOWN) { sport_client_.StandDown(req_); continue; }
      if (action == Action::RISESIT) { sport_client_.RiseSit(req_); continue; }

      if (update_cmd) {
        RCLCPP_INFO(this->get_logger(), "Cmd set: vx=%.2f vy=%.2f wz=%.2f (stopped=%d)",
                    vx_local, vy_local, wz_local, (int)stopped_local);
      }
    }
  }

private:
  SportClient sport_client_;
  unitree_api::msg::Request req_;

  rclcpp::Subscription<unitree_go::msg::SportModeState>::SharedPtr state_sub_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::thread teleop_thread_;

  std::mutex cmd_mtx_;
  double vx_{1.0}, vy_{0.0}, wz_{0.0};
  bool stopped_{false};
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<Go2SportTeleopNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}


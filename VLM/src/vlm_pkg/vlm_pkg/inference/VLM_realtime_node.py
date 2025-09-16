
## 이미지 토픽 수신 -> 콜백 실행 -> 이미지 처리 및 API 호출 -> 결과 퍼블리시 -> 종료 -> 다음 이미지 대기 및 반복

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image
from std_msgs.msg import String
from cv_bridge import CvBridge
import time
import cv2
import base64
import requests
import json
import csv


class VLMtalk_Node(Node):
    def __init__(self):
        super().__init__('VLMtalk_node')
        self.start_time = time.time()  # 시작 시간
        self.get_logger().info("VLMtalk 시작!")
        self.publisher = self.create_publisher(String, '/VLM_talk_phrase', 10)
        
        # 실시간 이미지 토픽 등록
        self.bridge = CvBridge()
        self.subscription = self.create_subscription(
            Image, 
            '/camera/camera/color/image_raw',  # realsense 이미지 토픽
            self.image_callback,
            10
        )

    def seconds_to_hhmmss(self, elapsed_sec):
        """초 단위로 받은 시간을 시:분:초 형식으로 변환"""
        hours = int(elapsed_sec // 3600)
        minutes = int((elapsed_sec % 3600) // 60)
        seconds = int(elapsed_sec % 60)
        return f"{hours:02d}:{minutes:02d}:{seconds:02d}"

    def track_time(self):
        """경과 시간을 계산하여 포맷된 시간 반환"""
        elapsed_time = time.time() - self.start_time  # 현재 시간에서 시작 시간을 뺌
        return float(elapsed_time)


    def image_callback(self, msg):
        """Realsense로부터 이미지 토픽 수신 시 호출되는 콜백 함수"""
        try:
            cv_image = self.bridge.imgmsg_to_cv2(msg, 'bgr8') #numpy 배열로 변환
            print(f"해상도: {msg.width} x {msg.height} (픽셀)")
            # 프레임 처리
            elapsed_sec = self.track_time()  # 경과 시간
            frame_idx = 0  # 비디오 프레임 대신, stream이므로 0 또는 카운터 사용
            self.process_frame_and_publish(cv_image, frame_idx=frame_idx, elapsed_sec=elapsed_sec)
        except Exception as e:
            self.get_logger().error(f"Error in image callback: {e}")


    def process_frame_and_publish(self, cv_image, frame_idx=None, elapsed_sec=None):
        """각 프레임을 처리하고, 한마디를 생성하여 퍼블리시하는 메소드"""
        try:
            # 이미지를 base64로 변환
            retval, buffer = cv2.imencode('.jpg', cv_image)
            image_base64 = base64.b64encode(buffer).decode('utf-8')

            # 프롬프트 구성
            prompt = f"""

        <start_of_turn>user 
        
First, check if there is a crosswalk in the image.
- If there is NO crosswalk, answer only: "횡단보도 없음" and do NOT answer any other question.
- If a crosswalk IS present, then:

1. Analyze the left side of the image and determine if any cars are present. Describe the position and number of the cars in detail.
2. Analyze the area near the crosswalk and describe if there are many people waiting or about to cross. Specify how many people you see and how closely they are grouped.
3. Based on your analysis above, determine the recommended action:
   - If there are cars on the left:
     * If there are many people: recommend waiting.
     * If there are few or no people: recommend crossing (explain why).
   - If there are no cars on the left:
     * If there are many people: recommend waiting.
     * If there are few or no people: recommend crossing (explain why).
4. Explain your reasoning for the recommended action, considering both the car and people situation.

Give your answer in a detailed, step-by-step format following the analysis and reasoning process above.

        <end_of_turn>
        <start_of_turn>model

            """
            # API 호출
            url = 'http://localhost:11434/api/generate'
            data = {
                "model": "gemma3:4b-it-qat",
                "prompt": prompt,
                "images": [image_base64],
                "temperature" : 0.2
            }

            self.get_logger().info("Ollama API에 요청 전송 중...")
            response = requests.post(url, json=data, stream=True)
            self.get_logger().info("응답 수신, 처리 중...")


            full_response = ""
            final_metrics = {}
            
            for line in response.iter_lines():
                
                if line:
                    try:
                        chunk = line.decode('utf-8')
                        if chunk.startswith('{') and chunk.endswith('}'):
                            resp = json.loads(chunk)
                            if 'response' in resp:
                                full_response += resp['response']
                                if resp.get('done', False):
                                    metric_keys = ['total_duration', 'load_duration', 'prompt_eval_count', 'prompt_eval_duration', 'eval_count', 'eval_duration']
                                    if any(k in resp for k in metric_keys):
                                        final_metrics = {k: resp[k] for k in metric_keys if k in resp}
                                        self.get_logger().info(f"메트릭 업데이트: {final_metrics}")
                        else:
                            self.get_logger().info(f"JSON이 아닌 청크: {chunk}")  # <-- 디버그
                    except Exception as e:
                        self.get_logger().error(f"응답 처리 중 에러: {e}")
                        self.get_logger().error(f"청크 내 원본: {line}")
                        continue



            # # 응답 결과 로그
            self.get_logger().info(f"BENBEN의 한마디: {full_response}")
            token_per_sec = 0
            
            if full_response.strip() != "특별한 상황이 아님":
                # 퍼블리시할 메시지 생성
                output_msg = String()
                output_msg.data = full_response
                self.publisher.publish(output_msg)

                # 시간, 분, 초 포맷
                time_str = self.seconds_to_hhmmss(elapsed_sec) if elapsed_sec is not None else "00:00:00"

                # 로그 기록
                print("final_metrics:", final_metrics)
                ###왜 안되묘 ㅠㅠ
                log_str = f"[Frame {frame_idx}|{time_str}] {full_response}"
                self.get_logger().info(log_str)
                
                # 토큰 수 기록!
                eval_count = final_metrics.get('eval_count')
                eval_duration = final_metrics.get('eval_duration')
                print(eval_count)
                print(eval_duration)
                if eval_count and eval_duration and eval_duration > 0:
                    token_per_sec = eval_count / eval_duration * 1e9
                    # token_per_sec는 초당 생성 토큰 수 (float)
                    print(f"생성 속도: {token_per_sec:.2f} token/s")

                # CSV 파일에 시간, 대사 저장
                with open("../data/out/video_2_vlm_test(realtime).csv", mode='a', newline='', encoding='utf-8') as f:
                    writer = csv.writer(f)
                    writer.writerow([time_str, round(token_per_sec,2), full_response])

        except Exception as e:
            self.get_logger().error(f"처리 중 에러 발생: {e}")


def main(args=None):
    rclpy.init(args=args)
    node = VLMtalk_Node()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()

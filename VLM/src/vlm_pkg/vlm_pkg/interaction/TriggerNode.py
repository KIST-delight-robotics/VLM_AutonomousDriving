# 1. /partial_text에서 특정 triggerword 감지시 topic a발행
# 2. topic a 구독시 TTS 발생

import rclpy
from rclpy.node import Node
from std_msgs.msg import String  # 메시지 타입

    
class triggerSubscriber(Node):
    def __init__(self):
        super().__init__('trigger_subscriber')
        self.get_logger().info("Node init 시작")
        self.subscription = self.create_subscription(String, '/partial_text', self.partial_text_callback, 10)
        self.get_logger().info("Subscription 생성 완료")
        self.tts_publisher = self.create_publisher(String, '/TTS_caller', 10)

    def partial_text_callback(self, msg):
        self.get_logger().info(f"partial_text_callback called with data: {msg.data}")
        text = msg.data.strip()
        if ("지금 건너" in text):
            self.get_logger().info("지금 건너도 되냐구 물어봤어요.")
            self.VLM_tts_caller()  # 해당 함수 호출

    def VLM_tts_caller(self):
        self.get_logger().info("지금 건너도 될지 추론중이에요...TTS_caller 발행!")
        msg = String()
        msg.data = "O"  # TTS에 전송할 내용
        self.tts_publisher.publish(msg)



def main(args=None):
    rclpy.init(args=args)
    subscriber = triggerSubscriber()
    rclpy.spin(subscriber)
    subscriber.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()

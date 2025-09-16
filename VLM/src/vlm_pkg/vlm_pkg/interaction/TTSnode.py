#TTSSubscriber가 해당 토픽을 받아 실제 TTS를 수행
import rclpy
from rclpy.node import Node
from std_msgs.msg import String

# TTS lib
import requests, time, os, threading
from pydub import AudioSegment


class TTSSubscriber(Node):
    def __init__(self):
        super().__init__('tts_subscriber')
        self.get_logger().info("TTSSubscriber Node started... 대기중")
        self.subscription = self.create_subscription(String, 'TTS_caller', self.tts_callback, 10)
        self.vlm_subscription= self.create_subscription(String, '/VLM_talk_phrase',self.vlm_talker,10)
        self.question_confirm_path = "/home/nvidia/ros2_ws/KIST/VLM_AutonomousDriving/VLM/data/mic/question_confirm.mp3"
        self.flag = 0 # vlm 발화 상태 flag

    def tts_callback(self, msg): # vlm 발화를 위한 조건 함수 
        text = msg.data
        if text == 'O':
            self.get_logger().info(f"TTS 요청 수신: {text}")
            self.flag=1


    def vlm_talker(self, msg): # vlm 발화 함수
        vlm_text = msg.data
        if self.flag==1:
            self.get_logger().info(f"vlm 텍스트 수신 후 TTS 발행하겠습니다.{vlm_text}")
            if self.text2speech(text):
                self.play_question_confirm_tts(self.question_confirm_path)
            self.flag = 0 


    def text2speech(self, text):
        """Naver Clova Voice API 호출하여 TTS 생성"""
        client_id = "fo0f88v3wl"
        client_secret = "KUa8Lcp8JAVE2EK92G0dtyn8ywWKFTH2iKOhnoaB"
        
        url = "https://naveropenapi.apigw.ntruss.com/tts-premium/v1/tts"
        headers = {
            "X-NCP-APIGW-API-KEY-ID": client_id,
            "X-NCP-APIGW-API-KEY": client_secret,
            "Content-Type": "application/x-www-form-urlencoded"
        }
        data = {
            "speaker": "nsangdo",
            "volume": "0",
            "speed": "0",
            "pitch": "0",
            "format": "mp3",
            "text": text
        }
        try:
            start_time = time.time()
            response = requests.post(url, headers=headers, data=data)
            if response.status_code == 200:
                with open(self.question_confirm_path, "wb") as f:
                    f.write(response.content)
                self.get_logger().info(f"🟢 TTS 생성 성공: {self.question_confirm_path}")
                self.get_logger().info(f"⏱️ TTS 생성 시간: {time.time() - start_time:.3f}초")
                return True
            else:
                self.get_logger().error(f"🔴 Naver Clova TTS 오류: {response.status_code}")
                return False
        except Exception as e:
            self.get_logger().error(f"🔴 TTS 호출 실패: {e}")
            return False

    def play_question_confirm_tts(self, audio_path):
        """TTS mp3 파일을 wav로 변환 후 재생"""
        def play_audio():
            try:
                sound = AudioSegment.from_file(audio_path, format="mp3")
                target_dBFS = -14.0
                change_in_dBFS = target_dBFS - sound.dBFS
                sound = sound.apply_gain(change_in_dBFS)

                temp_wav = "/tmp/question_confirm_normalized.wav"
                sound.export(temp_wav, format="wav")

                self.get_logger().info("🎵 TTS aplay 재생 시작")
                os.system(f"aplay {temp_wav}")
                self.get_logger().info("🎵 TTS aplay 재생 완료")
            except Exception as e:
                self.get_logger().error(f"❌ TTS 재생 실패: {e}")

        threading.Thread(target=play_audio, daemon=True).start()


def main(args=None):
    rclpy.init(args=args)
    tts_subscriber = TTSSubscriber()

    executor = rclpy.executors.MultiThreadedExecutor()
    executor.add_node(tts_subscriber)

    try:
        executor.spin()
    except KeyboardInterrupt:
        pass
    finally:
        tts_subscriber.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()







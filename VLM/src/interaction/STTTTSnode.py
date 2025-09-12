# 1. /partial_text에서 특정 triggerword 감지시 topic a발행
# 2. topic a 구독시 TTS 발생

import rclpy
from rclpy.node import Node
from std_msgs.msg import String  # 메시지 타입

    
class triggerSubscriber(Node):
    def __init__(self):
        super().__init__('trigger_subscriber')
        self.get_logger().info("triggerSubscriber Node started... 지금건너 감지중")
        self.subscription = self.create_subscription(
            String,
            '/partial_text',  # 구독할 토픽 이름
            self.partial_text_callback,
            10  # QoS 큐 사이즈
        )

    
    def partial_text_callback(self, msg):
        text = msg.data.strip()
        if "지금 건너" in text:
            self.get_logger().info("'/partial_text' topic에서 '지금 건너' 음성 감지됨.")
            self.VLM_tts_Talker()  # 해당 함수 호출

    def VLM_tts_Talker(self):
        print("hi")
        
    def text2speech(self, text):
        """Naver Clova Voice API 호출하여 질문 확인 음성 생성"""
        client_id = "fo0f88v3wl"
        client_secret = "KUa8Lcp8JAVE2EK92G0dtyn8ywWKFTH2iKOhnoaB"
        
        url = "https://naveropenapi.apigw.ntruss.com/tts-premium/v1/tts"
        
        headers = {
            "X-NCP-APIGW-API-KEY-ID": client_id,
            "X-NCP-APIGW-API-KEY": client_secret,
            "Content-Type": "application/x-www-form-urlencoded"
        }
        
        # Naver Clova Voice 설정
        data = {
            "speaker": "nsangdo",  # 음성 종류 (nara, clara, matt, shinji, meow, dinna 등)
            "volume": "0",      # 볼륨 (-5 ~ 5)
            "speed": "0",       # 속도 (-5 ~ 5)  
            "pitch": "0",       # 음높이 (-5 ~ 5)
            "format": "mp3",    # 출력 포맷 (mp3, wav, ogg)
            "text": text
        }

        try:
            start_time = time.time()
            response = requests.post(url, headers=headers, data=data)
            
            if response.status_code == 200:
                with open(self.question_confirm_path, "wb") as f:
                    f.write(response.content)
                
                generation_time = time.time() - start_time    
                self.get_logger().info(f"🟢 질문 확인 TTS 생성 성공 (Naver Clova) → {self.question_confirm_path}")
                self.get_logger().info(f"⏱️ TTS 생성 시간: {generation_time:.3f}초")

                # 🆕 타임스탬프 추출을 위한 WAV 변환
                sound = AudioSegment.from_file(self.question_confirm_path, format="mp3")

                wav_path = "/tmp/question_confirm_for_stt.wav"
                sound = sound.set_frame_rate(16000).set_channels(1)
                sound.export(wav_path, format="wav")
                
                # 🆕 타임스탬프 추출
                stt_timestamps = self.extract_question_confirm_timestamps(wav_path, text)
                
                # 🆕 원본 텍스트와 병합
                corrected_timestamps = self.merge_original_with_confirm_timestamps(text, stt_timestamps)

                return True
            else:
                self.get_logger().error(f"🔴 Naver Clova TTS 오류: {response.status_code}")
                self.get_logger().error(f"오류 내용: {response.text}")
                return False
                
        except Exception as e:
            self.get_logger().error(f"🔴 Naver Clova TTS 호출 실패: {e}")
            return False
            
    def play_question_confirm_tts(self, audio_path):
        """질문 확인 TTS를 재생하고 완료 시 상태 퍼블리시"""
        def play_audio():
            try:
                # 🔧 수정: TTS 시작 시 소리 추출 중단
                self.question_processing = False
                self.waiting_for_tts = False
                self.trigger_detected = False  # 여기서 최종적으로 False

                self.question_confirm_playing = True
                
                # 재생 시작 상태 퍼블리시
                self.publish_question_confirm_status("playing")


                # 🆕 자막 퍼블리시 시작
                if hasattr(self, 'question_confirm_subtitle_data'):
                    self.publish_question_confirm_subtitle(self.question_confirm_subtitle_data)
                


                # ✅ Mp3Player와 동일한 정규화 적용
                sound = AudioSegment.from_file(audio_path, format="mp3")
                
                # ✅ -14.0 dBFS로 정규화 (Mp3Player와 동일)
                target_dBFS = -14.0
                change_in_dBFS = target_dBFS - sound.dBFS
                sound = sound.apply_gain(change_in_dBFS)
                


                # ✅ 정규화된 오디오를 WAV로 변환하여 임시 파일로 저장
                temp_wav = "/tmp/question_confirm_normalized.wav"
                sound.export(temp_wav, format="wav")

                self.get_logger().info("🎵 질문 확인 TTS aplay 재생 시작")
                
                # aplay로 WAV 재생 (Mp3Player.py와 동일한 방식)
                os.system(f"aplay {temp_wav}")
                
                self.get_logger().info("🎵 질문 확인 TTS aplay 재생 완료")


            except Exception as e:
                self.get_logger().error(f"❌ TTS 재생 실패: {e}")
            finally:
                self.question_confirm_playing = False
                # 재생 완료 상태 퍼블리시
                self.publish_question_confirm_status("completed")
                
                # 🔧 대기 중인 mp4가 있으면 재생 허용 신호 전송
                if self.pending_mp4_data:
                    self.get_logger().info("📬 TTS 완료 - 대기 중인 mp4 재생 허용")
                    self.process_pending_mp4()

        # 별도 스레드에서 재생
        threading.Thread(target=play_audio, daemon=True).start()






def main(args=None):
    rclpy.init(args=args)
    subscriber = triggerSubscriber()
    rclpy.spin(subscriber)
    subscriber.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()

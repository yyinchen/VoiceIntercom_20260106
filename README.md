# VoiceIntercom_20260106
基于QT C++区域网语音对话和实时视频监控

注意：C++实现了语音对讲和视频接收功能 视频发送端使
# # 用python实现
```python
# 创建socket连接
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
receiver_addr = ('192.168.2.66', 8200)  # 接收端的IP  135
send_video_queue = queue.Queue(maxsize=5)
send_video_lock = threading.Lock()

def on_send_video():
    while 1:
        with send_video_lock:
            if not send_video_queue.empty():
                frame = send_video_queue.get()
                send_video_stream(frame)

def send_video_stream(frame, width=960, height=540, quality=80):
    compressed = cv2.resize(frame, (width, height), interpolation=cv2.INTER_AREA)
    encode_param = [cv2.IMWRITE_JPEG_QUALITY, quality] 
    rgb_frame = cv2.cvtColor(compressed, cv2.COLOR_BGR2RGB)
    _, buffer = cv2.imencode('.jpg', rgb_frame, encode_param)
    data = buffer.tobytes()
    bytes_sent = sock.sendto(data, receiver_addr)
    if bytes_sent == len(data):
        print(f"✅ 发送成功: {bytes_sent}/{len(data)} 字节")
    elif bytes_sent > 0:
        print(f"⚠️ 部分发送: {bytes_sent}/{len(data)} 字节")
    else:
        print(f"❌ 发送失败: 0 字节")

if __name__ == '__main__':
    cap = cv2.VideoCapture(0)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 2560)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 1440)
    cap.set(cv2.CAP_PROP_FPS, 25)
    cap.set(cv2.CAP_PROP_FOCUS, 42)
    focus_value = cap.get(cv2.CAP_PROP_FOCUS)
    print(f"当前焦距设置: {focus_value}")
    t_send_video = threading.Thread(target=on_send_video)
    t_send_video.start()
    while True:
        ret, frame = cap.read()
        if not ret:
            break
        with send_video_lock:
            send_video_queue.put(frame)
        cv2.imshow(' ', _frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
    sock.close()
    cap.release()
    cv2.destroyAllWindows()
```
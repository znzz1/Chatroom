import socket
import threading
import time

SERVER = ('127.0.0.1', 12345)  # 服务器地址和端口
CONNECTIONS = 100              # 并发连接数，可根据需要调整
MESSAGES = 1000                # 每个连接发送消息数
MESSAGE = b'hello\n'           # 发送内容

def worker():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(SERVER)
    for _ in range(MESSAGES):
        s.sendall(MESSAGE)
        data = s.recv(4096)
        # 可以校验 data == MESSAGE
    s.close()

threads = []
start = time.time()
for _ in range(CONNECTIONS):
    t = threading.Thread(target=worker)
    t.start()
    threads.append(t)

for t in threads:
    t.join()
end = time.time()

total = CONNECTIONS * MESSAGES
print(f"总请求数: {total}")
print(f"总耗时: {end - start:.2f} 秒")
print(f"QPS: {total / (end - start):.2f}") 
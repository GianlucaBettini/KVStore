import socket
import threading
import time
import sys

HOST = '127.0.0.1'
PORT = 8080
NUM_CLIENTS = 10000        # Concurrent connections
CMDS_PER_CLIENT = 50   # Commands each client will send

success_count = 0
fail_count = 0
lock = threading.Lock()

def client_task(client_id):
    global success_count, fail_count
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((HOST, PORT))
        
        # 1. Pipeline generation: "SET key_id_1 val\n SET key_id_2 val\n"
        payload = ""
        for i in range(CMDS_PER_CLIENT):
            key = f"key_{client_id}_{i}"
            val = f"val_{client_id}_{i}"
            payload += f"SET {key} {val}\n"
        
        # Add GET commands to verify
        for i in range(CMDS_PER_CLIENT):
            key = f"key_{client_id}_{i}"
            payload += f"GET {key}\n"

        payload_bytes = payload.encode('utf-8')
        
        # 2. Blast it all at once (Triggering the ET Flush and Pipelining)
        s.sendall(payload_bytes)
        
        # 3. Read responses
        # We expect exactly CMDS_PER_CLIENT "OK\n" and CMDS_PER_CLIENT "val_id_X\n" per client
        expected_responses = CMDS_PER_CLIENT * 2
        responses_received = 0
        buffer = ""
        
        while responses_received < expected_responses:
            chunk = s.recv(8192).decode('utf-8')
            if not chunk:
                break
            buffer += chunk
            
            # Count how many complete responses (newlines) we got
            responses = buffer.split('\n')
            
            # The last element is either empty (if ended with \n) or an incomplete response
            complete_responses = len(responses) - 1 
            responses_received += complete_responses
            
            # Keep the remainder for the next loop
            buffer = responses[-1]

        s.close()
        
        with lock:
            if responses_received == expected_responses:
                success_count += 1
            else:
                fail_count += 1
                print(f"[!] Client {client_id}: Expected {expected_responses} responses, got {responses_received}")

    except Exception as e:
        with lock:
            fail_count += 1
            print(f"[!] Client {client_id} Error: {e}")

print(f"🚀 Starting KV Store Stress Test...")
print(f"📡 {NUM_CLIENTS} clients sending {CMDS_PER_CLIENT * 2} commands each (Total: {NUM_CLIENTS * CMDS_PER_CLIENT * 2} ops)")
start_time = time.time()

threads = []
for i in range(NUM_CLIENTS):
    t = threading.Thread(target=client_task, args=(i,))
    threads.append(t)
    t.start()

for t in threads:
    t.join()

duration = time.time() - start_time

print("-" * 30)
print(f"⏱️ Time: {duration:.2f} seconds")
print(f"✅ Success (Perfect I/O): {success_count}")
print(f"❌ Failed: {fail_count}")
print(f"⚡ Throughput: {int((NUM_CLIENTS * CMDS_PER_CLIENT * 2) / duration)} ops/sec")
print("-" * 30)

if fail_count > 0:
    sys.exit(1)
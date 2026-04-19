import socket
import subprocess
import time
import sys

print("Starting the server in background...")
server_process = subprocess.Popen(['./kvstore'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

# Half second to the OS to open the port 8080
time.sleep(0.5) 

try:
    print("Connecting to the server through TCP (port 8080)...")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', 8080))

    cmds = "SET user:1 mario\nGET user:1\nDEL user:1\nGET user:1\n"
    s.sendall(cmds.encode('utf-8'))
    time.sleep(0.1)

    response_bytes = s.recv(1024)
    output = response_bytes.decode('utf-8')

    print("--- Server response ---")
    print(output, end="")
    print("---------------------------")

    responses = [r.strip() for r in output.split('\n') if r.strip()]

    assert len(responses) >= 4, f"Test Failed: Expected 4 responses, received {len(responses)}"
    assert responses[0] == "OK", f"Test Failed: SET has returned '{responses[0]}'"
    assert responses[1] == "mario", f"Test Failed: GET has returned '{responses[1]}'"
    assert responses[2] == "Deleted", f"Test Failed: DEL has returned '{responses[2]}'"
    assert responses[3] == "Not found", f"Test Failed: GET after DEL has returned '{responses[3]}'"

    print("All tests passed!")

except ConnectionRefusedError:
    print("Error: Impossible to connect. Is the server executing on port 8080?")
    sys.exit(1)
finally:
    s.close()
    server_process.terminate()
    server_process.wait()
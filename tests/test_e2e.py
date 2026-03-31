import subprocess
import sys

cmds = "SET user:1 mario\nGET user:1\nDEL user:1\nGET user:1\n"

print("Running E2E tests...")

try:
    res = subprocess.run(
        ['./kvstore'], 
        input=cmds, 
        capture_output=True, 
        text=True, 
        check=True
    )
except FileNotFoundError:
    print("Error: Executable './kvstore' not found. Did you run 'make'?")
    sys.exit(1)

output = res.stdout

print("--- Server Output ---")
print(output)
print("---------------------")


# Split the string every time the prompt "kv> " appears.
# [1:] skips the empty string before the very first prompt.
responses = output.split("kv> ")[1:]

# Clean each response from extra spaces or newlines (\n)
responses = [r.strip() for r in responses]

assert len(responses) >= 4, f"Test Failed: Expected 4 responses, got {len(responses)}"

assert responses[0] == "OK", f"Test Failed: SET returned '{responses[0]}' instead of 'OK'"
assert responses[1] == "mario", f"Test Failed: GET returned '{responses[1]}' instead of 'mario'"
assert responses[2] == "Deleted", f"Test Failed: DEL returned '{responses[2]}' instead of 'Deleted'"
assert responses[3] == "Not found", f"Test Failed: GET after DEL returned '{responses[3]}' instead of 'Not found'"

print("All E2E tests passed successfully!")
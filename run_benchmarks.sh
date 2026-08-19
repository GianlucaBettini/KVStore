#!/bin/bash
ulimit -n 65535 2>/dev/null || echo "Warning: failed to raise ulimit. Benchmarks with 10k clients might fail."

make clean && make release

go build -o bin_crud test/benchmark/benchmark_crud.go
go build -o bin_pipelining test/benchmark/benchmark_pipelining.go
go build -o bin_pingpong test/benchmark/benchmark_pingpong.go

echo "=== Starting C-KV Benchmark Suite ==="

run_benchmark() {
    local binary_name=$1
    echo -e "\n>>> Execution: $binary_name"
    
    ./kvstore &
    SERVER_PID=$!
    
    # Wait 1 second to let the socket do the bind()
    sleep 1
    
    ./$binary_name
    
    # Send SIGTERM to test the graceful shutdown and to free the port
    kill -TERM $SERVER_PID
    wait $SERVER_PID 2>/dev/null
}

run_benchmark "bin_crud"
run_benchmark "bin_pipelining"
run_benchmark "bin_pingpong"

echo -e "\n=== Benchmark Completed ==="

rm -f bin_crud bin_pipelining bin_pingpong
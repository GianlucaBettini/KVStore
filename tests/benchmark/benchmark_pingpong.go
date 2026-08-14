package main

/*
 * SET/GET Ping-Pong Benchmark
 *
 * Simulates low-density traffic by sending a single command (SET/GET), waiting for the
 * response, and only then sending the next command. 
 * This tests the TCP latency and OS context-switching overhead.
 */

import (
	"encoding/binary"
	"fmt"
	"io"
	"net"
	"sync"
	"sync/atomic"
	"time"
)

const (
	CmdSet byte = 0
	CmdGet byte = 1

	StatusOk byte = 0
)

const (
	NumClients    = 10000 
	CmdsPerClient = 50    // 50 SET + 50 GET 
)

var (
	totalSuccess uint64
	totalFail    uint64
)

func main() {
	fmt.Printf("Load Generator in Go (PING-PONG MODE)...\n")
	fmt.Printf("%d clients | %d ops per client\n", NumClients, CmdsPerClient*2)
	fmt.Printf("total operations: %d\n", NumClients*CmdsPerClient*2)

	var wg sync.WaitGroup
	wg.Add(NumClients)

	startTime := time.Now()

	for i := 0; i < NumClients; i++ {
		go clientWorker(i, &wg)
	}

	wg.Wait()
	duration := time.Since(startTime).Seconds()

	totOps := totalSuccess + totalFail

	fmt.Println("------------------------------")
	fmt.Printf("Time: %.2f seconds\n", duration)
	fmt.Printf("Successes: %d\n", totalSuccess)
	fmt.Printf("Fails: %d\n", totalFail)
	if duration > 0 {
		fmt.Printf("Throughput: %d ops/sec\n", int(float64(totOps)/duration))
	}
	fmt.Println("------------------------------")
}

func clientWorker(clientID int, wg *sync.WaitGroup) {
	defer wg.Done()

	conn, err := net.DialTimeout("tcp", "127.0.0.1:8080", 10*time.Second)
	if err != nil {
		atomic.AddUint64(&totalFail, uint64(CmdsPerClient*2))
		return
	}
	defer conn.Close()

	successes := uint64(0)
	fails := uint64(0)
	headerBuffer := make([]byte, 4)

	for i := 0; i < CmdsPerClient; i++ {
		key := []byte(fmt.Sprintf("c%d_k%d", clientID, i))
		val := []byte(fmt.Sprintf("v%d", i))

		// --- 1. SET in ping-pong ---
		packetSet := buildPacket(CmdSet, key, val)
		if _, err := conn.Write(packetSet); err != nil {
			fails++
			continue
		}
		if readSingleResponse(conn, headerBuffer) {
			successes++
		} else {
			fails++
		}

		// --- 2. GET in ping-pong ---
		packetGet := buildPacket(CmdGet, key, nil)
		if _, err := conn.Write(packetGet); err != nil {
			fails++
			continue
		}
		if readSingleResponse(conn, headerBuffer) {
			successes++
		} else {
			fails++
		}
	}

	atomic.AddUint64(&totalSuccess, successes)
	atomic.AddUint64(&totalFail, fails)
}

func readSingleResponse(conn net.Conn, headerBuffer []byte) bool {
	if _, err := io.ReadFull(conn, headerBuffer); err != nil {
		return false
	}
	payloadSize := binary.BigEndian.Uint32(headerBuffer)
	payload := make([]byte, payloadSize)
	if _, err := io.ReadFull(conn, payload); err != nil {
		return false
	}
	return payload[0] == StatusOk
}

func buildPacket(cmd byte, key []byte, val []byte) []byte {
	keyLen := len(key)
	valLen := len(val)

	var payloadSize uint32
	if cmd == CmdSet {
		payloadSize = uint32(1 + 2 + keyLen + 2 + valLen)
	} else {
		payloadSize = uint32(1 + 2 + keyLen)
	}

	packet := make([]byte, 4+payloadSize)
	binary.BigEndian.PutUint32(packet[0:4], payloadSize)
	packet[4] = cmd
	binary.BigEndian.PutUint16(packet[5:7], uint16(keyLen))
	copy(packet[7:], key)

	if cmd == CmdSet {
		offset := 7 + keyLen
		binary.BigEndian.PutUint16(packet[offset:offset+2], uint16(valLen))
		copy(packet[offset+2:], val)
	}

	return packet
}
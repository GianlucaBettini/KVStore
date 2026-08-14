package main

/*
 * CRUD Pipelining Benchmark
 *
 * Simulates complex high-density traffic by packing a full CRUD lifecycle
 * (SET, GET, DEL, GET) into a single TCP pipeline for every cycle.
 * Tests memory allocation stability and Hash Table throughput under stress.
 *
 * Expected behavior: The server must process the pipeline and reply with
 * exactly [OK, OK, OK, NOT_FOUND] for each iteration.
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
	CmdDel byte = 2

	StatusOk         byte = 0
	StatusError      byte = 1
	StatusNotFound   byte = 2
	StatusBadRequest byte = 3
)

const (
	NumClients    = 10000 
	CmdsPerClient = 50    // 50 SET + 50 GET + 50 DEL + 50 GET
)

var (
	totalSuccess uint64
	totalFail    uint64
)

func main() {
	fmt.Printf("Load Generator in Go (PIPELINING-CRUD-MODE)...\n")
	fmt.Printf("%d clients | %d ops per client\n", NumClients, CmdsPerClient*4)
	fmt.Printf("total operations: %d\n", NumClients*CmdsPerClient*4)

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
	defer wg.Done() // Segnala al WaitGroup che abbiamo finito alla fine della funzione

	conn, err := net.Dial("tcp", "127.0.0.1:8080")
	if err != nil {
		atomic.AddUint64(&totalFail, uint64(CmdsPerClient*2))
		return
	}
	defer conn.Close()

	var pipelineReq []byte
	expectedResponses := CmdsPerClient * 4 

	for i := 0; i < CmdsPerClient; i++ {
		key := []byte(fmt.Sprintf("c%d_k%d", clientID, i))
		val := []byte(fmt.Sprintf("v%d", i))

		pipelineReq = append(pipelineReq, buildPacket(CmdSet, key, val)...)
		pipelineReq = append(pipelineReq, buildPacket(CmdGet, key, nil)...)
		pipelineReq = append(pipelineReq, buildPacket(CmdDel, key, nil)...)
		pipelineReq = append(pipelineReq, buildPacket(CmdGet, key, nil)...) // Not Found
	}

	if _, err = conn.Write(pipelineReq); err != nil {
		atomic.AddUint64(&totalFail, uint64(expectedResponses))
		return
	}

	successes := uint64(0)
	fails := uint64(0)
	header := make([]byte, 4)

	// Expected replies for each iteration: OK, OK, OK, NOT_FOUND
	expectedStatuses := []byte{StatusOk, StatusOk, StatusOk, StatusNotFound}

	for i := 0; i < expectedResponses; i++ {
		if _, err := io.ReadFull(conn, header); err != nil {
			fails += uint64(expectedResponses - i)
			break
		}

		payloadSize := binary.BigEndian.Uint32(header)
		payload := make([]byte, payloadSize)
		if _, err := io.ReadFull(conn, payload); err != nil {
			fails += uint64(expectedResponses - i)
			break
		}

		status := payload[0]
		expectedStatus := expectedStatuses[i%4] 

		if status == expectedStatus {
			successes++
		} else {
			fails++
		}
	}

	atomic.AddUint64(&totalSuccess, successes)
	atomic.AddUint64(&totalFail, fails)
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
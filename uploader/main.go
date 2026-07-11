package main

import (
	"bufio"
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net"
	"os"
	"time"
)

type Message struct {
	Version   int            `json:"version"`
	Type      string         `json:"type"`
	MessageID string         `json:"message_id,omitempty"`
	Timestamp int64          `json:"timestamp,omitempty"`
	Topic     string         `json:"topic,omitempty"`
	QoS       int            `json:"qos,omitempty"`
	ClientID  string         `json:"client_id"`
	Payload   map[string]any `json:"payload,omitempty"`
}

const (
	defaultSocketPath = "/tmp/ipc_broker.sock"
	defaultTopic      = "analytics.alerts"
	defaultMockCloud  = "http://127.0.0.1:8081/mock-cloud/upload"
)

func getEnvOrDefault(key, defaultValue string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return defaultValue
}

func sendFrame(conn net.Conn, msg Message) error {
	encoded, err := json.Marshal(msg)
	if err != nil {
		return fmt.Errorf("marshal message: %w", err)
	}

	fmt.Printf("[uploader-go] outbound frame: %s\n", string(encoded))

	if _, err := conn.Write(append(encoded, '\n')); err != nil {
		return fmt.Errorf("write frame: %w", err)
	}

	return nil
}

func subscribe(conn net.Conn) error {
	sub := Message{
		Version:  1,
		Type:     "subscribe",
		Topic:    defaultTopic,
		ClientID: "uploader-go",
	}

	if err := sendFrame(conn, sub); err != nil {
		return err
	}
	fmt.Printf("[uploader-go] subscribed topic=%s\n", defaultTopic)
	return nil
}

func sendToMockCloud(client *http.Client, cloudURL string, payload map[string]any) error {
	body, err := json.Marshal(payload)
	if err != nil {
		return fmt.Errorf("encode cloud payload: %w", err)
	}

	req, err := http.NewRequest(http.MethodPost, cloudURL, bytes.NewReader(body))
	if err != nil {
		return fmt.Errorf("create cloud request: %w", err)
	}
	req.Header.Set("Content-Type", "application/json")

	resp, err := client.Do(req)
	if err != nil {
		return fmt.Errorf("mock cloud call failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		responseBody, _ := io.ReadAll(io.LimitReader(resp.Body, 1024))
		return fmt.Errorf("mock cloud status=%d body=%s", resp.StatusCode, string(responseBody))
	}

	return nil
}

func sendAck(conn net.Conn, msg Message) error {
	ack := Message{
		Version:   1,
		Type:      "ack",
		MessageID: msg.MessageID,
		Topic:     msg.Topic,
		ClientID:  "uploader-go",
	}
	return sendFrame(conn, ack)
}

func processIncoming(conn net.Conn, msg Message, cloudClient *http.Client, cloudURL string) {
	switch msg.Type {
	case "ack":
		fmt.Printf("[uploader-go] ack received id=%s topic=%s from=%s\n", msg.MessageID, msg.Topic, msg.ClientID)

	case "publish", "deliver":
		if msg.Topic != defaultTopic {
			fmt.Printf("[uploader-go] ignoring message on unexpected topic=%s\n", msg.Topic)
			return
		}

		if err := sendToMockCloud(cloudClient, cloudURL, msg.Payload); err != nil {
			fmt.Printf("[uploader-go] cloud upload failed id=%s err=%v\n", msg.MessageID, err)
			return
		}

		fmt.Printf("[uploader-go] cloud upload success id=%s\n", msg.MessageID)
		if err := sendAck(conn, msg); err != nil {
			fmt.Printf("[uploader-go] failed to send ack id=%s err=%v\n", msg.MessageID, err)
			return
		}
		fmt.Printf("[uploader-go] ack sent id=%s\n", msg.MessageID)

	default:
		fmt.Printf("[uploader-go] ignored message type=%s id=%s\n", msg.Type, msg.MessageID)
	}
}

func runSession(conn net.Conn, cloudURL string) error {
	if err := subscribe(conn); err != nil {
		return err
	}

	scanner := bufio.NewScanner(conn)
	scanner.Buffer(make([]byte, 1024), 1024*1024)
	cloudClient := &http.Client{Timeout: 3 * time.Second}

	for scanner.Scan() {
		line := scanner.Bytes()
		fmt.Printf("[uploader-go] inbound frame: %s\n", string(line))

		var msg Message
		if err := json.Unmarshal(line, &msg); err != nil {
			fmt.Printf("[uploader-go] invalid frame: %v\n", err)
			continue
		}

		processIncoming(conn, msg, cloudClient, cloudURL)
	}

	if err := scanner.Err(); err != nil {
		return fmt.Errorf("read stream: %w", err)
	}

	return nil
}

func main() {
	socketPath := getEnvOrDefault("BROKER_SOCKET", defaultSocketPath)
	cloudURL := getEnvOrDefault("MOCK_CLOUD_URL", defaultMockCloud)

	fmt.Printf("[uploader-go] starting with socket=%s cloud=%s\n", socketPath, cloudURL)

	for {
		conn, err := net.Dial("unix", socketPath)
		if err != nil {
			fmt.Printf("[uploader-go] connect failed: %v\n", err)
			time.Sleep(500 * time.Millisecond)
			continue
		}

		if err := runSession(conn, cloudURL); err != nil {
			fmt.Printf("[uploader-go] session ended with error: %v\n", err)
		} else {
			fmt.Printf("[uploader-go] session ended, reconnecting\n")
		}

		_ = conn.Close()
		time.Sleep(250 * time.Millisecond)
	}
}

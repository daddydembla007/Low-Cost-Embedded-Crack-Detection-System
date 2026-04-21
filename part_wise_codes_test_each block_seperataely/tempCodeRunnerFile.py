import cv2
import serial
import numpy as np
import time

PORT   = 'COM6'
BAUD   = 500000
WIDTH  = 240
HEIGHT = 320
HEADER = bytes([0xAB, 0xCD, 0xAB, 0xCD])

def rgb_to_rgb565(frame_bgr):
    r = (frame_bgr[:, :, 2].astype(np.uint16) >> 3) & 0x1F
    g = (frame_bgr[:, :, 1].astype(np.uint16) >> 2) & 0x3F
    b = (frame_bgr[:, :, 0].astype(np.uint16) >> 3) & 0x1F
    return ((r << 11) | (g << 5) | b).astype('>u2').tobytes()

print("Opening serial port...")
ser = serial.Serial(PORT, BAUD, timeout=3)
time.sleep(2)

print("Waiting for Arduino ready signal...")
ser.reset_input_buffer()
ready = ser.read(1)
if ready != b'R':
    print(f"Warning: expected 'R', got {ready}. Continuing anyway...")
else:
    print("Arduino ready!")

# ── Change 1: smaller capture resolution = less resize work ──
cap = cv2.VideoCapture(0, cv2.CAP_DSHOW)   # CAP_DSHOW faster on Windows
cap.set(cv2.CAP_PROP_FRAME_WIDTH,  320)    # capture at 320x240 directly
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 240)    # instead of 640x480 then resize
cap.set(cv2.CAP_PROP_FPS,          30)
cap.set(cv2.CAP_PROP_BUFFERSIZE,   1)      # ── Change 2: always get latest frame

frame_count = 0
t_start     = time.time()

print("Streaming... Press Ctrl+C to stop.\n")
try:
    while True:
        t0 = time.time()

        # ── Change 3: drain stale frames from webcam buffer ──
        cap.grab()
        ret, frame = cap.retrieve()
        if not ret:
            ret, frame = cap.read()
        if not ret:
            print("Webcam read failed, retrying...")
            continue

        frame   = cv2.resize(frame, (WIDTH, HEIGHT),
                             interpolation=cv2.INTER_LINEAR)
        frame   = cv2.flip(frame, 1)
        payload = rgb_to_rgb565(frame)

        ser.write(HEADER + payload)
        ser.flush()

        ack = ser.read(1)
        if ack != b'K':
            print(f"\nNo ACK received (got {ack}), flushing buffers...")
            ser.reset_input_buffer()
            ser.reset_output_buffer()
            continue

        frame_count += 1
        fps     = frame_count / (time.time() - t_start)
        elapsed = time.time() - t0
        print(f"\rFPS: {fps:5.2f}  frame {frame_count}  ({elapsed*1000:.0f}ms/frame)", end='')

except KeyboardInterrupt:
    print("\nStopped.")
finally:
    cap.release()
    ser.close()
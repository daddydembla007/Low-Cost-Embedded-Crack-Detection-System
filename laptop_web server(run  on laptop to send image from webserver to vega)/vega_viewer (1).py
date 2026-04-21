import cv2
import serial
import numpy as np
import time
import os

PORT   = 'COM6'
BAUD   = 115200 # Lowered to prevent the collage effect!
WIDTH  = 240
HEIGHT = 320
HEADER = bytes([0xAB, 0xCD, 0xAB, 0xCD])

script_dir = os.path.dirname(os.path.abspath(__file__))
# Pointing directly to the single file we want to read
LATEST_IMAGE = os.path.join(script_dir, 'detections', 'latest.jpg')

def rgb_to_rgb565(frame_bgr):
    r = (frame_bgr[:, :, 2].astype(np.uint16) >> 3) & 0x1F
    g = (frame_bgr[:, :, 1].astype(np.uint16) >> 2) & 0x3F
    b = (frame_bgr[:, :, 0].astype(np.uint16) >> 3) & 0x1F
    return ((r << 11) | (g << 5) | b).astype('>u2').tobytes()

print("Opening serial port...")
ser = serial.Serial(PORT, BAUD, timeout=3)
time.sleep(2)

print("Waiting for Vega board ready signal...")
ser.reset_input_buffer()
ready = ser.read(1)
if ready != b'R':
    print(f"Warning: expected 'R', got {ready}. Continuing anyway...")
else:
    print("Vega board ready!")

print("Watching for updates to latest.jpg... Press Ctrl+C to stop.\n")

last_modified_time = 0

try:
    while True:
        # Check if the file exists yet
        if not os.path.exists(LATEST_IMAGE):
            time.sleep(0.5)
            continue

        # Check when the file was last updated
        current_modified_time = os.path.getmtime(LATEST_IMAGE)

        # If the file has been updated since we last checked, send it!
        if current_modified_time != last_modified_time:
            t0 = time.time()

            frame = cv2.imread(LATEST_IMAGE)
            
            # If the image couldn't be read, skip and try again next loop
            if frame is None:
                time.sleep(0.1)
                continue

            frame = cv2.resize(frame, (WIDTH, HEIGHT), interpolation=cv2.INTER_LINEAR)
            payload = rgb_to_rgb565(frame)

            ser.reset_input_buffer()
            ser.reset_output_buffer()

            # Send data
            ser.write(HEADER)
            chunk_size = 4096 
            for i in range(0, len(payload), chunk_size):
                ser.write(payload[i:i+chunk_size])
            ser.flush()

            # Wait for ACK
            ack = ser.read(1)
            if ack != b'K':
                print("\nNo ACK received, flushing buffers...")
                ser.reset_input_buffer()
                ser.reset_output_buffer()
                continue

            # Update our tracker so we don't send the same image twice
            last_modified_time = current_modified_time

            elapsed = time.time() - t0
            print(f"\r✅ New image detected and sent to display ({elapsed*1000:.0f}ms)".ljust(60), end='')

        # Small pause so we don't max out the CPU checking the file
        time.sleep(0.2)

except KeyboardInterrupt:
    print("\n\nStopped by user.")
finally:
    ser.close()
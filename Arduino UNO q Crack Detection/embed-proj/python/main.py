import cv2
import numpy as np
import time
import threading
import requests
from arduino.app_utils import Bridge, App

# --- 1. CONFIGURATION ---
LAPTOP_IP = "10.7.24.90" 
SERVER_URL = f"http://{LAPTOP_IP}:5000/upload"

# --- 2. GLOBAL STATE ---
current_base_image = None
all_cracks = []     
reveal_index = 0    

# --- 3. CAMERA INITIALIZATION ---
cap = None
for index in [0, 1, 2, 3]:
    cap = cv2.VideoCapture(index)
    if cap.isOpened():
        print(f"CAMERA: Locked onto index {index}")
        break
    cap.release()

# --- 4. THE SCANNER ---
def scan_all_cracks(img):
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    rows, cols = gray.shape
    
    # 4.1 FFT Processing
    dft = cv2.dft(np.float32(gray), flags=cv2.DFT_COMPLEX_OUTPUT)
    dft_shift = np.fft.fftshift(dft)
    mask = np.ones((rows, cols, 2), np.uint8)
    crow, ccol = rows // 2, cols // 2
    mask[crow-35:crow+35, ccol-35:ccol+35] = 0
    fshift = dft_shift * mask
    img_back = cv2.magnitude(*cv2.split(cv2.idft(np.fft.ifftshift(fshift))))
    
    # 4.2 Filtering & Thresholding
    enhanced = cv2.normalize(img_back, None, 0, 255, cv2.NORM_MINMAX).astype(np.uint8)
    blurred = cv2.GaussianBlur(enhanced, (7,7), 0)
    _, thresh = cv2.threshold(blurred, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
    
    # Merge discrete regions
    kernel = np.ones((15,15), np.uint8) 
    thresh = cv2.morphologyEx(thresh, cv2.MORPH_CLOSE, kernel)
    
    contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

    found_points = []
    EDGE_BUFFER = 25
    for cnt in contours:
        if cv2.arcLength(cnt, True) > 80:
            M = cv2.moments(cnt)
            if M["m00"] != 0:
                cx, cy = int(M["m10"]/M["m00"]), int(M["m01"]/M["m00"])
                if (cx > EDGE_BUFFER and cx < cols - EDGE_BUFFER and 
                    cy > EDGE_BUFFER and cy < rows - EDGE_BUFFER):
                    found_points.append((cx, cy))

    # --- 4.3 SORTING: Bottom (L->R) then Top (L->R) ---
    bottom_half = sorted([p for p in found_points if p[1] > rows // 2], key=lambda p: p[0])
    top_half = sorted([p for p in found_points if p[1] <= rows // 2], key=lambda p: p[0])

    return bottom_half + top_half

def draw_markers(base_img, crack_list, count):
    temp_img = base_img.copy()
    # If count is 0, this loop won't run, leaving the image raw.
    for i in range(min(count, len(crack_list))):
        pos = crack_list[i]
        cv2.circle(temp_img, pos, 35, (0, 0, 255), -1)
        cv2.putText(temp_img, str(i+1), (pos[0]-12, pos[1]+10), 
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255, 255, 255), 2)
    return temp_img

# --- 5. PROGRESSIVE HARDWARE LOOP ---
def main_hardware_loop():
    global current_base_image, all_cracks, reveal_index
    print("\n--- SYSTEM READY: Press button to capture Raw Image ---")
    
    while True:
        time.sleep(0.05)
        if Bridge.call("check_button"):
            
            # --- TRANSITION 1: NEW CAPTURE (Click 1, or after all cracks revealed) ---
            # If we've shown all cracks (e.g., reveal_index 5 for 5 cracks), 
            # the next click triggers a reset.
            if current_base_image is None or reveal_index >= len(all_cracks):
                print("\n[CLICK] Action: CAPTURE -> Taking New Raw Image...")
                for _ in range(5): cap.read() 
                ret, frame = cap.read()
                if not ret: 
                    print("Camera Error!"); continue
                
                current_base_image = frame
                all_cracks = scan_all_cracks(frame)
                reveal_index = 0  # <--- KEY CHANGE: Start at 0 for Raw Image
                print(f"SCAN COMPLETE: Found {len(all_cracks)} cracks. Sending Raw Image.")
            
            # --- TRANSITION 2: REVEAL NEXT MARKER ---
            else:
                reveal_index += 1
                print(f"[CLICK] Action: REVEAL -> Showing Marker {reveal_index} of {len(all_cracks)}")

            # --- EXECUTION: DRAW & UPLOAD ---
            display_img = draw_markers(current_base_image, all_cracks, reveal_index)
            
            # Hardware Interrupts
            Bridge.call("trigger_stm32")
            Bridge.call("set_led_state", True)
            time.sleep(0.1)
            Bridge.call("set_led_state", False)
            
            # Send to Server
            try:
                img_resized = cv2.resize(display_img, (240, 320))
                _, buf = cv2.imencode('.jpg', img_resized)
                requests.post(SERVER_URL, data=buf.tobytes(), timeout=2)
                print(f"SUCCESS: Image (Markers: {reveal_index}) sent.")
            except:
                print("ERROR: Transmission failed.")

threading.Thread(target=main_hardware_loop, daemon=True).start()
App.run()
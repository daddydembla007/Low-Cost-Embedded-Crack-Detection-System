import cv2
import numpy as np

def detect_cracks_refined(image_path):
    img = cv2.imread(image_path)
    if img is None:
        print("Error: Image not found.")
        return None, []

    # 1. Grayscale
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

    # 2. FFT High-pass filter
    dft = cv2.dft(np.float32(gray), flags=cv2.DFT_COMPLEX_OUTPUT)
    dft_shift = np.fft.fftshift(dft)
    
    rows, cols = gray.shape
    crow, ccol = rows // 2, cols // 2

    mask = np.ones((rows, cols, 2), np.uint8)
    r = 40
    mask[crow-r:crow+r, ccol-r:ccol+r] = 0

    fshift = dft_shift * mask
    f_ishift = np.fft.ifftshift(fshift)

    img_back = cv2.idft(f_ishift)
    img_back = cv2.magnitude(img_back[:,:,0], img_back[:,:,1])

    fft_enhanced = cv2.normalize(img_back, None, 0, 255, cv2.NORM_MINMAX).astype(np.uint8)

    # 3. Blur
    blurred = cv2.GaussianBlur(fft_enhanced, (5, 5), 0)

    # 4. Sobel Edge Detection
    sobelx = cv2.Sobel(blurred, cv2.CV_64F, 1, 0, ksize=3)
    sobely = cv2.Sobel(blurred, cv2.CV_64F, 0, 1, ksize=3)
    sobel_combined = cv2.magnitude(sobelx, sobely)

    sobel_8u = np.uint8(cv2.normalize(sobel_combined, None, 0, 255, cv2.NORM_MINMAX))

    # 5. Threshold
    _, thresh = cv2.threshold(sobel_8u, 60, 255, cv2.THRESH_BINARY)

    # 6. Remove border noise
    margin = 15
    thresh[0:margin, :] = 0
    thresh[-margin:, :] = 0
    thresh[:, 0:margin] = 0
    thresh[:, -margin:] = 0

    # 7. Morphological closing
    kernel = np.ones((9, 9), np.uint8)
    closing = cv2.morphologyEx(thresh, cv2.MORPH_CLOSE, kernel)

    # 8. Find contours
    contours, _ = cv2.findContours(closing, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

    output_img = img.copy()
    crack_pixels = []

    # 9. Filter valid cracks
    for cnt in contours:
        perimeter = cv2.arcLength(cnt, True)

        if perimeter > 100:
            cv2.drawContours(output_img, [cnt], -1, (255, 0, 0), 2)

            M = cv2.moments(cnt)
            if M["m00"] != 0:
                cx = int(M["m10"] / M["m00"])
                cy = int(M["m01"] / M["m00"])
                crack_pixels.append((cx, cy))

    # 🚨 No cracks detected
    if len(crack_pixels) == 0:
        return None, []

    # 10. Compute global center
    avg_x = int(np.mean([p[0] for p in crack_pixels]))
    avg_y = int(np.mean([p[1] for p in crack_pixels]))

    offset = 15
    cv2.line(output_img, (avg_x - offset, avg_y), (avg_x + offset, avg_y), (0, 255, 0), 3)
    cv2.line(output_img, (avg_x, avg_y - offset), (avg_x, avg_y + offset), (0, 255, 0), 3)

    cv2.putText(output_img, f"Center ({avg_x},{avg_y})",
                (avg_x + 20, avg_y - 20),
                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

    return output_img, [(avg_x, avg_y)]


# --- MAIN EXECUTION ---
image_path = 'Test_Crack1.png'

result_view, pixel_coords = detect_cracks_refined(image_path)

if len(pixel_coords) == 0:
    print("No cracks found")
else:
    print(f"Final Validated Crack Count: {len(pixel_coords)}")
    print(f"Coordinates: {pixel_coords}")

    cv2.imshow('Final Detection Logic', result_view)
    cv2.waitKey(0)
    cv2.destroyAllWindows()
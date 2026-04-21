from flask import Flask, request
import os

app = Flask(__name__)

SAVE_DIR = r"C:\Offline\web server vega board\detections"
LATEST_IMAGE = os.path.join(SAVE_DIR, "latest.jpg")
TEMP_IMAGE = os.path.join(SAVE_DIR, "temp.jpg")

if not os.path.exists(SAVE_DIR):
    os.makedirs(SAVE_DIR)

@app.route('/upload', methods=['POST'])
def upload():
    try:
        # 1. Check if the client sent standard form-data (e.g., Postman, HTML forms)
        if request.files:
            # Grab the first file in the request, regardless of its key name
            file_obj = next(iter(request.files.values())) 
            file_obj.save(TEMP_IMAGE)
            print("Received image via multipart/form-data.")

        # 2. Check if the client sent raw binary data in the body
        elif request.data:
            with open(TEMP_IMAGE, "wb") as f:
                f.write(request.data)
            print("Received image via raw binary payload.")

        # 3. If neither is present, the request was empty
        else:
            print("Error: No data or files received in the request!")
            return "No data received", 400

        # Atomically replace the old image
        os.replace(TEMP_IMAGE, LATEST_IMAGE)
        
        # Verify the file actually has content
        file_size = os.path.getsize(LATEST_IMAGE)
        print(f"Success! latest.jpg updated. File size: {file_size} bytes")
        
        if file_size == 0:
            print("WARNING: The saved image is 0 bytes. The client sent an empty payload.")

        return "OK", 200

    except Exception as e:
        print(f"Server Error during upload: {e}")
        return "Internal Server Error", 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
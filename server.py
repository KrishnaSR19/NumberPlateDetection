
from flask import Flask, request, jsonify
import cv2
import numpy as np
from ultralytics import YOLO
import easyocr

# Load YOLOv8 Model (ensure the path is correct)
model = YOLO(r"C:\Users\krish\runs\detect\train5\weights\best.pt")

# Initialize EasyOCR Reader
reader = easyocr.Reader(['en'])

# Initialize Flask App
app = Flask(__name__)

@app.route('/upload', methods=['POST'])
def upload_image():
    try:
        # Receive image from ESP32
        image_data = request.data
        image_np = np.frombuffer(image_data, np.uint8)
        img = cv2.imdecode(image_np, cv2.IMREAD_COLOR)

        if img is None:
            return jsonify({"error": "Invalid image data"}), 400

        # Save the raw received image for debugging
        cv2.imwrite("received.jpg", img)

        # Convert image to RGB (YOLOv8 expects RGB images)
        rgb_img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)

        # Lower the confidence threshold to increase detection sensitivity (adjust if needed)
        results = model(rgb_img, conf=0.1)
        detections = results[0].boxes.data.cpu().numpy() if results[0].boxes.data is not None else []

        # If no plate detected, save debug image and return error
        if len(detections) == 0:
            cv2.imwrite("debug_no_detection.jpg", img)
            return jsonify({"error": "No number plate detected"}), 400

        # Draw bounding boxes on a copy of the original image for debugging
        debug_img = img.copy()
        for box in detections:
            x_min, y_min, x_max, y_max = map(int, box[:4])
            cv2.rectangle(debug_img, (x_min, y_min), (x_max, y_max), (0, 255, 0), 2)
        cv2.imwrite("debug_detection.jpg", debug_img)

        # Extract first detected plate bounding box
        x_min, y_min, x_max, y_max = map(int, detections[0][:4])
        plate_img = rgb_img[y_min:y_max, x_min:x_max]

        # Optional: Save cropped plate image for debugging
        cv2.imwrite("cropped_plate.jpg", cv2.cvtColor(plate_img, cv2.COLOR_RGB2BGR))

        # Perform OCR on the cropped plate region
        ocr_result = reader.readtext(plate_img, detail=0)
        plate_number = "".join(ocr_result).replace(" ", "")

        return jsonify({"plate_number": plate_number}), 200

    except Exception as e:
        return jsonify({"error": str(e)}), 500

if __name__ == '__main__':
    app.run(host="0.0.0.0", port=5000, debug=True)

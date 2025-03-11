import cv2
import dlib
from scipy.spatial import distance

# Initialization (Run only once)
def initialize():
    global cap, face_detector, dlib_facelandmark
    cap = cv2.VideoCapture(0)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 320)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 240)
    face_detector = dlib.get_frontal_face_detector()
    predictor_path = "./shape_predictor_68_face_landmarks.dat"
    dlib_facelandmark = dlib.shape_predictor(predictor_path)
    print("Initialization completed.")

# Eye aspect ratio function
def Detect_Eye(eye):
    poi_A = distance.euclidean(eye[1], eye[5])
    poi_B = distance.euclidean(eye[2], eye[4])
    poi_C = distance.euclidean(eye[0], eye[3])
    return (poi_A + poi_B) / (2 * poi_C)

# Detection logic (Call repeatedly)
def detect_drowsiness():
    print("detecting drowsiness")
    ret, frame = cap.read()
    if not ret:
        print("Failed to capture frame.")
        return
    
    gray_scale = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    faces = face_detector(gray_scale, 0)
    print("num_faces detected:", len(faces))

    for face in faces:
        landmarks = dlib_facelandmark(gray_scale, face)
        leftEye = [(landmarks.part(n).x, landmarks.part(n).y) for n in range(36, 42)]
        rightEye = [(landmarks.part(n).x, landmarks.part(n).y) for n in range(42, 48)]

        eye_ratio = (Detect_Eye(leftEye) + Detect_Eye(rightEye)) / 2
        eye_ratio = round(eye_ratio, 2)

        return eye_ratio

# Cleanup
def cleanup():
    cap.release()
    print("Camera released.")


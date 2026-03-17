import cv2
import mediapipe as mp

def main():
    # Inicializar MediaPipe
    mp_pose = mp.solutions.pose
    mp_drawing = mp.solutions.drawing_utils
    pose = mp_pose.Pose(
        min_detection_confidence=0.5,
        min_tracking_confidence=0.5
    )

    # Iniciar captura de video
    cap = cv2.VideoCapture(0)
    
    # Variables de estado
    repeticiones = 0
    estado_movimiento = "ABAJO"

    print("Presiona 'q' para salir, 'r' para reiniciar contador.")

    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            break

        # Convertir a RGB (MediaPipe usa RGB)
        image = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        image.flags.writeable = False
        
        # Procesar la imagen
        results = pose.process(image)
        
        # Volver a BGR para OpenCV
        image.flags.writeable = True
        image = cv2.cvtColor(image, cv2.RGB2BGR)

        if results.pose_landmarks:
            # Dibujar landmarks
            mp_drawing.draw_landmarks(
                image, 
                results.pose_landmarks, 
                mp_pose.POSE_CONNECTIONS
            )

            landmarks = results.pose_landmarks.landmark
            
            # Obtener Y (0 es arriba, 1 es abajo)
            nariz_y = landmarks[mp_pose.PoseLandmark.NOSE.value].y
            muneca_izq_y = landmarks[mp_pose.PoseLandmark.LEFT_WRIST.value].y
            muneca_der_y = landmarks[mp_pose.PoseLandmark.RIGHT_WRIST.value].y
            cadera_izq_y = landmarks[mp_pose.PoseLandmark.LEFT_HIP.value].y
            cadera_der_y = landmarks[mp_pose.PoseLandmark.RIGHT_HIP.value].y

            # Lógica de detección (Máquina de estados)
            manos_arriba = (muneca_izq_y < nariz_y) and (muneca_der_y < nariz_y)
            manos_abajo = (muneca_izq_y > cadera_izq_y) and (muneca_der_y > cadera_der_y)

            if manos_arriba:
                estado_movimiento = "ARRIBA"
            elif manos_abajo and estado_movimiento == "ARRIBA":
                repeticiones += 1
                estado_movimiento = "ABAJO"

        # Feedback en pantalla
        cv2.putText(image, f'Repeticiones: {repeticiones}', (20, 50), 
                    cv2.FONT_HERSHEY_SIMPLEX, 1.5, (0, 255, 0), 3)
        cv2.putText(image, f'Estado: {estado_movimiento}', (20, 100), 
                    cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 0, 0), 2)

        cv2.imshow('SkiMotion Counter (Python IoT)', image)

        # Controles
        key = cv2.waitKey(10) & 0xFF
        if key == ord('q'):
            break
        elif key == ord('r'):
            repeticiones = 0
            estado_movimiento = "ABAJO"

    cap.release()
    cv2.destroyAllWindows()

if __name__ == '__main__':
    main()
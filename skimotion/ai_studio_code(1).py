import cv2
import mediapipe as mp
import paho.mqtt.client as mqtt
import time

# ==========================================
# ⚙️ APARTADO DE CONFIGURACIÓN
# ==========================================

# --- Configuración MQTT ---
MQTT_BROKER = "192.168.1.100"       # Reemplaza con la IP de tu servidor Mosquitto
MQTT_PORT = 1883                    # Puerto por defecto de MQTT
MQTT_TOPIC = "carrera/pista1/avanzar" # El canal (topic) al que se enviará el "1"

# Si tu servidor Mosquitto requiere usuario y contraseña, descomenta y usa esto:
# MQTT_USER = "tu_usuario"
# MQTT_PASS = "tu_contraseña"

# --- Configuración de Visión ---
MOSTRAR_ESQUELETO = False           # Cambia a True si quieres ver las líneas del cuerpo
INDICE_CAMARA = 0                   # 0 es tu cámara web por defecto

# ==========================================

def setup_mqtt():
    """Inicializa y conecta el cliente MQTT."""
    # Usamos la API de versión más reciente recomendada por Paho
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2) 
    
    # client.username_pw_set(MQTT_USER, MQTT_PASS) # Descomentar si usas credenciales
    
    try:
        print(f"[*] Conectando al Broker MQTT en {MQTT_BROKER}:{MQTT_PORT}...")
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_start() # Inicia un hilo en segundo plano para manejar la red
        print("[+] Conexión MQTT exitosa.")
        return client
    except Exception as e:
        print(f"[-] Error conectando a MQTT: {e}")
        return None

def main():
    # 1. Iniciar MQTT
    mqtt_client = setup_mqtt()

    # 2. Inicializar MediaPipe Pose
    mp_pose = mp.solutions.pose
    mp_drawing = mp.solutions.drawing_utils
    
    # 3. Iniciar la Cámara
    cap = cv2.VideoCapture(INDICE_CAMARA)
    
    # Variables de estado del movimiento
    repeticiones = 0
    estado_movimiento = 'ABAJO'
    
    print("[*] Iniciando cámara. Presiona 'Q' para salir.")

    with mp_pose.Pose(min_detection_confidence=0.5, min_tracking_confidence=0.5) as pose:
        while cap.isOpened():
            ret, frame = cap.read()
            if not ret:
                print("[-] No se pudo leer la cámara.")
                break

            # Convertir imagen a RGB (MediaPipe lo requiere)
            image_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            image_rgb.flags.writeable = False
            
            # Procesar la pose
            results = pose.process(image_rgb)
            
            # Volver a BGR para mostrar con OpenCV
            image_rgb.flags.writeable = True
            image_bgr = cv2.cvtColor(image_rgb, cv2.COLOR_RGB2BGR)

            if results.pose_landmarks:
                # --- OPCIÓN DE QUITAR O PONER EL ESQUELETO ---
                if MOSTRAR_ESQUELETO:
                    mp_drawing.draw_landmarks(
                        image_bgr, 
                        results.pose_landmarks, 
                        mp_pose.POSE_CONNECTIONS,
                        mp_drawing.DrawingSpec(color=(0,0,255), thickness=2, circle_radius=2),
                        mp_drawing.DrawingSpec(color=(255,255,255), thickness=4, circle_radius=2)
                    )

                # Extraer coordenadas de los puntos (landmarks)
                landmarks = results.pose_landmarks.landmark
                
                # Índices de MediaPipe
                # 11: Hombro Izq, 12: Hombro Der, 15: Muñeca Izq, 16: Muñeca Der, 23: Cadera Izq, 24: Cadera Der
                muneca_izq_y = landmarks[15].y
                muneca_der_y = landmarks[16].y
                hombro_izq_y = landmarks[11].y
                hombro_der_y = landmarks[12].y
                cadera_izq_y = landmarks[23].y
                cadera_der_y = landmarks[24].y

                # Mitad del torso adaptativo
                mitad_torso_izq_y = (hombro_izq_y + cadera_izq_y) / 2
                mitad_torso_der_y = (hombro_der_y + cadera_der_y) / 2

                # Lógica de movimiento (en MediaPipe, Y=0 es arriba y Y=1 es abajo)
                manos_arriba = (muneca_izq_y < hombro_izq_y) and (muneca_der_y < hombro_der_y)
                manos_abajo = (muneca_izq_y > mitad_torso_izq_y) and (muneca_der_y > mitad_torso_der_y)

                # Máquina de estados
                if manos_arriba:
                    if estado_movimiento != 'ARRIBA':
                        estado_movimiento = 'ARRIBA'
                        
                elif manos_abajo:
                    if estado_movimiento == 'ARRIBA':
                        estado_movimiento = 'ABAJO'
                        repeticiones += 1
                        
                        # ==========================================
                        # 🚀 ENVIAR ACCIÓN MQTT PARA LA CARRERA LED
                        # ==========================================
                        if mqtt_client:
                            mqtt_client.publish(MQTT_TOPIC, "1")
                            print(f"⚡ Movimiento detectado! Repetición: {repeticiones} -> Enviando '1' a '{MQTT_TOPIC}'")

            # Interfaz gráfica sencilla sobre el video
            cv2.rectangle(image_bgr, (0, 0), (250, 80), (30, 30, 30), -1)
            cv2.putText(image_bgr, f"Reps: {repeticiones}", (15, 55), 
                        cv2.FONT_HERSHEY_SIMPLEX, 1.5, (0, 255, 0), 3)

            # Mostrar video
            cv2.imshow('SkiMotion Counter Pro - Python', image_bgr)

            # Salir si se presiona la tecla 'q'
            if cv2.waitKey(10) & 0xFF == ord('q'):
                break

    # Limpieza al salir
    cap.release()
    cv2.destroyAllWindows()
    if mqtt_client:
        mqtt_client.loop_stop()
        mqtt_client.disconnect()
        print("[*] Conexión MQTT cerrada.")

if __name__ == '__main__':
    main()
#!/bin/bash
# ============================================================
#  Control Manual Open LED Race (Jugador VERDE)
# ============================================================

MQTT_HOST="192.168.88.251"
MQTT_PORT="1883"
MQTT_PREFIX="track01"

# Crear un archivo FIFO (tubería) temporal para la conexión persistente
FIFO="/tmp/mqtt_green_$$"
mkfifo "$FIFO"

# Función de limpieza al salir (Ctrl+C o tecla Q)
cleanup() {
    echo -e "\n🛑 Saliendo y limpiando..."
    exec 3>&- 2>/dev/null  # Cierra el descriptor de archivo
    kill $PUB_PID 2>/dev/null || true
    rm -f "$FIFO"
    stty sane # Restaura el comportamiento normal de la terminal
    exit 0
}
trap cleanup SIGINT SIGTERM EXIT

clear
echo "============================================"
echo "   🏎️  CONTROL MANUAL - COCHE VERDE"
echo "============================================"
echo "Broker: $MQTT_HOST:$MQTT_PORT"
echo ""
echo "CONTROLES:"
echo "  [ S ] -> Iniciar la carrera (START / RESTART)"
echo "  [ _ ] -> (BARRA ESPACIADORA) Acelerar coche verde"
echo "  [ Q ] -> Salir del script"
echo "============================================"

# Arrancamos mosquitto_pub en segundo plano leyendo continuamente del FIFO
mosquitto_pub -h "$MQTT_HOST" -p "$MQTT_PORT" -t "$MQTT_PREFIX/redcar" -l < "$FIFO" &
PUB_PID=$!

# Abrimos el FIFO en el descriptor 3 para que no se cierre la tubería
exec 3> "$FIFO"

# Bucle infinito para capturar teclas silenciosamente
while true; do
    # Lee exactamente 1 caracter sin mostrarlo en pantalla
    read -r -s -n 1 key
    
    if [[ "$key" == "" ]]; then
        # La barra espaciadora se evalúa como una cadena vacía en 'read'
        echo "1" >&3
        echo -n "🟩" # Imprime un cuadradito verde en la terminal como feedback visual
        
    elif [[ "${key,,}" == "s" ]]; then
        # Se presionó la 's' o 'S' -> Enviar comando de Start
        echo -e "\n🚦 Enviando START. Espera el semáforo y... ¡A POR TODAS!"
        mosquitto_pub -h "$MQTT_HOST" -p "$MQTT_PORT" -t "$MQTT_PREFIX/start" -m "go"
        
    elif [[ "${key,,}" == "q" ]]; then
        # Se presionó la 'q' o 'Q' -> Salir
        break
    fi
done

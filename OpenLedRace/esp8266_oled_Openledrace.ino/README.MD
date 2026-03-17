## 💻 Preparación del Entorno (Arduino IDE)

Este proyecto está diseñado para funcionar en un microcontrolador de la familia **ESP8266** (como las placas *NodeMCU v3*, *NodeMCU 1.0 (ESP-12E)* o *Wemos D1 Mini*). 

### 1. Instalar la placa ESP8266 en Arduino IDE
Si nunca has programado un ESP8266 en tu ordenador, sigue estos pasos:
1. Abre Arduino IDE y ve a **Archivo > Preferencias** (*File > Preferences*).
2. En el campo **"Gestor de URLs Adicionales de Tarjetas"**, pega este enlace:
   `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
3. Ve a **Herramientas > Placa > Gestor de tarjetas** (*Tools > Board > Boards Manager*).
4. Busca **"esp8266"** e instala el paquete oficial (creado por *ESP8266 Community*).
5. Tras instalar, ve a **Herramientas > Placa** y selecciona tu modelo (suele ser **NodeMCU 1.0 (ESP-12E Module)** o **LOLIN(WEMOS) D1 R2 & mini**).

### 2. Librerías Necesarias
Para que el código compile sin errores, debes instalar varias librerías de terceros. 

**¿Cómo instalarlas?**
En Arduino IDE, ve a **Programa > Incluir Librería > Administrar Bibliotecas** (*Sketch > Include Library > Manage Libraries*). Busca los nombres exactos en la barra de búsqueda e instálalas.

| Nombre de la Librería | Buscar en el gestor como... | ¿Para qué sirve? |
| :--- | :--- | :--- |
| **PubSubClient** | `PubSubClient` (por *Nick O'Leary*) | Es el motor principal para enviar y recibir los mensajes MQTT del servidor. |
| **Adafruit NeoPixel** | `Adafruit NeoPixel` (por *Adafruit*) | Controla el encendido, apagado y colores de la tira de LEDs direccionables (WS2812B). |
| **WiFiManager** | `WiFiManager` (por *tzapu*) | Crea el punto de acceso WiFi temporal (Portal Cautivo) para configurar la red y la pista sin tener que reescribir el código. |
| **ArduinoJson** | `ArduinoJson` (por *Benoit Blanchon*) | (Instalar la versión 6.x). Permite leer y escribir los datos de configuración (vueltas, nombre de pista) en formato JSON para guardarlos en la memoria. |
| **ESP8266 OLED Driver** | `ESP8266 and ESP32 OLED driver for SSD1306` (por *ThingPulse*) | Controla la pantalla OLED I2C de forma optimizada, permitiendo distintos tamaños de letra y gráficos rápidos. |

*(Nota: Librerías como `ESP8266WiFi.h`, `Wire.h` o `LittleFS.h` ya vienen preinstaladas al añadir el soporte de la placa ESP8266 en el paso 1, no necesitas buscarlas).*

--- 

# 🏎️ Open LED Race - ESP8266 Edition (F1 Start & OLED)

Este proyecto es una versión mejorada del clásico juego de carreras en tira LED (Open LED Race) utilizando un microcontrolador **ESP8266**. Incluye soporte para pantallas OLED, configuración vía web (WiFiManager), guardado de ajustes en memoria interna (LittleFS), un sistema de inicio realista estilo Fórmula 1 y control total a través de MQTT.

## ✨ Características Principales
- **Secuencia de salida Fórmula 1:** 5 luces rojas que se encienden secuencialmente, pausa aleatoria y apagón repentino para iniciar. Los mandos quedan bloqueados durante la secuencia para evitar trampas.
- **Pantalla OLED:** Muestra IP, estado de conexión MQTT en tiempo real, nombre de la pista y contador de vueltas.
- **Cambio de vueltas en vivo:** Permite alterar la duración de la carrera enviando un mensaje MQTT y lo guarda en memoria.
- **Reseteo de fábrica por Hardware:** Borra redes WiFi y configuraciones puenteando un pin.

---

## 🔌 Esquema de Conexiones (Pinout)

| Componente | Pin ESP8266 (NodeMCU/Wemos) | Notas |
| :--- | :--- | :--- |
| **Tira LED (Datos)** | `D3` (GPIO 0) | Usar resistencia de 330Ω a 470Ω en serie si es necesario. |
| **Pantalla OLED (SDA)**| `D6` (GPIO 14) | Datos I2C. |
| **Pantalla OLED (SCL)**| `D5` (GPIO 12) | Reloj I2C. |
| **Botón/Cable Reset** | `D7` (GPIO 13) | Conectar a `GND` durante 3 segundos para formatear. |

---

## ⚙️ Configuración Inicial (Modo AP)

1. Enciende el ESP8266. Si es la primera vez o no encuentra su red habitual, creará un punto de acceso WiFi llamado **`OpenLedRace_AP`**.
2. Conéctate a esa red desde tu móvil o PC (Contraseña: `12345678`).
3. Se abrirá automáticamente un portal cautivo (o entra en `http://192.168.4.1`).
4. Introduce los datos de tu WiFi local y configura los parámetros de la carrera:
   - **Servidor MQTT:** (ej. `broker.mqtt-dashboard.com`)
   - **Prefijo Pista:** Nombre base para los canales MQTT (ej. `track01`).
   - **Vueltas:** Número inicial de vueltas (ej. `5`).
   - **LEDs:** Número total de LEDs de tu tira (Máx: 300).
5. Guarda y el ESP8266 se reiniciará conectándose a tu WiFi y al servidor MQTT.

---

## 📡 Tabla de Comunicación MQTT

El sistema se comunica mediante el protocolo MQTT. Sustituye `track01` por el prefijo que hayas configurado en el Modo AP.

### 📥 Enviar al ESP8266 (Para controlar la pista)
Desde tu aplicación móvil, script de Python o dashboard, debes publicar en estos "Topics":

| Topic (Canal) | Valores Aceptados | Descripción |
| :--- | :--- | :--- |
| `track01/greencar` | `1` (o cualquier texto) | **Acelerador P1 (Verde).** Cada mensaje recibido acelera un poco el coche verde. Solo funciona si la carrera está activa. |
| `track01/redcar` | `1` (o cualquier texto) | **Acelerador P2 (Rojo).** Cada mensaje recibido acelera un poco el coche rojo. Solo funciona si la carrera está activa. |
| `track01/start` | Cualquier texto | **Comando de Inicio.** Interrumpe cualquier estado actual y fuerza el inicio de la secuencia de salida Fórmula 1. |
| `track01/set_laps` | Un número del `1` al `99` | **Cambiar Vueltas.** Actualiza las vueltas máximas de la carrera, actualiza la pantalla OLED y guarda el valor en la memoria interna (LittleFS). Solo se acepta si los coches están parados en la salida. |

### 📤 Recibir del ESP8266 (Para telemetría)
Suscríbete a estos "Topics" si quieres visualizar datos en tiempo real en tu móvil/ordenador:

| Topic (Canal) | Formato del Valor | Descripción |
| :--- | :--- | :--- |
| `track01/greenspeed` | Float (ej: `12.50`) | Publica la velocidad actual en tiempo real del coche Verde (P1). |
| `track01/redspeed` | Float (ej: `14.20`) | Publica la velocidad actual en tiempo real del coche Rojo (P2). |

---

## 🏁 Dinámica de Carrera (Salida Fórmula 1)

1. Al encender o al enviar el comando `track01/start`, la pista se oscurece y los mandos dejan de responder.
2. Comienza el encendido progresivo de luces rojas en los primeros 10 LEDs (pares simulando los 5 semáforos gigantes de F1):
   - 🔴🔴 ⚪⚪ ⚪⚪ ⚪⚪ ⚪⚪ 
   - 🔴🔴 🔴🔴 ⚪⚪ ⚪⚪ ⚪⚪
   - 🔴🔴 🔴🔴 🔴🔴 ⚪⚪ ⚪⚪
   - 🔴🔴 🔴🔴 🔴🔴 🔴🔴 ⚪⚪
   - 🔴🔴 🔴🔴 🔴🔴 🔴🔴 🔴🔴
3. **Pausa Aleatoria**: El sistema espera un tiempo impredecible (entre 0.5 y 3 segundos) manteniendo la tensión.
4. **¡Apagón!** Todos los LEDs se apagan de golpe. En ese exacto momento, se liberan los mandos y los coches comienzan a reaccionar a las pulsaciones MQTT.

---

## 🛠️ Reseteo de Fábrica (Hardware Reset)

Si cambias de router, te equivocas de contraseña WiFi o quieres borrar la configuración interna (Vueltas, Prefijos, Servidor MQTT):
1. Toma un cable macho-macho (o puentea con unas pinzas).
2. Conecta el pin **`D7`** directamente a **`GND`** (masa) y mantén el contacto sin soltar.
3. Observa la pantalla OLED. Tras unos segundos, aparecerá el mensaje: **"Borrando Datos..."**.
4. Suelta el cable. El ESP8266 borrará la red WiFi, formateará su sistema de archivos interno y se reiniciará automáticamente volviendo al modo AP de configuración de fábrica.

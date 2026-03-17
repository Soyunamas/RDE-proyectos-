// --- MODIFICACIÓN ESPECÍFICA EN LA FUNCIÓN connectMQTT() ---

function connectMQTT() {
    const host = document.getElementById('mqtt-host').value.trim();
    const port = parseInt(document.getElementById('mqtt-port').value);
    currentPrefix = document.getElementById('mqtt-prefix').value.trim();
    const statusBadge = document.getElementById('mqtt-status');
    
    const clientId = "SkiWeb_" + Math.random().toString(16).substr(2, 8);
    
    // CAMBIO 1: Constructor simple (sin path aquí)
    mqttClient = new Paho.MQTT.Client(host, port, clientId);

    mqttClient.onConnectionLost = function(responseObject) {
        isMqttConnected = false;
        statusBadge.innerText = "🔌 Desconectado"; 
        statusBadge.style.backgroundColor = "#d32f2f";
    };

    mqttClient.onMessageArrived = function(message) {
        const topic = message.destinationName;
        const payload = message.payloadString;
        if(topic === `${currentPrefix}/greenspeed`) updateSpeedometer('green', payload);
        else if (topic === `${currentPrefix}/redspeed`) updateSpeedometer('red', payload);
    };

    const options = {
        onSuccess: function() {
            isMqttConnected = true;
            statusBadge.innerText = "🔌 Conectado ✅"; 
            statusBadge.style.backgroundColor = "#388e3c";
            toggleSettings(); 
            mqttClient.subscribe(`${currentPrefix}/greenspeed`);
            mqttClient.subscribe(`${currentPrefix}/redspeed`);
        },
        onFailure: function(err) {
            isMqttConnected = false; 
            statusBadge.innerText = "Error MQTT";
            console.error("Detalle del error:", err);
            alert("Error: " + err.errorMessage + ". Asegúrate de que Mosquitto acepte WebSockets.");
        },
        // CAMBIO 2: Definir el path aquí dentro de las opciones
        path: "/mqtt",
        useSSL: false,
        cleanSession: true
    };

    statusBadge.innerText = "Conectando..."; 
    statusBadge.style.backgroundColor = "#ff9800";
    
    // Conectar usando las opciones que incluyen el path
    mqttClient.connect(options);
}
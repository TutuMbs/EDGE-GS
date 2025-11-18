/*
 * Monitor IoT de Postura e Foco - ESP32 com Sensor PIR
 * Hardware: ESP32, Sensor PIR HC-SR501, Buzzer, LED RGB
 * Sistema: Pausas de 5 minutos via botão
 * MQTT: test.mosquitto.org
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ====== CONFIGURAÇÕES WiFi ======
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ====== CONFIGURAÇÕES MQTT ======
const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;
const char* mqtt_client_id = "ESP32_PostureMonitor_PIR";

// Tópicos MQTT
const char* topic_status = "iot/posture/status";
const char* topic_session = "iot/posture/session";
const char* topic_alert = "iot/posture/alert";
const char* topic_pause = "iot/posture/pause";

// ====== PINOS ======
#define PIR_PIN 23
#define BUZZER_PIN 25
#define LED_RED 26
#define LED_GREEN 27
#define LED_BLUE 14
#define BUTTON_PIN 32

// ====== CONFIGURAÇÕES ======
#define ALERT_INTERVAL 5400000     // 90 minutos
#define CHECK_INTERVAL 1000        // 1 segundo
#define MIN_PRESENCE_TIME 5000     // 5 segundos
#define MIN_ABSENCE_TIME 10000     // 10 segundos
#define PAUSE_DURATION 300000      // 5 minutos = 300000 ms

// ====== VARIÁVEIS GLOBAIS ======
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Controle de sessão
bool isSitting = false;
unsigned long sessionStartTime = 0;
unsigned long totalFocusTime = 0;
int sessionsToday = 0;
int breaksToday = 0;

// Controle de presença
bool presenceDetected = false;
unsigned long lastPresenceTime = 0;
unsigned long presenceStartTime = 0;
unsigned long absenceStartTime = 0;

// Controle de alertas
bool alertActive = false;
unsigned long lastAlertTime = 0;
unsigned long lastCheckTime = 0;
unsigned long lastStatusPublish = 0;

// Sistema de pausas
bool isPaused = false;
unsigned long pauseStartTime = 0;
unsigned long pauseRemainingTime = 0;

// Controle do botão
bool lastButtonState = HIGH;
unsigned long lastButtonPress = 0;

// Estatísticas
int pirTriggerCount = 0;

// ====== SETUP ======
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=================================");
  Serial.println("Monitor IoT de Postura - ESP32");
  Serial.println("Sensor: PIR HC-SR501");
  Serial.println("Sistema de Pausas: 5 minutos");
  Serial.println("=================================\n");
  
  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  setLED(255, 255, 0);
  
  Serial.println("⏳ Aguardando estabilização do sensor PIR...");
  for (int i = 10; i > 0; i--) {
    Serial.print("   ");
    Serial.print(i);
    Serial.println("s");
    delay(1000);
  }
  Serial.println("✓ Sensor PIR calibrado!\n");
  
  setupWiFi();
  mqttClient.setServer(mqtt_server, mqtt_port);
  
  setLED(0, 255, 0);
  Serial.println("✅ Sistema pronto!");
  Serial.println("🔘 Pressione o botão para iniciar pausa de 5 min");
  Serial.println("📊 Monitoramento iniciado...\n");
}

// ====== LOOP PRINCIPAL ======
void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();
  
  // Se está em pausa, gerenciar a pausa
  if (isPaused) {
    managePause();
    checkButton();
    delay(100);
    return;
  }
  
  // Verificar sensor periodicamente
  if (millis() - lastCheckTime >= CHECK_INTERVAL) {
    lastCheckTime = millis();
    checkPresence();
  }
  
  // Verificar alertas se estiver sentado
  if (isSitting) {
    checkAlerts();
  }
  
  // Verificar botão
  checkButton();
  
  // Publicar status periodicamente
  if (millis() - lastStatusPublish >= 10000) {
    lastStatusPublish = millis();
    publishStatus();
  }
  
  delay(100);
}

// ====== FUNÇÕES WiFi ======
void setupWiFi() {
  Serial.print("🌐 Conectando ao WiFi");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" ✓");
    Serial.print("   IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" ✗ Falha ao conectar WiFi");
  }
}

// ====== FUNÇÕES MQTT ======
void reconnectMQTT() {
  static unsigned long lastAttempt = 0;
  if (millis() - lastAttempt < 5000) return;
  lastAttempt = millis();
  
  Serial.print("📡 Conectando MQTT...");
  
  if (mqttClient.connect(mqtt_client_id)) {
    Serial.println(" ✓");
    publishStatus();
  } else {
    Serial.print(" ✗ (");
    Serial.print(mqttClient.state());
    Serial.println(")");
  }
}

void publishStatus() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<350> doc;
  doc["sitting"] = isSitting;
  doc["paused"] = isPaused;
  doc["presence"] = presenceDetected;
  doc["breaksToday"] = breaksToday;
  doc["pirTriggers"] = pirTriggerCount;
  doc["timestamp"] = millis();
  
  if (isSitting && !isPaused) {
    doc["sessionDuration"] = (millis() - sessionStartTime) / 1000;
  }
  
  if (isPaused) {
    doc["pauseRemaining"] = pauseRemainingTime / 1000;
  }
  
  char buffer[350];
  serializeJson(doc, buffer);
  mqttClient.publish(topic_status, buffer);
}

void publishSession(unsigned long duration) {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<400> doc;
  doc["totalSeconds"] = totalFocusTime / 1000;
  doc["sessionsCount"] = sessionsToday;
  doc["breaksCount"] = breaksToday;
  
  JsonObject newSession = doc.createNestedObject("newSession");
  newSession["duration"] = duration / 60000;
  
  unsigned long hours = (millis() / 3600000) % 24;
  unsigned long minutes = (millis() / 60000) % 60;
  char timeStr[10];
  sprintf(timeStr, "%02lu:%02lu", hours, minutes);
  newSession["time"] = timeStr;
  
  char buffer[400];
  serializeJson(doc, buffer);
  mqttClient.publish(topic_session, buffer);
  Serial.println("📤 Sessão publicada via MQTT");
}

void publishAlert(bool isAlert) {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<200> doc;
  doc["alert"] = isAlert;
  doc["timestamp"] = millis();
  
  if (isAlert) {
    doc["sessionDuration"] = (millis() - sessionStartTime) / 60000;
  }
  
  char buffer[200];
  serializeJson(doc, buffer);
  mqttClient.publish(topic_alert, buffer);
}

void publishPause(bool start) {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<250> doc;
  doc["pause"] = start ? "started" : "ended";
  doc["breaksToday"] = breaksToday;
  doc["timestamp"] = millis();
  
  if (start) {
    doc["duration"] = PAUSE_DURATION / 1000;
  }
  
  char buffer[250];
  serializeJson(doc, buffer);
  mqttClient.publish(topic_pause, buffer);
}

// ====== FUNÇÕES DE MONITORAMENTO ======
void checkPresence() {
  int pirValue = digitalRead(PIR_PIN);
  
  if (pirValue == HIGH) {
    presenceDetected = true;
    lastPresenceTime = millis();
    pirTriggerCount++;
    
    if (absenceStartTime > 0) {
      absenceStartTime = 0;
    }
    
    if (presenceStartTime == 0) {
      presenceStartTime = millis();
    }
    
    if (!isSitting && (millis() - presenceStartTime >= MIN_PRESENCE_TIME)) {
      startSession();
    }
    
  } else {
    if (millis() - lastPresenceTime < 3000) {
      return;
    }
    
    presenceDetected = false;
    
    if (presenceStartTime > 0) {
      presenceStartTime = 0;
    }
    
    if (absenceStartTime == 0 && isSitting) {
      absenceStartTime = millis();
    }
    
    if (isSitting && absenceStartTime > 0 && 
        (millis() - absenceStartTime >= MIN_ABSENCE_TIME)) {
      endSession();
      absenceStartTime = 0;
    }
  }
  
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 5000) {
    lastDebug = millis();
    Serial.print("PIR: ");
    Serial.print(pirValue ? "HIGH" : "LOW ");
    Serial.print(" | Status: ");
    Serial.print(isSitting ? "Sentado" : "Em pé");
    
    if (isSitting) {
      unsigned long elapsed = (millis() - sessionStartTime) / 60000;
      Serial.print(" (");
      Serial.print(elapsed);
      Serial.print(" min)");
    }
    Serial.println();
  }
}

void startSession() {
  if (!isSitting) {
    isSitting = true;
    sessionStartTime = millis();
    sessionsToday++;
    alertActive = false;
    
    setLED(0, 0, 255);
    playTone(1500, 200);
    
    Serial.println("\n🪑 ===== NOVA SESSÃO INICIADA =====");
    Serial.print("   Sessão #");
    Serial.println(sessionsToday);
    Serial.println();
    
    publishStatus();
  }
}

void endSession() {
  if (isSitting) {
    isSitting = false;
    unsigned long sessionDuration = millis() - sessionStartTime;
    
    if (sessionDuration > 60000) {
      totalFocusTime += sessionDuration;
      
      Serial.println("\n✓ ===== SESSÃO FINALIZADA =====");
      Serial.print("   Duração: ");
      Serial.print(sessionDuration / 60000);
      Serial.println(" minutos");
      Serial.print("   Total hoje: ");
      Serial.print(totalFocusTime / 3600000);
      Serial.print("h ");
      Serial.print((totalFocusTime / 60000) % 60);
      Serial.println("m");
      Serial.println();
      
      publishSession(sessionDuration);
    } else {
      Serial.println("\n⚠️  Sessão muito curta (< 1 min)");
    }
    
    setLED(0, 255, 0);
    alertActive = false;
    playTone(800, 150);
    
    publishStatus();
  }
}

void checkAlerts() {
  unsigned long sessionDuration = millis() - sessionStartTime;
  
  if (sessionDuration >= ALERT_INTERVAL && !alertActive) {
    triggerAlert();
  }
  
  if (alertActive && (millis() - lastAlertTime >= 1800000)) {
    triggerAlert();
  }
}

void triggerAlert() {
  alertActive = true;
  lastAlertTime = millis();
  
  unsigned long minutes = (millis() - sessionStartTime) / 60000;
  
  Serial.println("\n⚠️  ========== ALERTA DE POSTURA ========== ⚠️");
  Serial.print("   Você está sentado há ");
  Serial.print(minutes);
  Serial.println(" minutos!");
  Serial.println("   🔘 Pressione o botão para fazer uma pausa!");
  Serial.println();
  
  setLED(255, 0, 0);
  
  for (int i = 0; i < 5; i++) {
    playTone(1000, 400);
    delay(200);
  }
  
  publishAlert(true);
}

// ====== SISTEMA DE PAUSAS ======
void startPause() {
  isPaused = true;
  pauseStartTime = millis();
  pauseRemainingTime = PAUSE_DURATION;
  breaksToday++;
  
  // Desativa alerta se estiver ativo
  if (alertActive) {
    alertActive = false;
    publishAlert(false);
  }
  
  Serial.println("\n⏸️  ========== PAUSA INICIADA ==========");
  Serial.print("   ☕ Pausa #");
  Serial.print(breaksToday);
  Serial.println(" - Duração: 5 minutos");
  Serial.println("   💡 Aproveite para alongar e se hidratar!");
  Serial.println("   🔘 Pressione o botão novamente para encerrar");
  Serial.println();
  
  setLED(255, 255, 0);
  playTone(1200, 150);
  delay(100);
  playTone(1500, 150);
  delay(100);
  playTone(1800, 200);
  
  publishPause(true);
  publishStatus();
}

void managePause() {
  pauseRemainingTime = PAUSE_DURATION - (millis() - pauseStartTime);
  
  // Encerra automaticamente após 5 minutos
  if (pauseRemainingTime <= 0) {
    endPause();
    return;
  }
  
  // Mostra progresso a cada 30 segundos
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= 30000) {
    lastUpdate = millis();
    
    int remainingSeconds = pauseRemainingTime / 1000;
    int remainingMinutes = remainingSeconds / 60;
    int seconds = remainingSeconds % 60;
    
    Serial.print("⏸️  Pausa em andamento... Restam ");
    Serial.print(remainingMinutes);
    Serial.print(":");
    if (seconds < 10) Serial.print("0");
    Serial.println(seconds);
    
    publishStatus();
  }
}

void endPause() {
  isPaused = false;
  pauseRemainingTime = 0;
  
  Serial.println("\n✅ ========== PAUSA ENCERRADA ==========");
  Serial.print("   🎯 Pausa #");
  Serial.print(breaksToday);
  Serial.println(" concluída!");
  
  if (breaksToday >= 8) {
    Serial.println("   🏆 Meta de 8 pausas atingida!");
  } else if (breaksToday >= 6) {
    Serial.println("   💪 Quase lá! Faltam poucas pausas!");
  } else {
    Serial.println("   👍 Continue fazendo pausas regulares!");
  }
  Serial.println();
  
  setLED(0, 0, 255);
  playTone(2000, 150);
  delay(100);
  playTone(2300, 150);
  delay(100);
  playTone(2600, 200);
  
  publishPause(false);
  publishStatus();
}

// ====== CONTROLE DO BOTÃO ======
void checkButton() {
  bool buttonState = digitalRead(BUTTON_PIN);
  
  if (buttonState == LOW && lastButtonState == HIGH) {
    if (millis() - lastButtonPress > 500) {
      lastButtonPress = millis();
      handleButtonPress();
    }
  }
  
  lastButtonState = buttonState;
}

void handleButtonPress() {
  Serial.println("\n🔘 BOTÃO PRESSIONADO!");
  
  if (isPaused) {
    // Encerrar pausa antecipadamente
    Serial.println("   Encerrando pausa antecipadamente...");
    endPause();
  } else {
    // Iniciar nova pausa
    startPause();
  }
}

// ====== FUNÇÕES AUXILIARES ======
void setLED(int red, int green, int blue) {
  analogWrite(LED_RED, red);
  analogWrite(LED_GREEN, green);
  analogWrite(LED_BLUE, blue);
}

void playTone(int frequency, int duration) {
  ledcAttach(BUZZER_PIN, frequency, 8);
  ledcWrite(BUZZER_PIN, 128);
  delay(duration);
  ledcWrite(BUZZER_PIN, 0);
  ledcDetach(BUZZER_PIN);
}

/*
 * MicroCoaster - Module Switch Track ESP32
 * 
 * Module intelligent d'aiguillage sécurisé pour montagnes russes miniatures
 * Combine gestion WiFi automatique, contrôle d'aiguillage physique et communication WebSocket
 * 
 * Auteurs: CyberSpaceRS, Yamakajump
 * Version: 2.0.0
 */

#include <Arduino.h>          // Bibliothèque principale Arduino pour ESP32
#include <AyresWiFiManager.h> // Gestionnaire WiFi avec portail captif
#include <WebSocketsClient.h> // Client WebSocket pour communication serveur
#include <ArduinoJson.h>      // Manipulation des données JSON
#include "env.h"              // Configuration WiFi (identifiants point d'accès)

// ========================================
// CONFIGURATION PRINCIPALE
// ========================================

// Instance du gestionnaire WiFi intelligent avec portail captif
AyresWiFiManager wifi;

// Configuration serveur WebSocket sécurisé (WSS)
const char* server_host = "app.microcoaster.com";  // Adresse du serveur distant MicroCoaster
const uint16_t server_port = 443;                  // Port HTTPS/WSS standard
const char* websocket_path = "/esp32";             // Endpoint WebSocket dédié aux modules ESP32

// Identifiants uniques du module Switch Track
const String MODULE_ID = "MC-0001-ST";                        // ID unique du module (MicroCoaster-Switch Track)
const String MODULE_PASSWORD = "F674iaRftVsHGKOA8hq3TI93HQHUaYqZ"; // Mot de passe sécurisé pour authentification

// ========================================
// VARIABLES GLOBALES
// ========================================

// Client WebSocket pour communication avec le serveur
WebSocketsClient webSocket;

// État actuel de l'aiguillage ("left" ou "right")
String currentPosition = "left"; // Position initiale au démarrage

// Variables de monitoring
unsigned long uptimeStart = 0;   // Timestamp du démarrage pour calcul uptime
bool isAuthenticated = false;     // État d'authentification avec le serveur

// ========================================
// CONFIGURATION HARDWARE
// ========================================

// Pins des LEDs d'indication de position
const int LED_LEFT_PIN  = 2;      // GPIO 2 - LED position gauche
const int LED_RIGHT_PIN = 4;      // GPIO 4 - LED position droite

// Pins de contrôle du verrin via driver DRV8871
const int IN1 = 21;   // GPIO 21 - Commande IN1 du DRV8871 (sens horaire)
const int IN2 = 22;   // GPIO 22 - Commande IN2 du DRV8871 (sens anti-horaire)

// Temporisations pour le contrôle sécurisé du verrin
const unsigned long RUN_MS   = 1100;  // Durée de fonctionnement du verrin (1,1 seconde)
const unsigned long DEAD_MS  = 10;    // Temps de sécurité entre changements de direction

// ========================================
// FONCTIONS DE CONTRÔLE VERRIN
// ========================================

// Met le verrin en roue libre (arrêt sécurisé)
inline void coast() {
  digitalWrite(IN1, LOW);   // IN1 = 0
  digitalWrite(IN2, LOW);   // IN2 = 0
  // État: roue libre, aucun courant dans le moteur
}

// Active le verrin vers la droite (position "right")
inline void rightVerrin() {
  digitalWrite(IN1, HIGH);  // IN1 = 1
  digitalWrite(IN2, LOW);   // IN2 = 0
  // État: rotation sens horaire (vers position droite)
}

// Active le verrin vers la gauche (position "left")
inline void leftVerrin() {
  digitalWrite(IN1, LOW);   // IN1 = 0
  digitalWrite(IN2, HIGH);  // IN2 = 1
  // État: rotation sens anti-horaire (vers position gauche)
}

// Fonction sécurisée pour changer la direction du verrin
void safeDir(void (*dirFn)()) {
  coast();          // 1. Arrêt sécurisé
  delay(DEAD_MS);   // 2. Attente dead-time (évite les courts-circuits)
  dirFn();          // 3. Application de la nouvelle direction
  delay(DEAD_MS);   // 4. Stabilisation
}

// Déclarations des fonctions
void connectSocket();
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
void authenticateModule();
void handleConnected(const char* payload);
void handleCommand(const char* payload);
void handleError(const char* payload);
void updateLEDs();
void sendCommandResponse(const String& command, const String& status, const String& position);
void sendHeartbeat();
void sendTelemetry();

// ========================================
// FONCTION DE DÉMARRAGE (SETUP)
// ========================================

void setup() {
  // Initialisation de la communication série pour debug
  Serial.begin(115200);
  Serial.println();
  Serial.println("=========================================");
  Serial.println("🚀 MicroCoaster - Switch Track v2.0.0");
  Serial.println("=========================================");
  Serial.println();

  // Enregistrement du timestamp de démarrage pour calcul uptime
  uptimeStart = millis();
  
  // *** CONFIGURATION DES PINS ***
  
  // Configuration des LEDs en sortie
  pinMode(LED_LEFT_PIN, OUTPUT);   // GPIO 2 - LED gauche
  pinMode(LED_RIGHT_PIN, OUTPUT);  // GPIO 4 - LED droite

  // Configuration des pins de contrôle verrin en sortie
  pinMode(IN1, OUTPUT);  // GPIO 21 - Contrôle IN1 DRV8871
  pinMode(IN2, OUTPUT);  // GPIO 22 - Contrôle IN2 DRV8871
  
  // Mise en état sûr du verrin au démarrage
  coast(); // Arrêt du verrin (roue libre)
  Serial.println("[HARDWARE] ⚙️  DRV8871 initialisé - Contrôle verrin avec temporisations sécurisées");
  
  // Affichage de la position initiale et allumage LED correspondante
  updateLEDs();
  Serial.println("[SWITCH TRACK] 📍 Position initiale: " + currentPosition);

  // *** CONFIGURATION DU GESTIONNAIRE WIFI ***
  
  // Configuration du point d'accès de secours (fallback)
  Serial.println("📡 Configuration du point d'accès de secours...");
  wifi.setAPCredentials(ESP_WIFI_SSID, ESP_WIFI_PASSWORD);
  Serial.print("   ├─ SSID: ");
  Serial.println(ESP_WIFI_SSID);
  Serial.print("   └─ Mot de passe: ");
  Serial.println(ESP_WIFI_PASSWORD);
  
  // Configuration des timeouts du portail captif
  Serial.println("⏱️  Configuration des timeouts...");
  wifi.setPortalTimeout(3600);     // 60 minutes (très long pour debug)
  wifi.setAPClientCheck(true);     // Ne pas fermer si des clients sont connectés
  wifi.setWebClientCheck(true);    // Chaque requête HTTP remet à zéro le timer
  Serial.println("   ├─ Timeout portail: 60 minutes");
  Serial.println("   ├─ Vérification clients: activée");
  Serial.println("   └─ Vérification requêtes web: activée");
  
  // Configuration avancée du portail captif
  Serial.println("🔧 Configuration avancée...");
  wifi.setCaptivePortal(true);      // Activer les redirections pour portail captif
  Serial.println("   ├─ Portail captif: activé");
  
  // Configuration hybride : première connexion + production
  wifi.setFallbackPolicy(AyresWiFiManager::FallbackPolicy::ON_FAIL);
  wifi.setAutoReconnect(true);      // Reconnexion automatique en cas de déconnexion
  Serial.println("   ├─ Politique de secours: ON_FAIL");
  Serial.println("   └─ Reconnexion automatique: activée");
  
  // Protection des fichiers critiques (empêche leur suppression accidentelle)
  wifi.setProtectedJsons({"/wifi.json"});  // Protège le fichier de configuration WiFi
  Serial.println("🛡️  Protection fichiers: /wifi.json");
  
  // Activation du bouton de secours (GPIO 0 par défaut)
  wifi.enableButtonPortal(true);    // Bouton 2-5s = ouvre portail, ≥5s = efface identifiants
  Serial.println("🔘 Bouton de secours: GPIO 0 (2-5s = portail, ≥5s = reset)");
  
  // *** INITIALISATION DU WIFI MANAGER ***
  
  Serial.println();
  Serial.println("🔄 Initialisation du WiFi Manager...");
  wifi.begin();  // Monte le système de fichiers, charge /wifi.json si présent
  Serial.println("💾 Système de fichiers LittleFS monté");
  Serial.println("📁 Recherche du fichier de configuration /wifi.json...");
  
  Serial.println("🌐 Tentative de connexion WiFi...");
  wifi.run();    // Essaie de se connecter en STA; si ça échoue, applique la politique de fallback
  
  // Vérification du statut après initialisation
  delay(2000); // Attendre un peu pour que la connexion se stabilise
  
  // *** VÉRIFICATION ÉTAT CONNEXION ***
  
  if (wifi.isConnected()) {
    Serial.println("✅ Connexion WiFi réussie !");
    Serial.println("📡 IP: " + WiFi.localIP().toString());
    Serial.println("🌐 Mode: Client WiFi (STA)");
    
    // Connexion WebSocket automatique après succès WiFi
    connectSocket();
  } else {
    Serial.println("⚠️  Connexion WiFi échouée");
    Serial.println("🔧 Ouverture du portail de configuration...");
    Serial.println("📡 Point d'accès: WifiManager-MicroCoaster");
    Serial.println("🌐 IP du portail: 192.168.4.1");
    Serial.println("🔗 Connectez-vous au WiFi puis allez sur http://192.168.4.1");
  }
  
  Serial.println();
  Serial.println("✅ Initialisation terminée !");
  Serial.println("=========================================");
}

// ========================================
// BOUCLE PRINCIPALE (LOOP)
// ========================================

void loop() {
  // Mise à jour du gestionnaire WiFi (portail web, DNS, timeouts)
  wifi.update(); 
  
  // Variables statiques pour le monitoring périodique
  static unsigned long lastStatusCheck = 0;     // Dernier check de statut WiFi
  static unsigned long lastConnectionState = false; // Dernier état de connexion
  static unsigned long lastHeartbeat = 0;       // Dernier heartbeat envoyé
  static unsigned long lastTelemetry = 0;       // Dernière télémétrie envoyée
  unsigned long now = millis();                 // Timestamp actuel
  
  // *** MONITORING WIFI PÉRIODIQUE ***
  // Vérification du statut WiFi toutes les 30 secondes
  
  if (millis() - lastStatusCheck > 30000) {
    lastStatusCheck = millis();
    bool currentState = wifi.isConnected();
    
    // Affichage du statut de connexion
    if (currentState) {
      Serial.println("🟢 WiFi connecté - IP: " + WiFi.localIP().toString() + 
                     " | Signal: " + String(WiFi.RSSI()) + " dBm");
    } else {
      Serial.println("🔴 WiFi déconnecté - Portail de configuration actif sur 192.168.4.1");
    }
    
    // Détection des changements d'état WiFi pour actions automatiques
    if (currentState != lastConnectionState) {
      if (currentState) {
        Serial.println("🎉 Connexion WiFi établie !");
        // Reconnexion WebSocket automatique après retour WiFi
        connectSocket();
      } else {
        Serial.println("⚠️  Connexion WiFi perdue, basculement en mode portail...");
        // Reset de l'authentification et état sûr des LEDs
        isAuthenticated = false;
        digitalWrite(LED_LEFT_PIN, LOW);
        digitalWrite(LED_RIGHT_PIN, LOW);
      }
      lastConnectionState = currentState;
    }
  }
  
  // *** GESTION WEBSOCKET ET TÉLÉMÉTRIE ***
  // Traitement uniquement si connecté au WiFi
  
  if (wifi.isConnected()) {
    // Traitement des messages WebSocket entrants
    webSocket.loop();
    
    // Envoi périodique de heartbeat (keepalive) - toutes les 30 secondes
    if (isAuthenticated && now - lastHeartbeat > 30000) {
      sendHeartbeat();
      lastHeartbeat = now;
    }
    
    // Envoi périodique de télémétrie - toutes les 10 secondes
    if (isAuthenticated && now - lastTelemetry > 10000) {
      sendTelemetry();
      lastTelemetry = now;
    }
  }
  
  // Pause pour éviter la saturation CPU
  delay(100);
}

// ========================================
// FONCTIONS DE COMMUNICATION WEBSOCKET
// ========================================

// Établit la connexion WebSocket sécurisée avec le serveur
void connectSocket() {
  Serial.println("[WEBSOCKET] 🔗 Connexion WebSocket sécurisé...");
  Serial.println("[WEBSOCKET] 📍 Module ID: " + MODULE_ID);
  Serial.println("[WEBSOCKET] 🔑 Password: " + MODULE_PASSWORD.substring(0, 8) + "...");
  
  // Configuration de la connexion WebSocket sécurisée (WSS)
  webSocket.beginSSL(server_host, server_port, websocket_path);
  webSocket.onEvent(webSocketEvent);           // Gestionnaire d'événements
  webSocket.setReconnectInterval(5000);        // Reconnexion automatique toutes les 5s
  webSocket.enableHeartbeat(15000, 3000, 2);   // Heartbeat WebSocket: 15s interval, 3s timeout, 2 essais
  
  Serial.printf("[WEBSOCKET] 🤖 WebSocket: wss://%s:%d%s\n", server_host, server_port, websocket_path);
  Serial.println("[WEBSOCKET] ✅ ESP32 Switch Track prêt (Architecture hybride)!");
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_CONNECTED:
      Serial.println("[SWITCH TRACK] 🟢 Connecté au serveur WebSocket");
      authenticateModule();
      break;
      
    case WStype_DISCONNECTED:
      Serial.println("[SWITCH TRACK] 🔴 Déconnexion du serveur");
      isAuthenticated = false;
      digitalWrite(LED_LEFT_PIN, LOW);
      digitalWrite(LED_RIGHT_PIN, LOW);
      break;
      
    case WStype_TEXT: {
      Serial.println("[SWITCH TRACK] 📡 Message reçu: " + String((char*)payload));
      
      JsonDocument doc;
      deserializeJson(doc, (char*)payload);
      
      String msgType = doc["type"].as<String>();
      
      if (msgType == "connected") {
        handleConnected((char*)payload);
      } else if (msgType == "command") {
        handleCommand((char*)payload);
      } else if (msgType == "error") {
        handleError((char*)payload);
      } else {
        Serial.println("[SWITCH TRACK] ⚠️ Événement non géré: '" + msgType + "'");
        Serial.println("[SWITCH TRACK] 🔍 Message complet: " + String((char*)payload));
      }
      break;
    }
    
    default:
      break;
  }
}

void authenticateModule() {
  Serial.println("[SWITCH TRACK] 🔐 Authentification WebSocket natif...");
  
  // Format WebSocket natif
  JsonDocument authData;
  authData["type"] = "module_identify";
  authData["moduleId"] = MODULE_ID;
  authData["password"] = MODULE_PASSWORD;
  authData["moduleType"] = "switch-track";
  authData["uptime"] = millis() - uptimeStart;
  authData["position"] = currentPosition;
  
  String authMessage;
  serializeJson(authData, authMessage);
  webSocket.sendTXT(authMessage);
  
  Serial.println("[SWITCH TRACK] 📤 Authentification envoyée: " + authMessage);
}

void handleConnected(const char* payload) {
  Serial.println("[SWITCH TRACK] ✅ Module authentifié WebSocket natif");
  
  isAuthenticated = true;
  updateLEDs(); // Mettre à jour les LEDs selon la position
  
  // Envoyer télémétrie initiale
  delay(1000);
  sendTelemetry();
}

void handleCommand(const char* payload) {
  if (!isAuthenticated) {
    Serial.println("[SWITCH TRACK] ⚠️ Commande refusée - non authentifié");
    return;
  }
  
  // Parse du JSON WebSocket natif
  JsonDocument doc;
  deserializeJson(doc, payload);
  
  String command = doc["data"]["command"];
  Serial.println("[SWITCH TRACK] 🎮 Commande reçue: " + command);
  
  String newPosition = currentPosition;
  String status = "success";
  
  // Traitement des commandes
  if (command == "switch_left" || command == "left" || command == "switch_to_A") {
    newPosition = "left";
    Serial.println("[SWITCH TRACK] 🔄 Aiguillage basculé vers la GAUCHE");

     // Commande verrin gauche
     safeDir(leftVerrin);
     delay(RUN_MS);
     coast();
    
  } else if (command == "switch_right" || command == "right" || command == "switch_to_B") {
    newPosition = "right";
    Serial.println("[SWITCH TRACK] 🔄 Aiguillage basculé vers la DROITE");

     // Commande verrin droite
     safeDir(rightVerrin);
     delay(RUN_MS);
     coast();
    
  } else if (command == "get_position") {
    // Pas de changement de position, juste retourner l'état
    Serial.println("[SWITCH TRACK] 📍 Position actuelle: " + currentPosition);
    
  } else {
    Serial.println("[SWITCH TRACK] ❌ Commande inconnue: " + command);
    status = "unknown_command";
  }
  
  currentPosition = newPosition;
  updateLEDs(); // Mettre à jour les LEDs après changement de position
  
  // Envoyer la réponse de commande (WebSocket natif)
  sendCommandResponse(command, status, currentPosition);
  
  Serial.println("[SWITCH TRACK] ✅ Commande exécutée: " + currentPosition);
}

void handleError(const char* payload) {
  Serial.println("[SWITCH TRACK] ❌ Erreur reçue du serveur");
  
  isAuthenticated = false;
  // Éteindre toutes les LEDs en cas d'erreur
  digitalWrite(LED_LEFT_PIN, LOW);
  digitalWrite(LED_RIGHT_PIN, LOW);
}

void updateLEDs() {
  if (currentPosition == "left") {
    digitalWrite(LED_LEFT_PIN, HIGH);   // LED gauche ON
    digitalWrite(LED_RIGHT_PIN, LOW);   // LED droite OFF
    Serial.println("[SWITCH TRACK] 💡 LED GAUCHE allumée");
  } else if (currentPosition == "right") {
    digitalWrite(LED_LEFT_PIN, LOW);    // LED gauche OFF
    digitalWrite(LED_RIGHT_PIN, HIGH);  // LED droite ON
    Serial.println("[SWITCH TRACK] 💡 LED DROITE allumée");
  }
}

// Fonctions WebSocket natif
void sendCommandResponse(const String& command, const String& status, const String& position) {
  if (!isAuthenticated) return;
  
  JsonDocument doc;
  doc["type"] = "command_response";
  doc["moduleId"] = MODULE_ID;
  doc["password"] = MODULE_PASSWORD;
  doc["command"] = command;
  doc["status"] = status;
  doc["position"] = position;
  
  String message;
  serializeJson(doc, message);
  webSocket.sendTXT(message);
  
  Serial.printf("[SWITCH TRACK] 📤 Réponse: %s -> %s\n", command.c_str(), status.c_str());
}

void sendHeartbeat() {
  if (!isAuthenticated) return;
  
  JsonDocument doc;
  doc["type"] = "heartbeat";
  doc["moduleId"] = MODULE_ID;
  doc["password"] = MODULE_PASSWORD;
  doc["uptime"] = millis() - uptimeStart;
  doc["position"] = currentPosition;
  doc["wifiRSSI"] = WiFi.RSSI();
  doc["freeHeap"] = ESP.getFreeHeap();
  
  String message;
  serializeJson(doc, message);
  webSocket.sendTXT(message);
  
  Serial.println("[SWITCH TRACK] 💓 Heartbeat envoyé");
}

void sendTelemetry() {
  if (!isAuthenticated) return;
  
  JsonDocument doc;
  doc["type"] = "telemetry";
  doc["moduleId"] = MODULE_ID;
  doc["password"] = MODULE_PASSWORD;
  doc["uptime"] = millis() - uptimeStart;
  doc["position"] = currentPosition;
  doc["status"] = "operational";
  
  String message;
  serializeJson(doc, message);
  webSocket.sendTXT(message);
  
  Serial.println("[SWITCH TRACK] 📊 Télémétrie envoyée");
}
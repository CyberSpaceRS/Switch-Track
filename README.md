# 🎢 MicroCoaster - Module Switch Track ESP32# 🎢 MicroCoaster - Module Switch Track ESP32

> Module intelligent d'aiguillage sécurisé pour montagnes russes miniatures avec gestion WiFi automatique et contrôle distant.> Module intelligent d'aiguillage sécurisé pour montagnes russes miniatures avec gestion WiFi automatique et contrôle distant.

## 📋 Description du Projet

## À quoi sert ce projet ?

Ce projet implémente un **module d'aiguillage intelligent** pour un système de montagnes russes miniatures. Le module combine :MicroCoaster WiFiManager permet de connecter facilement chaque module de ton circuit de montagnes russes miniature (switch track, launch track, station, etc.) à un réseau WiFi local, puis à l’application web fournie. Il centralise la configuration WiFi, la gestion des accès et la communication entre les modules et l’interface web.

- **Gestion WiFi automatique** avec portail de configuration### Fonctionnalités principales

- **Contrôle d'aiguillage physique** via verrin électrique- **Portail de configuration WiFi** : chaque module peut être configuré via un portail web local (mode AP) pour entrer les identifiants WiFi de la box ou du réseau cible.

- **Communication WebSocket sécurisée** avec serveur distant- **Connexion automatique** : une fois configuré, le module se connecte automatiquement au réseau WiFi domestique et communique avec l’application web.

- **Interface LED** pour visualisation d'état- **Sécurité** : les identifiants WiFi ne sont jamais stockés dans le dépôt, mais dans un fichier local non versionné.

- **Authentification sécurisée** et télémétrie temps réel- **Gestion multi-modules** : chaque module (station, switch, launch, etc.) utilise le même firmware et peut être identifié dans l’application web.


## 🔧 Composants Hardware## Utilisation

### **ESP32**1. Flashe le firmware sur chaque module ESP32.

- Microcontrôleur principal avec WiFi intégré2. Au premier démarrage, connecte-toi au point d’accès WiFi créé par le module (ex: `WifiManager-MicroCoaster`).

- Gestion de toute la logique et communication3. Accède au portail de configuration (généralement http://192.168.4.1) et renseigne les identifiants de ton réseau WiFi domestique.

4. Le module redémarre et rejoint automatiquement le réseau.

### **Système d'Aiguillage**5. Depuis l’application web fournie, tu peux voir, piloter et configurer chaque module connecté.

- **Verrin électrique** : Actionneur pour basculer l'aiguillage

- **Driver DRV8871** : Contrôleur de moteur H-Bridge## Auteur

- **Pins de contrôle** : IN1 (GPIO 21) et IN2 (GPIO 22)

### **Interface Visuelle**

- **LED Gauche** : GPIO 2 (position "left")---

- **LED Droite** : GPIO 4 (position "right")Pour toute question ou contribution, ouvre une issue ou un pull request !

### **Bouton de Secours**
- **GPIO 0** : Bouton physique pour reset/portail

## 🚀 Fonctionnalités

### **1. Gestionnaire WiFi Intelligent**
- **Portail captif** automatique au premier démarrage
- **Configuration web** pour saisir identifiants WiFi
- **Reconnexion automatique** en cas de déconnexion
- **Mode hybride** : portail de secours si connexion échoue

### **2. Contrôle d'Aiguillage Sécurisé**
- **Positions** : "left" (gauche) ou "right" (droite)
- **Temporisations précises** : 1,1 seconde de mouvement
- **Dead-time** : 10ms de sécurité entre changements
- **État sûr** : roue libre automatique après mouvement

### **3. Communication WebSocket**
- **Serveur distant** : `app.microcoaster.com` (HTTPS/WSS)
- **Authentification** : Module ID + mot de passe sécurisé
- **Commandes temps réel** : switch_left, switch_right, get_position
- **Télémétrie** : position, uptime, signal WiFi, mémoire

### **4. Monitoring et Sécurité**
- **Heartbeat** : ping toutes les 30 secondes
- **Surveillance WiFi** : vérification continue de connexion
- **LEDs d'état** : indication visuelle de la position
- **Gestion d'erreurs** : récupération automatique

## 📡 Architecture de Communication

```
ESP32 Switch Track
       ↓ WiFi
   Réseau Local
       ↓ Internet
app.microcoaster.com
       ↓ WebSocket Sécurisé (WSS)
   Interface Web
```

### **Messages WebSocket**

#### **Authentification**
```json
{
  "type": "module_identify",
  "moduleId": "MC-0001-ST",
  "password": "F674iaRftVsHGKOA8hq3TI93HQHUaYqZ",
  "moduleType": "switch-track",
  "uptime": 12345,
  "position": "left"
}
```

#### **Commandes Reçues**
```json
{
  "type": "command",
  "data": {
    "command": "switch_left" // ou "switch_right", "get_position"
  }
}
```

#### **Télémétrie Envoyée**
```json
{
  "type": "telemetry",
  "moduleId": "MC-0001-ST",
  "position": "left",
  "status": "operational",
  "uptime": 12345
}
```

## 🔌 Schéma de Câblage

```
ESP32                    DRV8871               Verrin
GPIO 21 (IN1) ────────── IN1 ────────────────── +
GPIO 22 (IN2) ────────── IN2 ────────────────── -
3.3V ──────────────────── VCC
GND ───────────────────── GND

ESP32                    LEDs
GPIO 2 ─────────────────── LED Gauche (+)
GPIO 4 ─────────────────── LED Droite (+)
GND ────────────────────── LEDs (-)

ESP32                    Bouton
GPIO 0 ─────────────────── Bouton Reset
GND ────────────────────── Bouton (-)
```

## 🛠️ Installation et Configuration

### **1. Prérequis**
- **PlatformIO** (VS Code + extension)
- **ESP32** compatible
- **Bibliothèques** : AyresWiFiManager, WebSocketsClient, ArduinoJson

### **2. Configuration**
1. Cloner le projet
2. Modifier `include/env.h` avec vos identifiants de point d'accès
3. Compiler et flasher sur l'ESP32

### **3. Premier Démarrage**
1. L'ESP32 crée un point d'accès `WifiManager-MicroCoaster`
2. Se connecter au WiFi et aller sur `http://192.168.4.1`
3. Saisir les identifiants de votre réseau WiFi
4. Le module se connecte automatiquement

### **4. Utilisation**
- Le module se connecte automatiquement à `app.microcoaster.com`
- Contrôle via interface web ou API WebSocket
- Monitoring temps réel de l'état et des performances

## 📁 Structure du Projet

```
SwitchTrack-Final/
├── src/
│   └── main.cpp          # Code principal du module
├── include/
│   └── env.h            # Configuration WiFi (à créer)
├── data/                # Fichiers web du portail
│   ├── index.html
│   ├── success.html
│   ├── error.html
│   └── logo.png
├── platformio.ini       # Configuration PlatformIO
└── README.md           # Ce fichier
```

## 🔧 Configuration env.h

Créer le fichier `include/env.h` :

```cpp
#ifndef ENV_H
#define ENV_H

// Configuration du point d'accès de secours
#define ESP_WIFI_SSID "WifiManager-MicroCoaster"
#define ESP_WIFI_PASSWORD "microcoaster2024"

#endif
```

## 🎮 Commandes Disponibles

| Commande | Description | Action |
|----------|-------------|---------|
| `switch_left` | Basculer à gauche | Verrin + LED gauche |
| `switch_right` | Basculer à droite | Verrin + LED droite |
| `get_position` | Position actuelle | Retourne état sans mouvement |

## 📊 Surveillance et Debug

### **Logs Série**
- Connexion WiFi et WebSocket
- Exécution des commandes
- Télémétrie et heartbeat
- Erreurs et récupération

### **LEDs d'État**
- **LED Gauche ON** : Position "left"
- **LED Droite ON** : Position "right"
- **Toutes éteintes** : Erreur ou non authentifié

### **Bouton de Secours**
- **2-5 secondes** : Ouvre le portail de configuration
- **≥5 secondes** : Efface les identifiants WiFi sauvegardés

## 🔒 Sécurité

- **Authentification obligatoire** avant toute commande
- **Mot de passe unique** par module
- **WebSocket sécurisé** (WSS) avec certificat
- **Timeouts** et reconnexions automatiques
- **Protection fichiers** de configuration

## 🚨 Dépannage

### **Problème de Connexion WiFi**
1. Vérifier les identifiants dans le portail
2. Vérifier la portée du signal WiFi
3. Redémarrer l'ESP32
4. Utiliser le bouton de reset pour effacer la config

### **Problème WebSocket**
1. Vérifier que `app.microcoaster.com` est accessible
2. Vérifier les logs série pour les erreurs SSL
3. Tester la connectivité réseau

### **Problème Verrin**
1. Vérifier le câblage DRV8871
2. Vérifier l'alimentation du verrin
3. Contrôler les pins GPIO 21/22

## 👥 Auteurs

- **CyberSpaceRS** - Développement principal
- **Yamakajump** - Tests et intégration

## 📄 Licence

Ce projet est sous licence MIT. Voir le fichier LICENSE pour plus de détails.

## 🤝 Contribution

Les contributions sont les bienvenues ! N'hésitez pas à :
- Ouvrir une issue pour signaler un bug
- Proposer des améliorations
- Soumettre une pull request

---

**🎢 Projet MicroCoaster - Module Switch Track ESP32**

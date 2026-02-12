# GitHub Upload Anleitung

## ✅ Vorbereitet für GitHub!

Alles ist ready für Upload. Folge diesen Schritten:

## 📋 Auf GitHub vorbereiten

### 1. Neues Repository erstellen

1. Gehe zu **https://github.com**
2. Klicke **"New repository"** (grüner Button)
3. Fülle aus:
   - **Repository name:** `viclean-esp32-controller`
   - **Description:** `ESP32-based controller for ViClean DuschWC via Bluetooth LE`
   - **Public** oder **Private** (deine Wahl)
   - ❌ **NICHT** "Initialize with README" aktivieren (wir haben schon eins!)
   - ❌ **NICHT** .gitignore hinzufügen (haben wir schon!)
   - ✅ **License:** MIT (optional, wir haben schon eine)
4. Klicke **"Create repository"**

### 2. Repository URL kopieren

GitHub zeigt dir dann eine Seite mit Befehlen. Kopiere die **HTTPS URL**:
```
https://github.com/DEIN-USERNAME/viclean-esp32-controller.git
```

## 💻 Im Terminal (ViClean Ordner)

### 3. Ersten Commit erstellen

```bash
cd /home/faruktuefekli/ViClean

# Alle Dateien hinzufügen
git add .

# Ersten Commit erstellen
git commit -m "Initial commit - ViClean ESP32 Controller

- viclean_simple: 2-button UI with W/M programs
- viclean_serial_bridge_v2: Full serial control
- Complete BLE protocol documentation
- Reverse engineering findings
- Python analysis tools
"

# GitHub Remote hinzufügen (ERSETZE MIT DEINER URL!)
git remote add origin https://github.com/DEIN-USERNAME/viclean-esp32-controller.git

# Push zu GitHub
git push -u origin main
```

### 4. GitHub Login

Wenn gefragt, gib ein:
- **Username:** Dein GitHub Username
- **Password:** Dein GitHub Personal Access Token
  - Falls du keinen Token hast:
    1. GitHub → Settings → Developer settings → Personal access tokens → Tokens (classic)
    2. Generate new token
    3. Wähle: `repo` (full control of private repositories)
    4. Kopiere den Token und verwende ihn als Password

## ✅ Fertig!

Dein Repository ist jetzt auf GitHub! 🎉

URL: `https://github.com/DEIN-USERNAME/viclean-esp32-controller`

## 📝 Für spätere Updates

Wenn du Änderungen machst:

```bash
cd /home/faruktuefekli/ViClean

# Änderungen hinzufügen
git add .

# Commit mit Beschreibung
git commit -m "Beschreibung der Änderungen"

# Zu GitHub pushen
git push
```

## 🎨 Repository verschönern (optional)

### Badges hinzufügen

Füge am Anfang der README.md ein:

```markdown
![License](https://img.shields.io/badge/license-MIT-blue)
![ESP32](https://img.shields.io/badge/platform-ESP32-green)
![Arduino](https://img.shields.io/badge/IDE-Arduino-00979D)
```

### Topics hinzufügen

Auf GitHub → Repository → Settings → Topics:
- `esp32`
- `arduino`
- `bluetooth-le`
- `reverse-engineering`
- `smart-home`
- `iot`
- `lilygo-t-display`

## 📂 Was wird NICHT hochgeladen?

`.gitignore` verhindert Upload von:
- ❌ `decompiled/` (zu groß)
- ❌ `*.apk` (zu groß, urheberrechtlich geschützt)
- ❌ Build-Dateien
- ❌ IDE-Konfiguration

## 🔒 Wichtig

- Stelle sicher dass keine **persönlichen Daten** im Code sind
- Prüfe dass keine **Passwörter** oder **API Keys** enthalten sind
- Die APK ist **nicht** im Repository (durch .gitignore ausgeschlossen)

## 🐛 Probleme?

### "Permission denied"
→ Prüfe dein GitHub Token

### "Repository not found"
→ Prüfe die Repository URL

### "Already exists"
→ Du hast schon ein Repository mit dem Namen, wähle einen anderen

---

**Viel Erfolg!** 🚀

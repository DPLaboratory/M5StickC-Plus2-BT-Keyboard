/*
 * M5StickC Plus2 - Emulazione Tastiera Bluetooth HID
 * =====================================================
 * Premi il tasto A (frontale) per inviare un tasto via BT
 * Premi il tasto B (laterale) per cambiare il tasto da inviare
 *
 * Libreria richiesta: ESP32 BLE Keyboard
 * Installa da Arduino Library Manager: "ESP32 BLE Keyboard" by T-vK
 */

#include <M5StickCPlus2.h>
#include <BleKeyboard.h>
#include "DuckyInterpreter.h"

BleKeyboard bleKeyboard("Yellow-duck", "DPLab", 100);
DuckyInterpreter ducky;

// ── Tipo di tasto ──────────────────────────────────────────────
enum KeyType { KEY_TYPE_CHAR, KEY_TYPE_SPECIAL, KEY_TYPE_MEDIA, KEY_TYPE_LOOP, KEY_TYPE_PASSWORD, KEY_TYPE_DUCKY_SCRIPT};

struct KeyEntry {
  KeyType        type;
  uint8_t        charCode;       // usato per KEY_TYPE_CHAR / KEY_TYPE_SPECIAL
  const uint8_t* mediaCode;      // usato per KEY_TYPE_MEDIA
  const char*    label;
  const char*    password;
};

// ── Tabella tasti ─────────────────────────────────────────────
KeyEntry keys[] = {
  { KEY_TYPE_MEDIA,   0,          KEY_MEDIA_PLAY_PAUSE,    "Play/Pause" , ""  },
  // { KEY_TYPE_MEDIA,   0,          KEY_MEDIA_NEXT_TRACK,    "Next Track" , ""  },
  // { KEY_TYPE_MEDIA,   0,          KEY_MEDIA_PREVIOUS_TRACK,"Prev Track" , ""  },
  // { KEY_TYPE_MEDIA,   0,          KEY_MEDIA_VOLUME_UP,     "Volume +"   , ""  },
  // { KEY_TYPE_MEDIA,   0,          KEY_MEDIA_VOLUME_DOWN,   "Volume -"   , ""  },
  // { KEY_TYPE_MEDIA,   0,          KEY_MEDIA_MUTE,          "Mute"       , ""  },
  // { KEY_TYPE_CHAR,    'a',        nullptr,                 "Lettera A"  , ""  },
  // { KEY_TYPE_CHAR,    ' ',        nullptr,                 "Spazio"     , ""  },
  // { KEY_TYPE_SPECIAL, KEY_RETURN, nullptr,                 "Invio"      , ""  },
  // { KEY_TYPE_SPECIAL, KEY_ESC,    nullptr,                 "Escape"     , ""  },
  { KEY_TYPE_LOOP,    '.',        nullptr,                 "KeyLoop"    , ""  },
  { KEY_TYPE_PASSWORD,  0,        nullptr,                 "PW Aizoon"  , "XXXXXX" },
  { KEY_TYPE_PASSWORD,  0,        nullptr,                 "PW ETT"     , "ABCDEF" },
  { KEY_TYPE_DUCKY_SCRIPT, 0,      nullptr,                 "DUCKY SCRIPT", "" },
};

const char* DUCKY_SCRIPT = R"DUCKY(
  STRING DPLab Script BT Keyboard v.01.4
  ENTER
  STRING btn 1
  ENTER

  VAR $TIME = 60*15
  VAR $LO = 10
  VAR $RIGHE = 0

  WHILE ($TIME > 0)
    STRING $RIGHE
    $LO = 10
      WHILE ($LO > 0)
      STRING .
      DELAY 1000
      $LO = $LO-1
    END_WHILE
    ENTER
    $TIME = $TIME-1
    IF ($RIGHE >= 10) THEN
      $RIGHE = 0
      CONTROL a
      DELAY 100
      DELETE
      DELAY 100
    END_IF
    $RIGHE = $RIGHE + 1
  END_WHILE
)DUCKY";

const int NUM_KEYS = sizeof(keys) / sizeof(keys[0]);
int selectedKey = 0;

bool lastConnected   = false;
bool feedbackVisible = false;
bool ducky_run = false;
unsigned long feedbackTime = 0;
const unsigned long FEEDBACK_DURATION = 600;
int iloop = 0;

// ── UI ────────────────────────────────────────────────────────
void drawUI() {
  int livello    = M5.Power.getBatteryLevel();   // 0-100
  bool carica    = M5.Power.isCharging();

  auto& lcd = M5.Display;
  lcd.fillScreen(TFT_BLACK);

  lcd.setTextColor(TFT_CYAN);
  lcd.setTextSize(1);
  lcd.setCursor(4, 4);
  lcd.print("BT Keyboard");

  lcd.setCursor(lcd.width() - 30, 4);
  lcd.printf("%d%%", livello);

  lcd.setCursor(lcd.width() - 40, 18);
  if (carica) {
    lcd.print("carica");
  }


  lcd.setCursor(4, 18);
  if (bleKeyboard.isConnected()) {
    lcd.setTextColor(TFT_GREEN);
    lcd.print("Connesso");
  } else {
    lcd.setTextColor(TFT_RED);
    lcd.print("In attesa...");
  }

  lcd.drawLine(0, 30, lcd.width(), 30, TFT_DARKGREY);

  lcd.setTextColor(TFT_YELLOW);
  lcd.setTextSize(1);
  lcd.setCursor(4, 36);
  lcd.print("Opzione:");
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(2);
  lcd.setCursor(4, 50);
  lcd.print(keys[selectedKey].label);

  if (feedbackVisible) {
    lcd.setTextColor(TFT_GREEN);
    lcd.setTextSize(1);
    lcd.setCursor(4, 80);
    lcd.print(">> INVIATO!");
  }

  lcd.drawLine(0, lcd.height() - 28, lcd.width(), lcd.height() - 28, TFT_DARKGREY);
  lcd.setTextSize(1);
  lcd.setTextColor(TFT_LIGHTGREY);
  lcd.setCursor(4, lcd.height() - 22);
  lcd.print("A=Invia  B=Cambia");
}

// ── Invio tasto ───────────────────────────────────────────────
void sendSelectedKey() {
  KeyEntry& k = keys[selectedKey];

  switch (k.type) {

    case KEY_TYPE_MEDIA:
      // I tasti media sono const uint8_t* — si passano direttamente a press()
      bleKeyboard.press(k.mediaCode);
      delay(50);
      bleKeyboard.release(k.mediaCode);
      break;

    case KEY_TYPE_SPECIAL:
      bleKeyboard.press(k.charCode);
      delay(50);
      bleKeyboard.releaseAll();
      break;

    case KEY_TYPE_CHAR:
      bleKeyboard.print((char)k.charCode);
      break;

    case KEY_TYPE_LOOP:
      iloop = 0;
      while (M5.BtnB.wasPressed() == false) {
        M5.update();
        bleKeyboard.print((char)k.charCode);
        delay(1000);
        if (iloop > 10) {
          bleKeyboard.press(KEY_RETURN);
          delay(50);
          bleKeyboard.releaseAll();
          iloop=0;
        }
        iloop++;
      }
      break;

    case KEY_TYPE_PASSWORD:
      for (int i = 0; i < strlen(k.password); i++) {
        bleKeyboard.print((char)k.password[i]);
        delay(50);
      }
      bleKeyboard.press(KEY_RETURN);
      delay(50);
      bleKeyboard.releaseAll();
      break;

    case KEY_TYPE_DUCKY_SCRIPT:
      ducky.run();
      ducky_run = true;
      break;
  }
}

// ── Setup ─────────────────────────────────────────────────────
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Display.setRotation(3);
  M5.Display.setBrightness(80);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(10, 30);
  M5.Display.print("Avvio BT...");

  bleKeyboard.begin();
  delay(500);
  drawUI();


  // ── Collega i callback ────────────────────────────────────────────────
  ducky.onSendKey = [](uint8_t modifier, uint8_t key) {
      if (!bleKeyboard.isConnected()) return;
      if (key == 0 && modifier == 0) {
          bleKeyboard.releaseAll();
          return;
      }
      // Premi
      KeyReport report;
      memset(&report, 0, sizeof(report));
      report.modifiers = modifier;
      report.keys[0]   = key;
      bleKeyboard.sendReport(&report);
      delay(20);
      bleKeyboard.releaseAll();
  };

  ducky.onSendChar = [](char c) {
      if (!bleKeyboard.isConnected()) return;
      bleKeyboard.print(c);
  };

  ducky.onSendString = [](const String &s) {
      if (!bleKeyboard.isConnected()) return;
      bleKeyboard.print(s);
  };

  ducky.onLog = [](const String &msg) {
      M5.Display.println(msg);
      Serial.println(msg);
  };

  // Carica lo script
  ducky.load(String(DUCKY_SCRIPT));
}

// ── Loop ──────────────────────────────────────────────────────
void loop() {
  M5.update();

  if (M5.BtnA.wasPressed()) {
    if (bleKeyboard.isConnected()) {
      sendSelectedKey();
      feedbackVisible = true;
      feedbackTime    = millis();
    } else {
      M5.Display.fillScreen(TFT_RED);
      delay(150);
    }
    drawUI();
  }

  if (M5.BtnB.wasPressed()) {
    if (ducky_run) {
      ducky.stop();
    }  
    selectedKey     = (selectedKey + 1) % NUM_KEYS;
    feedbackVisible = false;
    drawUI();
  }

  if (feedbackVisible && (millis() - feedbackTime > FEEDBACK_DURATION)) {
    feedbackVisible = false;
    drawUI();
  }

  bool nowConnected = bleKeyboard.isConnected();
  if (nowConnected != lastConnected) {
    lastConnected = nowConnected;
    drawUI();
  }

  delay(20);
}

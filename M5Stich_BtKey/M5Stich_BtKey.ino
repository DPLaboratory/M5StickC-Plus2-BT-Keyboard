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

BleKeyboard bleKeyboard("M5Stick Tastiera", "M5Stack", 100);

// ── Tipo di tasto ──────────────────────────────────────────────
enum KeyType { KEY_TYPE_CHAR, KEY_TYPE_SPECIAL, KEY_TYPE_MEDIA, KEY_TYPE_LOOP};

struct KeyEntry {
  KeyType        type;
  uint8_t        charCode;       // usato per KEY_TYPE_CHAR / KEY_TYPE_SPECIAL
  const uint8_t* mediaCode;      // usato per KEY_TYPE_MEDIA
  const char*    label;
};

// ── Tabella tasti ─────────────────────────────────────────────
KeyEntry keys[] = {
  { KEY_TYPE_MEDIA,   0,          KEY_MEDIA_PLAY_PAUSE,    "Play/Pause"  },
  { KEY_TYPE_MEDIA,   0,          KEY_MEDIA_NEXT_TRACK,    "Next Track"  },
  { KEY_TYPE_MEDIA,   0,          KEY_MEDIA_PREVIOUS_TRACK,"Prev Track"  },
  { KEY_TYPE_MEDIA,   0,          KEY_MEDIA_VOLUME_UP,     "Volume +"    },
  { KEY_TYPE_MEDIA,   0,          KEY_MEDIA_VOLUME_DOWN,   "Volume -"    },
  { KEY_TYPE_MEDIA,   0,          KEY_MEDIA_MUTE,          "Mute"        },
  { KEY_TYPE_CHAR,    'a',        nullptr,                 "Lettera A"   },
  { KEY_TYPE_CHAR,    ' ',        nullptr,                 "Spazio"      },
  { KEY_TYPE_SPECIAL, KEY_RETURN, nullptr,                 "Invio"       },
  { KEY_TYPE_SPECIAL, KEY_ESC,    nullptr,                 "Escape"      },
  { KEY_TYPE_LOOP,    '.',        nullptr,                 "KeyLoop"     },
};

const int NUM_KEYS = sizeof(keys) / sizeof(keys[0]);
int selectedKey = 0;

bool lastConnected   = false;
bool feedbackVisible = false;
unsigned long feedbackTime = 0;
const unsigned long FEEDBACK_DURATION = 600;

// ── UI ────────────────────────────────────────────────────────
void drawUI() {
  auto& lcd = M5.Display;
  lcd.fillScreen(TFT_BLACK);

  lcd.setTextColor(TFT_CYAN);
  lcd.setTextSize(1);
  lcd.setCursor(4, 4);
  lcd.print("BT Keyboard");

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
  lcd.print("Tasto:");
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
      while (M5.BtnB.wasPressed() == false) {
        bleKeyboard.print((char)k.charCode);
        delay(1000);
      }
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

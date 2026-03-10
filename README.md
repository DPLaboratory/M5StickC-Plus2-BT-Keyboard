# M5StickC-Plus2-BT-Keyboard

* Premi il tasto A (frontale) per inviare un tasto via BT
* Premi il tasto B (laterale) per cambiare il tasto da inviare
*
* Libreria richiesta: ESP32 BLE Keyboard
* Installa da Arduino Library Manager: "ESP32 BLE Keyboard" by T-vK

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

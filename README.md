# M5StickC-Plus2-BT-Keyboard

**M5StickC Plus2 – Emulazione Tastiera Bluetooth HID**

Trasforma un M5StickC Plus2 in una tastiera/telecomando Bluetooth HID, con selezione del tasto tramite il display e invio con la pressione di un pulsante.

---

## Funzionamento

| Pulsante | Funzione |
|---|---|
| **A** (frontale) | Invia via Bluetooth il tasto attualmente selezionato |
| **B** (laterale) | Passa al tasto successivo nella lista |

Il display mostra sempre:
- lo **stato della connessione** BT (verde = connesso, rosso = in attesa)
- il **tasto correntemente selezionato**
- un feedback visivo **">> INVIATO!"** dopo ogni invio riuscito

---

## Tasti disponibili

| Etichetta | Cosa invia |
|---|---|
| Play/Pause | Tasto media Play/Pause |
| Next Track | Tasto media Traccia successiva |
| Prev Track | Tasto media Traccia precedente |
| Volume + | Tasto media Volume su |
| Volume - | Tasto media Volume giù |
| Mute | Tasto media Muto |
| Lettera A | Carattere `a` |
| Spazio | Barra spaziatrice |
| Invio | Tasto Invio (Return) |
| Escape | Tasto Escape |
| KeyLoop | Invia `.` ogni secondo finché non si preme **B** |

---

## Librerie richieste

Installa le seguenti librerie tramite l'**Arduino Library Manager**:

1. **M5StickCPlus2** – by M5Stack (`M5StickCPlus2`)
2. **ESP32 BLE Keyboard** – by T-vK (`BleKeyboard`)

---

## Configurazione Arduino IDE

- **Board**: `M5StickC-Plus2` (pacchetto *M5Stack* for ESP32)
- **Sketch**: aprire `M5Stich_BtKey/M5Stich_BtKey.ino`

---

## Utilizzo

1. Caricare lo sketch sulla scheda.
2. Sul dispositivo ricevente (PC, smartphone…) abbinare il dispositivo Bluetooth **"M5Stick Tastiera"**.
3. Usare il pulsante **B** per selezionare il tasto desiderato.
4. Premere il pulsante **A** per inviarlo.

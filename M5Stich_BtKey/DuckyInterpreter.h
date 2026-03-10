#pragma once
/*
 * DuckyInterpreter.h
 * Libreria per interpretare DuckyScript su M5StickC Plus2
 * Supporta: comandi base, variabili, IF/ELSE, WHILE, funzioni, STRING_DELAY
 *
 * Dipendenze: M5StickCPlus2.h, BleKeyboard (o USB HID se preferito)
 * https://github.com/T-vK/ESP32-BLE-Keyboard
 */

#include <Arduino.h>
#include <map>
#include <vector>
#include <functional>

// ── Costanti ────────────────────────────────────────────────────────────────
#define DUCKY_MAX_VARS       32
#define DUCKY_MAX_CALL_DEPTH 16
#define DUCKY_DEFAULT_DELAY  0      // ms tra comandi (DEFAULT_DELAY)
#define DUCKY_STRING_DELAY   0      // ms tra caratteri (STRING_DELAY)

// ── Tipi ────────────────────────────────────────────────────────────────────
using SendKeyFn   = std::function<void(uint8_t modifier, uint8_t key)>;
using SendCharFn  = std::function<void(char c)>;
using SendStrFn   = std::function<void(const String &s)>;
using PrintLogFn  = std::function<void(const String &msg)>;

// ── Classe principale ────────────────────────────────────────────────────────
class DuckyInterpreter {
public:
    // Callbacks da impostare prima di run()
    SendKeyFn   onSendKey;   // invia modificatore + keycode HID
    SendCharFn  onSendChar;  // invia singolo carattere
    SendStrFn   onSendString;// invia stringa intera (opzionale, fallback su char)
    PrintLogFn  onLog;       // debug log

    DuckyInterpreter() {
        _defaultDelay  = DUCKY_DEFAULT_DELAY;
        _stringDelay   = DUCKY_STRING_DELAY;
        _jitter        = 0;
        _stopped       = false;
    }

    // ── Carica lo script ────────────────────────────────────────────────────
    void load(const String &script) {
        _lines.clear();
        _vars.clear();
        _functions.clear();
        _stopped = false;

        // Split per righe
        int start = 0;
        for (int i = 0; i <= (int)script.length(); i++) {
            if (i == (int)script.length() || script[i] == '\n') {
                String line = script.substring(start, i);
                line.trim();
                _lines.push_back(line);
                start = i + 1;
            }
        }
        _prescanFunctions();
    }

    // ── Esegui lo script ────────────────────────────────────────────────────
    void run() {
        _stopped = false;
        _execute(0, _lines.size());
    }

    // ── Stop asincrono ──────────────────────────────────────────────────────
    void stop() { _stopped = true; }

private:
    std::vector<String>          _lines;
    std::map<String, int>        _vars;       // variabili intere
    std::map<String, int>        _functions;  // nome → indice riga FUNCTION
    int                          _defaultDelay;
    int                          _stringDelay;
    int                          _jitter;
    bool                         _stopped;

    // ────────────────────────────────────────────────────────────────────────
    // Pre-scansione: individua le FUNCTION per poterle chiamare prima
    // ────────────────────────────────────────────────────────────────────────
    void _prescanFunctions() {
        for (int i = 0; i < (int)_lines.size(); i++) {
            String l = _lines[i];
            if (l.startsWith("FUNCTION ")) {
                String name = l.substring(9);
                name.trim();
                _functions[name] = i;
            }
        }
    }

    // ────────────────────────────────────────────────────────────────────────
    // Esecutore principale: righe da [from, to)
    // Ritorna l'indice della prossima riga da eseguire nel contesto padre
    // ────────────────────────────────────────────────────────────────────────
    int _execute(int from, int to) {
        int i = from;
        while (i < to && !_stopped) {
            String &line = _lines[i];

            // Riga vuota o commento
            if (line.length() == 0 || line.startsWith("REM") || line.startsWith("//")) {
                i++; continue;
            }

            // ── DEFAULT_DELAY ───────────────────────────────────────────
            if (line.startsWith("DEFAULT_DELAY ") || line.startsWith("DEFAULTDELAY ")) {
                _defaultDelay = _eval(line.substring(line.indexOf(' ') + 1));
                i++; continue;
            }

            // ── DEFAULT_JITTER ──────────────────────────────────────────
            if (line.startsWith("DEFAULT_JITTER ")) {
                _jitter = _eval(line.substring(15));
                i++; continue;
            }

            // ── DELAY ───────────────────────────────────────────────────
            if (line.startsWith("DELAY ")) {
                int ms = _eval(line.substring(6));
                delay(ms);
                i++; continue;
            }

            // ── STRING_DELAY ────────────────────────────────────────────
            if (line.startsWith("STRING_DELAY ")) {
                // Sintassi: STRING_DELAY <ms> <testo>
                String rest = line.substring(13);
                int sp = rest.indexOf(' ');
                int ms = _eval(rest.substring(0, sp));
                String txt = rest.substring(sp + 1);
                _sendStringWithDelay(txt, ms);
                i++; _applyDefaultDelay(); continue;
            }

            // ── STRING ──────────────────────────────────────────────────
            if (line.startsWith("STRING ")) {
                String txt = line.substring(7);
                _sendText(txt);
                i++; _applyDefaultDelay(); continue;
            }

            // ── STRINGLN ────────────────────────────────────────────────
            if (line.startsWith("STRINGLN ")) {
                String txt = line.substring(9);
                _sendText(txt);
                _sendKey(0, 0x28); // ENTER
                i++; _applyDefaultDelay(); continue;
            }

            // ── VAR ─────────────────────────────────────────────────────
            if (line.startsWith("VAR ")) {
                // VAR $nome = valore
                _parseVar(line.substring(4));
                i++; continue;
            }

            // ── Assegnazione diretta $VAR = expr ────────────────────────
            if (line.startsWith("$")) {
                _parseVar(line);
                i++; continue;
            }

            // ── IF / IF-ELSE ─────────────────────────────────────────────
            if (line.startsWith("IF ") || line == "IF") {
                i = _handleIf(i, to);
                continue;
            }

            // ── WHILE ───────────────────────────────────────────────────
            if (line.startsWith("WHILE ")) {
                i = _handleWhile(i, to);
                continue;
            }

            // ── FUNCTION ────────────────────────────────────────────────
            if (line.startsWith("FUNCTION ")) {
                // Salta il corpo finché END_FUNCTION
                i = _skipBlock(i + 1, "END_FUNCTION") + 1;
                continue;
            }

            // ── CALL ────────────────────────────────────────────────────
            if (line.startsWith("CALL ")) {
                String fname = line.substring(5);
                fname.trim();
                _callFunction(fname);
                i++; _applyDefaultDelay(); continue;
            }

            // ── WAIT_FOR_BUTTON_PRESS ────────────────────────────────────
            if (line == "WAIT_FOR_BUTTON_PRESS") {
                _waitButton();
                i++; continue;
            }

            // ── HOLD / RELEASE ───────────────────────────────────────────
            if (line.startsWith("HOLD ")) {
                _parseKeyCombo(line.substring(5), true);
                i++; _applyDefaultDelay(); continue;
            }
            if (line.startsWith("RELEASE ")) {
                _parseKeyCombo(line.substring(8), false);
                i++; _applyDefaultDelay(); continue;
            }

            // ── Tasti speciali / combo ───────────────────────────────────
            {
                uint8_t mod = 0, key = 0;
                if (_parseKeyLine(line, mod, key)) {
                    _sendKey(mod, key);
                    i++; _applyDefaultDelay(); continue;
                }
            }

            // Riga non riconosciuta → log e skip
            _log("UNKNOWN: " + line);
            i++;
        }
        return i;
    }

    // ────────────────────────────────────────────────────────────────────────
    // WHILE
    // ────────────────────────────────────────────────────────────────────────
    int _handleWhile(int whileLine, int to) {
        String cond = _lines[whileLine].substring(6); // dopo "WHILE "
        int bodyStart = whileLine + 1;
        int bodyEnd   = _skipBlock(bodyStart, "END_WHILE"); // indice END_WHILE

        while (!_stopped) {
            bool result = _evalCondition(cond);
            if (!result) break;
            _execute(bodyStart, bodyEnd);
        }
        return bodyEnd + 1; // continua dopo END_WHILE
    }

    // ────────────────────────────────────────────────────────────────────────
    // IF / ELSE IF / ELSE / END_IF
    // ────────────────────────────────────────────────────────────────────────
    int _handleIf(int ifLine, int to) {
        String cond = _lines[ifLine].substring(3); // dopo "IF "
        cond.trim();
        // Trova struttura: [IF .. THEN] body [ELSE IF .. | ELSE] [END_IF]
        // Cerchiamo THEN opzionale sulla stessa riga
        int thenPos = cond.indexOf(" THEN");
        if (thenPos >= 0) cond = cond.substring(0, thenPos);

        bool executed = _evalCondition(cond);

        // Raccoglie blocchi: { condition (vuota=else), startLine, endLine }
        struct Block { String cond; int start; int end; };
        std::vector<Block> blocks;

        int cur = ifLine + 1;
        blocks.push_back({cond, cur, -1});

        // Scansiona per ELSE IF / ELSE / END_IF (gestendo IF annidati)
        int depth = 1;
        while (cur < to) {
            String &l = _lines[cur];
            if (l.startsWith("IF ")) depth++;
            if (l == "END_IF" || l == "ENDIF") {
                depth--;
                if (depth == 0) {
                    blocks.back().end = cur;
                    break;
                }
            }
            if (depth == 1 && (l.startsWith("ELSE IF ") || l.startsWith("ELSE_IF "))) {
                blocks.back().end = cur;
                String c2 = l.substring(8);
                c2.trim();
                blocks.push_back({c2, cur + 1, -1});
            }
            if (depth == 1 && l == "ELSE") {
                blocks.back().end = cur;
                blocks.push_back({"", cur + 1, -1});
            }
            cur++;
        }

        int endIfLine = cur; // indice END_IF

        // Esegui il primo blocco la cui condizione è vera
        bool done = false;
        for (auto &b : blocks) {
            if (!done) {
                bool run = (b.cond == "") ? !done : _evalCondition(b.cond);
                // Per il primo IF la condizione è già valutata
                if (&b == &blocks[0]) run = executed;
                if (run && b.end >= 0) {
                    _execute(b.start, b.end);
                    done = true;
                }
            }
        }
        return endIfLine + 1;
    }

    // ────────────────────────────────────────────────────────────────────────
    // Salta un blocco cercando il tag di chiusura al giusto livello di nesting
    // Ritorna l'indice della riga del tag di chiusura
    // ────────────────────────────────────────────────────────────────────────
    int _skipBlock(int start, const String &endTag) {
        // Determina il tag di apertura corrispondente
        String openTag = "";
        if (endTag == "END_WHILE")    openTag = "WHILE ";
        if (endTag == "END_FUNCTION") openTag = "FUNCTION ";
        if (endTag == "END_IF" || endTag == "ENDIF") openTag = "IF ";

        int depth = 1;
        int i = start;
        while (i < (int)_lines.size()) {
            String &l = _lines[i];
            if (openTag.length() && l.startsWith(openTag)) depth++;
            if (l == endTag || l == endTag.substring(0, endTag.indexOf('_'))) {
                depth--;
                if (depth == 0) return i;
            }
            i++;
        }
        return i; // non trovato → fine script
    }

    // ────────────────────────────────────────────────────────────────────────
    // FUNCTION call
    // ────────────────────────────────────────────────────────────────────────
    void _callFunction(const String &name) {
        if (_functions.count(name) == 0) {
            _log("CALL: funzione non trovata: " + name);
            return;
        }
        int fLine = _functions[name];
        int bodyStart = fLine + 1;
        int bodyEnd   = _skipBlock(bodyStart, "END_FUNCTION");
        _execute(bodyStart, bodyEnd);
    }

    // ────────────────────────────────────────────────────────────────────────
    // Valuta una condizione booleana
    // Supporta: ==, !=, <, >, <=, >=
    // Supporta variabili $VAR e valori numerici
    // ────────────────────────────────────────────────────────────────────────
    bool _evalCondition(const String &cond) {
        // Operatori in ordine di priorità
        const char* ops[] = {"==","!=","<=",">=","<",">", nullptr};
        for (int o = 0; ops[o]; o++) {
            int pos = cond.indexOf(ops[o]);
            if (pos < 0) continue;
            String lhs = cond.substring(0, pos);
            String rhs = cond.substring(pos + strlen(ops[o]));
            lhs.trim(); rhs.trim();
            int l = _eval(lhs), r = _eval(rhs);
            if (strcmp(ops[o],"==") == 0) return l == r;
            if (strcmp(ops[o],"!=") == 0) return l != r;
            if (strcmp(ops[o],"<=") == 0) return l <= r;
            if (strcmp(ops[o],">=") == 0) return l >= r;
            if (strcmp(ops[o],"<")  == 0) return l <  r;
            if (strcmp(ops[o],">")  == 0) return l >  r;
        }
        // Nessun operatore: vero se != 0
        return _eval(cond) != 0;
    }

    // ────────────────────────────────────────────────────────────────────────
    // Valuta un'espressione: supporta +, -, *, /, %, variabili $VAR, letterali
    // ────────────────────────────────────────────────────────────────────────
    int _eval(const String &expr) {
        String e = expr;
        e.trim();

        // Operatori binari (precedenza semplice: prima +/- poi */%  )
        for (int i = e.length() - 1; i >= 0; i--) {
            char c = e[i];
            if ((c == '+' || c == '-') && i > 0) {
                int l = _eval(e.substring(0, i));
                int r = _eval(e.substring(i + 1));
                return (c == '+') ? l + r : l - r;
            }
        }
        for (int i = e.length() - 1; i >= 0; i--) {
            char c = e[i];
            if (c == '*' || c == '/' || c == '%') {
                int l = _eval(e.substring(0, i));
                int r = _eval(e.substring(i + 1));
                if (c == '*') return l * r;
                if (c == '/') return r != 0 ? l / r : 0;
                if (c == '%') return r != 0 ? l % r : 0;
            }
        }

        // Variabile $NAME
        if (e.startsWith("$")) {
            String name = e.substring(1);
            name.trim();
            if (_vars.count(name)) return _vars[name];
            return 0;
        }

        // TRUE / FALSE
        if (e.equalsIgnoreCase("TRUE"))  return 1;
        if (e.equalsIgnoreCase("FALSE")) return 0;

        // Letterale intero
        return e.toInt();
    }

    // ────────────────────────────────────────────────────────────────────────
    // Parsing VAR $nome = espressione   oppure   $nome = espressione
    //         VAR $nome += espressione  (operatori composti)
    // ────────────────────────────────────────────────────────────────────────
    void _parseVar(const String &s) {
        // cerca operatori composti prima
        const char* cops[] = {"+=","-=","*=","/=", nullptr};
        for (int o = 0; cops[o]; o++) {
            int pos = s.indexOf(cops[o]);
            if (pos < 0) continue;
            String lhs = s.substring(0, pos);
            String rhs = s.substring(pos + 2);
            lhs.trim(); rhs.trim();
            if (lhs.startsWith("$")) lhs = lhs.substring(1);
            if (lhs.startsWith("VAR ")) lhs = lhs.substring(4);
            lhs.trim();
            int cur = _vars.count(lhs) ? _vars[lhs] : 0;
            int val = _eval(rhs);
            if (cops[o][0] == '+') _vars[lhs] = cur + val;
            else if (cops[o][0] == '-') _vars[lhs] = cur - val;
            else if (cops[o][0] == '*') _vars[lhs] = cur * val;
            else if (cops[o][0] == '/') _vars[lhs] = val ? cur / val : 0;
            return;
        }
        // Assegnazione semplice =
        int pos = s.indexOf('=');
        if (pos < 0) return;
        String lhs = s.substring(0, pos);
        String rhs = s.substring(pos + 1);
        lhs.trim(); rhs.trim();
        if (lhs.startsWith("$")) lhs = lhs.substring(1);
        if (lhs.startsWith("VAR ")) lhs = lhs.substring(4);
        lhs.trim();
        _vars[lhs] = _eval(rhs);
    }

    // ────────────────────────────────────────────────────────────────────────
    // Invio testo carattere per carattere con _stringDelay
    // ────────────────────────────────────────────────────────────────────────
    void _sendText(const String &txt) {
        if (_stringDelay == 0 && onSendString) {
            onSendString(txt);
        } else {
            for (int i = 0; i < (int)txt.length(); i++) {
                if (onSendChar) onSendChar(txt[i]);
                if (_stringDelay > 0) delay(_stringDelay);
            }
        }
    }

    void _sendStringWithDelay(const String &txt, int ms) {
        for (int i = 0; i < (int)txt.length(); i++) {
            if (onSendChar) onSendChar(txt[i]);
            delay(ms);
        }
    }

    void _sendKey(uint8_t mod, uint8_t key) {
        if (onSendKey) onSendKey(mod, key);
    }

    void _applyDefaultDelay() {
        int d = _defaultDelay;
        if (_jitter > 0) d += random(0, _jitter);
        if (d > 0) delay(d);
    }

    void _log(const String &msg) {
        if (onLog) onLog(msg);
    }

    void _waitButton() {
        // Implementazione dipende dall'hardware; qui polling sul pulsante A
        // L'utente può sovrascrivere questa funzione
        while (!_stopped) {
            if (digitalRead(37) == LOW) { delay(50); break; } // BTN_A M5StickCPlus2
            delay(10);
        }
    }

    // ────────────────────────────────────────────────────────────────────────
    // Parsing tasto/combo → codici HID
    // Ritorna true se la riga è riconosciuta come comando tasto
    // ────────────────────────────────────────────────────────────────────────
    bool _parseKeyLine(const String &line, uint8_t &mod, uint8_t &key) {
        // HID modifier masks (USB HID spec)
        // LEFT_CTRL=0x01, LEFT_SHIFT=0x02, LEFT_ALT=0x04, LEFT_GUI=0x08
        static const uint8_t MOD_CTRL  = 0x01;
        static const uint8_t MOD_SHIFT = 0x02;
        static const uint8_t MOD_ALT   = 0x04;
        static const uint8_t MOD_GUI   = 0x08;

        mod = 0; key = 0;
        String l = line;
        l.toUpperCase();

        // Estrai modificatori
        auto consumeMod = [&](const String &token) -> bool {
            if (token == "CTRL" || token == "CONTROL") { mod |= MOD_CTRL; return true; }
            if (token == "SHIFT")   { mod |= MOD_SHIFT; return true; }
            if (token == "ALT")     { mod |= MOD_ALT;   return true; }
            if (token == "GUI" || token == "WINDOWS" || token == "COMMAND") { mod |= MOD_GUI; return true; }
            return false;
        };

        // Spacca la riga in token
        std::vector<String> tokens;
        int s = 0;
        for (int i = 0; i <= (int)l.length(); i++) {
            if (i == (int)l.length() || l[i] == ' ') {
                if (i > s) tokens.push_back(l.substring(s, i));
                s = i + 1;
            }
        }

        for (auto &t : tokens) consumeMod(t);

        // L'ultimo token non-modifier è il tasto
        String keyToken = "";
        for (int i = tokens.size() - 1; i >= 0; i--) {
            if (!consumeMod(tokens[i])) { keyToken = tokens[i]; break; }
        }

        key = _keyNameToHID(keyToken);
        return (key != 0 || mod != 0);
    }

    void _parseKeyCombo(const String &combo, bool hold) {
        uint8_t mod = 0, key = 0;
        _parseKeyLine(combo, mod, key);
        if (hold)  _sendKey(mod, key);
        else       _sendKey(0, 0); // RELEASE: invia 0
    }

    // ── Mappa nomi tasto → codici HID Usage ID ──────────────────────────────
    uint8_t _keyNameToHID(const String &name) {
        struct { const char* n; uint8_t h; } map[] = {
            // Funzione
            {"F1",0x3A},{"F2",0x3B},{"F3",0x3C},{"F4",0x3D},
            {"F5",0x3E},{"F6",0x3F},{"F7",0x40},{"F8",0x41},
            {"F9",0x42},{"F10",0x43},{"F11",0x44},{"F12",0x45},
            // Navigazione
            {"ENTER",0x28},{"RETURN",0x28},
            {"ESCAPE",0x29},{"ESC",0x29},
            {"BACKSPACE",0x2A},{"DELETE",0x4C},
            {"TAB",0x2B},{"SPACE",0x2C},
            {"CAPSLOCK",0x39},
            {"INSERT",0x49},{"HOME",0x4A},{"PAGEUP",0x4B},
            {"END",0x4D},{"PAGEDOWN",0x4E},
            {"RIGHT",0x4F},{"LEFT",0x50},{"DOWN",0x51},{"UP",0x52},
            {"NUMLOCK",0x53},{"PRINTSCREEN",0x46},{"SCROLLLOCK",0x47},{"PAUSE",0x48},
            {"MENU",0x65},{"APP",0x65},
            // Numpad
            {"NUMPAD0",0x62},{"NUMPAD1",0x59},{"NUMPAD2",0x5A},
            {"NUMPAD3",0x5B},{"NUMPAD4",0x5C},{"NUMPAD5",0x5D},
            {"NUMPAD6",0x5E},{"NUMPAD7",0x5F},{"NUMPAD8",0x60},
            {"NUMPAD9",0x61},{"NUMPADENTER",0x58},
            {nullptr,0}
        };
        for (int i = 0; map[i].n; i++) {
            if (name == map[i].n) return map[i].h;
        }
        // Singolo carattere ASCII → HID (lettere a-z)
        if (name.length() == 1) {
            char c = name[0];
            if (c >= 'A' && c <= 'Z') return 0x04 + (c - 'A');
            if (c >= '1' && c <= '9') return 0x1E + (c - '1');
            if (c == '0') return 0x27;
        }
        return 0;
    }
};

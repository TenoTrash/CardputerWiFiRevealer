/*

Teno Wifi Revealer 2026
=======================

Sencillo escaner de WiFi para M5 Cardputer ADV
Se debe proveer el archivo .potfile y guardarlo en la microSD

Muchas ideas sacadas de M5 Evil Cardputer!
https://github.com/7h30th3r0n3/Evil-M5Project

Tuve que recurrir a Claude para que me arme algo simple con el struct del potfile porque programo para el ojete

*/

#include <M5Cardputer.h>
#include <WiFi.h>
#include <SD.h>

/*
  Colores:
    BLACK
    NAVY
    DARKGREEN
    DARKCYAN
    MAROON
    PURPLE
    OLIVE
    LIGHTGREY
    DARKGREY
    BLUE
    GREEN
    CYAN
    RED
    MAGENTA
    YELLOW
    WHITE
    ORANGE
    GREENYELLOW
    PINK
*/

#define COLOR_BG       BLACK
#define COLOR_TITLE    GREENYELLOW
#define COLOR_NORMAL   WHITE
#define COLOR_FOUND    GREEN
#define COLOR_DETAIL   YELLOW
#define COLOR_SCROLL   DARKGREY

// Struct de las WiFis
struct WifiNet {
  String ssid;
  int    rssi;
  bool   cracked;
  String password;
};

// Globales (que pueden estar en otro lado...)
std::vector<WifiNet> nets;
int scrollOffset = 0;
int selectedIdx  = 0;
bool showingPass = false;

const int LINES_PER_PAGE = 7;

// Variables para el scroll del SSID seleccionado
int     ssidScrollPos    = 0;       // posición actual del scroll (en caracteres)
unsigned long lastSsidScroll = 0;   // timestamp del último avance
const int SSID_SCROLL_DELAY = 300;  // ms entre cada paso de scroll
const int SSID_MAX_VISIBLE  = 16;   // caracteres visibles en la fila

// Animación de escaneo
void drawScanProgress(int current, int total) {
  static const char* spinner[] = { "|", "/", "—", "\\" };
  static int spinIdx = 0;

  int lineH = M5Cardputer.Display.fontHeight();
  int w = M5Cardputer.Display.width();
  int h = M5Cardputer.Display.height();

  int barW = w - 20;
  int barX = 10;
  int barY = h / 2;
  int barH = 10;

  M5Cardputer.Display.drawRect(barX, barY, barW, barH, WHITE);

  if (total > 0) {
    int fill = (barW - 2) * current / total;
    M5Cardputer.Display.fillRect(barX + 1, barY + 1, fill, barH - 2, CYAN);
  } else {
    static int pulse = 0;
    int fill = (barW - 2) * (pulse % 100) / 100;
    M5Cardputer.Display.fillRect(barX + 1, barY + 1, barW - 2, barH - 2, COLOR_BG);
    M5Cardputer.Display.fillRect(barX + 1, barY + 1, fill, barH - 2, CYAN);
    pulse += 7;
  }

  M5Cardputer.Display.fillRect(0, barY + barH + 4, w, lineH * 2, COLOR_BG);
  M5Cardputer.Display.setTextColor(COLOR_TITLE);
  M5Cardputer.Display.setCursor(barX, barY + barH + 6);
  if (total > 0) {
    M5Cardputer.Display.printf("%s Verificando %d/%d...", spinner[spinIdx], current, total);
  } else {
    M5Cardputer.Display.printf("%s Escaneando canales...", spinner[spinIdx]);
  }

  spinIdx = (spinIdx + 1) % 4;
}

// Busca el SSID en el potfile
bool lookupPotfile(const String& ssid, String& outPass) {
  File f = SD.open("/wpa-sec.founds.potfile", FILE_READ);
  if (!f) return false;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    int p0 = line.indexOf(':');
    if (p0 < 0) continue;
    int p1 = line.indexOf(':', p0 + 1);
    if (p1 < 0) continue;
    int p2 = line.indexOf(':', p1 + 1);
    if (p2 < 0) continue;

    String fileSsid = line.substring(p1 + 1, p2);
    String filePass = line.substring(p2 + 1);
    filePass.trim();

    if (fileSsid == ssid) {
      outPass = filePass;
      f.close();
      return true;
    }
  }
  f.close();
  return false;
}

// Escaneo WiFi con animación para que no suene a que se colgó
void doScan() {
  nets.clear();
  scrollOffset = 0;
  selectedIdx  = 0;
  showingPass  = false;

  M5Cardputer.Display.fillScreen(COLOR_BG);
  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setTextColor(COLOR_TITLE);
  M5Cardputer.Display.setCursor(0, 10);
  M5Cardputer.Display.println(" Teno WiFi Scanner");
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(COLOR_NORMAL);
  M5Cardputer.Display.setCursor(0, 38);
  M5Cardputer.Display.println(" Buscando redes...");

  WiFi.scanNetworks(true);

  int n = -1;
  while (n < 0) {
    drawScanProgress(0, 0);
    delay(200);
    n = WiFi.scanComplete();
  }

  // Fase 2 de verificación contra el potfile
  M5Cardputer.Display.fillScreen(COLOR_BG);
  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setTextColor(COLOR_FOUND);
  M5Cardputer.Display.setCursor(0, 10);
  M5Cardputer.Display.println(" Teno WiFi Scanner");
  M5Cardputer.Display.setTextSize(1);

  for (int i = 0; i < n; i++) {
    drawScanProgress(i + 1, n);
    delay(60);

    WifiNet w;
    w.ssid    = WiFi.SSID(i);
    w.rssi    = WiFi.RSSI(i);
    w.cracked = lookupPotfile(w.ssid, w.password);
    nets.push_back(w);
  }

  WiFi.scanDelete();
}

// Dibuja una sola fila del listado (para redibujar solo la línea que scrollea)
void drawRow(int idx, int y, bool selected) {
  int w     = M5Cardputer.Display.width();
  int lineH = M5Cardputer.Display.fontHeight();

  // Limpiar la fila
  M5Cardputer.Display.fillRect(0, y, w - 4, lineH, COLOR_BG);

  if (selected) {
    M5Cardputer.Display.fillRect(0, y, w - 4, lineH, DARKGREY);
  }

  WifiNet& net = nets[idx];
  uint16_t col = net.cracked ? COLOR_FOUND : COLOR_NORMAL;
  M5Cardputer.Display.setTextColor(col);
  M5Cardputer.Display.setCursor(2, y);

  if (selected && (int)net.ssid.length() > SSID_MAX_VISIBLE) {
    // SSID largo seleccionado: mostrar ventana deslizante
    // Espacios al final para que haya pausa antes de reiniciar
    String padded = net.ssid + "   ";
    int len = padded.length();
    String visible = "";
    for (int c = 0; c < SSID_MAX_VISIBLE; c++) {
      visible += padded[(ssidScrollPos + c) % len];
    }
    M5Cardputer.Display.printf("%-16s%4d", visible.c_str(), net.rssi);
  } else {
    // SSID corto o no seleccionado: mostrar normal truncado
    String ssidShort = net.ssid;
    if (ssidShort.length() > 19) ssidShort = ssidShort.substring(0, 18) + "~";
    M5Cardputer.Display.printf("%-16s%4d", ssidShort.c_str(), net.rssi);
  }
}

// Dibuja la lista ->
void drawList() {
  // Resetear scroll del SSID al redibujar la lista completa
  ssidScrollPos = 0;
  lastSsidScroll = millis();

  M5Cardputer.Display.fillScreen(COLOR_BG);
  M5Cardputer.Display.setTextSize(1.7);

  int lineH = M5Cardputer.Display.fontHeight();
  int w     = M5Cardputer.Display.width();

  M5Cardputer.Display.setTextColor(COLOR_TITLE);
  M5Cardputer.Display.setCursor(0, 0);
  M5Cardputer.Display.printf(" Teno WiFi Scanner [%d]\n", (int)nets.size());

  int y = lineH + 3;

  for (int i = 0; i < LINES_PER_PAGE; i++) {
    int idx = scrollOffset + i;
    if (idx >= (int)nets.size()) break;
    drawRow(idx, y, idx == selectedIdx);
    y += lineH + 2;
  }

  if ((int)nets.size() > LINES_PER_PAGE) {
    int totalH = M5Cardputer.Display.height() - lineH - 4;
    int barY0  = lineH + 3;
    int trkH   = max(4, totalH * LINES_PER_PAGE / (int)nets.size());
    int trkY   = barY0 + totalH * scrollOffset / (int)nets.size();
    int barX   = w - 3;
    M5Cardputer.Display.drawFastVLine(barX, barY0, totalH, COLOR_SCROLL);
    M5Cardputer.Display.drawFastVLine(barX, trkY, trkH, CYAN);
  }

  int footerY = M5Cardputer.Display.height() - M5Cardputer.Display.fontHeight();
  M5Cardputer.Display.setTextSize(1.3);
  M5Cardputer.Display.setTextColor(COLOR_SCROLL);
  M5Cardputer.Display.setCursor(0, footerY);
  M5Cardputer.Display.print(" ^/v: scroll  Ent: pass  R: scan");
}

// Muestra contraseña
void drawPassword() {
  if (selectedIdx < 0 || selectedIdx >= (int)nets.size()) return;
  WifiNet& net = nets[selectedIdx];

  M5Cardputer.Display.fillScreen(COLOR_BG);
  M5Cardputer.Display.setTextSize(1.6);

  M5Cardputer.Display.setTextColor(COLOR_TITLE);
  M5Cardputer.Display.setCursor(0, 0);
  M5Cardputer.Display.println(" Credenciales halladas:");

  M5Cardputer.Display.setTextColor(COLOR_NORMAL);
  M5Cardputer.Display.setTextSize(1.3);
  M5Cardputer.Display.printf("\nSSID:\n");
  M5Cardputer.Display.setTextSize(1.4);
  M5Cardputer.Display.printf("  %s\n", net.ssid.c_str());
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.printf("RSSI: %d dBm\n", net.rssi);

  if (net.cracked) {
    M5Cardputer.Display.setTextColor(COLOR_FOUND);
    M5Cardputer.Display.setTextSize(1.3);
    M5Cardputer.Display.printf("\nPASS:\n");
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(COLOR_DETAIL);
    M5Cardputer.Display.printf("  %s\n", net.password.c_str());
  } else {
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COLOR_SCROLL);
    M5Cardputer.Display.println("\nSin coincidencia\nen potfile.");
  }

  int lineH = M5Cardputer.Display.fontHeight();
  M5Cardputer.Display.setTextSize(1.4);
  M5Cardputer.Display.setTextColor(COLOR_SCROLL);
  M5Cardputer.Display.setCursor(0, M5Cardputer.Display.height() - lineH);
  M5Cardputer.Display.print(" Enter/Del: volver");
}

// Setup 
void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.fillScreen(COLOR_BG);

  if (!SD.begin()) {
    M5Cardputer.Display.setTextSize(1.5);
    M5Cardputer.Display.setTextColor(RED);
    M5Cardputer.Display.setCursor(0, 0);
    M5Cardputer.Display.println("SD no\nencontrada!");
    delay(2000);
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  doScan();
  drawList();
}

// Loop principal
void loop() {
  M5Cardputer.update();

  // Scroll automático del SSID seleccionado (solo en la lista, no en la pantalla de pass)
  if (!showingPass && !nets.empty()) {
    WifiNet& selNet = nets[selectedIdx];
    if ((int)selNet.ssid.length() > SSID_MAX_VISIBLE) {
      unsigned long now = millis();
      if (now - lastSsidScroll >= SSID_SCROLL_DELAY) {
        lastSsidScroll = now;

        // Avanzar un carácter y dar vuelta con pausa (3 espacios de padding)
        String padded = selNet.ssid + "   ";
        ssidScrollPos = (ssidScrollPos + 1) % padded.length();

        // Calcular la posición Y de la fila seleccionada y redibujarla sola
        M5Cardputer.Display.setTextSize(1.7);
        int lineH = M5Cardputer.Display.fontHeight();
        int rowY  = (lineH + 3) + (selectedIdx - scrollOffset) * (lineH + 2);
        drawRow(selectedIdx, rowY, true);
      }
    }
  }

  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    Keyboard_Class::KeysState ks = M5Cardputer.Keyboard.keysState();

    if (showingPass) {
      if (ks.enter || ks.del) {
        showingPass = false;
        drawList();
      }
    } else {
      bool up = false, down = false;
      for (auto k : ks.word) {
        if (k == ';') up   = true;
        if (k == '.') down = true;
      }

      if (up && selectedIdx > 0) {
        selectedIdx--;
        if (selectedIdx < scrollOffset) scrollOffset = selectedIdx;
        drawList();
      } else if (down && selectedIdx < (int)nets.size() - 1) {
        selectedIdx++;
        if (selectedIdx >= scrollOffset + LINES_PER_PAGE)
          scrollOffset = selectedIdx - LINES_PER_PAGE + 1;
        drawList();
      } else if (ks.enter) {
        showingPass = true;
        drawPassword();
      } else {
        for (auto k : ks.word) {
          if (k == 'r' || k == 'R') {
            doScan();
            drawList();
            break;
          }
        }
      }
    }
  }
}

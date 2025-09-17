#include <avr/wdt.h>  // for software reset
#define RESET_BUTTON_PIN 7  // button connected between pin 7 and GND

#include <usbhid.h>
#include <usbhub.h>
#include <hiduniversal.h>
#include <hidboot.h>
#include <SPI.h>
#include <Keyboard.h>

#define TRIGGER_PIN 6       // 5V output pin
#define LED_PIN 8           // LED blink pin

unsigned long triggerStart = 0;
bool triggerActive = false;
const unsigned long triggerDuration = 3000; // 3 seconds

// LED blink control
unsigned long ledLastToggle = 0;
int ledBlinkCount = 0;
bool ledBlinking = false;
const unsigned long ledInterval = 200; // ms per ON/OFF step

String scannedData = "";

class MyParser : public HIDReportParser {
  public:
    MyParser();
    void Parse(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf);

  protected:
    uint8_t KeyToAscii(uint8_t mod, uint8_t key);
    virtual void OnKeyScanned(uint8_t mod, uint8_t key);
    virtual void OnScanFinished();
};

MyParser::MyParser() {}

void MyParser::Parse(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf) {
  if (buf[2] == 1 || buf[2] == 0) return;

  for (int i = 7; i >= 2; i--) {
    if (buf[i] == 0) continue;

    if (buf[i] == UHS_HID_BOOT_KEY_ENTER) {
      OnScanFinished();
    } else {
      OnKeyScanned(buf[0], buf[i]);
    }
    return;
  }
}

uint8_t MyParser::KeyToAscii(uint8_t mod, uint8_t key) {
  bool shift = (mod & (0x22));

  if (key >= 0x04 && key <= 0x1D) return shift ? (key - 0x04 + 'A') : (key - 0x04 + 'a');
  if (key >= 0x1E && key <= 0x27) {
    const char shiftNums[] = { '!', '@', '#', '$', '%', '^', '&', '*', '(', ')' };
    return shift ? shiftNums[key - 0x1E] : (key == 0x27 ? '0' : key - 0x1E + '1');
  }

  if (key == 0x2C) return ' ';
  if (key == 0x2D) return shift ? '_' : '-';
  if (key == 0x2E) return shift ? '+' : '=';
  if (key == 0x2F) return shift ? '{' : '[';
  if (key == 0x30) return shift ? '}' : ']';
  if (key == 0x31) return shift ? '|' : '\\';
  if (key == 0x33) return shift ? ':' : ';';
  if (key == 0x34) return shift ? '"' : '\'';
  if (key == 0x36) return shift ? '<' : ',';
  if (key == 0x37) return shift ? '>' : '.';
  if (key == 0x38) return shift ? '?' : '/';

  return 0;
}

void MyParser::OnKeyScanned(uint8_t mod, uint8_t key) {
  uint8_t ascii = KeyToAscii(mod, key);
  if (ascii) {
    scannedData += (char)ascii;
    Serial.print((char)ascii);
  }
}

void MyParser::OnScanFinished() {
  Serial.println(" - Finished");

  Keyboard.print(scannedData);
  Keyboard.write(KEY_RETURN);

  // Trigger output
  digitalWrite(TRIGGER_PIN, HIGH);
  triggerStart = millis();
  triggerActive = true;

  // Start LED blink (3 times)
  ledBlinking = true;
  ledBlinkCount = 0;
  ledLastToggle = millis();
  digitalWrite(LED_PIN, HIGH);

  scannedData = "";
}

USB Usb;
USBHub Hub(&Usb);
HIDUniversal Hid(&Usb);
MyParser Parser;

void setup() {
  Serial.begin(115200);
  Keyboard.begin();

  if (Usb.Init() == -1) {
    Serial.println("USB Host Shield did not start.");
  } else {
    Serial.println("USB Host Shield initialized.");
  }

  Hid.SetReportParser(0, &Parser);

  pinMode(TRIGGER_PIN, OUTPUT);
  digitalWrite(TRIGGER_PIN, LOW);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  Serial.println("Ready to scan...");
}

void loop() {
  // check reset button
  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
    delay(50); // debounce
    if (digitalRead(RESET_BUTTON_PIN) == LOW) {
      softwareReset(); // trigger reset
    }
  }

  Usb.Task();

  // handle trigger pin timeout
  if (triggerActive && (millis() - triggerStart >= triggerDuration)) {
    digitalWrite(TRIGGER_PIN, LOW);
    triggerActive = false;
  }

  // handle LED blinking (3 blinks)
  if (ledBlinking && (millis() - ledLastToggle >= ledInterval)) {
    ledLastToggle = millis();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // toggle LED
    ledBlinkCount++;

    if (ledBlinkCount >= 6) { // 6 toggles = 3 full blinks
      ledBlinking = false;
      digitalWrite(LED_PIN, LOW); // ensure LED is off
    }
  }

  // Recover from USB error
  if (Usb.getUsbTaskState() == USB_STATE_ERROR) {
    Serial.println("USB error detected. Reinitializing...");
    Usb.Init();
  }
}

// Function to reset using watchdog timer
void softwareReset() {
  wdt_enable(WDTO_15MS); // enable watchdog with 15ms timeout
  while (1) {}           // wait for watchdog to reset
}

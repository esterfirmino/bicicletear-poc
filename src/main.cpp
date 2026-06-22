#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// BLE
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcd1234-1234-1234-1234-abcdef123456"

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("BLE conectado");
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("BLE desconectado");
    pServer->startAdvertising();
  }
};

// Pinos
#define PIN_GUIDAO 27
#define PIN_VEL    32

#define PIN_START  26
#define PIN_STOP   33
#define PIN_BTN3   14
#define PIN_BTN4   13

enum EstadoSistema {
  IDLE,
  RUNNING,
  REPORT
};

EstadoSistema estado = IDLE;

// Dados
float angulo = 0;
float velocidade = 0;

bool startPress = false;
bool stopPress = false;
bool btn3Press = false;
bool btn4Press = false;

// Contadores
unsigned long pacotes = 0;
unsigned long contBtn3 = 0;
unsigned long contBtn4 = 0;

unsigned long contDireita = 0;
unsigned long contCentro = 0;
unsigned long contEsquerda = 0;
unsigned long contVelAmostras = 0;

// Timers
unsigned long tBluetooth = 0;
unsigned long tLCD = 0;

const unsigned long INTERVALO_BT = 50;
const unsigned long INTERVALO_LCD = 500;

// Protótipos
void lerSensores();
void lerBotoes();
void enviarBluetooth();
void atualizarLCD();
String direcaoGuidao();
void resetarRelatorio();
void enviarBLE(String msg);
void iniciarBLE();

void setup() {
  Serial.begin(115200);

  pinMode(PIN_GUIDAO, INPUT);
  pinMode(PIN_VEL, INPUT);

  pinMode(PIN_START, INPUT_PULLUP);
  pinMode(PIN_STOP, INPUT_PULLUP);
  pinMode(PIN_BTN3, INPUT_PULLUP);
  pinMode(PIN_BTN4, INPUT_PULLUP);

  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("BicicleTEAr");
  lcd.setCursor(0, 1);
  lcd.print("Aguardando...");

  iniciarBLE();

  Serial.println("Sistema pronto");
}

void loop() {
  lerBotoes();
  lerSensores();

  if (estado == RUNNING) {
    enviarBluetooth();
  }

  atualizarLCD();
}

void iniciarBLE() {
  BLEDevice::init("BicicleTEAr_BLE");

  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  pCharacteristic->addDescriptor(new BLE2902());

  pCharacteristic->setValue("BicicleTEAr pronto");

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->start();

  Serial.println("BLE iniciado: BicicleTEAr_BLE");
}

void enviarBLE(String msg) {
  if (deviceConnected) {
    pCharacteristic->setValue(msg.c_str());
    pCharacteristic->notify();
  }

  Serial.println(msg);
}

void lerBotoes() {
  static bool antStart = HIGH;
  static bool antStop = HIGH;
  static bool antBtn3 = HIGH;
  static bool antBtn4 = HIGH;

  bool startAtual = digitalRead(PIN_START);
  bool stopAtual = digitalRead(PIN_STOP);
  bool btn3Atual = digitalRead(PIN_BTN3);
  bool btn4Atual = digitalRead(PIN_BTN4);

  startPress = startAtual == LOW;
  stopPress = stopAtual == LOW;
  btn3Press = btn3Atual == LOW;
  btn4Press = btn4Atual == LOW;

  if (antStart == HIGH && startAtual == LOW) {
    resetarRelatorio();
    estado = RUNNING;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Iniciando...");

    enviarBLE("EVENT,START");

    delay(500);
  }

  if (antStop == HIGH && stopAtual == LOW) {
    estado = REPORT;

    enviarBLE("EVENT,STOP");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sessao");
    lcd.setCursor(0, 1);
    lcd.print("finalizada");

    delay(800);
  }

  if (estado == RUNNING) {
    if (antBtn3 == HIGH && btn3Atual == LOW) {
      contBtn3++;
      enviarBLE("EVENT,BTN3");
    }

    if (antBtn4 == HIGH && btn4Atual == LOW) {
      contBtn4++;
      enviarBLE("EVENT,BTN4");
    }
  }

  antStart = startAtual;
  antStop = stopAtual;
  antBtn3 = btn3Atual;
  antBtn4 = btn4Atual;
}

void lerSensores() {
  int leituraGuidao = analogRead(PIN_GUIDAO);
  int leituraVel = analogRead(PIN_VEL);

  angulo = map(leituraGuidao, 0, 4095, 0, 180);

  velocidade = (leituraVel / 4095.0) * 100.0;

  if (estado == RUNNING) {
    contVelAmostras++;

    String dir = direcaoGuidao();

    if (dir == "D") {
      contDireita++;
    } else if (dir == "C") {
      contCentro++;
    } else if (dir == "E") {
      contEsquerda++;
    }
  }
}

String direcaoGuidao() {
  if (angulo < 80) {
    return "D";
  } else if (angulo <= 100) {
    return "C";
  } else {
    return "E";
  }
}

void enviarBluetooth() {
  unsigned long agora = millis();

  if (agora - tBluetooth >= INTERVALO_BT) {
    tBluetooth = agora;
    pacotes++;

    String msg = "DATA,";
    msg += "pac=" + String(pacotes) + ",";
    msg += "ang=" + String(angulo) + ",";
    msg += "dir=" + direcaoGuidao() + ",";
    msg += "vel=" + String(velocidade) + ",";
    msg += "btn3=" + String(btn3Press ? 1 : 0) + ",";
    msg += "btn4=" + String(btn4Press ? 1 : 0) + ",";
    msg += "state=RUNNING";

    enviarBLE(msg);
  }
}

void atualizarLCD() {
  unsigned long agora = millis();

  if (agora - tLCD < INTERVALO_LCD) {
    return;
  }

  tLCD = agora;

  static int tela = 0;

  lcd.clear();

  if (estado == IDLE) {
    lcd.setCursor(0, 0);
    lcd.print("BicicleTEAr");
    lcd.setCursor(0, 1);
    lcd.print("Aguardando...");
  }

  else if (estado == RUNNING) {
    lcd.setCursor(0, 0);
    lcd.print("A:");
    lcd.print((int)angulo);
    lcd.print(direcaoGuidao());
    lcd.print(" V:");
    lcd.print((int)velocidade);

    lcd.setCursor(0, 1);
    lcd.print("3:");
    lcd.print(btn3Press);
    lcd.print(" 4:");
    lcd.print(btn4Press);
    lcd.print(" P:");
    lcd.print(pacotes);

    tela++;
  }

  else if (estado == REPORT) {
    lcd.setCursor(0, 0);
    lcd.print("B3:");
    lcd.print(contBtn3);
    lcd.print(" B4:");
    lcd.print(contBtn4);
    lcd.print(" V:");
    lcd.print(contVelAmostras);

    lcd.setCursor(0, 1);
    lcd.print("D:");
    lcd.print(contDireita);
    lcd.print(" C:");
    lcd.print(contCentro);
    lcd.print(" E:");
    lcd.print(contEsquerda);

    tela++;
    if (tela > 2) tela = 0;
  }
}

void resetarRelatorio() {
  pacotes = 0;

  contBtn3 = 0;
  contBtn4 = 0;

  contDireita = 0;
  contCentro = 0;
  contEsquerda = 0;
  contVelAmostras = 0;
}
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#define PIR_PIN 27
#define LDR_PIN 34
#define LED_PIN 2

const char* WORKER_URL =
  "https://flat-recipe-5685.vinidotto02.workers.dev";

#define LIMIAR_LUZ 2000

const unsigned long INTERVALO_ENVIO_LUZ = 20000;
const unsigned long TEMPO_OCUPADA = 30000;
const unsigned long TEMPO_DESPERDICIO = 60000;
const unsigned long INTERVALO_PISCA = 500;

unsigned long ultimoEnvioLuz = 0;
unsigned long ultimoMovimento = 0;
unsigned long inicioLuzAcesaSemMovimento = 0;
unsigned long ultimoPisca = 0;

bool estadoLed = false;
bool estavaEmDesperdicio = false;

volatile unsigned int movimentosPendentes = 0;

// ISR curta: apenas registra o evento
void IRAM_ATTR detectarMovimento() {
  movimentosPendentes++;
}

void conectarWiFi() {
  Serial.print("Conectando ao Wi-Fi");

  WiFi.begin("Wokwi-GUEST", "", 6);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi conectado!");
}

void enviarMovimento() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] Wi-Fi desconectado");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  String url = String(WORKER_URL) + "/movimento";

  http.begin(client, url);

  int httpCode = http.POST("");

  Serial.print("HTTP /movimento: ");
  Serial.println(httpCode);

  if (httpCode > 0) {
    Serial.println(http.getString());
  } else {
    Serial.print("Erro HTTP: ");
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
}

void enviarLuz(int valor, bool acesa) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] Wi-Fi desconectado");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  String url = String(WORKER_URL) + "/luz";

  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  String json = "{";
  json += "\"valor\":";
  json += valor;
  json += ",";
  json += "\"acesa\":";
  json += acesa ? "1" : "0";
  json += "}";

  int httpCode = http.POST(json);

  Serial.print("HTTP /luz: ");
  Serial.println(httpCode);

  if (httpCode > 0) {
    Serial.println(http.getString());
  } else {
    Serial.print("Erro HTTP: ");
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
}

void atualizarLed(bool luzAcesa) {
  unsigned long agora = millis();

  bool ocupada =
    ultimoMovimento > 0 &&
    (agora - ultimoMovimento < TEMPO_OCUPADA);

  // Sala ocupada: LED fica aceso
  if (ocupada) {
    digitalWrite(LED_PIN, HIGH);

    estadoLed = true;
    inicioLuzAcesaSemMovimento = 0;
    estavaEmDesperdicio = false;

    return;
  }

  // Sala vazia e luz apagada: LED desligado
  if (!luzAcesa) {
    digitalWrite(LED_PIN, LOW);

    estadoLed = false;
    inicioLuzAcesaSemMovimento = 0;
    estavaEmDesperdicio = false;

    return;
  }

  // Inicia a contagem de luz acesa com sala vazia
  if (inicioLuzAcesaSemMovimento == 0) {
    inicioLuzAcesaSemMovimento = agora;
  }

  unsigned long tempoSemMovimento =
    agora - inicioLuzAcesaSemMovimento;

  // Após 60s, pisca o LED como alerta
  if (tempoSemMovimento >= TEMPO_DESPERDICIO) {

    if (!estavaEmDesperdicio) {
      Serial.println();
      Serial.println("!!! ALERTA DE DESPERDICIO !!!");
      estavaEmDesperdicio = true;
    }

    if (agora - ultimoPisca >= INTERVALO_PISCA) {
      ultimoPisca = agora;

      estadoLed = !estadoLed;

      digitalWrite(
        LED_PIN,
        estadoLed ? HIGH : LOW
      );
    }

  } else {
    digitalWrite(LED_PIN, LOW);
    estadoLed = false;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);

  attachInterrupt(
    digitalPinToInterrupt(PIR_PIN),
    detectarMovimento,
    RISING
  );

  conectarWiFi();

  delay(1500);

  Serial.println();
  Serial.println("=============================");
  Serial.println(" Monitoramento de sala");
  Serial.println("=============================");
}

void loop() {
  unsigned long agora = millis();

  if (movimentosPendentes > 0) {
    noInterrupts();

    unsigned int quantidade =
      movimentosPendentes;

    movimentosPendentes = 0;

    interrupts();

    for (unsigned int i = 0; i < quantidade; i++) {
      ultimoMovimento = millis();

      Serial.println();
      Serial.println("MOVIMENTO DETECTADO");

      enviarMovimento();
    }
  }

  int valorLuz = analogRead(LDR_PIN);

  bool luzAcesa =
    valorLuz >= LIMIAR_LUZ;

  atualizarLed(luzAcesa);

  if (
    agora - ultimoEnvioLuz
    >= INTERVALO_ENVIO_LUZ
  ) {
    ultimoEnvioLuz = agora;

    Serial.println();
    Serial.println("-----------------------");

    Serial.print("LDR: ");
    Serial.println(valorLuz);

    Serial.print("Luz: ");
    Serial.println(
      luzAcesa ? "ACESA" : "APAGADA"
    );

    bool ocupada =
      ultimoMovimento > 0 &&
      (agora - ultimoMovimento < TEMPO_OCUPADA);

    Serial.print("Sala: ");
    Serial.println(
      ocupada ? "OCUPADA" : "VAZIA"
    );

    enviarLuz(
      valorLuz,
      luzAcesa
    );

    Serial.println("-----------------------");
  }

  delay(10);
}
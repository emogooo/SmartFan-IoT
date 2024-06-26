#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <dht11.h>
#include <ArduinoJson.h>

const uint8_t PWM = 5;                 // Fana PWM verdiğimiz ve kontrol ettiğimiz pin.
const uint8_t RELAY = 16;              // Verilen PWM 0 olduğunda fanın gücünü kesecek rölenin pini. (CPU fanları genelde 0 PWM'de (en düşük devirde) dönmeye devam ediyor.)
const uint8_t TEMPERATURE_SENSOR = 4;  // Sıcaklık sensörü pini (DHT11)
const uint8_t CONNECTION_LED = 13;     // Bağlantı durumumuzu gösteren ledin pini.
const uint8_t MESSAGE_IN_LED = 12;     // Mesaj alındığında yanan ledin pini.
const uint8_t MESSAGE_OUT_LED = 14;    // Mesaj gönderildiğinde yanan ledin pini.

const char* ssid = "WiFi-Name-Here";
const char* password = "WiFi-Password-Here";
const char* mqtt_server = "test.mosquitto.org";

WiFiClient espClient;
PubSubClient client(espClient);
dht11 DHT;
StaticJsonDocument<200> doc;

uint16_t messageSendingFrequency = 5000;
uint32_t lastMessageSendingTime = 0;
const char* outTopicName = "emogoOut";
const char* inTopicName = "emogoIn";
String messageOut;
uint8_t pwmRate = 0;
uint16_t counter = 0;

void setup() {
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  pinMode(PWM, OUTPUT);
  pinMode(CONNECTION_LED, OUTPUT);
  pinMode(MESSAGE_IN_LED, OUTPUT);
  pinMode(MESSAGE_OUT_LED, OUTPUT);
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, LOW);
  digitalWrite(PWM, LOW);
  digitalWrite(CONNECTION_LED, LOW);
  digitalWrite(MESSAGE_IN_LED, LOW);
  digitalWrite(MESSAGE_OUT_LED, LOW);
}

void loop() {
  createMessageToSend();
  if (!client.connected()) {
    digitalWrite(CONNECTION_LED, LOW);
    reconnect();
  }
  client.loop();
  if (millis() - lastMessageSendingTime > messageSendingFrequency) {
    lastMessageSendingTime = millis();
    char message[messageOut.length() + 1];
    messageOut.toCharArray(message, (messageOut.length() + 1));
    client.publish(outTopicName, message);
    counter += 1;
    blinkRed();
  }
}

void blinkRed() {
  for (int i = 0; i < 4; i++) {
    digitalWrite(MESSAGE_OUT_LED, HIGH);
    delay(50);
    digitalWrite(MESSAGE_OUT_LED, LOW);
    delay(50);
  }
}

void blinkGreen() {
  digitalWrite(MESSAGE_IN_LED, HIGH);
  delay(200);
  digitalWrite(MESSAGE_IN_LED, LOW);
}

void createMessageToSend() {
  DHT.read(TEMPERATURE_SENSOR);
  messageOut = "#" + String(counter) + " | Nem Oranı: %" + String(DHT.humidity) + " | Sıcaklık: " + String(DHT.temperature) + " C" + " | Fan Hızı: %" + String((pwmRate * 0.39215), 1);
}

void processMessage(String msg) {
  /*
  Gelen json stringini işler ve fanı kontrol eder.
  {"pwm":15}
  */
  DeserializationError error = deserializeJson(doc, msg);
  if (error || (doc["pwm"] < 0 || doc["pwm"] > 255)) return;
  pwmRate = doc["pwm"];
  if (pwmRate == 0) digitalWrite(RELAY, LOW);
  else digitalWrite(RELAY, HIGH);
  analogWrite(PWM, pwmRate);
}

void setup_wifi() {
  delay(10);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  randomSeed(micros());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String messageIn = "";
  for (int i = 0; i < length; i++) {
    messageIn += (char)payload[i];
  }
  processMessage(messageIn);
  blinkGreen();
}

void reconnect() {
  /*
  Bağlantı kurulana kadar fonksiyondan çıkış yapılmaz. 
  Bağlantının kurulamama sebebi WiFi ağının bilgilerinin yanlış olması,
  kartın komponentlerinde bozukluk veya
  modemin internete bağlı olmamasından kaynaklı olabilir.
  */
  while (!client.connected()) {
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);  // Rastgele bir client ID üretiriz ve bu ID ile bağlanmaya çalışırız.
    if (client.connect(clientId.c_str())) {   // Bağlantı kurulduğunda sunucuya bağlantı kurulduğunu belirten mesajı gönderir ve pinleri yakarız.
      digitalWrite(CONNECTION_LED, HIGH);
      messageOut = "MQTT Sunucusuna Baglanti Saglandi";
      char message[messageOut.length() + 1];
      messageOut.toCharArray(message, (messageOut.length() + 1));
      client.publish(outTopicName, message);  // Verilen başlığa, verilen mesajı yayınlar.
      client.subscribe(inTopicName);          // Verilen başlığı dinler. Mesaj geldiğinde callback fonksiyonu çalışır.
      digitalWrite(MESSAGE_OUT_LED, HIGH);
      delay(300);
      digitalWrite(MESSAGE_OUT_LED, LOW);
    } else {
      for (int i = 0; i < 6; i++) {
        digitalWrite(CONNECTION_LED, HIGH);
        delay(250);
        digitalWrite(CONNECTION_LED, LOW);
        delay(250);
      }
      delay(2000);
    }
  }
}
// Bibliotecas utilizadas no desenvolvimento do código
#include <Arduino.h>
#include <NewPing.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <FS.h>          
#include <WiFiManager.h> 
#include <DNSServer.h>
#include <WebServer.h>
#include <time.h>

// Variáveis utilizadas para coletar o horário atual
const long gmtOffset_sec = 0; 
const int daylightOffset_sec = 0;
const char* ntpServer = "pool.ntp.org";
long timezone = -3;
byte daysavetime = 1;


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec, daylightOffset_sec);

//NTPClient timeClient(ntpUDP, "pool.ntp.org");

DynamicJsonDocument docQtd(4096);
DynamicJsonDocument docTemp(4096);

WiFiServer server(8090);
String header;
unsigned long currentTime = millis();

// Vez anterior
unsigned long previousTime = 0; 
//Defina o tempo limite em milissegundos 
const long timeoutTime = 2000;

//Incia a biblioteca WifiManager
WiFiManager wifiManager;

// Status para controle de Led
int Status_Led = LOW; 

//Criação de variaveis 
const int trigPin = 5;
const int echoPin = 18; //definir a velocidade do som em cm/uS
const float taxa_variacao_media = -0.2915;
float litros;
String dataFinal;
float tf;
float sensor = 35;
long duracao;
float distancCm;
float distancInch;
int apoio;
#define Led  2
#define MAX_DISTANCE 190 // Distância máxima de medição em centímetros (ajuste conforme necessário)
#define POTE_ALTURA  24  // Altura do pote em centímetros
#define POTE_LARGURA 15  // Largura do pote em centímetros
#define POTE_PROFUNDIDADE 15

NewPing sonar(trigPin, echoPin, MAX_DISTANCE);

// Função onde são definidos os parametros de configuração
void setup() {
  Serial.begin(115200);
  Serial.println("Configurar Rede Wifi");
  wifiManager.resetSettings(); //Apaga todas as redes
  wifiManager.autoConnect("WINE_TECH", "Wine2023@");
  configTime(3600 * timezone, daysavetime * 3600, "time.nist.gov", "0.pool.ntp.org", "1.pool.ntp.org");

  delay(150);
  WiFi.begin();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("------------------------------------------------------------");
    Serial.println("Conectado IP: ");
    Serial.println(WiFi.localIP());
    Serial.println("------------------------------------------------------------");

    delay(150);
    server.begin();
  }

  delay(50);
  pinMode(Led,   OUTPUT);
  digitalWrite(Led,   HIGH);
  Serial.println("INICIOU **************************");

  timeClient.begin();
  timeClient.forceUpdate();
}

//Função principal, onde é chamada todas as outras funções e também formata a data e hora atual para enviar a API
void loop() { 

  conectaWifi();

  struct tm tmstruct;
  tmstruct.tm_year = 0;
  getLocalTime(&tmstruct);
  String date = (String(( tmstruct.tm_mon) + 1) + "-" + String(tmstruct.tm_mday));
  String hour = (String((tmstruct.tm_hour) - 1) + ":" + String(tmstruct.tm_min) + ":" + String(tmstruct.tm_sec));
  //Serial.println("DATA FINAL: " + dataFinalQtd);
  String dataFinalTemp = "2024-" + date + "T" + hour;
  String dataFinalQtd = "2024-" + date + "T" + hour;

  leTemp();
  leQtd();
  postTemp(tf, dataFinalTemp);
  postQtd(litros, dataFinalQtd);
}

//Função responsável por ler a temperatura dentro do tonel
void leTemp(){

  float voltage = (analogRead(sensor)); 
  float temperaturaC0 = voltage * 0.0306;
  float t1, t2, t3, t4 ,t5;
  t1 = temperaturaC0;
  delay(500);
  t2 = temperaturaC0;
  delay(500);
  t3 = temperaturaC0;
  delay(500);
  t4 = temperaturaC0;
  delay(500);
  t5 = temperaturaC0;

  tf = (t1 + t2 + t3 + t4 + t5) / 5;
  if (tf <= 0){
    tf = t5;
  }
}

//Função responsável por ler a quantidade dentro do tonel
void leQtd(){
  unsigned int distancia = sonar.ping_cm() + 1; 
  if(distancia < apoio){
    apoio = distancia;
    distancia = apoio;
  }
  litros = 5.0 + (distancia - 2.5) * taxa_variacao_media;

  delay(5000);
}

//Função que faz o post de quantidade para a API
void postQtd(float litros, String dataFinal){
  HTTPClient http;
  http.begin("https://app-tcc-winetech-dd2c345ee059.herokuapp.com/mgwine/Qtd");
  http.addHeader("Content-Type", "application/json");
  
  docQtd["qtd"] = litros;
  docQtd["dateTime"] = dataFinal;

  String jsonStringQtd;
  serializeJson(docQtd, jsonStringQtd);
  int httpResponseCode = http.POST(jsonStringQtd);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  } else {
    Serial.print("Erro na solicitação HTTP. Código de resposta: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

//Função que faz o post de temperatura para a API
void postTemp(float tf, String dataFinal){
  HTTPClient http;
  http.begin("https://app-tcc-winetech-dd2c345ee059.herokuapp.com/mgwine/Temp");
  http.addHeader("Content-Type", "application/json");

  docTemp["temp"] = tf; 
  docTemp["dateTime"] = dataFinal;

  String jsonStringTemp;
  serializeJson(docTemp, jsonStringTemp);
  int httpResponseCode = http.POST(jsonStringTemp);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  } else {
    Serial.print("Erro na solicitação HTTP. Código de resposta: ");
    Serial.println(httpResponseCode);
  }
  http.end();
} 

// Função responsável por conectar a ESP32 via WiFi
void conectaWifi(){
  WiFiClient client = server.available();    
   if (client) {                             
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          
    String currentLine = "";                
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  
      currentTime = millis();
      if (client.available()) {             
        char c = client.read();             
        Serial.write(c);                    
        header += c;
        if (c == '\n') {
          if (currentLine.length() == 0) {

            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            break;                                
          } else {
            currentLine = "";                     
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    header = "";

    client.stop();
    Serial.println("Client Disconnected.");
    Serial.println("------------------------------------------------------------");
  }
}

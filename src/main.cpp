#include <Arduino.h>
#include <ESP8266WiFi.h> //Библиотека для работы с WIFI
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h> // Библиотека для OTA-прошивки
#include <PubSubClient.h>
#include <TimerMs.h>
#include <OneWire.h>
#include <CD74HC4067.h>
CD74HC4067 my_mux(D1, D2, D3, D4);
OneWire ds(D7);
// Address: 40 231 23 69 146 22 2 99
// Address: 40 255 85 74 144 21 1 145
uint8_t address_1[8] = {40, 231, 23, 69, 146, 22, 2, 99}; //+0.5
uint8_t address_2[8] = {40, 255, 85, 74, 144, 21, 1, 145};
#define GAZ D5
#define DUCH D6
WiFiClient espClient;
PubSubClient client(espClient);
TimerMs OTA_Wifi(10, 1, 0);
TimerMs dallos(5000, 1, 0);
TimerMs termo_control(300000, 1, 0);
TimerMs step_rezistor(300000, 1, 0);
const char *name_client = "kotel";         // Имя клиента и сетевого порта для ОТА
const char *mqtt_reset = "kotel_reset";    // Имя топика для перезагрузки
const char *ssid = "Beeline";              // Имя точки доступа WIFI
const char *password = "sl908908908908sl"; // пароль точки доступа WIFI
const char *mqtt_server = "192.168.1.221";
String s;
byte buff_clear, state_GAZ = 0, state_DUCH = 0, start = 0, step_up = 0;
int data = 0, dht_tik = 0, data_rezistor = 0;
int Dima_t = 0, zal_t = 0, temper_ul = 0, Dima_t_tik = 0, zal_t_tik = 0;
int podacha, obratka, set_rez_raw = 0, podacha_kontrol;

float dht_raw = 0, dht_gr = 0, dht_sr = 0;

void publish_send_f(const char *top, float &ex_data) // Отправка Показаний с сенсоров
{
  char send_mqtt[10];
  dtostrf(ex_data, -2, 1, send_mqtt);
  client.publish(top, send_mqtt, 1);
}

void publish_send_i(const char *top, int &ex_data) // Отправка Показаний с сенсоров
{
  char send_mqtt[10];
  itoa(ex_data, send_mqtt, 10);
  client.publish(top, send_mqtt, 1);
}

void rezistor_set(int set)
{
  if (set > 15)
  {
    set = 15;
  }
  else if (set < 0)
  {
    set = 0;
  }

  if (set != set_rez_raw)
  {
    int set_raz = fabs(set - set_rez_raw);

    if (set < set_rez_raw)
    {
      for (int i = 0; i < set_raz; i++)
      {
        set_rez_raw--;
        my_mux.channel(set_rez_raw);
        ESP.wdtFeed();
        delay(2000);
      }
    }
    else
    {
      for (int i = 0; i < set_raz; i++)
      {
        set_rez_raw++;
        my_mux.channel(set_rez_raw);
        ESP.wdtFeed();
        delay(2000);
      }
    }
  }
  if (set == 0)
  {
    my_mux.channel(0);
    set_rez_raw = 0;
  }
}

void callback(char *topic, byte *payload, unsigned int length)
{

  s = ""; // очищаем перед получением новых данных

  for (unsigned int iz = 0; iz < length; iz++)
  {
    s = s + ((char)payload[iz]); // переводим данные в String
  }

  data = atoi(s.c_str());         // переводим данные в int
  float data_f = atof(s.c_str()); // переводим данные в float

  if ((String(topic)) == "kotel_rezistor" && start == 0)
  {
    rezistor_set(data);
  }

  if ((String(topic)) == "Dima_temper")
  {
    Dima_t = Dima_t + (data_f * 10);
    Dima_t_tik++;
  }

  if ((String(topic)) == "zal_temper")
  {
    zal_t = zal_t + (data_f * 10);
    zal_t_tik++;
  }

  if ((String(topic)) == "temper_s_z_v")
  {
    temper_ul = data_f * 10;
  }
}

void wi_fi_con()
{
  WiFi.mode(WIFI_STA);
  WiFi.hostname(name_client); // Имя клиента в сети
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    delay(5000);
    ESP.restart();
  }
  ArduinoOTA.setHostname(name_client); // Задаем имя сетевого порта
  ArduinoOTA.begin();                  // Инициализируем OTA
}

void loop()
{
  ESP.wdtFeed();

  if (digitalRead(GAZ) == 1 && state_GAZ == 1)
  {
    client.publish("state_GAZ_kotel", "0", 1);
    delay(1000);
    state_GAZ = 0;
  }

  if (digitalRead(GAZ) == 0 && state_GAZ == 0)
  {
    client.publish("state_GAZ_kotel", "1", 1);
    delay(1000);
    state_GAZ = 1;
  }

  if (digitalRead(DUCH) == 1 && state_DUCH == 1)
  {
    client.publish("state_DUSH_kotel", "1", 1);
    delay(1000);
    state_DUCH = 0;
  }

  if (digitalRead(DUCH) == 0 && state_DUCH == 0)
  {
    client.publish("state_DUSH_kotel", "0", 1);
    delay(1000);
    state_DUCH = 1;
  }

  if (dallos.tick())
  {
    byte i;
    byte data_ds[9];
    int16_t raw;

    float celsius;
    ds.reset();
    ds.select(address_1);
    ds.write(0xBE);
    for (i = 0; i < 9; i++)
    {
      data_ds[i] = ds.read();
    }
    raw = (data_ds[1] << 8) | data_ds[0];
    celsius = ((float)raw / 16.0) + 0.5;
    obratka = celsius * 10;
    publish_send_f("kotel_obratka", celsius);

    ds.reset();
    ds.select(address_2);
    ds.write(0xBE);
    for (i = 0; i < 9; i++)
    {
      data_ds[i] = ds.read();
    }
    raw = (data_ds[1] << 8) | data_ds[0];
    celsius = (float)raw / 16.0;
    podacha = celsius * 10;
    publish_send_f("kotel_podacha", celsius);

    ds.reset();
    ds.select(address_1);
    ds.write(0x44, 1);
    ds.reset();
    ds.select(address_2);
    ds.write(0x44, 1);
  }

  if (OTA_Wifi.tick()) // Поддержание "WiFi" и "OTA"  и Пинок :) "watchdog" и подписка на "Топики Mqtt"
  {
    ArduinoOTA.handle();     // Всегда готовы к прошивке
    client.loop();           // Проверяем сообщения и поддерживаем соедениние
    if (!client.connected()) // Проверка на подключение к MQTT
    {
      while (!client.connected())
      {
        ESP.wdtFeed();                   // Пинок :) "watchdog"
        if (client.connect(name_client)) // имя на сервере mqtt
        {
          client.subscribe(mqtt_reset);       // подписались на топик
                                              // client.subscribe("kotel_rezistor"); // подписались на топик
          client.subscribe("kotel_rezistor"); // подписались на топик
          client.subscribe("Dima_temper");    // подписались на топик
          client.subscribe("zal_temper");     // подписались на топик
          client.subscribe("temper_s_z_v");   // подписались на топик
          client.subscribe(name_client);      // подписались на топик

          // Отправка IP в mqtt
          char IP_ch[20];
          String IP = (WiFi.localIP().toString().c_str());
          IP.toCharArray(IP_ch, 20);
          client.publish(name_client, IP_ch);
        }
        else
        {
          delay(5000);
        }
      }
    }
  }

  if (termo_control.tick())
  {
    step_up++;
    if (step_up > 2)
    {
      step_up = 2;
    }

    if (zal_t > 0 && Dima_t > 0)
    {
      Dima_t = Dima_t / Dima_t_tik;
      zal_t = zal_t / zal_t_tik;
    }
    else
    {
      Dima_t = 235;
      zal_t = 235;
    }
    // если температура так и растёт !!!
    if (Dima_t >= 232 && state_GAZ == 1 && state_DUCH == 1 && set_rez_raw < 15)
    {
      rezistor_set(set_rez_raw + 1);
      ESP.wdtFeed();
      delay(5000);
      ESP.wdtFeed();
      delay(5000);
      ESP.wdtFeed();
      delay(5000);
      ESP.wdtFeed();
      delay(5000);

      while (state_GAZ == 1 && state_DUCH == 1 && set_rez_raw != 15)
      {
        rezistor_set(set_rez_raw + 1);
        ESP.wdtFeed();
        delay(5000);
        ESP.wdtFeed();
        delay(5000);
        ESP.wdtFeed();
        delay(5000);
        ESP.wdtFeed();
        delay(5000);
      }
    }
    // если температура так и не поднялась делаем еще шаги в низ !!!
    if ((Dima_t <= 230 || zal_t <= 230) && set_rez_raw > 0 && state_GAZ == 0 && state_DUCH == 1)
    {
      rezistor_set(set_rez_raw - 1);
    }

    // если температура так и не поднялась а котел работает делаем еще шаги в низ !!!
    if ((Dima_t <= 229 || zal_t <= 229) && set_rez_raw > 0 && state_GAZ == 1 && state_DUCH == 1 && step_up > 1)
    {
      rezistor_set(set_rez_raw - 1);
      step_up = 0;
    }
    publish_send_i("kotel_rezistor", set_rez_raw);
    publish_send_i("kotel_Dima_t", Dima_t);
    publish_send_i("kotel_zal_t", zal_t);
    zal_t = 0;
    zal_t_tik = 0;
    Dima_t = 0;
    Dima_t_tik = 0;
  }
}

void setup()
{
  wi_fi_con();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  ESP.wdtDisable(); // Активация watchdog
  pinMode(GAZ, INPUT);
  pinMode(DUCH, INPUT);
  delay(500);
  my_mux.channel(0);
}
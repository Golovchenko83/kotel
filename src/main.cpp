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
byte buff_clear, state_GAZ = 0, state_DUCH = 0, start = 0, step_up = 0, state_kotel = 0;
int data = 0, temper_set = 230, dht_tik = 0, data_rezistor = 0, rezistor_min = 0, rezistor_max = 15;
int Dima_t = 0, zal_t = 0, temper_ul = 0, Dima_t_tik = 0, zal_t_tik = 0, Mari_t = 0, Mari_t_tik = 0, state_mem = 0;
;
int podacha = 0, obratka = 0, set_rez_raw = 0, podacha_kontrol;

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

void network()
{
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
          client.subscribe("temper_set_dom"); // подписались на топик
          client.subscribe("Mari_temper");    // подписались на топик
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
}

void rezistor_set(int set)
{
  if (set > rezistor_max)
  {
    set = rezistor_max;
  }
  else if (set < rezistor_min)
  {
    set = rezistor_min;
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
        network();
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
        network();
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

  if ((String(topic)) == "kotel_rezistor")
  {
    rezistor_set(data);
  }

  if ((String(topic)) == "Dima_temper")
  {
    int Dima_raw = (data_f * 10);
    Dima_t = Dima_t + Dima_raw;
    Dima_t_tik++;
  }

  if ((String(topic)) == "Mari_temper")
  {
    int Mari_raw = (data_f * 10);
    Mari_t = Mari_t + Mari_raw;
    Mari_t_tik++;
  }

  if ((String(topic)) == "zal_temper")
  {
    zal_t = zal_t + (data_f * 10);
    zal_t_tik++;
  }

  if ((String(topic)) == "temper_s_z_v")
  {
    temper_ul = data_f * 10;
    if (temper_ul < -50)
    {
      rezistor_max = 14;
    }
    else
    {
      rezistor_max = 15;
    }
  }

  if ((String(topic)) == "temper_set_dom")
  {
    temper_set = data_f * 10;
  }
}

void sensor()
{
  if (digitalRead(GAZ) == 1 && state_GAZ == 1)
  {
    client.publish("kotel_state_GAZ", "0", 1);
    delay(1000);
    state_GAZ = 0;
  }

  if (digitalRead(GAZ) == 0 && state_GAZ == 0)
  {
    client.publish("kotel_state_GAZ", "1", 1);
    delay(1000);
    state_GAZ = 1;
  }

  if (digitalRead(DUCH) == 1 && state_DUCH == 1)
  {
    client.publish("kotel_state_GVS", "1", 1);
    delay(1000);
    state_DUCH = 0;
  }

  if (digitalRead(DUCH) == 0 && state_DUCH == 0)
  {
    client.publish("kotel_state_GVS", "0", 1);
    delay(1000);
    state_DUCH = 1;
  }

  if (state_GAZ == 0 && state_DUCH == 1)
  {
    state_kotel = 0;
  }
  else if (state_GAZ == 1 && state_DUCH == 1)
  {
    state_kotel = 1;
  }
  else if (state_GAZ == 1 && state_DUCH == 0)
  {
    state_kotel = 2;
  }
  if (state_kotel != state_mem)
  {
    state_mem = state_kotel;
    publish_send_i("kotel_state", state_mem);
  }
}

void rezistor_plus()
{
  rezistor_set(set_rez_raw + 1);
  ESP.wdtFeed();
  delay(2000);
  network();
  ESP.wdtFeed();
  delay(2000);
  network();
  ESP.wdtFeed();
  delay(2000);
  ESP.wdtFeed();
  network();
  delay(2000);
  network();
  sensor();

  while (state_GAZ == 1 && state_DUCH == 1 && set_rez_raw != rezistor_max)
  {
    rezistor_set(set_rez_raw + 1);
    ESP.wdtFeed();
    delay(2000);
    ESP.wdtFeed();
    network();
    delay(2000);
    ESP.wdtFeed();
    network();
    delay(2000);
    ESP.wdtFeed();
    network();
    delay(2000);
    network();
    sensor();
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
  network();
  ESP.wdtFeed();

  sensor();

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

  if (termo_control.tick())
  {
    step_up++;

    if (step_up > 5)
    {
      step_up = 5;
    }

    if (zal_t_tik > 0)
    {
      zal_t = (zal_t + 15) / zal_t_tik;
      if (zal_t < 100)
      {
        zal_t = 235;
      }
    }
    else
    {
      zal_t = 235;
    }

    if (Dima_t_tik > 0)
    {
      Dima_t = (Dima_t + 15) / Dima_t_tik;
      if (Dima_t < 100)
      {
        Dima_t = 235;
      }
    }
    else
    {
      Dima_t = 235;
    }

    if (Mari_t_tik > 0)
    {
      Mari_t = (Mari_t + 15) / Mari_t_tik;
      if (Mari_t < 100)
      {
        Mari_t = 235;
      }
    }
    else
    {
      Mari_t = 235;
    }

    publish_send_i("kotel_Dima_t", Dima_t);
    publish_send_i("kotel_zal_t", zal_t);
    publish_send_i("kotel_Mari_t", Mari_t);
    publish_send_i("kotel_set_temper", temper_set);
    publish_send_i("kotel_state", state_mem);
    // если температура выше установленной делаем шаги вверх.
    if (Dima_t >= (temper_set + 1) && zal_t >= temper_set && Mari_t >= temper_set && state_GAZ == 1 && state_DUCH == 1 && set_rez_raw < rezistor_max)
    {
      if (temper_ul > -50)
      {
        rezistor_plus();
      }
      else if (obratka > 280)
      {
        rezistor_plus();
      }
    }

    // если температура так и не поднялась и котел не работает делаем еще шаги в низ !!!
    if ((Dima_t <= temper_set || zal_t <= temper_set || Mari_t <= temper_set) && set_rez_raw > 0 && state_GAZ == 0 && state_DUCH == 1)
    {
      if (set_rez_raw > 7)
      {
        rezistor_set(set_rez_raw - 2);
        step_up = 0;
      }
      else
      {
        rezistor_set(set_rez_raw - 1);
        step_up = 0;
      }
    }

    // если температура так и не поднялась а котел работает делаем еще шаги в низ !!!
    if ((Dima_t <= (temper_set - 1) || zal_t <= (temper_set - 1) || Mari_t <= (temper_set - 1)) && set_rez_raw > 0 && state_GAZ == 1 && state_DUCH == 1 && step_up > 2)
    {
      if (set_rez_raw > 7)
      {
        rezistor_set(set_rez_raw - 2);
        step_up = 0;
      }
      else
      {
        rezistor_set(set_rez_raw - 1);
        step_up = 0;
      }
    }

    if (obratka < 270 && temper_ul < -50 && state_GAZ == 0 && state_DUCH == 1 && (Dima_t <= (temper_set + 5) || zal_t <= (temper_set + 5) || Mari_t <= (temper_set + 5)))
    {
      int mem_rez = set_rez_raw; // сохраняем текущие значение
      rezistor_set(8);           // пинаем котел чтобы стартанул
      ESP.wdtFeed();
      delay(2000);
      network();
      ESP.wdtFeed();
      delay(2000);
      network();
      rezistor_set(mem_rez); // возвращаем прежние значение
    }

    publish_send_i("kotel_rezistor", set_rez_raw);

    zal_t = 0;
    zal_t_tik = 0;
    Dima_t = 0;
    Dima_t_tik = 0;
    Mari_t = 0;
    Mari_t_tik = 0;
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
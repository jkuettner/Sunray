#include "config.h"

#ifdef USE_MQTT

#include "src/adapter.h"

//#include <WiFi.h>
/*
   MQTT
   Joel Gaehwiler
   2.5.0
   https://github.com/256dpi/arduino-mqtt
*/

//#include <MQTT.h>
#include <MQTTClient.h>

/*
    ArduinoJson
   Benoit Blanchon
   6.18.4
   https://arduinojson.org/
*/
#include <ArduinoJson.h>

#define MQTT_MOWER_POLL_INTERVAL      1000
#define MQTT_MOWER_POLL_BACKOFF       5000
#define MQTT_MOWER_POLL_ATS           5000
#define MQTT_MOWER_POLL_ATT           60000

WiFiClient mqttNet;
MQTTClient mqttClient(1024);
bool mqttPendingPublishProps = false;
bool mqttPendingPublishState = false;
bool mqttPendingPublishStats = false;

void mqtt_on_message(String &topic, String &payload);
void mqtt_handle_command_start();
void mqtt_handle_command_stop();
void mqtt_handle_command_dock();
void mqtt_handle_command_reboot();
void mqtt_handle_command_shutdown();
void mqtt_poll_mower(uint32_t now);
void mqtt_connect();
void mqtt_publish_props();
void mqtt_publish_state();
void mqtt_publish_stats();


String mqtt_topic(String postfix) {
  String result = MQTT_PREFIX;
  result += MQTT_CLIENT_ID;
  result += postfix;

  return result;
}

void mqtt_setup() {
  mower.addStateListener([ = ](ArduMower::State::State & state) {
    mqttPendingPublishState = true;
  });
  mower.addPropertiesListeners([ = ](ArduMower::Properties & props) {
    mqttPendingPublishProps = true;
  });
  mower.addStatsListeners([ = ](ArduMower::Stats & stats) {
    mqttPendingPublishStats = true;
  });

  mqttClient.begin(MQTT_HOSTNAME, MQTT_PORT, mqttNet);
  mqttClient.onMessage(mqtt_on_message);
}

void mqtt_loop() {
  mqtt_poll_mower(millis());
  mqtt_connect();
  mqttClient.loop();
  mqtt_publish_props();
  mqtt_publish_state();
  mqtt_publish_stats();
}

void mqtt_poll_mower(uint32_t now) {
  static uint32_t next_time = 0;
  if (now < next_time) return;
  next_time = now + MQTT_MOWER_POLL_INTERVAL;

  if (mower.ageAtv(0) == 0) {
    static uint32_t last_time = 0;
    if (now - last_time > MQTT_MOWER_POLL_BACKOFF) {
      last_time = now;
      mower.sendCommand("AT+V");
      return;
    }
  }
  
  if (mower.ageAts(now) >= MQTT_MOWER_POLL_ATS) {
    static uint32_t last_time = 0;
    if (now - last_time > MQTT_MOWER_POLL_BACKOFF) {
      last_time = now;
      mower.sendCommand("AT+S");
      return;
    }
  }
  
  if (mower.ageAtt(now) >= MQTT_MOWER_POLL_ATT) {
    static uint32_t last_time = 0;
    if (now - last_time > MQTT_MOWER_POLL_BACKOFF) {
      last_time = now;
      mower.sendCommand("AT+T");
      return;
    }
  }
}

void mqtt_connect() {
  if (mqttClient.connected()) return;
  
  if (!mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) return;

  mqttClient.subscribe(mqtt_topic("/command").c_str());
  mqttClient.publish(mqtt_topic("/online").c_str(), "true");
  mqttClient.setWill(mqtt_topic("/online").c_str(), "false");
}

void mqtt_on_message(String &topic, String &payload) {
  CONSOLE.println(payload);
  if (payload.startsWith("{"))
  {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    const char *command = doc["command"];
  }
  if (payload == "start") {
    mqtt_handle_command_start();
  } else if (payload == "stop") {
    mqtt_handle_command_stop();
  } else if (payload == "dock") {
    mqtt_handle_command_dock();
  } else if (payload == "reboot") {
    mqtt_handle_command_reboot();
  } else if (payload == "shutdown") {
    mqtt_handle_command_shutdown();
  }
}

void mqtt_publish_props() {
  if (!mqttPendingPublishProps) return;

  if (!mqttClient.publish(mqtt_topic("/props").c_str(), mower.props.toJson().c_str())) return;

  mqttPendingPublishProps = false;
}

void mqtt_publish_state() {
  if (!mqttPendingPublishState) return;

  if (!mqttClient.publish(mqtt_topic("/state").c_str(), mower.state.toJson().c_str())) return;

  mqttPendingPublishState = false;
}

void mqtt_publish_stats() {
  if (!mqttPendingPublishStats) return;

  if (!mqttClient.publish(mqtt_topic("/stats").c_str(), mower.stats.toJson().c_str())) return;

  mqttPendingPublishStats = false;
}

void mqtt_handle_command_start() {
  String at = "AT+C,-1,1,0.1,100,0,-1,-1,1";
  mower.sendCommand(at);
}

void mqtt_handle_command_stop() {
  mower.sendCommand("AT+C,-1,0,-1,-1,-1,-1,-1,-1");
}

void mqtt_handle_command_dock() {
  mower.sendCommand("AT+C,-1,4,-1,-1,-1,-1,-1,1");
}

void mqtt_handle_command_reboot() {
  mower.sendCommand("AT+Y");
}

void mqtt_handle_command_shutdown() {
  mower.sendCommand("AT+Y3");
}

#endif  // USE_MQTT

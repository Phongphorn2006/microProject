#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <MQTTClient.h>
#include "config.h"

void mqttInit();
void mqttUpdate();    // เรียกใน loop()
void mqttPublish(float currentFreq, bool isAutoMode,
                 int fanRpm, int fanPercent, int animSpeed);
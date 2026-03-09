#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include "config.h"

extern WebServer        httpServer;
extern WebSocketsServer wsServer;

void wsInit();
void wsHandle();
void wsBroadcast(float currentFreq, bool isAutoMode,
                 int fanRpm, int fanPercent, int animSpeed);
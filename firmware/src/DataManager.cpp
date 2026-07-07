#include "DataManager.h"

void DataManager::begin() {
  _mutex = xSemaphoreCreateMutex();
}

SystemState DataManager::getSystemState() {
  SystemState state;
  xSemaphoreTake(_mutex, portMAX_DELAY);
  state = _systemState;
  xSemaphoreGive(_mutex);
  return state;
}

void DataManager::setSystemState(SystemState state) {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  bool changed = (_systemState != state);
  _systemState = state;
  xSemaphoreGive(_mutex);
  if (changed) {
    Serial.printf("[STATE] -> %s\n", toString(state));
  }
}

SensorReading DataManager::getSensor1() {
  SensorReading reading;
  xSemaphoreTake(_mutex, portMAX_DELAY);
  reading = _sensor1;
  xSemaphoreGive(_mutex);
  return reading;
}

void DataManager::setSensor1(const SensorReading& reading) {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  _sensor1 = reading;
  xSemaphoreGive(_mutex);
}

SensorReading DataManager::getSensor2() {
  SensorReading reading;
  xSemaphoreTake(_mutex, portMAX_DELAY);
  reading = _sensor2;
  xSemaphoreGive(_mutex);
  return reading;
}

void DataManager::setSensor2(const SensorReading& reading) {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  _sensor2 = reading;
  xSemaphoreGive(_mutex);
}

void DataManager::pushHourValue(const HourValue& value) {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  _ringbuffer[_ringbufferNextIndex] = value;
  _ringbufferNextIndex = (_ringbufferNextIndex + 1) % RINGBUFFER_SIZE;
  if (_ringbufferCount < RINGBUFFER_SIZE) _ringbufferCount++;
  xSemaphoreGive(_mutex);
}

size_t DataManager::getRingbuffer(HourValue* out, size_t maxCount) {
  size_t count;
  xSemaphoreTake(_mutex, portMAX_DELAY);
  count = min(_ringbufferCount, maxCount);
  size_t startIndex = (_ringbufferNextIndex + RINGBUFFER_SIZE - _ringbufferCount) % RINGBUFFER_SIZE;
  for (size_t i = 0; i < count; i++) {
    out[i] = _ringbuffer[(startIndex + i) % RINGBUFFER_SIZE];
  }
  xSemaphoreGive(_mutex);
  return count;
}

void DataManager::pushLogEntry(const String& message) {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  _log[_logNextIndex].timestamp = time(nullptr);
  _log[_logNextIndex].message = message;
  _logNextIndex = (_logNextIndex + 1) % LOG_CAPACITY;
  if (_logCount < LOG_CAPACITY) _logCount++;
  xSemaphoreGive(_mutex);
  Serial.printf("[LOG] %s\n", message.c_str());
}

size_t DataManager::getLogEntries(LogEntry* out, size_t maxCount) {
  size_t count;
  xSemaphoreTake(_mutex, portMAX_DELAY);
  count = min(_logCount, maxCount);
  // Neueste zuerst: rueckwaerts ab dem zuletzt geschriebenen Eintrag lesen.
  for (size_t i = 0; i < count; i++) {
    size_t index = (_logNextIndex + LOG_CAPACITY - 1 - i) % LOG_CAPACITY;
    out[i] = _log[index];
  }
  xSemaphoreGive(_mutex);
  return count;
}

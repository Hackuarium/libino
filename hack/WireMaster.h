// setting ATmega32U4 as I2C slave.

#ifdef THR_WIRE_MASTER


#define I2C_HARDWARE 1
#define I2C_TIMEOUT  10
#define I2C_SLOWMODE 1
     
#include "../SoftI2CMaster/SoftWire.h"
SoftWire Wire = SoftWire();


// #include <Wire.h>


#define WIRE_MAX_DEVICES 8
byte numberI2CDevices = 0;
byte wireDeviceID[WIRE_MAX_DEVICES];

void wireUpdateList();


NIL_WORKING_AREA(waThreadWireMaster, 200);
NIL_THREAD(ThreadWireMaster, arg) {

  nilThdSleepMilliseconds(1000);

  unsigned int wireEventStatus = 0;

  Wire.begin();

  while (true) {

    #ifdef WIRE_MASTER_HOT_PLUG
      // allows to log when devices are plugged in / out
      // not suitable for i2c slave sleep mode
      if (wireEventStatus % 25 == 0) {
        wireUpdateList();
      }
    #endif
    wireEventStatus++;

    nilThdSleepMilliseconds(200);
  }
}


int wireReadInt(uint8_t address) {
  protectThread();
  Wire.requestFrom(address, (uint8_t)2);
  if(Wire.available() != 2) {
    unprotectThread();
    return ERROR_VALUE;
  }
  int16_t value=(Wire.read() << 8) | Wire.read();
  unprotectThread();
  return value;
}

void wireWakeup(uint8_t address) {
  protectThread();
  Wire.beginTransmission(address);
  Wire.endTransmission(); // Send data to I2C dev with option for a repeated start
  unprotectThread();
}

void wireSetRegister(uint8_t address, uint8_t registerAddress) {
  protectThread();
  Wire.beginTransmission(address);
  Wire.write(registerAddress);
  Wire.endTransmission(); // Send data to I2C dev with option for a repeated start
  unprotectThread();
}

int wireReadIntRegister(uint8_t address, uint8_t registerAddress) {
  wireSetRegister(address, registerAddress);
  return wireReadInt(address);
}

int wireCopyParameter(uint8_t address, uint8_t registerAddress, uint8_t parameterID) {
  setParameter(parameterID, wireReadIntRegister(address, registerAddress));
}

void wireWriteIntRegister(uint8_t address, uint8_t registerAddress, int value) {
  protectThread();
  Wire.beginTransmission(address);
  Wire.write(registerAddress);
  if (value > 255 || value < 0) Wire.write(value >> 8);
  Wire.write(value & 255);
  Wire.endTransmission(); // Send data to I2C dev with option for a repeated start
  unprotectThread();
}

void printWireInfo(Print* output) {
  wireUpdateList();
  output->println("I2C");

  for (byte i = 0; i < numberI2CDevices; i++) {
    output->print(i);
    output->print(F(": "));
    output->print(wireDeviceID[i]);
    output->print(F(" - "));
    output->println(wireDeviceID[i], BIN);
  }
}

void printWireDeviceParameter(Print* output, uint8_t wireID) {
  output->println(F("I2C device: "));
  output->println(wireID);
  for (byte i = 0; i < 26; i++) {
    output->print((char)(i+65));
    output->print(" : ");
    output->print(i);
    output->print(F(" - "));
    output->println(wireReadIntRegister(wireID, i));
  }
}


void wireRemoveDevice(byte id) {
  for (byte i = id; i < numberI2CDevices - 1; i++) {
    wireDeviceID[i] = wireDeviceID[i + 1];
  }
  numberI2CDevices--;
}

void wireInsertDevice(byte id, byte newDevice) {
  //Serial.println(id);

  if (numberI2CDevices < WIRE_MAX_DEVICES) {
    for (byte i = id + 1; i < numberI2CDevices - 1; i++) {
      wireDeviceID[i] = wireDeviceID[i + 1];
    }
    wireDeviceID[id] = newDevice;
    numberI2CDevices++;
  }
}

boolean wireDeviceExists(byte id) {
  for (byte i = 0; i < numberI2CDevices; i++) {
    if (wireDeviceID[i] == id) return true;
  }
  return false;
}


void wireUpdateList() {
  // 16ms
  byte _data;
  byte currentPosition = 0;
  // I2C Module Scan, from_id ... to_id
  for (byte i = 0; i <= 127; i++) {
    protectThread();
    Wire.beginTransmission(i);
    Wire.write(&_data, 0);
    // I2C Module found out!
    if (Wire.endTransmission() == 0) {
      // there is a device, we need to check if we should add or remove a previous device
      if (currentPosition < numberI2CDevices && wireDeviceID[currentPosition] == i) { // it is still the same device that is at the same position, nothing to do
        currentPosition++;
      } else if (currentPosition < numberI2CDevices && wireDeviceID[currentPosition] < i) { // some device(s) disappear, we need to delete them
        wireRemoveDevice(currentPosition);
        i--;
      } else if (currentPosition >= numberI2CDevices || wireDeviceID[currentPosition] > i) { // we need to add a device
        wireInsertDevice(currentPosition, i);
        currentPosition++;
      }
    }
    unprotectThread();
    nilThdSleepMilliseconds(1);
  }
  while (currentPosition < numberI2CDevices) {
    wireRemoveDevice(currentPosition);
  }
}

void printWireHelp(Print* output) {
  output->println(F("(il) List devices"));
  output->println(F("(ip) List parameters"));
}


void processWireCommand(char command, char* paramValue, Print* output) { // char and char* ??
  switch (command) {
    case 'p':
      if (paramValue[0] == '\0') {
        output->println(F("Missing device ID"));
      } else {
        printWireDeviceParameter(output, atoi(paramValue));
      }
      break;
    case 'l':
      printWireInfo(output);
      break;
    default:
      printWireHelp(output);
  }
}

#endif


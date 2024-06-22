/**
 *  Class to persist the iotFeeder module configuration data structure to EEPROM 
 *  Xavier Grosjean 2021
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */
 
#include "Config.h"


FeederConfigClass::FeederConfigClass(unsigned int version, const char* name):ModuleConfigClass(version, "iotFeeder", name, sizeof(FeederConfigStruct)) {
  for (uint8_t p = 0 ; p < PROGRAM_COUNT; p ++) {
    _programs[p] = new Program(&(_getDataPtr()->programs[p]));
  }
}

/**
 * Reset the config data structure to the default values.
 * This is done each time the data structure version is different from the one saved in EEPROM
 * NB: version and name are handled by base class 
 */
void FeederConfigClass::initFromDefault() {
  Debug("Reset iotFeeder Config to defaults");
  ModuleConfigClass::initFromDefault(); // handles version and name init, ssid and pwd
  for (uint8_t p = 0 ; p < PROGRAM_COUNT; p ++) {
    FeederProgram *fp = &(_getDataPtr()->programs[p]);
    fp->active = false;
    fp->hour = 0;
    fp->quantity = 0;
    Debug("fp config init default 2 - %d hour:%d\n", p, fp->hour);

  }
  _getDataPtr()->irThreshold = 3;
  saveToEeprom();
}

const char* FeederConfigClass::getDefaultUIClassName() {
  Debug("FeederConfigClass::getDefaultUIClassName");
  return "FeederUIClass";
}

Program* FeederConfigClass::getProgram(uint8_t offset) {
  return _programs[offset];
}

uint8_t FeederConfigClass::getIrThreshold() {
  return _getDataPtr()->irThreshold;
}
void FeederConfigClass::setIrThreshold(uint8_t t) {
  _getDataPtr()->irThreshold = t;
}

/**
 * Return the typed data structure object
 *
 */
FeederConfigStruct* FeederConfigClass::_getDataPtr(void) {
  return (FeederConfigStruct*)ModuleConfigClass::_getDataPtr();
}
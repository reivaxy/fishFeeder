/**
 *
 *  Definition of the config data structure for the iotFeeder module and the class to persist it to EEPROM 
 *  Xavier Grosjean 2018
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */
 
#pragma once

#include <XIOTConfig.h>
#include <XUtils.h>
#include "Program.h"

#define CONFIG_VERSION 2
#define MODULE_NAME "Feeder"

#define PROGRAM_COUNT 3

// Add specific config fields to the generic module config structure
struct FeederConfigStruct:ModuleConfigStruct {
  FeederProgram programs[PROGRAM_COUNT];
  uint8_t irThreshold;
};


class FeederConfigClass:public ModuleConfigClass {
public:
  FeederConfigClass(unsigned int version, const char* name);
  void initFromDefault() override;
  const char* getDefaultUIClassName() override;
  void saveProgram(Program *program, uint8_t offset);
  Program* getProgram(uint8_t offset);
  uint8_t getIrThreshold();
  void setIrThreshold(uint8_t);

protected:
  FeederConfigStruct* _getDataPtr(void);  
  Program* _programs[PROGRAM_COUNT];
};

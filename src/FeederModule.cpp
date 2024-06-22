/**
 *  iotinator iotFeeder Agent module
 *  Xavier Grosjean 2022
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */
 
#include "FeederModule.h"

#define HOUR_PARAM_NAME "p_h_%d"
#define QUANTITY_PARAM_NAME "p_q_%d"
#define ACTIVE_PARAM_NAME "p_a_%d"
 
#define REVERSE_STEP_COUNT 40

#define MANUAL_STEP_COUNT 10000
#define RESET_DELAY_MS 1*60*1000 // in milliseconds
#include "settingsPageHtml.h"

#define IR_IN  A0

FeederModule::FeederModule(FeederConfigClass* config, int displayAddr, int displaySda, 
                            int displayScl, int forwardPin, int reversePin):XIOTModule(config, displayAddr, displaySda, displayScl, false, config->getBrightness()) {
  pinMode(forwardPin, INPUT);
  pinMode(reversePin, INPUT);
  lastStatus[0] = 0;
  _forwardPin = forwardPin;
  _reversePin = reversePin;
  _oledDisplay->setLineAlignment(2, TEXT_ALIGN_CENTER);
  _config = config;
  lastTriggerTime = 0;
  stepper = Stepper();
  initMsgSchedule();
  Debug("IR Threshold: %d\n", _config->getIrThreshold());

  _oledDisplay->setLineAlignment(1, TEXT_ALIGN_CENTER); 
  _oledDisplay->setLineAlignment(3, TEXT_ALIGN_CENTER);
  _oledDisplay->setTransientDuration(2, 30000); 
  _oledDisplay->setTransientDuration(3, 30000); 
  _server->on("/feederApi/saveSettings", HTTP_POST, [&]() {
    saveSettings();
  });
  _server->on("/", HTTP_GET, [&]() {
    settingsPage();
  });

  _server->on("/feederApi/feedOnce", HTTP_POST, [&]() {
    feedOnce();
  });
}

void FeederModule::feedOnce() {
  String quantity = _server->arg("test_qtity");
  int16_t qtity = quantity.toInt();
  if (qtity < -100 || qtity > 5000) {    
    sendHtml(MSG_INIT_ILLEGAL_VALUE, 200);
    return;
  }
  if (qtity != 0) {
    stepper.addRun(qtity, true, false);
  }

 sendHtml(MSG_INIT_DONE, 200);
}

void FeederModule::initMsgSchedule() {
  *messageSchedule = 0;
  char one[4];
  for (uint8_t p = 0; p < PROGRAM_COUNT; p ++) {
    Program *prgm = _config->getProgram(p);
    if (prgm->isActive()) {
      sprintf(one, "%02d ", prgm->getHour());
      strcat(messageSchedule, one);
    }
  }
  _oledDisplay->setLine(1, messageSchedule);
}

void FeederModule::settingsPage() {
  MemSize("before displaying settings page");
  int maxSize = strlen_P(settingsBeginingPage) + strlen(_config->getName())  + strlen(NTP.getTimeDateString().c_str()) + strlen(lastStatus) 
                                             + strlen_P(settingsEndingPage)+ PROGRAM_COUNT * (strlen_P(settingTemplate) + 20) + 50 ;  
  Debug("Setting page size %d\n", maxSize);
  char* result = (char*)malloc(maxSize);
  sprintf_P(result, settingsBeginingPage, _config->getName(), NTP.getTimeDateString().c_str(), lastStatus);
  char *resultPtr = result + strlen(result);
  for (uint8_t p = 0; p < PROGRAM_COUNT; p ++) {
    Program *prgm = _config->getProgram(p);
    sprintf_P(resultPtr, settingTemplate, p, prgm->getHour(),
                                    p, prgm->getQuantity(),
                                    p, prgm->isActive()?"checked":"");
    resultPtr = result + strlen(result);
  }
  strcat_P(result, settingsEndingPage);
  Debug("Used setting page size %d\n", strlen(result));
  sendHtml(result, 200);
  free(result);

  // Debugging message fifo
  // for(int i= 0; i < 22; i++) {
  //   char message[100];
  //   sprintf(message, "Message %d", i);
  //   firebase->differMessage(message);
  // }
  MemSize("after displaying settings page");
}

void FeederModule::saveSettings() {
  MemSize("before saving settings");
  // sizing param names for PROGRAM_COUNT < 100
  char hourParamName[7];
  char quantityParamName[7];
  char activeParamName[7];
  Program *prgms[PROGRAM_COUNT];
  FeederProgram fprgms[PROGRAM_COUNT];
  for (uint8_t p = 0; p < PROGRAM_COUNT; p ++) { 
    sprintf(hourParamName, HOUR_PARAM_NAME, p);
    sprintf(quantityParamName, QUANTITY_PARAM_NAME, p);
    sprintf(activeParamName, ACTIVE_PARAM_NAME, p);
    String hour = _server->arg(hourParamName);
    String quantity = _server->arg(quantityParamName);
    String active = _server->arg(activeParamName);
    Program* prgm = new Program(&fprgms[p]);
    uint16_t qtity = (uint16_t)quantity.toInt();
    if (qtity > 5000) {
      qtity = 0;
    }
    prgm->setQuantity(qtity);
    prgm->setHour((uint8_t)hour.toInt());
    prgm->setActive(active.equals("on")?true:false);
    prgms[p] = prgm;
    //Serial.printf("p%d Hour %s Qtity %s Active %s\n", p, hour.c_str(), quantity.c_str(), active.c_str());
  }
  int n = sizeof(prgms) / sizeof(prgms[0]);
  std::sort(prgms, prgms + n, [](Program* a, Program* b) {
    return a->getHour() < b->getHour();
  });

  for (uint8_t p = 0; p < PROGRAM_COUNT; p ++) { 
    Program *prgm = _config->getProgram(p);
    prgm->setHour(prgms[p]->getHour());
    prgm->setQuantity(prgms[p]->getQuantity());
    prgm->setActive(prgms[p]->isActive());    
    delete prgms[p];
  }
  _config->saveToEeprom();
  // Trying to send a message while processing an incoming request crashes the module
  // So we send it later
  firebase->differLog(MSG_LOG_SCHEDULE_UPDATED);
  sendHtml(MSG_INIT_DONE, 200);
  initMsgSchedule();
  MemSize("end of saving settings");
}

void FeederModule::setCustomModuleRecordFields(JsonObject *jsonBufferRoot) {
  bool with_ir = true;
  #ifdef NO_IR
  with_ir = false;
  #endif
  jsonBufferRoot->set("with_ir", with_ir);
}

void FeederModule::dispensingFailed(bool transientDisplay) {
    Debug("%s WARNING NO FOOD DETECTED\n", NTP.getTimeDateString(now()).c_str()); 
    _oledDisplay->setLine(3, MSG_DISPLAY_FAILED, transientDisplay, false, true);
    strcat(lastStatus, ": ");
    strcat(lastStatus, MSG_ALERT_DISPENSING_FAILURE);
    firebase->differAlert(MSG_ALERT_DISPENSING_FAILURE);
    // Sending notif Could be handled by a Firebase function but not sure it's best
    sendPushNotif(_config->getName(), MSG_ALERT_DISPENSING_FAILURE);
}

char *FeederModule::customFormInitPage() {
  char input[150];
  sprintf(input, "%s <br/><input name='irThreshold' type='number' min='0' max='100' value='%d'/><br/>", MSG_INIT_IR_THRESHOLD, _config->getIrThreshold());
  return(XUtils::mallocAndCopy(input));
}

int FeederModule::customSaveConfig() {
    String irThresholdStr = _server->arg("irThreshold");
    if (irThresholdStr.length() > 0) {
      uint8_t irThreshold = (uint8_t)irThresholdStr.toInt();
      Debug("Setting irThreshold to %d\n", irThreshold);
      _config->setIrThreshold(irThreshold);
    }
}

void FeederModule::loop() {
  XIOTModule::loop();  // takes care of server, display, ...

  // Check programs and trigger stepper if necessary
  uint16_t quantity = 0; 
  if (XUtils::isElapsedDelay(millis(), &lastTriggerTime, RESET_DELAY_MS)) {
    quantity = checkQuantity();
  }
  if (quantity != 0) {
    sprintf(lastStatus, "%s: %s %d ", NTP.getTimeDateString(now()).c_str(), MSG_LOG_AUTO_DISPENSING, quantity);
    Debug("%s\n", NTP.getTimeDateString(now()).c_str());
    char message[50];
    sprintf(message, "%s %02d:%02d, %s: %d\n", MSG_DISPLAY_AT, (uint8_t)hour(), (uint8_t)minute(), MSG_DISPLAY_QTITY, quantity);
    Debug(message);
    _oledDisplay->setLine(2, message);
    _oledDisplay->setLine(3, "");
    lastTriggerTime = millis();
    // Activate the stepper, first backwards to unstuck just in case
    // IR detector is powered up when stepper is running, since it's plugged to the EN pin            
    #ifdef NO_IR 
    bool checkIR = false;
    #else
    bool checkIR = true;
    #endif
    stepper.addRun(-REVERSE_STEP_COUNT, false, true);
    stepper.addRun(quantity + REVERSE_STEP_COUNT, checkIR, true);
    _programRunning = true;
  }


  
  // Re enable programs after some delay since last program triggered and disabled itself
  if (lastTriggerTime != 0 && XUtils::isElapsedDelay(millis(), &lastTriggerTime, RESET_DELAY_MS)) {
    Debug("Re-enabling programs");
    for (uint8_t p = 0; p < PROGRAM_COUNT; p ++) {
      _config->getProgram(p)->reEnable();     
    }
    _programRunning = false;
    lastTriggerTime = 0; // no need to re enable before one program is triggered
  }
 
  bool stepperRan = stepper.refresh();

  // If stepper had something to do
  if (stepperRan) {
    if (!stepper.isDone()) {
      // We don't check IR on backwards runs, or if device not equipped with IR sensor
      if (stepper.shouldCheckIR()) {
        int level = analogRead(IR_IN)/10;                                                                                                                   
        if (abs(_previousLevel - level) > _config->getIrThreshold()) {
          if (_previousLevel != -1) {
            //Serial.printf("%s IR Detection ! %d\n", NTP.getTimeDateString(now()).c_str(),  level);    
            foodDetected = true;
          }
        }
        _previousLevel = level;  
      }

    } else {
      // A run just ended
      Debug("A run of %ld steps just ended\n", stepper.currentQuantity());
      if (stepper.shouldCheckIR()) {
        Debug("IR Check required. Food detected: %s\n", foodDetected?"true":"false");
        if (!foodDetected) {
          // Send alert and display non transient message
          dispensingFailed(false);
        }        
      }  
  
      if (_manualReverse) {
        _manualReverse = false;
        firebase->differLog(MSG_LOG_MANUAL_REVERSE);
      }
      if (stepper.currentQuantity() > 0 && stepper.currentQuantity() < MANUAL_STEP_COUNT) {
        if (stepper.isProgrammedRun()) {
          long quantity = stepper.currentQuantity() - REVERSE_STEP_COUNT; // It was added when the run was created to compensate for going backward first
          DynamicJsonBuffer jsonBuffer(firebase->getBufferSize(2));
          JsonObject& jsonBufferRoot = jsonBuffer.createObject();
          jsonBufferRoot["message"] = MSG_LOG_AUTO_DISPENSING;
          jsonBufferRoot["quantity"] = quantity;
          firebase->differLog(&jsonBufferRoot);
        } else{
          // This features also allows to go backward by providing a negative quantity on the webpage.
          // we don't want to display nor warn 
          if (stepper.currentQuantity() > 0) {
            sprintf(lastStatus, "%s: %s %ld ", NTP.getTimeDateString(now()).c_str(), MSG_INIT_TEST_QUANTITY, stepper.currentQuantity());
            char message[40];
            sprintf(message, "%s %02d:%02d, %s: %ld\n", MSG_DISPLAY_AT, hour(), minute(), MSG_DISPLAY_QTITY, stepper.currentQuantity());

            _oledDisplay->setLine(2, message);
            _oledDisplay->setLine(3, "");
      
          }
          DynamicJsonBuffer jsonBuffer(firebase->getBufferSize(2));
          JsonObject& jsonBufferRoot = jsonBuffer.createObject();
          jsonBufferRoot["message"] = MSG_INIT_TEST_QUANTITY;
          jsonBufferRoot["quantity"] = stepper.currentQuantity();
          firebase->differLog(&jsonBufferRoot);
        }
      }
    }
  } else {
    _previousLevel = -1;
    foodDetected = false;
  }

  // Check the "move forward" button, and move forward accordingly
  int pushForward = digitalRead(_forwardPin);
  if (pushForward == HIGH) {
    if (!_manualForward) {
      _manualForward = true;
      // Activate the stepper for a big step count, it will be stopped when button is released
      // This also powers up the IR detector since it's plugged to the EN pin
      stepper.addRun(MANUAL_STEP_COUNT, true, false);
    }
  } else {
    if (_manualForward) {
      _manualForward = false;
      long remainingSteps = stepper.interrupt();
      long stepCount = MANUAL_STEP_COUNT - remainingSteps;
      sprintf(lastStatus, "%s: %s %ld ", NTP.getTimeDateString(now()).c_str(), MSG_LOG_MANUAL_DISPENSING, stepCount);
      char message[40];
      // sprintf(message, "%s: %ld\n", MSG_DISPLAY_QTITY, stepCount);   
      sprintf(message, "%s %02d:%02d, %s: %ld\n", MSG_DISPLAY_AT, hour(), minute(), MSG_DISPLAY_QTITY, stepCount);
   
      _oledDisplay->setLine(2, message);
      _oledDisplay->setLine(3, "");
     
      DynamicJsonBuffer jsonBuffer(firebase->getBufferSize(2));
      JsonObject& jsonBufferRoot = jsonBuffer.createObject();
      jsonBufferRoot["message"] = MSG_LOG_MANUAL_DISPENSING;
      jsonBufferRoot["quantity"] = stepCount;
      firebase->differLog(&jsonBufferRoot);
    }
  }

  // Check the "move back" button, and move back by a small step quantity if necessary
  int pushReverse = digitalRead(_reversePin);
  if (pushReverse == HIGH) {
    if (!_manualReverse && XUtils::isElapsedDelay(millis(), &lastReverseTime, 20000)) {
      _manualReverse = true;
      lastReverseTime = millis();
      stepper.addRun(-40, false, false);   
    }
  } else {
    lastReverseTime = 0;  // Once button was released, can be used again
  }

}

long FeederModule::checkQuantity() {
  Debug("%s: Check quantity\n", NTP.getTimeDateString(now()).c_str());
  long quantity = 0;
  int mi = minute();
  // when in test, return non null quantity every X mn where
  // X is either the value of PRGM_TEST or 5 if PRGM_TEST is 0
#ifdef PRGM_TEST
  if (!isTimeInitialized()) {
    return 0;
  }
  int testPeriod = 5;
  if (PRGM_TEST > 0) {
    testPeriod = PRGM_TEST;
  }
  Debug("%s: Check quantity debug mode,  every %dmn\n", NTP.getTimeDateString(now()).c_str(), testPeriod);
  int remainder = mi % testPeriod;
  if (remainder == 0) {
    quantity = 500;
  }
#else
  // Only check programs if minute is 0
  if (mi == 0) {
    int s = second();
    // Only check programs during the first 10 seconds of each hour
    // This is a security in case the esp restarts while dispensing food, we don't want to dispense it again.
    // The time span should be at most the one needed to obtain time from NTP after a restart
    // When a program triggers, it disables itself to not trigger again, until re enabled
    if (!_programRunning && s < 10) {
      uint8_t h = (uint8_t)hour();
      for (uint8_t p = 0; p < PROGRAM_COUNT; p ++) {
        // If an active program is set for the h hour, it will return a non 0 quantity
        quantity = _config->getProgram(p)->triggerQuantity(h);       
        if (quantity > 0) {
          break;
        }
      }
    }
  }
  #endif

  return quantity;
}
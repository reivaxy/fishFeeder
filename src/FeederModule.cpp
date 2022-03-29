/**
 *  iotinator iotFeeder Agent module
 *  Xavier Grosjean 2022
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */
 
#include "FeederModule.h"

#define HOUR_PARAM_NAME "p_h_%d"
#define QUANTITY_PARAM_NAME "p_q_%d"
#define ACTIVE_PARAM_NAME "p_a_%d"
 
 #define MANUAL_STEP_COUNT 10000
#define RESET_DELAY_MS 1*60*1000 // in milliseconds
#include "settingsPageHtml.h"

#define IR_IN  A0

FeederModule::FeederModule(FeederConfigClass* config, int displayAddr, int displaySda, 
                            int displayScl, int forwardPin, int reversePin):XIOTModule(config, displayAddr, displaySda, displayScl, false, 255) {
  pinMode(forwardPin, INPUT);
  pinMode(reversePin, INPUT);

  _forwardPin = forwardPin;
  _reversePin = reversePin;
  _oledDisplay->setLineAlignment(2, TEXT_ALIGN_CENTER);
  _config = config;
  lastTriggerTime = 0;
  stepper = Stepper();
  initMsgSchedule();
  _oledDisplay->setLineAlignment(1, TEXT_ALIGN_CENTER);
  _oledDisplay->setLineAlignment(3, TEXT_ALIGN_CENTER);
  _oledDisplay->setTransientDuration(2, 30000);  // List to display step count at end of test session
  _server->on("/feederApi/saveSettings", HTTP_POST, [&]() {
    saveSettings();
  });
  _server->on("/", HTTP_GET, [&]() {
    settingsPage();
  });
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
  String pageTemplate(FPSTR(settingsBeginingPage));
  String endingTemplate(FPSTR(settingsEndingPage));
  String formTemplate(FPSTR(settingTemplate));
  int maxSize = strlen(pageTemplate.c_str()) + strlen(endingTemplate.c_str()) + strlen(_config->getName()) + PROGRAM_COUNT * (strlen(formTemplate.c_str()) + 15) ;  
  Serial.printf("Size %d\n", maxSize);
  char* result = (char*)malloc(maxSize);
  sprintf(result, pageTemplate.c_str(), _config->getName());
  char *resultPtr = result + strlen(result);
  for (uint8_t p = 0; p < PROGRAM_COUNT; p ++) {
    Program *prgm = _config->getProgram(p);
    sprintf(resultPtr, formTemplate.c_str(), p, prgm->getHour(),
                                    p, prgm->getQuantity(),
                                    p, prgm->isActive()?"checked":"");
    resultPtr = result + strlen(result);
  }
  strcat(result, endingTemplate.c_str());
  sendHtml(result, 200);
  free(result);
}

void FeederModule::saveSettings() {
  uint32_t freeMem = system_get_free_heap_size();
  Serial.printf("%s Heap before sorting programs: %d\n", NTP.getTimeDateString().c_str(), freeMem); 
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
    prgm->setHour((uint8_t)hour.toInt());
    prgm->setQuantity((uint16_t)quantity.toInt());
    prgm->setActive(active.equals("on")?true:false);
    prgms[p] = prgm;
    //Serial.printf("p%d Hour %s Qtity %s Active %s\n", p, hour.c_str(), quantity.c_str(), active.c_str());
  }
  std::sort(&prgms[0], &prgms[PROGRAM_COUNT], [](Program* a, Program* b) {
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
  freeMem = system_get_free_heap_size();
  Serial.printf("%s Heap after sorting programs: %d\n", NTP.getTimeDateString().c_str(), freeMem);   
  // Trying to send a message while processing an incoming request crashes the module
  // So we send it later
  firebase->sendDifferedLog("Schedule updated");
  sendHtml("Config saved", 200);
  initMsgSchedule();
}

void FeederModule::logProgramedDispensing(uint16_t quantity) {

}

void FeederModule::loop() {
  XIOTModule::loop();  // takes care of server, display, ...

  // Check programs and trigger stepper if necessary
  if(isTimeInitialized()) {
    uint16_t quantity = 0;
    // Only check programs if minute is 0
    int mi = minute();
    if (mi == 0) {
      int s = second();
      // Only check programs during the first 10 seconds of each hour
      // This is a security in case the esp restarts while dispensing food, we don't want to dispense it again.
      // The delay should be at most the one needed to obtain time from NTP after a restart
      // When a program triggers, it disables itself to not trigger again, until re enabled
      if (s < 10) {
        uint8_t h = (uint8_t)hour();
        for (uint8_t p = 0; p < PROGRAM_COUNT; p ++) {
          quantity = _config->getProgram(p)->triggerQuantity(h);        
          if (quantity != 0) {
            Serial.printf("%s\n", NTP.getTimeDateString(now()).c_str());
            char message[50];
            sprintf(message, "At %d:00, Qtity: %d\n", h, quantity);
            Serial.printf(message);
            _oledDisplay->setLine(2, message);
            lastTriggerTime = millis();
            // Activate the stepper
            // This also powers up the IR detector since it's plugged to the EN pin
            stepper.start(quantity);
            // logProgramedDispensing(quantity);
            char log[50];
            sprintf(log, MSG_LOG_AUTO_DISPENSING, quantity);
            firebase->sendLog(log);
            break;
          }
        }
      }
    } 
  } 
  
  // Re enable programs after some delay since last program triggered and disabled itself
  if (lastTriggerTime != 0 && XUtils::isElapsedDelay(millis(), &lastTriggerTime, RESET_DELAY_MS)) {
    Serial.println("Re-enabling programs");
    for (uint8_t p = 0; p < PROGRAM_COUNT; p ++) {
      _config->getProgram(p)->reEnable();     
    }
    lastTriggerTime = 0; // no need to re enable before one program is triggered
  }

  if (stepper.remaining()) {
    // this test needs to be done before calling "stepper.run" since it will reset the firstStep flag
    if (stepper.isFirstStep()) {
      mustWarnNoFoodDetected = true;
    }
    stepper.run();
    int level = analogRead(IR_IN)/10;                                                                                                                   
    if (abs(_previousLevel - level) > 3) {
      if (_previousLevel != -1 && isTimeInitialized()) {
        //Serial.printf("%s IR Detection ! %d\n", NTP.getTimeDateString(now()).c_str(),  level);    
        mustWarnNoFoodDetected = false;
      }
    }
    _previousLevel = level;  
  } else {
    stepper.stop();
#ifndef NO_IR    
    if (mustWarnNoFoodDetected && !_manualReverse) {
      Serial.printf("%s WARNING NO FOOD DETECTED\n", NTP.getTimeDateString(now()).c_str()); 
      firebase->sendAlert(MSG_ALERT_DISPENSING_FAILURE);
      // Sending notif Could be handled by a Firebase function but not sure it's best
      sendPushNotif(_config->getName(), MSG_ALERT_DISPENSING_FAILURE);
    }
#endif    
    _manualReverse = false;
    mustWarnNoFoodDetected = false;
  }


  // Check the "move forward" button, and move forward accordingly
  int pushForward = digitalRead(_forwardPin);
  if (pushForward == HIGH) {
    if (!_manualForward) {
      _manualForward = true;
      // Activate the stepper for a big step count, it will be stopped when button is released
      // This also powers up the IR detector since it's plugged to the EN pin
      stepper.start(MANUAL_STEP_COUNT);
    }
  } else {
    if (_manualForward) {
      _manualForward = false;
      long stepCount = MANUAL_STEP_COUNT - stepper.remaining();
      stepper.stop();
      char message[40];
      sprintf(message, "Quantity: %ld\n", stepCount);      
      _oledDisplay->setLine(2, message, true, false, true);
      char log[50];
      sprintf(log, MSG_LOG_MANUAL_DISPENSING, stepCount);
      firebase->sendLog(log);
    }
  }

  // Check the "move back" button, and move back by a small step quantity if necessary
  int pushReverse = digitalRead(_reversePin);
  if (pushReverse == HIGH) {
    _manualReverse = true;
    if (XUtils::isElapsedDelay(millis(), &lastReverseTime, 2000)) {
      lastReverseTime = millis();
      stepper.start(-40);    
      firebase->sendLog(MSG_LOG_MANUAL_REVERSE);
    }
  }

}


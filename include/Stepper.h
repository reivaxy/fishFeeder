/**
 *  iotinator iotFeeder Agent module
 *  Xavier Grosjean 2022
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */

#pragma once

#include <Arduino.h>
#include <AccelStepper.h>
#include "StepperRun.h"

#define MAX_RUN_COUNT 4

class Stepper {
public:
   Stepper();
   long remaining();
   long stop(); // returns the number of remaining steps and stops
   void initRuns();
   long interrupt();
   bool refresh();
   bool shouldCheckIR();
   bool isFirstStep();
   bool isDone();
   bool isProgrammedRun();
   long currentQuantity();
   int addRun(int count, bool checkIR, bool automaticRun);

   int IN1 = 13; 
   int IN2 = 12; 
   int EN = 14;  
   
   AccelStepper stepper;
   bool firstStep = false;
   StepperRun* currentRun = NULL;

   StepperRun* runs[MAX_RUN_COUNT];

};
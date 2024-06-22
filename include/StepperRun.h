/**
 *  iotinator iotFeeder Agent module
 *  Xavier Grosjean 2022
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */
#pragma once

#include <Arduino.h>
#include <XIOTModuleDebug.h>
#include <AccelStepper.h>

class StepperRun {
public:
   StepperRun(AccelStepper *stepper, long stepCount, bool checkIR, bool programmedRun);

   long _stepCount = 0;
   bool _checkIR = false;  // IR should not be checked on reverse rotation, or non IR enabled device
   bool _programmedRun = false;  // This run was triggered by feeder program

   AccelStepper *_stepper;
   bool _running = false;
   bool _done = false;
   bool _firstStep = true;

   bool isRunning();
   bool isDone();
   bool isFirstStep();
   bool shouldCheckIR();
   bool isProgrammedRun();
   long stop();
   void start();
   long interrupt();
   long refresh();
   long getStepCount();

};
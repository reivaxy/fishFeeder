/**
 *  iotinator iotFeeder Agent module
 *  Xavier Grosjean 2022
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */


#include "Stepper.h"

Stepper::Stepper() {
   stepper = AccelStepper(AccelStepper::FULL2WIRE, IN1, IN2);
   initRuns();
   stepper.setEnablePin(EN);
   stepper.setCurrentPosition(0);
   stepper.setMaxSpeed(200.0);
   stepper.setAcceleration(100.0);
   stepper.disableOutputs();
}

void Stepper::initRuns() {
   for (int offset=0 ; offset < MAX_RUN_COUNT; offset ++) {
      runs[offset] = NULL;
   }
}

long Stepper::remaining() {
   return stepper.distanceToGo();
}

bool Stepper::isFirstStep() {
   if (currentRun != NULL) {
      return currentRun->isFirstStep();
   }
   return false;
}

long Stepper::stop() {
   long remainingSteps = stepper.distanceToGo();
   if (currentRun != NULL) {
      remainingSteps = currentRun->stop();
   }
   return remainingSteps;
}

long Stepper::interrupt() {
   long remainingSteps = 0;
   if (currentRun != NULL) {
      remainingSteps = currentRun->interrupt();
   }
   return remainingSteps;
}

bool Stepper::shouldCheckIR() {
   if (currentRun != NULL) {
      return currentRun->shouldCheckIR();
   }
   return false;
}

bool Stepper::isDone() {
   if (currentRun != NULL) {
      return currentRun->isDone();
   }
   return false;   
}

bool Stepper::isProgrammedRun() {
   if (currentRun != NULL) {
      return currentRun->isProgrammedRun();
   }
   return false;   
}

long Stepper::currentQuantity() {
   if (currentRun != NULL) {
      return currentRun->getStepCount();
   }
   return 0;   
}

bool Stepper::refresh() {
   bool somethingChanged = false;
   if (currentRun != NULL) {
      bool done = currentRun->isDone();
      if (done) {
         delete(currentRun);
         currentRun = NULL;
         for(int offset=0; offset < MAX_RUN_COUNT-1; offset++) {
            runs[offset] = runs[offset + 1];
         }
         runs[MAX_RUN_COUNT - 1] = NULL;
      } else {
         currentRun->refresh();
         somethingChanged = true;
      }

   } else {
      if (runs[0] != NULL) {
         currentRun = runs[0];
         currentRun->start();
      }
   }
   return somethingChanged;
}

int Stepper::addRun(int count, bool checkIR, bool automaticRun) {
   StepperRun* sr = new StepperRun(&stepper, count, checkIR, automaticRun);
   uint offset = 0;
   while(offset < MAX_RUN_COUNT) {
      if (runs[offset] == NULL) {
         runs[offset] = sr;
         return offset;
      }
      offset++;
   }
   return -1; // couldn't add run

}
/**
 *  iotinator iotFeeder Agent module
 *  Xavier Grosjean 2022
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */


#include "StepperRun.h"

StepperRun::StepperRun(AccelStepper *stepper, long stepCount, bool checkIR, bool programmedRun) {
   _stepCount = stepCount;
   _checkIR = checkIR;
   _stepper = stepper;
   _programmedRun = programmedRun;
}

long StepperRun::refresh() {
   long remainingSteps = _stepper->distanceToGo();
   if (remainingSteps == 0) {
     stop();
   } else {
      _stepper->run();
      _firstStep = false;
   }  
   return _stepper->distanceToGo();
}

long StepperRun::getStepCount() {
   return _stepCount;
}

bool StepperRun::isDone() {
   return _done;
}
bool StepperRun::isFirstStep() {
   return _firstStep;
}
bool StepperRun::shouldCheckIR() {
   return _checkIR;
}
bool StepperRun::isProgrammedRun() {
   return _programmedRun;
}

long StepperRun::stop() {
   long remainingSteps = interrupt(); // can be stopped before the end
   _done = true;
   _running = false;
   return remainingSteps;   
}

long StepperRun::interrupt() {
   long remainingSteps = _stepper->distanceToGo(); // can be stopped before the end
   Debug("Stopping run of %ld steps after %ld steps, checkIR: %s\n", _stepCount, _stepCount - remainingSteps, _checkIR?"true":"false");
   _stepper->stop();
   _stepper->setCurrentPosition(0);
   _stepper->disableOutputs();
   return remainingSteps;
}

void StepperRun::start() {
   Debug("Starting run for %ld steps, checkIR: %s\n", _stepCount, _checkIR?"true":"false");
   _firstStep = true;
   _running = true;
   _stepper->move(_stepCount);
   _stepper->enableOutputs();
}
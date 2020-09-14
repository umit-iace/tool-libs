.. todo:: say something about the TickServer?

.. todo:: say something about  Transport & Min? 

.. todo:: say something about EXP_KEEPALIVE or enough in detailed utils/Experiment.rst ?



Overview
========
This is an collection of libraries used at the UMIT-Tirol Institute of Automation and Control Engineering. It includes different sensor implementations, an abstraction layer for the HAL-libraries and miscellaneous utils, like a RequestQueues and DynamicArrays.

The main part is an easy implementation of a experiment process combined with `pyWisp`.

Experiment
----------
Therefore, the experiment process follows a control oriented structure by interconnecting
different ExperimentModules in a defined framework.

Every registered module get's called via the state machine, in the order they were registered, of the experiment. This assures easy control to start, pause or stop the experiment with the pyWisp interface. 


.. tikz::
    :include: latex/exp_state_machine_blocks.tikz


Interconnection between ExperimentModules
------------------------------------------
The connection between different ExperimentModules is done after constructing each module, the user can simply call **registerModules(ExperimentModule *mod, ...)** and pass any modules.

The user can decide how he want's to store his data. The user has two different double arrays. with a fixed length specified in the constructor to work with. Those arrays are called **outputs** and **states**. This allows to differenciate betweeen internal process variables or input variables and the final output of the module, e.g. the control signal **u**.


minimal example 
----------------------

To create a control loop, `ExpTraj`, `ExpRig` and `ExpController` have to derive from ExperimentModule.

.. tikz::
   :include: latex/exp_modules_connections.tikz


The following diagramm shows an minimal closed loop configuration, containing a trajectory-generator,  PID-controller and a system with one 4-wire fan. 
Signals are defined as:

 - **w** := setpoint for the controller, stored in `ExpTraj.outputs[0]`
 - **e** := w - y, current error of the controller, stored in `ExpCtrl.dError`
 - **u** := pwm signal of the controller, stored in `ExpCtrl.outputs[0]`
 - **y** := rpm signal of the fan, stored in `ExpRig.states[0]`

.. tikz::
    :include: latex/exp_modules.tikz

After the Modules are interconnected, the user can simply dereference the ExperimentModule pointer and call either **getStates()** or **getOutputs()**, to get access to the data. 



.. code-block:: cpp

   #include "utils/Experiment.h"
   #include "stm/hal.h"
   #include "stm/timer.h"
   #include "stm32f4xx_it.h"
   #include "Min.h"
   #include "ExperimentModules.h"

   Experiment *experiment();

   // define control timer parameters
   #define EXP_DT                        10          ///< samplerate in [ms]
   #define EXP_TIMER                     TIM7        ///< timer to used
   #define EXP_TIMER_IRQ                 TIM7_IRQn   ///< interrupt used 
   #define EXP_TIMER_PRIO                4,4         ///< timer priority 


   // interrupt callback for timer running with defined samplerate 
   void expCallback(TIM_HandleTypeDef *){
        experiment->run();
        }

   int main() {
       
       // configure experiment  communication 
       Min MIN;
       Transport transport(&MIN);

       // create experiment modules 
       ExpTraj traj;
       ExpRig rig;
       ExpCtrl ctrl;

       // connect modules 
       rig.registerModules(&ctrl);
       ctrl.registerModules(&traj, &rig);

       // setup experiment 
       experiment = new Experiment();
       experiment.registerModules(&traj);
       experiment.registerModules(&ctrl);
       experiment.registerModules(&rig);

       // setup control timer with defined samplerate
       HardwareTimer expTim(EXP_TIMER, 42000 - 1, 2 * EXP_DT);
       hControlTim = expTim.handle();
       expTim.configCallback(expCallback, EXP_TIMER_IRQ, EXP_TIMER_PRIO);
       expTim.start();
   
       // if transport protocol with CRC is activated 
     for (;;) {
        #ifdef TRANSPORT_PROTOCOL
           Min::poll();
        #endif
     }
   }


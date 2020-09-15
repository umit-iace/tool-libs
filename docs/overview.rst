.. todo:: say something about the TickServer?

.. todo:: say something about  Transport & Min? 

.. todo:: say something about EXP_KEEPALIVE or enough in detailed utils/Experiment.rst ?



Overview
========
This is a collection of libraries used at the UMIT-Tirol Institute of Automation and Control Engineering. It includes different sensor implementations, an abstraction layer built on top of the STM-HAL-libraries and miscellaneous utilities, like RequestQueues and DynamicArrays.

The main part is a simple implementation of an experiment process designed to be used with `pyWisp <https://github.com/umit-iace/tool-pywisp>`_.

Experiment
----------
The :doc:`experiment<utils-doc/Experiment_h>` process follows a control oriented structure by interconnecting
different :doc:`modules<utils-doc/ExperimentModule_h>` in a defined framework.

Every registered module gets called via the experiment's state machine. This assures easy control to start, pause, or stop the experiment with the pyWisp interface. 


.. tikz::
    :include: latex/exp_state_machine_blocks.tikz


Interconnection between ExperimentModules
------------------------------------------
The connection between different ExperimentModules is done after constructing each module, the user can simply call ``module.registerModules(module2, module3, ...)`` and pass any modules.

The user can decide how he wants to store his data.
Every module has two arrays called ``outputs`` and ``states`` of fixed lengths, which have to be specified in the constructor. This allows to differenciate betweeen internal process variables or input variables and the final output of the module.


Minimal example
---------------

Let's create a small control loop with a trajectory generator, a controller,
and the rig we want to control. We'll call these modules ``ExpTraj``,
``ExpController``, and ``ExpRig``, respectively.

The following diagram shows the closed loop configuration of these modules,
also displaying the necessary connections between them.

Signals are defined as:

 - **w** := trajectory output
 - **e** := w - y, current error of the controller
 - **u** := controller output
 - **y** := measured rig state

.. tikz::
    :include: latex/exp_modules.tikz

Both the trajectory generator and the controller therefore need an **output**,
while the rig has a **state** (if necessary, the controller could also store
the error in a **state** variable).

The ExpModules we create have to inherit the abstract ``ExperimentModule``
class, and implement a number of methods. Most notably the ``compute(..)``
method which is called cyclically by the controlling :doc:`experiment
<utils-doc/Experiment_h>`. For interconnections the ``registerModules(..)``
method is also required.
.. tikz::
   :include: latex/exp_modules_connections.tikz

ExpTraj implementation
~~~~~~~~~~~~~~~~~~~~~~
.. code-block:: cpp

        #include <cmath>
        class ExpTraj : ExperimentModule {
        public:
                ExpTraj() : ExperimentModule(1, 0) { }

                double compute(uint32_t lTime) override {
                        // simple sine generator. f = 2Hz
                        this->outputs[0] = sin(lTime / 1000 * 4 * M_PI);
                }
        }

ExpController implementation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.. code-block:: cpp

        #include <cstdarg>
        class ExpController : ExperimentModule {
        private:
                // pointers to connected modules
                ExperimentModule *rig = nullptr, *traj = nullptr;
        public:
                ExpController() : ExperimentModule(1, 0) { }

                double compute(uint32_t lTime) override {
                        double e = traj->getOutput()[0] - rig->getState()[0];
                        // simple p controller
                        const double Kp = 10;
                        this->outputs[0] = this->Kp * e;
                }

                void registerModules(ExperimentModule *mod, ...) override {
                        traj = mod;
                        va_list args;
                        va_start(args, mod);
                        rig = (ExperimentModule *)va_arg(args, ExperimentModule *);
                        va_end(args);
                }
        }

ExpRig implementation
~~~~~~~~~~~~~~~~~~~~~
.. code-block:: cpp

        class ExpRig : ExperimentModule {
        private:
                // pointer to connected module
                ExperimentModule *ctrl = nullptr
        public:
                ExpRig() : ExperimentModule(0, 1) { }

                double compute(uint32_t lTime) override {
                        // act according to controller
                        this->act(ctrl->getOutput()[0]);
                        // measure current state
                        this->states[0] = this->measurestate();
                }

                void registerModules(ExperimentModule *mod, ...) override {
                        ctrl = mod;
                }
        }


main
~~~~
.. code-block:: cpp

       // create experiment modules 
       ExpTraj traj;
       ExpRig rig;
       ExpCtrl ctrl;

       // connect modules 
       rig.registerModules(&ctrl);
       ctrl.registerModules(&traj, &rig);

       // setup experiment 
       Experiment experiment;
       experiment.registerModules(&traj);
       experiment.registerModules(&ctrl);
       experiment.registerModules(&rig);

When all this is set up, you just have to ensure that ``experiment.run(dT)`` is
called cyclically, with ``dT`` being the number of milliseconds since last run.

.. code-block:: cpp

   #include "utils/Experiment.h"
   #include "stm/hal.h"
   #include "stm/timer.h"
   #include "stm32f4xx_it.h"
   #include "Min.h"
   #include "ExperimentModules.h"

   Experiment *experiment;

   // define control timer parameters
   #define EXP_DT                        10          ///< samplerate in [ms]
   #define EXP_TIMER                     TIM7        ///< timer to used
   #define EXP_TIMER_IRQ                 TIM7_IRQn   ///< interrupt used 
   #define EXP_TIMER_PRIO                4,4         ///< timer priority 


   // interrupt callback for timer running with defined samplerate 
   void expCallback(TIM_HandleTypeDef *){
        experiment->run(EXP_DT);
        }

   int main() {
       
       // configure experiment communication 
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
       experiment->registerModules(&traj);
       experiment->registerModules(&ctrl);
       experiment->registerModules(&rig);

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


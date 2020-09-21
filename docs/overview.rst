Overview
========
This is a collection of libraries used at the UMIT-Tirol Institute of Automation and Control Engineering. It includes different sensor implementations, an abstraction layer built on top of the STM-HAL-libraries and miscellaneous utilities, like RequestQueues and DynamicArrays.

The main part is a simple implementation of an experiment process designed to be used with `pyWisp <https://github.com/umit-iace/tool-pywisp>`_.

Experiment
----------
The :doc:`experiment<utils-doc/Experiment_h>` (exp) process follows a control oriented structure by interconnecting
different :doc:`modules<utils-doc/ExperimentModule_h>` in a defined framework.

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
``ExpCtrl`` and ``ExpRig`` respectively.

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

ExpCtrl implementation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.. code-block:: cpp

        #include <cstdarg>
        class ExpCtrl : ExperimentModule {
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


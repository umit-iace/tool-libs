/** @file ExperimentModule.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef EXPERIMENTMODULE_H
#define EXPERIMENTMODULE_H

#include <cstdint>

/**
 * class describing a generic experiment module
 */
class ExperimentModule {
public:
    /**
     * function is called regularly in EXP_DT ms intervals
     * @param lTime current experiment time in ms
     */
    virtual void compute(uint32_t lTime) {}

    /**
     * incoming Frame
     *
     * called from class: Transport if ExperimentModule registered for
     * at least one id. gets called for every registered id separately
     * @param id id of current frame to be unpacked
     */
    virtual void handleFrame(uint8_t id) {}

    /**
     * outgoing Frame
     *
     * called from class: Transport if ExperimentModule registered for
     * at least one id. gets called for every registered id separately
     * @param id id of current frame to be packed
     * @param lTime current experiment time in ms
     */
    virtual void sendFrame(uint8_t id, uint32_t lTime) {}

    /**
     * used for connecting experiment modules.
     *
     * is called from class: Experiment upon initialization.
     *
     * @param mod first module that the called modules needs data from.
     * @param ... list of modules that the called module needs data from.
     */
    virtual void registerModules(ExperimentModule *mod, ...) {}

    /**
     * return list of the module's outputs.
     * @return class: List of output variables
     */
    double *getOutput() { return outputs; }

    /**
     * return list of the module's states.
     * @return class: List of state variables
     */
    double *getState() { return states; }

    /**
     * preprocess.
     * called cyclically in the Experiment PRE state,
     * until all Modules signal no need for preprocessing
     * @return
     */
    virtual bool pre() { return false; }

    /**
     * reset ExperimentModule.
     * called upon Experiment STOP
     */
    virtual void reset() {}

    /**
     * Constructor for allocating state and output lists
     * @param outputs number of outputs for this ExperimentModule
     * @param states number of states for this ExperimentModule
     */
    ExperimentModule(int outputs, int states) {
        this->outputs = new double[outputs]();
        this->states = new double[states]();
    }

    ~ExperimentModule() {
        delete[] this->outputs;
        delete[] this->states;
    }

protected:
    double *outputs = nullptr;
    double *states = nullptr;
};

#endif //EXPERIMENTMODULE_H

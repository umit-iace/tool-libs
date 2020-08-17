/** @file Experiment.h
 *
 * Copyright (c) 2019 IACE
 */
#ifndef EXPERIMENT_H
#define EXPERIMENT_H

#include "ExperimentModules.h"
#include "utils/Transport.h"
#include <cstdarg>

/**
 * General class of an experiment used to control the experiment flow at a test rig.
 */
class Experiment {
public:
    /**
     * Initialize the experiment with a fixed amount of ExperimentModules
     * @param numberofModules number of ExperimentModules the experiment will run
     */
    Experiment(unsigned int numberofModules) {
        pExp = this;
        expMod = new ExperimentModule *[numberofModules]();
        expModLen = numberofModules;
        Transport::registerHandler(1, unpackExp);
    }

    /**
     * register all ExperimentModules in the order they will run
     * @param mod first ExperimentModule
     * @param ... following ExperimentModules
     */
    void registerModules(ExperimentModule *mod, ...) {
        expMod[0] = mod;
        va_list args;
        va_start(args, mod);
        for (int i = 1; i < expModLen; ++i) {
            expMod[i] = (ExperimentModule *) va_arg(args, ExperimentModule * );
        }
        va_end(args);
    }

    /**
     * stop Experiment in emergencies
     */
    static void stop() {
        pExp->bExperimentActive = false;
    }

    /// enum of possible experiment states
    enum ExpState {
        IDLE,
        INIT,
        RUN,
        STOP
    };

    /**
     * read current Experiment state
     */
    static enum ExpState getState() {
        return pExp->eState;
    }

private:
    inline static Experiment *pExp = nullptr;  ///< static pointer to experiment instance. Needed for interrupt callback

    ExperimentModule **expMod = nullptr;
    unsigned int expModLen = 0;

    enum ExpState eState = IDLE;                ///< current state of experiment

    unsigned long lTime = 0;                    ///< milliseconds since start of experiment
    bool bExperimentActive = false;
    unsigned long keepaliveTime = 0;

    bool init = false;

    /**
     * unpack active bit from wire
     */
    static void unpackExp() {
        uint8_t iData = 0;
        Transport::unPack(iData);
        if (iData & 2) {
            pExp->keepaliveTime = pExp->lTime;
        } else {
            pExp->bExperimentActive = iData & 1;
        }
    }

public:
    /**
     * run experiment state machine
     */
    void run() {
        switch (this->eState) {
            case IDLE:
                //do stuff
                // state machine
                if (this->bExperimentActive) {
                    this->eState = INIT;
                }
                break;
            case INIT:
                init = false;
                for (int i = 0; i < expModLen; ++i) {
                    init |= expMod[i]->init();
                }
                // state machine
                if (!init) {
                    this->eState = RUN;
                }
                break;
            case RUN:
                // do stuff
                for (int i = 0; i < expModLen; ++i) {
                    expMod[i]->compute(lTime);
                }

                // send data
                Transport::sendData(this->lTime);

                // update time of experiment
                this->lTime += EXP_DT;

                // keepalive signal
                if (EXP_KEEPALIVE && lTime > keepaliveTime + EXP_KEEPALIVE) {
                    this->bExperimentActive = false;
                }
                // state machine
                if (!this->bExperimentActive) {
                    this->eState = STOP;
                }
                break;
            case STOP:
                // do stuff
                this->lTime = 0;
                for (int i = 0; i < expModLen; ++i) {
                    expMod[i]->stop();
                }
                // state machine
                this->eState = IDLE;
                break;
        }
    }
    ///\endcond
};

#endif //EXPERIMENT_H

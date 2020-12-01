/** @file Experiment.h
 *
 * Copyright (c) 2019 IACE
 */
#ifndef EXPERIMENT_H
#define EXPERIMENT_H

#include <cstdint>

#include "utils/DynamicArray.h"
#include "utils/ExperimentModule.h"
#include "utils/Transport.h"

/**
 * General class of an experiment used to control the experiment flow of a test rig.
 */
class Experiment {
public:
    /**
     * Initialize the experiment
     */
    Experiment() {
        pExp = this;
        Transport::registerHandler(1, unpackExp);
    }

    /**
     * enables automatic shutdown of rig upon missing heartbeat
     * @param lKeepalive time in ms to wait for heartbeat
     */
    void enableHeartbeat(unsigned long lKeepalive) {
        this->lHeartBeatWait = lKeepalive;
    }

    /**
     * register all ExperimentModules in the order they will run
     * @param mod ExperimentModule
     */
    void registerModules(ExperimentModule *mod) {
        expMod.push_back(mod);
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
    ///\cond false
    inline static Experiment *pExp = nullptr;  ///< static pointer to experiment instance. Needed for interrupt callback

    DynamicArray<ExperimentModule *>expMod;

    enum ExpState eState = IDLE;                ///< current state of experiment

    unsigned long lTime = 0;                    ///< milliseconds since start of experiment
    bool bExperimentActive = false;
    unsigned long lHeartBeatWait = 0;           ///< milliseconds to wait for new heartbeat
    unsigned long lHeartBeatLast = 0;           ///< time of last heartbeat in milliseconds

    bool init = false;

    /**
     * unpack active bit from wire
     */
    static void unpackExp() {
        uint8_t iData = 0;
        Transport::unPack(iData);
        if (iData & 2) {
            pExp->lHeartBeatLast = pExp->lTime;
        } else {
            pExp->bExperimentActive = iData & 1;
        }
    }
    ///\endcond
public:
    /**
     * run experiment state machine
     * @param lDt time in ms since last call
     */
    void run(unsigned long lDt) {
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
                for (unsigned int i = 0; i < expMod.len(); ++i) {
                    init |= expMod[i]->init();
                }
                // state machine
                if (!init) {
                    this->eState = RUN;
                } else if (!this->bExperimentActive) {
                    this->eState = STOP;
                }
                break;
            case RUN:
                // do stuff
                for (unsigned int i = 0; i < expMod.len(); ++i) {
                    expMod[i]->compute(lTime);
                }

                // send data
                Transport::sendData(this->lTime);

                // update time of experiment
                this->lTime += lDt;

                // keepalive signal
                if (lHeartBeatWait && lTime > lHeartBeatLast + lHeartBeatWait) {
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
                this->lHeartBeatLast = 0;
                for (unsigned int i = 0; i < expMod.len(); ++i) {
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

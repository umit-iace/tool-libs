/** @file Transport.h
 *
 * Copyright (c) 2019 IACE
 */
#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <cstdint>

#include "utils/ExperimentModule.h"

/**
 * @brief Generic abstraction of a frame based connection
 */
class Connection {
public:
    /**
     * send a frame to connection
     * @param id ID of frame
     * @param payload pointer to payload buffer
     * @param len length of payload buffer
     */
    virtual int send(uint8_t id, const uint8_t *payload, uint8_t len) = 0;

    /**
     * register frame handler to be called by connection upon
     * receiving a valid frame
     * @param handler function pointer to frame handler
     */
    virtual void registerFrameHandler(void (*handler)(uint8_t id, const uint8_t *payload)) = 0;
};

/**
 * @brief Layer class for the min protocol communication between the host and the microcontroller
 */
class Transport {
public:
    /**
     * Constructor, that initialize the min protocol
     */
    Transport(Connection *conn, uint8_t maxPayload = 80) : conn(conn) {
        pThis = this;
        payload = new unsigned char[maxPayload]();

        conn->registerFrameHandler([](uint8_t iId, const uint8_t *iPayload) {
                                       pThis->handleFrame(iId, iPayload);
                                   }
        );
    }

    static void registerModule(ExperimentModule *expM, uint8_t id) {
        pThis->expM[id] = expM;
    }

    static void registerHandler(uint8_t id, void (*fun)()) {
        pThis->frameHandler[id] = fun;
    }

    /**
      * Handles incoming data frames.
      * @param id frame identifier
      * @param p frame data as pointer
      */
    void handleFrame(uint8_t id, const uint8_t *p) {
        unPackPointer = p;
        if (frameHandler[id]) {
            frameHandler[id]();
        } else if (expM[id]) {
            expM[id]->handleFrame(id);
        }
    };

    /**
     * Function for sending the \p benchData to the Host
     */
    static void sendData(uint32_t lTime) {
        for (int id = 0; id < 64; ++id) {
            if (pThis->expM[id]) {
                pThis->cCursor = 0;
                pThis->expM[id]->sendFrame(id, lTime);
                if (pThis->cCursor) {
                    pThis->conn->send(id, pThis->payload, pThis->cCursor);
                }
            }
        }
    };

    // indirection
    template<typename T>
    static void pack(T value) {
        pThis->_pack(value);
    }

    // indirection
    template<typename T>
    static void unPack(T &value) {
        pThis->_unPack(value);
    }

    //\cond false

private:
    template<typename T>
    void _pack(T value) {
        auto *origin = (uint8_t * ) & value;
        for (int i = sizeof(T) - 1; i >= 0; --i) {
            payload[cCursor + i] = *origin++;
        }
        cCursor += sizeof(T);
    }

    template<typename T>
    void _unPack(T &value) {
        auto *dest = (uint8_t * ) & value;
        for (int i = sizeof(T) - 1; i >= 0; --i) {
            *dest++ = unPackPointer[i];
        }
        unPackPointer += sizeof(T);
    }

    Connection *conn = nullptr;
    unsigned char *payload = nullptr;
    const unsigned char *unPackPointer = nullptr;
    unsigned char cCursor = 0;
    inline static Transport *pThis = nullptr;
    ExperimentModule *expM[64] = {};

    void (*frameHandler[64])() = {};
    //\endcond
};

#endif //TRANSPORT_H

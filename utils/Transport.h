/** @file Transport.h
 *
 * Copyright (c) 2019 IACE
 */
#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <stdio.h>

#include "define.h"
#include "utils/ExperimentModule.h"

#include "Min.h"

/**
 * @brief Layer class for the min protocol communication between the host and the microcontroller
 */
class Transport {
public:
    /**
     * Constructor, that initialize the min protocol
     */
    Transport() : cMin() {
	pThis = this;

        cMin.add_application_function([](uint8_t iId, uint8_t *iPayload) {
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
      * @param iId frame identifier
      * @param iPayload frame data as pointer
      */
    void handleFrame(unsigned char id, unsigned char *p) {
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
#ifdef TRANSPORT_PROTOCOL
                    pThis->cMin.queue_frame(id, pThis->payload, pThis->cCursor);
#else
				    pThis->cMin.send_frame(id, pThis->payload, pThis->cCursor);
#endif
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
		auto *origin = (uint8_t *) &value;
		for (int i = sizeof(T) - 1; i >= 0; --i) {
			payload[cCursor + i] = *origin++;
		}
		cCursor += sizeof(T);
	}

	template<typename T>
	void _unPack(T &value) {
		auto *dest = (uint8_t *) &value;
		for (int i = sizeof(T) - 1; i >= 0; --i) {
			*dest++ = unPackPointer[i];
		}
		unPackPointer += sizeof(T);
	}

	Min cMin;
	unsigned char payload[TRANSPORT_MAX_PAYLOAD];
	unsigned char *unPackPointer;
	unsigned char cCursor = 0;
	inline static Transport *pThis = nullptr;
    ExperimentModule *expM[64] = {};
    void (*frameHandler[64])() = {};
};

#endif //TRANSPORT_H

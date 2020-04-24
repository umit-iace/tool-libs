/** @file Tick.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef TICK_H
#define TICK_H

#include <stdint.h>

/**
 * Class describing a client to the @ref TickServer.
 *
 * provides a method to be called cyclically by the server.
 */
class TickClient {
public:
    /**
     * Method is called cyclically by tick server.
     */
    virtual void tick() = 0;
};

/**
 * Class describing a tick server to a @ref TickClient.
 */
class TickServer {
private:
    inline static TickServer *pThis = nullptr;

    static TickServer *server() {
        if (!pThis) {
            pThis = new TickServer(TICK_MAX_CLIENTS);
        }
        return pThis;
    }

    TickServer(uint8_t numberofClients) : maxClients(numberofClients) {
        clients = new Client[numberofClients]();
    }

public:
    /**
     * Method must be called initially by tick client.
     * @param client reference to client
     * @param ms Tick request time in ms
     */
    static bool registerClient(TickClient *client, uint32_t ms) {
        auto *This = TickServer::server();
        if (This->iClients != This->maxClients) {
            This->clients[This->iClients].client = client;
            This->clients[This->iClients].ms = ms;
            This->clients[This->iClients].msLastCall = 0;
            ++This->iClients;
            return true;
        }
        return false;
    }

protected:
    struct Client {
        TickClient *client;
        uint32_t ms;
        uint32_t msLastCall;
    } *clients;
    uint8_t iClients = 0;
    uint8_t maxClients = 0;
    uint32_t millis = 0;
public:
    /**
     * process connected @ref TickClient clients
     *
     * method must be called cyclically
     * @param dT time difference in ms since last call
     */
    static void msCall(uint32_t dT) {
        auto *This = TickServer::server();
        This->millis += dT;
        for (int i = 0; i < This->iClients; ++i) {
            Client *client = &This->clients[i];
            if (This->millis - client->ms >= client->msLastCall) {
                client->client->tick();
                client->msLastCall = This->millis;
            }
        }
    }
};

#endif //TICK_H

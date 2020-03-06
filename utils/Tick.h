/** @file Tick.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef TICK_H
#define TICK_H

/**
 * Class describing a client to the @ref TickServer.
 *
 * provides a method to be called cyclically by the server.
 */
class TickClient {
public:
    /**
     * Method must is called cyclically by tick server.
     */
    virtual void tick() = 0;
};

/**
 * Class describing a tick server to a @ref TickClient.
 */
class TickServer {
public:
    TickServer(uint8_t numberofClients) : maxClients(numberofClients) {
        clients = new Client[numberofClients]();
    }
    /**
     * Method must be called initially by tick client.
     * @param ms Tick request time in ms
     */
    bool registerClient(TickClient *client, uint32_t ms) {
        if (iClients != maxClients) {
            clients[iClients].client = client;
            clients[iClients].ms = ms;
            clients[iClients].msLastCall = 0;
            ++iClients;
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
    void msCall(uint32_t dT) {
        millis += dT;
        for (int i = 0; i < this->iClients; ++i) {
            Client *client = &this->clients[i];
            if (millis - client->ms >= client->msLastCall) {
                client->client->tick();
                client->msLastCall = millis;
            }
        }
    }
};

#endif //TICK_H

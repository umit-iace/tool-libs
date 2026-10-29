#pragma once
#include <utils/queue.h>
#include <comm/can.h>
#include <linux/can.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

namespace CAN {
    struct HW : public CAN {
        int sock{};
        Queue<Message> rx;
        Queue<struct can_frame> tx;
        HW( const char *ifname ) {
            /* open CAN_RAW socket */
            sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
            /* set NONBLOCK */
            int flags = fcntl(sock, F_GETFL, 0);
            if (flags == -1) {
                perror("cannot get CAN socket flags");
                return;
            }
            if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
                perror("cannot set O_NONBLOCK on CAN socket");
                return;
            }
            /* */
            struct ifreq ifr = {};
            strncpy(ifr.ifr_name, ifname, IF_NAMESIZE - 1);
            if (ioctl(sock, SIOCGIFINDEX, &ifr) == -1) {
                perror("cannot find CAN interface index");
                return;
            }
            /* setup address for bind */
            struct sockaddr_can addr {
                .can_family = PF_CAN,
                .can_ifindex = ifr.ifr_ifindex,
            };
            /* bind socket to the can0 interface */
            if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
                perror("cannot bind socket to CAN interface");
                return;
            }
        }
        ~HW() {
            close(sock);
        }
        bool full() override {
            return tx.full();
        }
        void push(Message &&msg) override {
            struct can_frame frame = frameFromMessage(std::move(msg));
            tx.push(frame);
            process();
        }
        void process() {
            // transmit side
            while (!tx.empty()) {
                auto frame = tx.front();
                int l = write(sock, &frame, sizeof(frame));
                if (l < 0) {
                    if (errno != EAGAIN) {
                        perror("writing on CAN socket error");
                    }
                    break;
                } else if ((size_t)l < sizeof(frame)) {
                    fprintf(stderr, "write: incomplete CAN frame: %d\n", l);
                } else { // success
                    tx.pop();
                }
            }
            // receive side
            empty();
        }
        using Sink::push;
        Message pop() override {
            return rx.pop();
        }
        bool empty() override {
            if (rx.full()) return false; // consumer has to catch up first
            struct can_frame frame;
            int l = read(sock, &frame, sizeof(frame));
            if (l < 0) {
                if (errno != EAGAIN) {
                    perror("reading on CAN socket error");
                }
                return rx.empty();
            } else if ((size_t)l < sizeof(frame)) { /* paranoid check ... */
                fprintf(stderr, "read: incomplete CAN frame: %d\n", l);
                return rx.empty();
            }
            // error frames only show up if asked for, never pass them on
            if (frame.can_id & CAN_ERR_FLAG) return rx.empty();
            rx.push(messageFromFrame(frame));
            return false;
        }
        static struct can_frame frameFromMessage(Message &&msg) {
            struct can_frame f {};
            if (msg.opts.ide) {
                f.can_id = (msg.id & CAN_EFF_MASK) | CAN_EFF_FLAG;
            } else {
                f.can_id = msg.id & CAN_SFF_MASK;
            }
            if (msg.opts.rtr) f.can_id |= CAN_RTR_FLAG;
            f.len = msg.opts.dlc > CAN_MAX_DLEN ? CAN_MAX_DLEN : msg.opts.dlc;
            if (!msg.opts.rtr) memcpy(f.data, &msg.data, f.len);
            return f;
        }
        static Message messageFromFrame(const struct can_frame &frame) {
            bool ide = frame.can_id & CAN_EFF_FLAG;
            bool rtr = frame.can_id & CAN_RTR_FLAG;
            uint8_t len = frame.len > CAN_MAX_DLEN ? CAN_MAX_DLEN : frame.len;
            Message msg {
                .id = frame.can_id & (ide ? CAN_EFF_MASK : CAN_SFF_MASK),
                .opts = {
                    .rtr = rtr,
                    .ide = ide,
                    .dlc = len,
                },
            };
            if (!rtr) memcpy(&msg.data, frame.data, len);
            return msg;
        }
    };
}

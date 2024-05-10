#include <core/kern.h>
#include <sys/comm.h>

Kernel k;
IFACE i{"/dev/tty"};
int *null = nullptr;

void help() {
    i.print("h -- show help message\n");
    i.print("q -- quit\n");
    i.print("e <msg> -- echo <msg> back\n");
    i.print("s -- cause segmentation fault (-> gracious shutdown)\n");
}

int main() {
    i.print("Welcome to IFACE test&example\n");
    help();
    i.print("> ");
    k.every(100, [](uint32_t, uint32_t) {
            if (i.empty()) return;
            auto msg = i.pop();
            switch (msg[0]) {
            case 'h':
                help();
                break;
            case 'q':
                i.print("goodbye\n");
                k.exit(0);
                break;
            case 's':
                i.print("causing SIGSEGV\n");
                *null = 1234;
                i.warn("THIS SHOULD NOT PRINT");
                break;
            case 'e':
                i.print("%.*s\n", msg.len-3, msg.buf+2);
                break;
            };
            i.print("> ");
            });
    return k.run();
}


#include <core/kern.h>

#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <setjmp.h>
using namespace std::chrono;

time_point start{steady_clock::now()};
auto dt{microseconds{1000}};
time_point next{steady_clock::now() + dt};

void Kernel::idle() {
    std::this_thread::sleep_until(next);
    auto now = steady_clock::now();
    next += dt;
    auto runtime = duration_cast<milliseconds>(now-start);
    this->tick(1);
}

void Kernel::setTimeStep(uint16_t dt_us) {
    dt = microseconds{dt_us};
}

void sighandler(int signum) {
    k.exit(signum == SIGINT ? 0 : -1);
    if (signum == SIGSEGV) {
        /* fprintf(stderr, "detected SIGSEGV, attempting clean shutdown\n"); */
        longjmp(k.jbf, 1);
    }
}

bool hal = [](){ // initialize signal handlers
        struct sigaction act{};
        act.sa_handler = sighandler;
        sigfillset(&act.sa_mask);
        sigaction(SIGINT, &act, NULL);
        sigaction(SIGSEGV, &act, NULL);
        return true;
}();


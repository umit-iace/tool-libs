/** linux backend: clean shutdown after SIGABRT/SIGSEGV */
#include <doctest/doctest.h>
#include <core/kern.h>
#include <csignal>
Kernel k;
void sighandler(int); // linux/hal.cpp

TEST_CASE("tool-libs: linux: kernel returns after SIGABRT, signals work again") {
    // doctest installs its own crash handlers, use the kernel's for this
    struct sigaction ours{}, theirs{};
    ours.sa_handler = sighandler;
    sigfillset(&ours.sa_mask);
    sigaction(SIGABRT, &ours, &theirs);

    k.every(5, [](uint32_t t, uint32_t) {
        if (t >= 10) raise(SIGABRT);
    });
    int code = k.run();
    sigaction(SIGABRT, &theirs, nullptr);
    CHECK(code == -1);

    // the handler's mask (all signals) must not stick after jumping out
    sigset_t blocked;
    sigprocmask(SIG_BLOCK, nullptr, &blocked);
    CHECK_FALSE(sigismember(&blocked, SIGINT));
    CHECK_FALSE(sigismember(&blocked, SIGABRT));
}

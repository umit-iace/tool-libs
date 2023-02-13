#define log(...)
#include <cassert>
#include <iostream>
#include <x/FrameRegistry.h>
using namespace std;
void handle(Frame &f) {
    double d;
    bool e;
    switch (f.id) {
        case 1:
            f.unPack(e);
            if (e) {
                cout << "experiment started" << endl;
            } else {
                cout << "experiment stopped" << endl;
            }
            break;
        case 10:
            f.unPack(d);
            cout << "f.id = 10: data = " << d << endl;
            break;
    }
}
struct Exp{
    void runlog(Frame &f) {
        bool run = f.unpack<bool>();
        cout << "Classexperiment " << (run?"start":"stop") << endl;
    }
    void getPi(Frame &f) {
        double pi = f.unpack<double>();
        assert(pi - 3.14 < 0.01);
        cout << "pi = " << pi << endl;
    }
};
void func() {
    FrameRegistry reg;
    Frame e{1}, f{10}, g{1};
    e.pack(true);
    f.pack(3.14);
    g.pack(false);
    reg.setHandler(1, handle);
    reg.setHandler(10, handle);
    reg.handle(std::move(e));
    reg.handle(std::move(f));
    reg.handle(std::move(g));
}

void meth() {
    FrameRegistry reg;
    Exp exp;
    reg.setHandler(1, exp, &Exp::runlog);
    reg.setHandler(10, exp, &Exp::getPi);
    Frame f{1}, p{10};
    f.pack(true);
    p.pack(3.14);
    reg.handle(std::move(f));
    reg.handle(std::move(p));
    f = Frame{1};
    f.pack(false);
    reg.handle(std::move(f));
}
int main() {
    func();
    meth();
}

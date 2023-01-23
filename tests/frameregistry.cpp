#include <cassert>
#include <iostream>
#include <utils/FrameRegistry.h>
using namespace std;
FrameRegistry reg;
void handle(Frame &f, void* stuff) {
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
void simple() {
    Frame e{1}, f{10}, g{1};
    e.pack(true);
    f.pack(3.14);
    g.pack(false);
    reg.setHandler(1, handle);
    reg.setHandler(10, handle);
    reg.handle(e);
    reg.handle(f);
    reg.handle(g);
}
struct complexstruct {
    double d;
    bool b;
} cs{};
void handleargs(Frame &f, void* stuff) {
    auto c = (complexstruct *)stuff;
    f.unPack(c->d);
    f.unPack(c->b);
    cout << c->d <<" "<< (c->b?"true":"false") << endl;
}
void voidargs() {
    Frame e{2};
    e.pack(3.14);
    e.pack(true);
    reg.setHandler(2, handleargs, &cs);
    reg.handle(e);
}
int main() {
    simple();
    voidargs();
}

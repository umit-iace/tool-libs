#include <cassert>
#include <cstdio>
#define DEBUGLOG(...) fprintf(__VA_ARGS__)
#include "utils/Queue.h"
#include "utils/Buffer.h"
#include <queue>
#include <iostream>
using namespace std;
struct data {
    double d;
    data(double d): d(d) {
        cout << "constr " << d << endl;
    }
    ~data() {
        cout << "destr " << d << endl;
        d = 0;
    }
    operator double() { return d; }
};
void strdat(){
    /* queue<struct data> q; */
    Queue<struct data, 3> q;
    q.push(4.);
    q.push(8.);
    q.push(3.14);
    while (!q.empty()) {
        cout << q.front() << " ";// << q.back() << endl;
        /* q.pop(); */
        cout << q.pop() << " ";
    }
    cout << endl;
}
void buf() {
    Buffer<uint8_t> b{5};
    b.append('a').append('b').append('c').append('d').append('e');
    Queue<Buffer<uint8_t>, 3> q;
    q.push(std::move(b));
    Buffer<uint8_t> c = q.pop();
    for (size_t ix = 0; ix < c.len; ++ix) {
        cout << c.at(ix) << ' ';
    }
    cout << endl;
}
void roundabout() {
    Queue<double, 4> q;
    for (size_t i = 1; i < 20; ++i) {
        q.push(10./i);
        cout << q.pop() << " ";
    }
    cout << endl;
}
int main() {
    strdat();
    buf();
    roundabout();
}

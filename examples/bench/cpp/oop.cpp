#include <cstdio>
struct Point {
    long long x, y;
    Point(long long x_, long long y_) : x(x_), y(y_) {}
    void add(const Point& o) { x += o.x; y += o.y; }
};
int main() {
    long long total = 0;
    for (long long i = 0; i < 5000000; i++) {
        Point a(i, i + 1);
        Point b(1, 1);
        a.add(b);
        total += a.x + a.y;
    }
    std::printf("%lld\n", total);
}
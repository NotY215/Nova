#include <cstdio>
struct Point {
    long long x, y;
    Point(long long x_, long long y_) : x(x_), y(y_) {}
    long long mag_sq() const { return x * x + y * y; }
};
int main() {
    long long total = 0, i = 0;
    while (i < 5000000) {
        Point p(i, i + 1);
        total += p.mag_sq();
        i++;
    }
    std::printf("%lld\n", total);
}
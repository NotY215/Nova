#include <cstdio>
int main() {
    long long s = 0, i = 0;
    while (i < 50000000) { s = s + i; i = i + 1; }
    std::printf("%lld\n", s);
}
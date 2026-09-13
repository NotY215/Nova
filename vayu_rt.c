#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct { int64_t len; char data[]; } VayuStr;

void* vayu_alloc(int64_t size) {
    void* p = malloc((size_t)size);
    if (!p) { fprintf(stderr, "vayu: oom\n"); exit(1); }
    return p;
}

void vayu_print_int_noln(long long v) { printf("%lld", v); }
void vayu_print_bool_noln(long long v) { printf("%s", v ? "true" : "false"); }
void vayu_print_str_noln(VayuStr* s) { fwrite(s->data, 1, (size_t)s->len, stdout); }
void vayu_print_space(void) { putchar(' '); }
void vayu_print_ln(void) { putchar('\n'); }

long long vayu_floordiv(long long a, long long b) {
    if (b == 0) { fprintf(stderr, "division by zero\n"); exit(1); }
    long long q = a / b;
    if ((a ^ b) < 0 && q * b != a) q--;
    return q;
}

long long vayu_mod(long long a, long long b) {
    if (b == 0) { fprintf(stderr, "modulo by zero\n"); exit(1); }
    long long r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}

extern void vayu_main(void);

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    vayu_main();
    return 0;
}
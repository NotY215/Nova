#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct { int64_t len; char data[]; } VayuStr;
typedef struct { int64_t len; int64_t cap; int64_t* items; } VayuList;
typedef struct { VayuStr* key; int64_t value; uint8_t used; } VayuMapEntry;
typedef struct { int64_t len; int64_t cap; VayuMapEntry* entries; } VayuMap;

static int g_argc = 0;
static char** g_argv = NULL;

void* vayu_alloc(int64_t size) {
    void* p = malloc((size_t)size);
    if (!p) { fprintf(stderr, "vayu: oom\n"); exit(1); }
    return p;
}

static VayuStr* vayu_mkstr(const char* s, int64_t n) {
    VayuStr* r = (VayuStr*)malloc(sizeof(VayuStr) + (size_t)n + 1);
    r->len = n;
    if (n > 0) memcpy(r->data, s, (size_t)n);
    r->data[n] = 0;
    return r;
}

void vayu_print_int_noln(long long v) { printf("%lld", v); }
void vayu_print_bool_noln(long long v) { printf("%s", v ? "true" : "false"); }
void vayu_print_str_noln(VayuStr* s) { fwrite(s->data, 1, (size_t)s->len, stdout); }
void vayu_print_space(void) { putchar(' '); }
void vayu_print_ln(void) { putchar('\n'); }

VayuStr* vayu_str_concat(VayuStr* a, VayuStr* b) {
    int64_t n = a->len + b->len;
    VayuStr* r = (VayuStr*)malloc(sizeof(VayuStr) + (size_t)n + 1);
    r->len = n;
    if (a->len) memcpy(r->data, a->data, (size_t)a->len);
    if (b->len) memcpy(r->data + a->len, b->data, (size_t)b->len);
    r->data[n] = 0;
    return r;
}
int64_t vayu_str_eq(VayuStr* a, VayuStr* b) {
    if (a == b) return 1;
    if (a->len != b->len) return 0;
    return memcmp(a->data, b->data, (size_t)a->len) == 0;
}
int64_t vayu_str_ne(VayuStr* a, VayuStr* b) { return !vayu_str_eq(a, b); }

VayuStr* vayu_int_to_str(long long v, long long kind) {
    char buf[64]; int n;
    if (kind == 1) n = snprintf(buf, sizeof(buf), "%s", v ? "true" : "false");
    else           n = snprintf(buf, sizeof(buf), "%lld", v);
    return vayu_mkstr(buf, n);
}
VayuStr* vayu_str_upper(VayuStr* s) {
    VayuStr* r = (VayuStr*)malloc(sizeof(VayuStr) + (size_t)s->len + 1);
    r->len = s->len;
    for (int64_t i = 0; i < s->len; ++i) {
        char c = s->data[i];
        r->data[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
    }
    r->data[s->len] = 0;
    return r;
}
VayuStr* vayu_str_lower(VayuStr* s) {
    VayuStr* r = (VayuStr*)malloc(sizeof(VayuStr) + (size_t)s->len + 1);
    r->len = s->len;
    for (int64_t i = 0; i < s->len; ++i) {
        char c = s->data[i];
        r->data[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    r->data[s->len] = 0;
    return r;
}
int64_t vayu_str_contains(VayuStr* s, VayuStr* sub) {
    if (sub->len == 0) return 1;
    if (sub->len > s->len) return 0;
    for (int64_t i = 0; i + sub->len <= s->len; ++i)
        if (memcmp(s->data + i, sub->data, (size_t)sub->len) == 0) return 1;
    return 0;
}
int64_t vayu_str_find(VayuStr* s, VayuStr* sub) {
    if (sub->len == 0) return 0;
    if (sub->len > s->len) return -1;
    for (int64_t i = 0; i + sub->len <= s->len; ++i)
        if (memcmp(s->data + i, sub->data, (size_t)sub->len) == 0) return i;
    return -1;
}
int64_t vayu_str_starts_with(VayuStr* s, VayuStr* p) {
    if (p->len > s->len) return 0;
    return memcmp(s->data, p->data, (size_t)p->len) == 0;
}
int64_t vayu_str_ends_with(VayuStr* s, VayuStr* p) {
    if (p->len > s->len) return 0;
    return memcmp(s->data + (s->len - p->len), p->data, (size_t)p->len) == 0;
}
int64_t vayu_str_to_int(VayuStr* s) {
    int64_t n = 0; int64_t i = 0; int neg = 0;
    while (i < s->len && (s->data[i] == ' ' || s->data[i] == '\t')) ++i;
    if (i < s->len && (s->data[i] == '-' || s->data[i] == '+')) {
        neg = (s->data[i] == '-'); ++i;
    }
    for (; i < s->len; ++i) {
        char c = s->data[i];
        if (c < '0' || c > '9') break;
        n = n * 10 + (c - '0');
    }
    return neg ? -n : n;
}
VayuStr* vayu_str_char_at(VayuStr* s, int64_t i) {
    if (i < 0) i += s->len;
    if (i < 0 || i >= s->len) { fprintf(stderr, "IndexError: string\n"); exit(1); }
    VayuStr* r = (VayuStr*)malloc(sizeof(VayuStr) + 2);
    r->len = 1;
    r->data[0] = s->data[i];
    r->data[1] = 0;
    return r;
}

VayuList* vayu_list_new() {
    VayuList* l = (VayuList*)malloc(sizeof(VayuList));
    l->len = 0; l->cap = 4;
    l->items = (int64_t*)malloc(sizeof(int64_t) * 4);
    return l;
}
static void vayu_list_grow(VayuList* l) {
    if (l->len < l->cap) return;
    l->cap *= 2;
    l->items = (int64_t*)realloc(l->items, sizeof(int64_t) * (size_t)l->cap);
}
void vayu_list_push(VayuList* l, int64_t v) {
    vayu_list_grow(l);
    l->items[l->len++] = v;
}
int64_t vayu_list_get(VayuList* l, int64_t i) {
    if (i < 0) i += l->len;
    if (i < 0 || i >= l->len) { fprintf(stderr, "IndexError: list\n"); exit(1); }
    return l->items[i];
}
void vayu_list_set(VayuList* l, int64_t i, int64_t v) {
    if (i < 0) i += l->len;
    if (i < 0 || i >= l->len) { fprintf(stderr, "IndexError: list\n"); exit(1); }
    l->items[i] = v;
}
int64_t vayu_list_len(VayuList* l) { return l->len; }
int64_t vayu_list_contains(VayuList* l, int64_t v) {
    for (int64_t i = 0; i < l->len; ++i) if (l->items[i] == v) return 1;
    return 0;
}
int64_t vayu_list_pop(VayuList* l) {
    if (l->len == 0) { fprintf(stderr, "IndexError: pop from empty list\n"); exit(1); }
    return l->items[--l->len];
}

static uint64_t hash_str(VayuStr* s) {
    uint64_t h = 1469598103934665603ULL;
    for (int64_t i = 0; i < s->len; ++i) { h ^= (uint8_t)s->data[i]; h *= 1099511628211ULL; }
    return h;
}
VayuMap* vayu_map_new() {
    VayuMap* m = (VayuMap*)malloc(sizeof(VayuMap));
    m->len = 0; m->cap = 8;
    m->entries = (VayuMapEntry*)calloc((size_t)m->cap, sizeof(VayuMapEntry));
    return m;
}
static VayuMapEntry* map_find(VayuMap* m, VayuStr* k) {
    uint64_t h = hash_str(k) & (uint64_t)(m->cap - 1);
    for (int64_t p = 0; p < m->cap; ++p) {
        VayuMapEntry* e = &m->entries[h];
        if (!e->used) return NULL;
        if (vayu_str_eq(e->key, k)) return e;
        h = (h + 1) & (uint64_t)(m->cap - 1);
    }
    return NULL;
}
static void map_grow(VayuMap* m) {
    if (m->len * 2 < m->cap) return;
    int64_t newCap = m->cap * 2;
    VayuMapEntry* ne = (VayuMapEntry*)calloc((size_t)newCap, sizeof(VayuMapEntry));
    for (int64_t i = 0; i < m->cap; ++i) {
        if (!m->entries[i].used) continue;
        uint64_t h = hash_str(m->entries[i].key) & (uint64_t)(newCap - 1);
        while (ne[h].used) h = (h + 1) & (uint64_t)(newCap - 1);
        ne[h] = m->entries[i];
    }
    free(m->entries);
    m->entries = ne;
    m->cap = newCap;
}
void vayu_map_put(VayuMap* m, VayuStr* k, int64_t v) {
    VayuMapEntry* e = map_find(m, k);
    if (e) { e->value = v; return; }
    map_grow(m);
    uint64_t h = hash_str(k) & (uint64_t)(m->cap - 1);
    while (m->entries[h].used) h = (h + 1) & (uint64_t)(m->cap - 1);
    m->entries[h].used = 1;
    m->entries[h].key = k;
    m->entries[h].value = v;
    m->len++;
}
int64_t vayu_map_get(VayuMap* m, VayuStr* k) {
    VayuMapEntry* e = map_find(m, k);
    if (!e) { fprintf(stderr, "KeyError\n"); exit(1); }
    return e->value;
}
int64_t vayu_map_has(VayuMap* m, VayuStr* k) { return map_find(m, k) != NULL; }
int64_t vayu_map_len(VayuMap* m) { return m->len; }

int64_t vayu_len(int64_t v, int64_t kind) {
    if (kind == 0) return ((VayuStr*)v)->len;
    if (kind == 1) return ((VayuList*)v)->len;
    if (kind == 2) return ((VayuMap*)v)->len;
    return 0;
}

long long vayu_floordiv(long long a, long long b) {
    if (b == 0) { fprintf(stderr, "ZeroDivisionError\n"); exit(1); }
    long long q = a / b;
    if ((a ^ b) < 0 && q * b != a) q--;
    return q;
}
long long vayu_mod(long long a, long long b) {
    if (b == 0) { fprintf(stderr, "ZeroDivisionError\n"); exit(1); }
    long long r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}

VayuStr* vayu_read_file(VayuStr* path) {
    char buf[4096];
    int64_t n = path->len < 4095 ? path->len : 4095;
    memcpy(buf, path->data, (size_t)n);
    buf[n] = 0;
    FILE* f = fopen(buf, "rb");
    if (!f) { fprintf(stderr, "cannot open file\n"); exit(1); }
    fseek(f, 0, SEEK_END);
    long long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    VayuStr* s = (VayuStr*)malloc(sizeof(VayuStr) + (size_t)sz + 1);
    s->len = sz;
    if (sz > 0) fread(s->data, 1, (size_t)sz, f);
    s->data[sz] = 0;
    fclose(f);
    return s;
}

int64_t vayu_file_exists(VayuStr* path) {
    char buf[4096];
    int64_t n = path->len < 4095 ? path->len : 4095;
    memcpy(buf, path->data, (size_t)n);
    buf[n] = 0;
    FILE* f = fopen(buf, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

VayuList* vayu_get_args(void) {
    VayuList* l = vayu_list_new();
    int i = 1;
    while (i < g_argc) {
        vayu_list_push(l, (int64_t)vayu_mkstr(g_argv[i], (int64_t)strlen(g_argv[i])));
        i = i + 1;
    }
    return l;
}

void vayu_exit(int64_t code) { exit((int)code); }

extern void vayu_main(void);

int main(int argc, char** argv) {
    g_argc = argc;
    g_argv = argv;
    vayu_main();
    return 0;
}
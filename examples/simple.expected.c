#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <stdio.h>
#include <stdlib.h>

static size_t pine_bounds_check(int64_t index, size_t length) {
    if (index < 0 || (uint64_t)index >= (uint64_t)length) {
        fprintf(stderr, "Pine bounds check failed: index %lld out of length %zu\n", (long long)index, length);
        abort();
    }
    return (size_t)index;
}

typedef struct Vec2 {
    int32_t x;
    int32_t y;
} Vec2;

const int32_t DEFAULT_LIMIT = 4;
const char MARK = 'P';
int32_t global_bias = 1;
char* GREETING = "pine";

int32_t add(int32_t a, int32_t b) {
    return (a + b);
}

int32_t sum(Vec2 value) {
    return (value.x + value.y);
}

int32_t count_to(int32_t limit) {
    int32_t values[4] = {0};
    int32_t mask = ((limit << 1) | 1);
    int32_t first = 0;
    int32_t total = ((mask & 3) + global_bias);
    {
        int32_t* ptr = (&values[pine_bounds_check((int64_t)(0), 4)]);
        first = (*ptr);
    }
    for (int32_t i = 0; ((i < limit) && (!(total > 20))); i = (i + 1)) {
        if (((i % 2) == 0)) {
            continue;
        }
        total = add(total, values[pine_bounds_check((int64_t)(i), 4)]);
        if (((total > 10) || (i > 8))) {
            break;
        }
    }
    switch (limit) {
        case 0:
            total = (total + 1);
            break;
        case 4:
            total = (total + 2);
            break;
        default:
            total = (total + 3);
            break;
    }
    return ((total + first) + ((~mask) ^ (mask >> 1)));
}

int32_t main(void) {
    Vec2 point = {0};
    return (count_to(DEFAULT_LIMIT) + sum(point));
}

/*
    MIT License

    Copyright (c) 2026 baynarikattu

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

/*
    ```terminal
    cc -O3 -ffast-math -o raymarcher raymarcher.c -lm
    ```
*/

/* TODO:
   - add sdf for triangle
   - add runtime model loader
   - add GIF output
   - add runtime shaders
   - THREAD_COUNT = cpu core count at runtime
   - add optional flag for output filename
   - add optional flag for specifying width and height
   - add optional flag for specifying rendering settings
     (`MAX_ITER`, `MAX_DIST`, `MIN_DIST`, `EPSILON`, `SAMPLES_PER_RAY`, `MAX_REFLECTION`)
*/

/* BUG:
    When compiling a shader like this with `clang-19`:
    ```c
    vec4 shader(vec2 fragCoord) {
        const float grayScale = fragCoord.x / iResolution.x;
        return (vec4) {grayScale, grayScale, grayScale, 0};
    }
    ```
    the gradient produced with `-O0` or `-O1` flag is correct - from black `vec3(0.0)` to `white vec3(1.0)`,
    but with `-O2` or `-O3` flag it is incorrect - from black `vec3(0.0)` to gray `vec3(0.5)`.
*/

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <threads.h>

#define WIDTH 800
#define HEIGHT 600
#define ASPECT ((float)WIDTH / HEIGHT)

#define MAX_ITER 800
#define MAX_DIST 100
#define MIN_DIST 0.001
#define EPSILON 0.001
#define SAMPLES_PER_RAY 8
#define MAX_REFLECTION 2

#define THREAD_COUNT 4


#define ASSERT(...) assert(__VA_ARGS__)

#if __STDC_VERSION__ >= 201112L
#  define STATIC_ASSERT(...) static_assert(__VA_ARGS__);
#else
#  define STATIC_ASSERT(...)
#endif

#define UNREACHABLE(...) assert(!"unreachable")

#define ARRAY_LEN(arr) (sizeof((arr))/sizeof((arr)[0]))

/* Vectors */

typedef struct {
    float x, y;
} vec2;

typedef struct {
    float x, y, z;
} vec3;

typedef struct {
    float x, y, z, w;
} vec4;

vec3 vec3_f(float f) {
    return (vec3) {f, f, f};
}

vec2 vec2_add(vec2 a, vec2 b) {
    return (vec2) {
        .x = a.x + b.x,
        .y = a.y + b.y,
    };
}

vec2 vec2_sub(vec2 a, vec2 b) {
    return (vec2) {
        .x = a.x - b.x,
        .y = a.y - b.y,
    };
}

vec2 vec2_mul(vec2 a, vec2 b) {
    return (vec2) {
        .x = a.x * b.x,
        .y = a.y * b.y,
    };
}

vec2 vec2_div(vec2 a, vec2 b) {
    return (vec2) {
        .x = a.x / b.x,
        .y = a.y / b.y,
    };
}

vec2 vec2_mulf(vec2 a, float f) {
    return (vec2) {
        .x = a.x * f,
        .y = a.y * f,
    };
}

vec2 vec2_divf(vec2 a, float f) {
    return (vec2) {
        .x = a.x / f,
        .y = a.y / f,
    };
}

vec3 vec3_add(vec3 a, vec3 b) {
    return (vec3) {
        .x = a.x + b.x,
        .y = a.y + b.y,
        .z = a.z + b.z,
    };
}

vec3 vec3_sub(vec3 a, vec3 b) {
    return (vec3) {
        .x = a.x - b.x,
        .y = a.y - b.y,
        .z = a.z - b.z,
    };
}

vec3 vec3_mul(vec3 a, vec3 b) {
    return (vec3) {
        .x = a.x * b.x,
        .y = a.y * b.y,
        .z = a.z * b.z,
    };
}

vec3 vec3_div(vec3 a, vec3 b) {
    return (vec3) {
        .x = a.x / b.x,
        .y = a.y / b.y,
        .z = a.z / b.z,
    };
}

vec3 vec3_neg(vec3 v) {
    return (vec3) {
        .x = -v.x,
        .y = -v.y,
        .z = -v.z,
    };
}

vec3 vec3_addf(vec3 v, float f) {
    return (vec3) {
        .x = v.x + f,
        .y = v.y + f,
        .z = v.z + f,
    };
}

vec3 vec3_subf(vec3 v, float f) {
    return (vec3) {
        .x = v.x - f,
        .y = v.y - f,
        .z = v.z - f,
    };
}

vec3 vec3_mulf(vec3 v, float f) {
    return (vec3) {
        .x = v.x * f,
        .y = v.y * f,
        .z = v.z * f,
    };
}

vec3 vec3_divf(vec3 v, float f) {
    return (vec3) {
        .x = v.x / f,
        .y = v.y / f,
        .z = v.z / f,
    };
}

float vec3_dot(vec3 a, vec3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

float vec3_length(vec3 v) {
    return sqrtf(vec3_dot(v, v));
}

vec3 vec3_norm(vec3 v) {
    return vec3_divf(v, vec3_length(v));
}

/* Colors */

typedef struct {
    uint8_t r, g, b;
} cvec3;

const cvec3 RED     = { 1, 0, 0 };
const cvec3 GREEN   = { 0, 1, 0 };
const cvec3 BLUE    = { 0, 0, 1 };
const cvec3 YELLOW  = { 1, 1, 0 };
const cvec3 MAGENTA = { 1, 0, 1 };
const cvec3 CYAN    = { 0, 1, 1 };
const cvec3 WHITE   = { 1, 1, 1 };
const cvec3 BLACK   = { 0, 0, 0 };

cvec3 cvec3_from_int(uint32_t color) {
    return (cvec3) {
        .r = (color >> 0)  & 0xFF,
        .g = (color >> 8)  & 0xFF,
        .b = (color >> 16) & 0xFF,
    };
}

/* Framebuffer */

static cvec3 fb[WIDTH * HEIGHT];

void fb_plot(cvec3 *fb, int x, int y, cvec3 color) {
    ASSERT(fb);

    fb[x + y * WIDTH] = color;
}

void fb_fill(cvec3 *fb, cvec3 color) {
    ASSERT(fb);

    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        fb[i] = color;
    }
}

/* General Graphics */

float clamp(float x, float min, float max) {
    return fmaxf(fminf(x, max), min);
}

vec3 mix(vec3 a, vec3 b, float t) {
    return (vec3) {
        .x = a.x * (1.0 - t) + b.x * t,
        .y = a.y * (1.0 - t) + b.y * t,
        .z = a.z * (1.0 - t) + b.z * t,
    };
}

vec3 reflect(vec3 i, vec3 n) {
    return vec3_sub(i, vec3_mulf(n, 2.0 * vec3_dot(n, i)));
}

/* TODO:
    - `rand()` -> `pseudoRand()`
    - optimize `randomOnSphere()`
*/
vec3 randomOnSphere(void) {
    vec3 v;
    while (1) {
        v.x = (float)rand() / (float)RAND_MAX * 2.0 - 1.0;
        v.y = (float)rand() / (float)RAND_MAX * 2.0 - 1.0;
        v.z = (float)rand() / (float)RAND_MAX * 2.0 - 1.0;
        if (vec3_length(v) <= 1.0) break;
    }
    return vec3_norm(v);
}

vec3 randomOnHemisphere(vec3 n) {
    ASSERT(fabs(vec3_length(n)) - 1.0 < EPSILON);

    vec3 p = randomOnSphere();
    if (vec3_dot(p, n) < 0.0) p = vec3_neg(p);
    return p;
}

/* Ray Marching */

const vec2 iResolution = { WIDTH, HEIGHT };

cvec3 vec4_to_cvec3(vec4 v) {
    return (cvec3) {
        .r = clamp(v.x * 255, 0, 255),
        .g = clamp(v.y * 255, 0, 255),
        .b = clamp(v.z * 255, 0, 255),
    };
}

float sdSphere(vec3 p, float r) {
    return vec3_length(p) - r;
}

/* TODO: add `MATERIAL_TRANSPARENT` to `Material` enum */
typedef enum {
    MATERIAL_REFLECTIVE,
    MATERIAL_MATTE,
    MATERIAL_COUNT,
} Material;

typedef struct {
    float dist;
    Material mat;
    vec3 color;
} Fetch;

Fetch map(vec3 p) {
    STATIC_ASSERT(MATERIAL_COUNT == 2, "update this")

    vec3 spherePos = {-1.0, 0.0, 0.0};
    float sphere = sdSphere(vec3_sub(p, spherePos), 1.0);

    vec3 spherePos2 = {1.0, 0.0, 0.0};
    float sphere2 = sdSphere(vec3_sub(p, spherePos2), 1.0);

    float plane = p.y + 1;

    float dists[3] = { sphere, sphere2, plane };
    Material mats[3] = { MATERIAL_REFLECTIVE, MATERIAL_MATTE, MATERIAL_MATTE };
    vec3 colors[3] = {
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 1.0},
        {0.41, 0.41, 0.41},
    };
    /* Find the index of min. */
    size_t index = 0;
    for (size_t i = 0; i < ARRAY_LEN(dists); i++) {
        if (dists[i] < dists[index]) index = i;
    }

    return (Fetch) {
        .dist = dists[index],
        .mat = mats[index],
        .color = colors[index],
    };
}

typedef struct {
    float dist;
    bool hit;
    Material mat;
    vec3 color;
} Hit;

Hit rayMarch(vec3 ro, vec3 rd) {
    float t = 0.0;
    bool hit = false;
    Material mat;
    vec3 color;

    for (int i = 0; i < MAX_ITER; i++) {
        vec3 p = vec3_add(ro, vec3_mulf(rd, t));

        Fetch f = map(p);
        mat = f.mat;
        color = f.color;
        float d = f.dist;

        t += d;

        if (t > MAX_DIST) {
            hit = false;
            break;
        }
        if (d < MIN_DIST) {
            hit = true;
            break;
        }
    }
    return (Hit) {
        .dist = t,
        .hit = hit,
        .mat = mat,
        .color = color,
    };
}

vec3 getNormal(vec3 p) {
    const vec3 dx = {EPSILON, 0, 0};
    const vec3 dy = {0, EPSILON, 0};
    const vec3 dz = {0, 0, EPSILON};

    return vec3_norm((vec3) {
        .x = map(vec3_add(p, dx)).dist - map(vec3_sub(p, dx)).dist,
        .y = map(vec3_add(p, dy)).dist - map(vec3_sub(p, dy)).dist,
        .z = map(vec3_add(p, dz)).dist - map(vec3_sub(p, dz)).dist,
    });
}

vec3 getSkyColor(vec3 rd) {
    const float t = 0.5 * (rd.y + 1.0);
    return mix((vec3) {0.8, 0.9, 1.0}, (vec3) {0.2, 0.4, 0.8}, t);
}

vec3 trace(vec3 ro, vec3 rd, int depth) {
    if (depth <= 0) return (vec3) {0.0, 0.0, 0.0};

    const vec3 sky = getSkyColor(rd);

    const Hit h = rayMarch(ro, rd);
    if (!h.hit) return sky;

    const vec3 p = vec3_add(ro, vec3_mulf(rd, h.dist));
    const vec3 n = getNormal(p);
    const vec3 o = vec3_add(p, vec3_mulf(n, 0.01));

    STATIC_ASSERT(MATERIAL_COUNT == 2, "update this")
    vec3 d, col;
    switch (h.mat) {
        case MATERIAL_REFLECTIVE: {
            d = reflect(rd, n);
            col = trace(o, d, depth - 1);
        } break;
        case MATERIAL_MATTE: {
            vec3 sum = {0.0, 0.0, 0.0};
            for (int i = 0; i < SAMPLES_PER_RAY; i++) {
                d = randomOnHemisphere(n);
                vec3 sampleColor = trace(o, d, depth - 1);
                sum = vec3_add(sum, sampleColor);
            }
            col = vec3_divf(sum, SAMPLES_PER_RAY);
        } break;
        case MATERIAL_COUNT: UNREACHABLE();
    }

    return mix(h.color, col, 0.5);
}

vec4 shader(vec2 fragCoord) {
    const vec2 uv = vec2_divf(vec2_sub(vec2_mulf(fragCoord, 2.0), iResolution), iResolution.y);
    const vec3 ro = {0.0, 0.0, -3.0};
    const vec3 rd = vec3_norm((vec3) {uv.x, uv.y, 1.0});

    vec3 avg = trace(ro, rd, MAX_REFLECTION);
    const vec3 col = avg;

    return (vec4) {col.x, col.y, col.z, 1.0};
}

/* Multithreading */

/* TODO: HEIGHT should not depend on divisibility by THREAD_COUNT */
STATIC_ASSERT(HEIGHT % THREAD_COUNT == 0, "HEIGHT should be divisible by THREAD_COUNT")

typedef struct {
    int id;
} ThreadContext;

int worker(void *arg) {
    const ThreadContext ctx = *(ThreadContext *)arg;
    const int height = HEIGHT / THREAD_COUNT;

    /* Each thread has it's own memory subbuffer
       which doesn't overlap with others, so no race occurs. */
    cvec3 *tfb = &fb[ctx.id * height * WIDTH];

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < WIDTH; i++) {
            const vec2 fragCoord = { i, HEIGHT - (j + ctx.id * height)};

            const vec4 shaderOut = shader(fragCoord);
            const cvec3 color = vec4_to_cvec3(shaderOut);

            fb_plot(tfb, i, j, color);
        }
    }

    return 0;
}

void run_workers(void) {
    thrd_t threads[THREAD_COUNT];
    ThreadContext ctxs[THREAD_COUNT];

    /* Run workers */
    for (int i = 0; i < THREAD_COUNT; i++) {
        ThreadContext *ctx = &ctxs[i];
        ctx->id = i;
        thrd_create(&threads[i], worker, ctx);
    }

    /* Wait for workers */
    for (int i = 0; i < THREAD_COUNT; i++) {
        thrd_join(threads[i], NULL);
    }
}

/* PPM (Portable Pixmap) */

void write_ppm(const char *filename, int width, int height, cvec3 *data) {
    ASSERT(filename && data);

    FILE *f = fopen(filename, "wb");
    ASSERT(f);

    fprintf(f, "P6 %d %d 255\n", width, height);
    fwrite(data, sizeof(cvec3), width*height, f);
    fclose(f);
}

int main(void) {
    /* Fill fb with MAGENTA so multithreading buffer splitting errors become obvious. */
    fb_fill(fb, MAGENTA);

    /* Run rendering on multiple cores */
    run_workers();

    /* Write the image to a file */
    write_ppm("output.ppm", WIDTH, HEIGHT, fb);

    return 0;
}

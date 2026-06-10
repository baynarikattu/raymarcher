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
	odin build . -out:raymarcher_odin -o:speed
    ```
*/

package raymarcher

import "core:fmt"
import "core:math"
import "core:math/linalg"
import "core:math/rand"
import "core:os"
import "core:slice"
import "core:thread"

WIDTH :: 800
HEIGHT :: 600

MAX_ITER :: 800
MAX_DIST :: 100
MIN_DIST :: 0.001
EPSILON :: 0.001
SAMPLES_PER_RAY :: 8
MAX_REFLECTION :: 2

// Vectors

Vector2 :: distinct [2]f64
Vector3 :: distinct [3]f64
Vector4 :: distinct [4]f64

// Colors

Color :: distinct [3]u8

RED :: Color{255, 0, 0}
GREEN :: Color{0, 255, 0}
BLUE :: Color{0, 0, 255}
YELLOW :: Color{255, 255, 0}
MAGENTA :: Color{255, 0, 255}
CYAN :: Color{0, 255, 255}
WHITE :: Color{255, 255, 255}
BLACK :: Color{0, 0, 0}

// Framebuffer

fb := [WIDTH * HEIGHT]Color{}

fb_plot :: proc(fb: []Color, x: int, y: int, color: Color) {
	fb[x + y * WIDTH] = color
}

fb_fill :: proc(fb: []Color, color: Color) {
	assert(len(fb) == WIDTH * HEIGHT)

	for &c in fb {
		c = color
	}
}

// General Graphics

mix :: proc(a: Vector3, b: Vector3, t: f64) -> Vector3 {
	return (Vector3) {
		a.x * (1.0 - t) + b.x * t,
		a.y * (1.0 - t) + b.y * t,
		a.z * (1.0 - t) + b.z * t,
	}
}

reflect :: proc(i: Vector3, n: Vector3) -> Vector3 {
	return i - n * 2.0 * linalg.dot(n, i)
}

/* TODO:
    - `rand()` -> `pseudoRand()`
    - optimize `randomOnSphere()`
*/
randomOnSphere :: proc() -> Vector3 {
	v: Vector3
	for true {
		v.x = rand.float64() * 2.0 - 1.0
		v.y = rand.float64() * 2.0 - 1.0
		v.z = rand.float64() * 2.0 - 1.0
		if linalg.length(v) <= 1.0 {break}
	}
	return linalg.normalize(v)
}

randomOnHemisphere :: proc(n: Vector3) -> Vector3 {
	assert(math.abs(linalg.length(n)) - 1.0 < EPSILON)

	p := randomOnSphere()
	return linalg.normalize(p * linalg.dot(p, n))
}

// Ray Marching

Material :: enum {
	REFLECTIVE,
	MATTE,
}

Fetch :: struct {
	dist:  f64,
	mat:   Material,
	color: Vector3,
}

Hit :: struct {
	dist:  f64,
	hit:   bool,
	mat:   Material,
	color: Vector3,
}

iResolution := Vector2{WIDTH, HEIGHT}

sdSphere :: proc(p: Vector3, r: f64) -> f64 {
	return linalg.length(p) - r
}

getNormal :: proc(p: Vector3) -> Vector3 {
	dx := Vector3{EPSILON, 0, 0}
	dy := Vector3{0, EPSILON, 0}
	dz := Vector3{0, 0, EPSILON}

	return linalg.normalize(
		Vector3 {
			mymap(p + dx).dist - mymap(p - dx).dist,
			mymap(p + dy).dist - mymap(p - dy).dist,
			mymap(p + dz).dist - mymap(p - dz).dist,
		},
	)
}

getSkyColor :: proc(rd: Vector3) -> Vector3 {
	t := 0.5 * (rd.y + 1.0)
	return mix(Vector3{0.8, 0.9, 1.0}, Vector3{0.2, 0.4, 0.8}, t)
}

mymap :: proc(p: Vector3) -> Fetch {
	spherePos := Vector3{-1.0, 0.0, 0.0}
	sphere := sdSphere(p - spherePos, 1.0)

	spherePos2 := Vector3{1.0, 0.0, 0.0}
	sphere2 := sdSphere(p - spherePos2, 1.0)

	plane := p.y + 1

	dists := []f64{sphere, sphere2, plane}
	mats := []Material{Material.REFLECTIVE, Material.MATTE, Material.MATTE}
	colors := []Vector3{{1.0, 0.0, 0.0}, {0.0, 1.0, 1.0}, {0.41, 0.41, 0.41}}
	index := 0
	for d, i in dists {
		if d < dists[index] {
			index = i
		}
	}

	return Fetch{dist = dists[index], mat = mats[index], color = colors[index]}
}

rayMarch :: proc(ro: Vector3, rd: Vector3) -> Hit {
	t := 0.0
	hit := false
	mat: Material
	color: Vector3

	for i in 0 ..< MAX_ITER {
		p := ro + rd * t

		f := mymap(p)
		mat = f.mat
		color = f.color
		d := f.dist

		t += d

		if (t > MAX_DIST) {
			hit = false
			break
		}
		if (d < MIN_DIST) {
			hit = true
			break
		}
	}
	return Hit{dist = t, hit = hit, mat = mat, color = color}
}

trace :: proc(ro: Vector3, rd: Vector3, depth: int) -> Vector3 {
	if depth <= 0 {return (Vector3){0.0, 0.0, 0.0}}

	sky := getSkyColor(rd)

	h := rayMarch(ro, rd)
	if !h.hit {return sky}

	p := ro + rd * h.dist
	n := getNormal(p)
	o := p + n * 0.01

	d, col: Vector3
	switch (h.mat) {
	case Material.REFLECTIVE:
		d = reflect(rd, n)
		col = trace(o, d, depth - 1)
	case Material.MATTE:
		sum := Vector3{0.0, 0.0, 0.0}
		for i in 0 ..< SAMPLES_PER_RAY {
			d = randomOnHemisphere(n)
			sampleColor := trace(o, d, depth - 1)
			sum = sum + sampleColor
		}
		col = sum / SAMPLES_PER_RAY
	}

	return mix(h.color, col, 0.5)
}

shader :: proc(fragCoord: Vector2) -> Vector4 {
	uv := (fragCoord * 2.0 - iResolution) / iResolution.y
	ro := Vector3{0.0, 0.0, -3.0}
	rd := linalg.normalize(Vector3{uv.x, uv.y, 1.0})

	col := trace(ro, rd, MAX_REFLECTION)

	return Vector4{col.x, col.y, col.z, 1.0}
}

// Multithreading

ThreadContext :: struct {
	id:          int,
	num_threads: int,
}

worker :: proc(data: rawptr) {
	ctx := cast(^ThreadContext)data
	height := HEIGHT / ctx.num_threads

	for j := ctx.id; j < HEIGHT; j += ctx.num_threads {
		for i in 0 ..< WIDTH {
			fragCoord := Vector2{f64(i), f64(HEIGHT - j)}

			shaderOut := shader(fragCoord)
			color := Color {
				u8(math.clamp(shaderOut.x * 255, 0, 255)),
				u8(math.clamp(shaderOut.y * 255, 0, 255)),
				u8(math.clamp(shaderOut.z * 255, 0, 255)),
			}

			fb_plot(fb[:], i, j, color)
		}
	}
}

run_workers :: proc(worker: proc(_: rawptr)) {
	num_cores := min(os.get_processor_core_count(), HEIGHT)
	fmt.println("The renderer is using", num_cores, "cores")

	threads := make([dynamic]^thread.Thread, 0, num_cores)
	defer delete(threads)

	ctxs := make([dynamic]ThreadContext, 0, num_cores)
	defer delete(ctxs)

	// Run threads
	for i in 0 ..< num_cores {
		ctx := ThreadContext {
			id          = i,
			num_threads = num_cores,
		}

		append(&ctxs, ctx)
		append(&threads, thread.create_and_start_with_data(&ctxs[i], worker, priority = .High))
	}

	thread.join_multiple(..threads[:])

	// Destroy threads
	for t in threads {
		thread.destroy(t)
	}
}

// PPM (Portable Pixmap)

write_ppm :: proc(filename: string, width: int, height: int, data: []u8) {
	assert(width * height * size_of(Color) == len(data))

	f, err := os.open(filename, {.Write, .Create})
	if err != nil {panic("bad apple")}
	defer os.close(f)

	fmt.fprintf(f, "P6 %d %d 255\n", width, height)
	os.write(f, data)
}

main :: proc() {
	// Fill fb with MAGENTA so multithreading buffer splitting errors become obvious.
	fb_fill(fb[:], MAGENTA)

	// Run rendering on multiple cores
	run_workers(worker)

	// Write the image to a file
	write_ppm("output.ppm", WIDTH, HEIGHT, slice.to_bytes(fb[:]))
}


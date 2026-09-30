// Trace spike: how fast the engine's map walk traces rays on the GPU against
// the M5's hardware ray tracing (Metal intersector over a triangle
// acceleration structure), on real Marathon 2 levels, and how often the two
// disagree. The walk follows Marathon's polygon map through its openings,
// so it is exact in 5D space (polygons that overlap in plan and height but
// are not connected); the hardware sees every triangle at once, so where
// 5D space overlaps it hits walls, floors and ceilings that are not there
// for the walk. Measured for the Rampant tier plan (shadows that see
// sprites, reflections, world-space ambient occlusion). Windowless: it
// opens no window and never touches the game or its preferences.
//
// Run from the repository root:
//   xcrun swift scripts/trace-spike.swift [level indices...]
// That interprets it unoptimised: about 90 s for the default levels, most
// of it building the ray sets on the CPU (4 x 1M rays a level). Compiled
// with optimisation it takes about 15 s:
//   mkdir -p .deps/spike && xcrun swiftc -O scripts/trace-spike.swift -o .deps/spike/trace-spike
//   .deps/spike/trace-spike [level indices...]
// Levels: 0-27 solo, 28-40 net (40 is "5-D Space"). Default 0 1 2 6 10 12
// 23 40. The table is printed and written to .deps/spike/trace-spike.txt.
// GPU rates suffer from anything else using the GPU: check first.
//
// Environment:
//   TRACE_SPIKE_RAYS=<n>   rays per set (default 1048576)
//   TRACE_SPIKE_DEBUG=<n>  print each set's disagreements by cause, and the
//                          first n that neither the walk's own flaws nor a
//                          5D polygon explain (n >= 1000: the first n of all)
//
// What is traced (both tracers get identical rays, from a fixed seed):
//   Origins: a random polygon (3+ vertices, over 1/32 WU high), a random
//   convex combination of its vertices pulled 2% towards the centre, from
//   1/64 WU above its floor to 1/64 WU below its ceiling.
//   A  shadow segments, 1-2 WU, random direction, the end also in open air
//      (1/64 WU clear of the floor and ceiling of a polygon over it): the
//      engine's light_reaches (light polygon -2, so a segment that ends
//      inside the polygon reached counts as lit) against an any-hit query
//   B  occlusion rays, 0.5 WU, cosine-weighted about a random axis, nearest hit
//   C  long rays, 16 WU reach, random direction, nearest hit
//   D  as C in blocks of 64 sharing an origin polygon and a base direction
//      (5 degree jitter, origins within 1/32 WU): neighbouring pixels
// The walks are the engine's own (DurandalMetalShaders.h: light_reaches,
// polygon_exit, and trace_radiance returning where it stops instead of a
// colour), on the map buffer DurandalLights::BuildMap builds, with the
// same step limits (24 and 48). Heights are the stored ones (platforms as
// the map file has them). Coordinates are raw world units (1024 = 1 WU).
// The hardware's triangles: floors and ceilings as fans; per edge, a full
// wall with no polygon across, else the step up to its floor and the drop
// from its ceiling. One primitive acceleration structure a level, built
// for refitting; build and refit are timed on their own.
//
// Disagreement: segments, lit/blocked differs; nearest hits, hit/miss
// differs or the distances differ by more than 1% (and more than 1 unit).
// Each disagreement is put down to the walk (step limit, a segment ending
// beyond the floor or ceiling of the polygon it reached, float slip, or
// the walk's tolerances at corners and openings, each checked against a
// double-precision walk on the CPU) or else to the hardware (5D space).
import Foundation
import Metal
import simd

setvbuf(stdout, nil, _IOLBF, 0)

let env = ProcessInfo.processInfo.environment
let rayCount = max(Int(env["TRACE_SPIKE_RAYS"] ?? "") ?? (1 << 20), 64) / 64 * 64
let debugCount = Int(env["TRACE_SPIKE_DEBUG"] ?? "") ?? 0
let requested = CommandLine.arguments.dropFirst().compactMap { Int($0) }
let levelIndexes = requested.isEmpty ? [0, 1, 2, 6, 10, 12, 23, 40] : requested

// The repository root: the folder above this script, else the working folder
let root: URL = {
	let script = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent()
	let fm = FileManager.default
	if fm.fileExists(atPath: script.appendingPathComponent("data/Scenarios/Marathon 2/Map.sceA").path) { return script }
	return URL(fileURLWithPath: fm.currentDirectoryPath)
}()
let mapURL = root.appendingPathComponent("data/Scenarios/Marathon 2/Map.sceA")
let outDir = root.appendingPathComponent(".deps/spike")

var report = ""
func say(_ s: String = "") { print(s); report += s + "\n" }
func fail(_ s: String) -> Never { print("trace-spike: " + s); exit(1) }

// MARK: - The map file

// A MacBinary-wrapped Marathon WAD, big-endian throughout
guard let raw = try? Data(contentsOf: mapURL) else { fail("cannot read \(mapURL.path)") }
let wad = [UInt8](raw.dropFirst(128))
func u16(_ o: Int) -> Int { Int(UInt16(wad[o]) << 8 | UInt16(wad[o + 1])) }
func i16(_ o: Int) -> Int { Int(Int16(bitPattern: UInt16(wad[o]) << 8 | UInt16(wad[o + 1]))) }
func i32(_ o: Int) -> Int {
	Int(Int32(bitPattern: UInt32(wad[o]) << 24 | UInt32(wad[o + 1]) << 16 | UInt32(wad[o + 2]) << 8 | UInt32(wad[o + 3])))
}
let wadVersion = i16(0)
let directoryOffset = i32(72), wadCount = i16(76), appDataSize = i16(78)
let entryHeaderSize = i16(80), directoryEntrySize = i16(82)
guard (1...4).contains(wadVersion), wadCount > 0, directoryOffset > 0 else { fail("not a Marathon WAD (version \(wadVersion))") }

struct Polygon {
	var vertices: [SIMD2<Float>]
	var adjacent: [Int]
	var floor: Float
	var ceiling: Float
	var boxMin: SIMD2<Float>
	var boxMax: SIMD2<Float>
	var centroid: SIMD2<Float>
}

struct Level {
	let index: Int
	let name: String
	let polygons: [Polygon]
	let adjacencyMismatches: Int	// stored adjacency against the lines' owners
}

func readLevel(_ index: Int) -> Level? {
	guard index >= 0 && index < wadCount else { return nil }
	let entry = directoryOffset + index * (directoryEntrySize + appDataSize)
	let offset = i32(entry)
	var name = ""
	if appDataSize >= 74 {
		let start = entry + directoryEntrySize + 8
		let bytes = wad[start..<start + 66].prefix { $0 != 0 }
		name = String(data: Data(bytes), encoding: .macOSRoman) ?? ""
	}
	var chunks: [String: Range<Int>] = [:]
	var p = offset
	while true {
		let tag = String(decoding: wad[p..<p + 4], as: UTF8.self)
		let next = i32(p + 4), length = i32(p + 8)
		chunks[tag] = (p + entryHeaderSize)..<(p + entryHeaderSize + length)
		if next == 0 { break }
		p = offset + next
	}
	guard let polyChunk = chunks["POLY"] else { return nil }
	var points: [SIMD2<Float>] = []
	if let e = chunks["EPNT"] {
		for k in 0..<(e.count / 16) { points.append(SIMD2(Float(i16(e.lowerBound + k * 16 + 6)), Float(i16(e.lowerBound + k * 16 + 8)))) }
	} else if let e = chunks["PNTS"] {
		for k in 0..<(e.count / 4) { points.append(SIMD2(Float(i16(e.lowerBound + k * 4)), Float(i16(e.lowerBound + k * 4 + 2)))) }
	} else { return nil }
	let lines = chunks["LINS"]
	var polygons: [Polygon] = []
	var mismatches = 0
	let count = polyChunk.count / 128
	for k in 0..<count {
		// polygon_data (map.h): vertex_count +6, endpoint_indexes +8,
		// line_indexes +24, floor/ceiling height +44/+46,
		// adjacent_polygon_indexes +68
		let r = polyChunk.lowerBound + k * 128
		let n = min(u16(r + 6), 8)
		var vertices: [SIMD2<Float>] = [], adjacent: [Int] = []
		for i in 0..<n {
			let e = i16(r + 8 + 2 * i)
			vertices.append(e >= 0 && e < points.count ? points[e] : SIMD2(0, 0))
			adjacent.append(i16(r + 68 + 2 * i))
			// The engine recalculates adjacency from the lines when a map has
			// no redundant data: check the two agree
			if let l = lines {
				let line = i16(r + 24 + 2 * i)
				if line >= 0 && line < l.count / 32 {
					let cw = i16(l.lowerBound + line * 32 + 16), ccw = i16(l.lowerBound + line * 32 + 18)
					if (cw == k ? ccw : cw) != adjacent[i] { mismatches += 1 }
				}
			}
		}
		var lo = SIMD2<Float>(repeating: .infinity), hi = SIMD2<Float>(repeating: -.infinity), c = SIMD2<Float>(0, 0)
		for v in vertices { lo = pointwiseMin(lo, v); hi = pointwiseMax(hi, v); c += v }
		if n > 0 { c /= Float(n) }
		polygons.append(Polygon(vertices: vertices, adjacent: adjacent, floor: Float(i16(r + 44)),
								ceiling: Float(i16(r + 46)), boxMin: lo, boxMax: hi, centroid: c))
	}
	return Level(index: index, name: name, polygons: polygons, adjacencyMismatches: mismatches)
}

// 5D pairs, as the survey counts them: two unconnected polygons whose
// heights overlap and whose outlines overlap by at least 1/8 WU in plan
func fiveDPairs(_ polys: [Polygon]) -> [(Int, Int)] {
	func overlap(_ a: [SIMD2<Float>], _ b: [SIMD2<Float>]) -> Bool {
		for shape in [a, b] {
			for i in 0..<shape.count {
				let p0 = shape[i], p1 = shape[(i + 1) % shape.count]
				let axis = SIMD2<Double>(Double(p1.y - p0.y), Double(p0.x - p1.x))
				let len = (axis * axis).sum().squareRoot()
				let pa = a.map { Double($0.x) * axis.x + Double($0.y) * axis.y }
				let pb = b.map { Double($0.x) * axis.x + Double($0.y) * axis.y }
				if min(pa.max()!, pb.max()!) - max(pa.min()!, pb.min()!) < 128 * (len > 0 ? len : 1) { return false }
			}
		}
		return true
	}
	var pairs: [(Int, Int)] = []
	for a in 0..<polys.count where polys[a].vertices.count >= 3 {
		for b in (a + 1)..<max(polys.count, a + 1) where polys[b].vertices.count >= 3 {
			let A = polys[a], B = polys[b]
			if A.adjacent.contains(b) { continue }
			if min(A.ceiling, B.ceiling) - max(A.floor, B.floor) <= 0 { continue }
			if A.boxMax.x < B.boxMin.x || B.boxMax.x < A.boxMin.x || A.boxMax.y < B.boxMin.y || B.boxMax.y < A.boxMin.y { continue }
			if overlap(A.vertices, B.vertices) { pairs.append((a, b)) }
		}
	}
	return pairs
}

// MARK: - The engine's map buffer and the triangles

// DurandalLights::BuildMap: 9 float4 per polygon, (vertex count, floor,
// ceiling, w) then per edge (x, y of its first vertex, the polygon across
// it or -1, w); unused slots (0, 0, -1, 0). The w components carry light
// and liquid for the fog, which the walks never read: zero here, but for
// o[2].w = -1e9 (no liquid), as the engine writes.
func buildMap(_ polys: [Polygon]) -> [SIMD4<Float>] {
	var out = [SIMD4<Float>](repeating: SIMD4(0, 0, -1, 0), count: max(polys.count, 1) * 9)
	for (p, poly) in polys.enumerated() {
		let n = poly.vertices.count
		out[p * 9] = SIMD4(Float(n), poly.floor, poly.ceiling, 0)
		for i in 0..<n { out[p * 9 + 1 + i] = SIMD4(poly.vertices[i].x, poly.vertices[i].y, Float(poly.adjacent[i]), 0) }
		out[p * 9 + 2].w = -1e9
	}
	return out
}

// Floors and ceilings as fans; per edge, a full wall where there is no
// polygon across, else the step up to its floor and the drop from its
// ceiling, where they are within this polygon's height
func buildTriangles(_ polys: [Polygon]) -> (vertices: [Float], polygon: [Int32]) {
	var v: [Float] = [], owner: [Int32] = []
	func tri(_ a: SIMD3<Float>, _ b: SIMD3<Float>, _ c: SIMD3<Float>, _ p: Int) {
		v += [a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z]
		owner.append(Int32(p))
	}
	func quad(_ a: SIMD2<Float>, _ b: SIMD2<Float>, _ z0: Float, _ z1: Float, _ p: Int) {
		guard z1 > z0 else { return }
		tri(SIMD3(a, z0), SIMD3(b, z0), SIMD3(b, z1), p)
		tri(SIMD3(a, z0), SIMD3(b, z1), SIMD3(a, z1), p)
	}
	for (p, poly) in polys.enumerated() where poly.vertices.count >= 3 {
		let vs = poly.vertices, n = vs.count
		for i in 1..<(n - 1) {
			tri(SIMD3(vs[0], poly.floor), SIMD3(vs[i], poly.floor), SIMD3(vs[i + 1], poly.floor), p)
			tri(SIMD3(vs[0], poly.ceiling), SIMD3(vs[i + 1], poly.ceiling), SIMD3(vs[i], poly.ceiling), p)
		}
		for i in 0..<n {
			let a = vs[i], b = vs[(i + 1) % n], across = poly.adjacent[i]
			if across < 0 || across >= polys.count {
				quad(a, b, poly.floor, poly.ceiling, p)
			} else {
				let other = polys[across]
				if other.floor > poly.floor { quad(a, b, poly.floor, min(other.floor, poly.ceiling), p) }
				if other.ceiling < poly.ceiling { quad(a, b, max(other.ceiling, poly.floor), poly.ceiling, p) }
			}
		}
	}
	return (v, owner)
}

// MARK: - Ray sets

struct Random {
	var state: UInt64
	mutating func next() -> UInt64 {
		state &+= 0x9E3779B97F4A7C15
		var z = state
		z = (z ^ (z >> 30)) &* 0xBF58476D1CE4E5B9
		z = (z ^ (z >> 27)) &* 0x94D049BB133111EB
		return z ^ (z >> 31)
	}
	mutating func unit() -> Float { Float(next() >> 40) / Float(1 << 24) }
	mutating func range(_ a: Float, _ b: Float) -> Float { a + (b - a) * unit() }
	mutating func below(_ n: Int) -> Int { Int(next() % UInt64(n)) }
	mutating func sphere() -> SIMD3<Float> {
		let z = range(-1, 1), phi = range(0, 2 * .pi), r = (1 - z * z).squareRoot()
		return SIMD3(r * cos(phi), r * sin(phi), z)
	}
}

func basis(_ n: SIMD3<Float>) -> (SIMD3<Float>, SIMD3<Float>) {
	let t1 = simd_normalize(abs(n.z) < 0.9 ? simd_cross(n, SIMD3(0, 0, 1)) : simd_cross(n, SIMD3(1, 0, 0)))
	return (t1, simd_cross(n, t1))
}

// Point in a convex polygon, as the engine's inside_polygon (strictly)
func inside(_ poly: Polygon, _ p: SIMD2<Float>) -> Bool {
	let vs = poly.vertices, n = vs.count
	guard n >= 3 else { return false }
	var sign: Float = 0
	for i in 0..<n {
		let a = vs[i], b = vs[(i + 1) % n]
		let c = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x)
		if c == 0 { return false }
		if sign == 0 { sign = c } else if c * sign < 0 { return false }
	}
	return true
}

// Which polygons lie over a point: a 1 WU grid of the polygons' boxes
struct Grid {
	let lo: SIMD2<Float>
	let nx: Int, ny: Int
	var cells: [[Int32]]
	init(_ polys: [Polygon]) {
		var a = SIMD2<Float>(repeating: .infinity), b = SIMD2<Float>(repeating: -.infinity)
		for p in polys where p.vertices.count >= 3 { a = pointwiseMin(a, p.boxMin); b = pointwiseMax(b, p.boxMax) }
		lo = a
		nx = Int((b.x - a.x) / 1024) + 1
		ny = Int((b.y - a.y) / 1024) + 1
		cells = Array(repeating: [], count: nx * ny)
		for (k, p) in polys.enumerated() where p.vertices.count >= 3 {
			let x0 = Int((p.boxMin.x - lo.x) / 1024), x1 = Int((p.boxMax.x - lo.x) / 1024)
			let y0 = Int((p.boxMin.y - lo.y) / 1024), y1 = Int((p.boxMax.y - lo.y) / 1024)
			for y in y0...y1 { for x in x0...x1 { cells[y * nx + x].append(Int32(k)) } }
		}
	}
	func polygons(at p: SIMD2<Float>) -> [Int32] {
		let x = Int(((p.x - lo.x) / 1024).rounded(.down)), y = Int(((p.y - lo.y) / 1024).rounded(.down))
		guard x >= 0, y >= 0, x < nx, y < ny else { return [] }
		return cells[y * nx + x]
	}
}

// Rays as the GPU reads them: (origin, start polygon), then (segment end,
// 0) or (unit direction, reach)
struct RaySet {
	let name: String
	let nearest: Bool
	var rays: [SIMD4<Float>]
}

let margin: Float = 16	// origins and segment ends stay 1/64 WU off floors and ceilings

func makeRaySets(_ level: Level, grid: Grid) -> [RaySet] {
	let polys = level.polygons
	let eligible = polys.indices.filter { polys[$0].vertices.count >= 3 && polys[$0].ceiling - polys[$0].floor > 2 * margin }
	guard !eligible.isEmpty else { return [] }
	var rng = Random(state: 0xD0_5A_17 &+ UInt64(level.index) &* 0x1000193)
	func origin(_ p: Int) -> SIMD3<Float> {
		let poly = polys[p]
		var sum = SIMD2<Float>(0, 0), total: Float = 0
		for v in poly.vertices { let w = -log(max(rng.unit(), 1e-7)); sum += v * w; total += w }
		var q = sum / total
		q += (poly.centroid - q) * 0.02
		return SIMD3(q, rng.range(poly.floor + margin, poly.ceiling - margin))
	}
	func inAir(_ p: SIMD3<Float>) -> Bool {
		for k in grid.polygons(at: SIMD2(p.x, p.y)) {
			let poly = polys[Int(k)]
			if p.z > poly.floor + margin && p.z < poly.ceiling - margin && inside(poly, SIMD2(p.x, p.y)) { return true }
		}
		return false
	}
	let n = rayCount
	var a = [SIMD4<Float>](), b = [SIMD4<Float>](), c = [SIMD4<Float>](), d = [SIMD4<Float>]()
	a.reserveCapacity(2 * n); b.reserveCapacity(2 * n); c.reserveCapacity(2 * n); d.reserveCapacity(2 * n)

	// A: shadow segments between two points in open air
	while a.count < 2 * n {
		let p = eligible[rng.below(eligible.count)]
		let o = origin(p)
		for _ in 0..<16 {
			let to = o + rng.sphere() * rng.range(1024, 2048)
			if inAir(to) {
				a.append(SIMD4(o, Float(p))); a.append(SIMD4(to, 0))
				break
			}
		}
	}
	// B: short occlusion rays about a random axis
	for _ in 0..<n {
		let p = eligible[rng.below(eligible.count)]
		let o = origin(p), axis = rng.sphere(), (t1, t2) = basis(axis)
		let r1 = rng.unit(), phi = rng.range(0, 2 * .pi), r = r1.squareRoot()
		let dir = simd_normalize(t1 * (r * cos(phi)) + t2 * (r * sin(phi)) + axis * max(1 - r1, 0).squareRoot())
		b.append(SIMD4(o, Float(p))); b.append(SIMD4(dir, 512))
	}
	// C: long rays
	for _ in 0..<n {
		let p = eligible[rng.below(eligible.count)]
		c.append(SIMD4(origin(p), Float(p))); c.append(SIMD4(rng.sphere(), 16384))
	}
	// D: long rays in coherent blocks of 64
	let cone = Float(cos(5.0 * Double.pi / 180))
	for _ in 0..<(n / 64) {
		let p = eligible[rng.below(eligible.count)], poly = polys[p]
		let base = origin(p), axis = rng.sphere(), (t1, t2) = basis(axis)
		for _ in 0..<64 {
			var o = base + SIMD3(rng.range(-32, 32), rng.range(-32, 32), rng.range(-16, 16))
			if !(inside(poly, SIMD2(o.x, o.y)) && o.z > poly.floor + margin && o.z < poly.ceiling - margin) { o = base }
			let z = rng.range(cone, 1), phi = rng.range(0, 2 * .pi), s = max(1 - z * z, 0).squareRoot()
			let dir = simd_normalize(t1 * (s * cos(phi)) + t2 * (s * sin(phi)) + axis * z)
			d.append(SIMD4(o, Float(p))); d.append(SIMD4(dir, 16384))
		}
	}
	return [RaySet(name: "A shadow segments 1-2 WU", nearest: false, rays: a),
			RaySet(name: "B occlusion 0.5 WU", nearest: true, rays: b),
			RaySet(name: "C long 16 WU", nearest: true, rays: c),
			RaySet(name: "D coherent 16 WU", nearest: true, rays: d)]
}

// MARK: - GPU

let shaderSource = #"""
#include <metal_stdlib>
#include <metal_raytracing>
using namespace metal;
using namespace metal::raytracing;

struct SpikeRay {
	float4 a;	// origin, start polygon
	float4 b;	// segment end, or unit direction and reach
};

// ---- The engine's walks (DurandalMetalShaders.h), with a count of the
// ---- polygons visited (and for light_reaches, the last one); nothing else
// ---- changed

static bool light_reaches(device const float4* map, int poly, float3 from, float3 to, int light_poly, thread int& steps,
						  thread int& last)
{
	const float2 a = from.xy, d = to.xy - from.xy;
	float t_prev = 0.0;
	for (int step = 0; step < 24; ++step) {
		steps = step + 1;
		last = poly;
		if (poly == light_poly)
			return true;
		if (poly < 0)
			return false;
		const float4 h = map[poly * 9];
		const int n = int(h.x);
		float best = 2.0;
		int next = -1;
		for (int i = 0; i < n; ++i) {
			const float4 e0 = map[poly * 9 + 1 + i];
			const float4 e1 = map[poly * 9 + 1 + (i + 1 == n ? 0 : i + 1)];
			const float2 e = e1.xy - e0.xy;
			const float den = d.x * e.y - d.y * e.x;
			if (abs(den) < 1e-6)
				continue;
			const float2 w = e0.xy - a;
			const float t = (w.x * e.y - w.y * e.x) / den;
			const float s = (w.x * d.y - w.y * d.x) / den;
			if (t > t_prev + 1e-4 && t < best && s >= -1e-3 && s <= 1.0 + 1e-3) {
				best = t;
				next = int(e0.z);
			}
		}
		if (best > 1.0)
			return true;	// the light is within this polygon's reach
		if (next < 0)
			return false;	// a solid wall
		const float z = mix(from.z, to.z, best);
		const float4 hn = map[next * 9];
		if (z < max(h.y, hn.y) - 1.0 || z > min(h.z, hn.z) + 1.0)
			return false;	// a step, ledge or lower ceiling in the way
		poly = next;
		t_prev = best;
	}
	steps = 25;	// gave up: the engine counts the light as reaching
	return true;
}

static float polygon_exit(device const float4* map, int poly, float2 a, float2 d, float after, thread int& next,
						  thread int& edge)
{
	const int n = int(map[poly * 9].x);
	float best = 1e9;
	next = -1;
	edge = -1;
	for (int i = 0; i < n; ++i) {
		const float4 e0 = map[poly * 9 + 1 + i];
		const float4 e1 = map[poly * 9 + 1 + (i + 1 == n ? 0 : i + 1)];
		const float2 e = e1.xy - e0.xy;
		const float den = d.x * e.y - d.y * e.x;
		if (abs(den) < 1e-6)
			continue;
		const float2 w = e0.xy - a;
		const float t = (w.x * e.y - w.y * e.x) / den;
		const float s = (w.x * d.y - w.y * d.x) / den;
		if (t > after + 1e-3 && t < best && s >= -1e-3 && s <= 1.0 + 1e-3) {
			best = t;
			next = int(e0.z);
			edge = i;
		}
	}
	return best;
}

// trace_radiance's walk, returning where it stops: the distance, -1 when
// nothing is within reach, -2 when it gave up (step limit; the engine then
// returns the room's light at t = far); `hit` is the polygon whose floor,
// ceiling or wall stopped it
static float walk_nearest(device const float4* map, int poly, float3 o, float3 d, float far, thread int& hit,
						  thread int& steps)
{
	float t_prev = 0.0;
	for (int step = 0; step < 48; ++step) {
		steps = step + 1;
		const float4 h = map[poly * 9];
		int next = -1, edge = -1;
		const float leave = polygon_exit(map, poly, o.xy, d.xy, t_prev, next, edge);
		// The floor or ceiling, if the ray meets it inside this polygon
		float t_plane = 1e9;
		int plane = -1;
		if (d.z < -1e-5) { t_plane = (h.y - o.z) / d.z; plane = 0; }
		else if (d.z > 1e-5) { t_plane = (h.z - o.z) / d.z; plane = 1; }
		if (plane >= 0 && t_plane <= leave) {
			hit = poly;
			return t_plane > far ? -1.0 : t_plane;
		}
		if (leave > far || edge < 0)
			return -1.0;
		const float z = o.z + d.z * leave;
		bool wall = next < 0;
		if (!wall) {
			const float4 hn = map[next * 9];
			wall = z < hn.y || z > hn.z;
		}
		if (wall) {
			hit = poly;
			return leave;
		}
		poly = next;
		t_prev = leave;
	}
	return -2.0;
}

// ---- Kernels. Output per ray: segments (1 lit / 0 blocked; for the walk
// ---- polygons walked + 32 x (last polygon + 1), for the hardware the
// ---- triangle hit); nearest hits (distance or -1/-2; the polygon hit, for
// ---- the walk + 4096 x polygons walked)

kernel void walk_segments(device const SpikeRay* rays [[buffer(0)]], device float2* out [[buffer(1)]],
						  device const float4* map [[buffer(2)]], constant uint& count [[buffer(3)]],
						  uint id [[thread_position_in_grid]])
{
	if (id >= count)
		return;
	const SpikeRay r = rays[id];
	int steps = 0, last = -1;
	const bool lit = light_reaches(map, int(r.a.w), r.a.xyz, r.b.xyz, -2, steps, last);
	out[id] = float2(lit ? 1.0 : 0.0, float(steps) + 32.0 * float(last + 1));
}

kernel void walk_nearest_hits(device const SpikeRay* rays [[buffer(0)]], device float2* out [[buffer(1)]],
							  device const float4* map [[buffer(2)]], constant uint& count [[buffer(3)]],
							  uint id [[thread_position_in_grid]])
{
	if (id >= count)
		return;
	const SpikeRay r = rays[id];
	int hit = -1, steps = 0;
	const float t = walk_nearest(map, int(r.a.w), r.a.xyz, r.b.xyz, r.b.w, hit, steps);
	out[id] = float2(t, float(hit) + 4096.0 * float(steps));
}

kernel void hw_segments(device const SpikeRay* rays [[buffer(0)]], device float2* out [[buffer(1)]],
						primitive_acceleration_structure accel [[buffer(2)]], constant uint& count [[buffer(3)]],
						device const int* tri_poly [[buffer(4)]], uint id [[thread_position_in_grid]])
{
	if (id >= count)
		return;
	const SpikeRay q = rays[id];
	const float3 v = q.b.xyz - q.a.xyz;
	const float len = length(v);
	ray r(q.a.xyz, v / len, 0.0, len);
	intersector<triangle_data> i;
	i.accept_any_intersection(true);
	i.assume_geometry_type(geometry_type::triangle);
	i.force_opacity(forced_opacity::opaque);
	const intersection_result<triangle_data> res = i.intersect(r, accel);
	const bool lit = res.type == intersection_type::none;
	out[id] = float2(lit ? 1.0 : 0.0, lit ? -1.0 : float(res.primitive_id));
}

kernel void hw_nearest_hits(device const SpikeRay* rays [[buffer(0)]], device float2* out [[buffer(1)]],
							primitive_acceleration_structure accel [[buffer(2)]], constant uint& count [[buffer(3)]],
							device const int* tri_poly [[buffer(4)]], uint id [[thread_position_in_grid]])
{
	if (id >= count)
		return;
	const SpikeRay q = rays[id];
	ray r(q.a.xyz, q.b.xyz, 0.0, q.b.w);
	intersector<triangle_data> i;
	i.assume_geometry_type(geometry_type::triangle);
	i.force_opacity(forced_opacity::opaque);
	const intersection_result<triangle_data> res = i.intersect(r, accel);
	if (res.type == intersection_type::none)
		out[id] = float2(-1.0, -1.0);
	else
		out[id] = float2(res.distance, float(tri_poly[res.primitive_id]));
}
"""#

guard let device = MTLCreateSystemDefaultDevice() else { fail("no Metal device") }
guard device.supportsRaytracing else { fail("\(device.name) has no Metal ray tracing") }
guard let queue = device.makeCommandQueue() else { fail("no command queue") }
let library: MTLLibrary
do {
	let options = MTLCompileOptions()
	options.mathMode = .safe	// as the engine compiles its shaders
	library = try device.makeLibrary(source: shaderSource, options: options)
} catch { fail("shader compile: \(error)") }
func pipeline(_ name: String) -> MTLComputePipelineState {
	guard let f = library.makeFunction(name: name), let p = try? device.makeComputePipelineState(function: f) else { fail("pipeline \(name)") }
	return p
}
let walkSegments = pipeline("walk_segments"), walkNearest = pipeline("walk_nearest_hits")
let hwSegments = pipeline("hw_segments"), hwNearest = pipeline("hw_nearest_hits")

func buffer<T>(_ array: [T]) -> MTLBuffer {
	array.withUnsafeBytes { device.makeBuffer(bytes: $0.baseAddress!, length: max($0.count, 16), options: .storageModeShared)! }
}

func gpuMilliseconds(_ encode: (MTLCommandBuffer) -> Void) -> Double {
	let cb = queue.makeCommandBuffer()!
	encode(cb)
	cb.commit()
	cb.waitUntilCompleted()
	if let e = cb.error { fail("GPU error: \(e)") }
	return (cb.gpuEndTime - cb.gpuStartTime) * 1000
}

func median(_ v: [Double]) -> Double {
	let s = v.sorted()
	return s.count % 2 == 1 ? s[s.count / 2] : 0.5 * (s[s.count / 2 - 1] + s[s.count / 2])
}

func dispatch(_ cb: MTLCommandBuffer, _ pso: MTLComputePipelineState, rays: MTLBuffer, out: MTLBuffer, count: Int,
			  bind: (MTLComputeCommandEncoder) -> Void) {
	let enc = cb.makeComputeCommandEncoder()!
	enc.setComputePipelineState(pso)
	enc.setBuffer(rays, offset: 0, index: 0)
	enc.setBuffer(out, offset: 0, index: 1)
	var n = UInt32(count)
	enc.setBytes(&n, length: 4, index: 3)
	bind(enc)
	enc.dispatchThreads(MTLSize(width: count, height: 1, depth: 1), threadsPerThreadgroup: MTLSize(width: 64, height: 1, depth: 1))
	enc.endEncoding()
}

// MARK: - Checking a disagreement

// The two walks again, on the CPU in double precision, with the same step
// limits. With the engine's tolerances: where one of these agrees with the
// hardware and the GPU walk does not, the GPU walk's float arithmetic
// slipped (at a shared edge the crossing found from the far side can land
// a hair beyond the one found from the near side, past the 1e-3
// tolerance, and the walk steps back across the edge it has just crossed).
// Strict (no tolerance on where an edge is crossed, on how far beyond the
// last crossing the next must be, or on the opening's heights): where only
// this one agrees, the ray passed within the walk's tolerance of a corner
// or an opening's edge (an edge counts as crossed up to 1/1000 of its
// length beyond its ends, so a ray grazing a corner can take either edge;
// the next crossing must be 1/10000 of a segment beyond the last, so a
// segment grazing a corner can miss the wall just past it; a segment may
// clip an opening by 1 unit)
func lightReachesD(_ map: [SIMD4<Float>], _ start: Int, _ from: SIMD3<Double>, _ to: SIMD3<Double>, strict: Bool = false) -> Bool {
	let sTol = strict ? 0.0 : 1e-3, zTol = strict ? 0.0 : 1.0, tTol = strict ? 1e-9 : 1e-4
	var poly = start
	let d = SIMD2(to.x - from.x, to.y - from.y)
	var tPrev = 0.0
	for _ in 0..<24 {
		if poly < 0 { return false }
		let h = map[poly * 9], n = Int(h.x)
		var best = 2.0, next = -1
		for i in 0..<n {
			let e0 = map[poly * 9 + 1 + i], e1 = map[poly * 9 + 1 + (i + 1 == n ? 0 : i + 1)]
			let e = SIMD2(Double(e1.x) - Double(e0.x), Double(e1.y) - Double(e0.y))
			let den = d.x * e.y - d.y * e.x
			if abs(den) < 1e-6 { continue }
			let w = SIMD2(Double(e0.x) - from.x, Double(e0.y) - from.y)
			let t = (w.x * e.y - w.y * e.x) / den, s = (w.x * d.y - w.y * d.x) / den
			if t > tPrev + tTol && t < best && s >= -sTol && s <= 1 + sTol { best = t; next = Int(e0.z) }
		}
		if best > 1 { return true }
		if next < 0 { return false }
		let z = from.z + (to.z - from.z) * best, hn = map[next * 9]
		if z < max(Double(h.y), Double(hn.y)) - zTol || z > min(Double(h.z), Double(hn.z)) + zTol { return false }
		poly = next
		tPrev = best
	}
	return true
}

func walkNearestD(_ map: [SIMD4<Float>], _ start: Int, _ o: SIMD3<Double>, _ d: SIMD3<Double>, _ far: Double,
				  strict: Bool = false) -> Double {
	let sTol = strict ? 0.0 : 1e-3, tTol = strict ? 1e-6 : 1e-3
	var poly = start, tPrev = 0.0
	for _ in 0..<48 {
		let h = map[poly * 9], n = Int(h.x)
		var leave = 1e9, next = -1, edge = -1
		for i in 0..<n {
			let e0 = map[poly * 9 + 1 + i], e1 = map[poly * 9 + 1 + (i + 1 == n ? 0 : i + 1)]
			let e = SIMD2(Double(e1.x) - Double(e0.x), Double(e1.y) - Double(e0.y))
			let den = d.x * e.y - d.y * e.x
			if abs(den) < 1e-6 { continue }
			let w = SIMD2(Double(e0.x) - o.x, Double(e0.y) - o.y)
			let t = (w.x * e.y - w.y * e.x) / den, s = (w.x * d.y - w.y * d.x) / den
			if t > tPrev + tTol && t < leave && s >= -sTol && s <= 1 + sTol { leave = t; next = Int(e0.z); edge = i }
		}
		var tPlane = 1e9, plane = -1
		if d.z < -1e-5 { tPlane = (Double(h.y) - o.z) / d.z; plane = 0 } else if d.z > 1e-5 { tPlane = (Double(h.z) - o.z) / d.z; plane = 1 }
		if plane >= 0 && tPlane <= leave { return tPlane > far ? -1 : tPlane }
		if leave > far || edge < 0 { return -1 }
		let z = o.z + d.z * leave
		var wall = next < 0
		if !wall { let hn = map[next * 9]; wall = z < Double(hn.y) || z > Double(hn.z) }
		if wall { return leave }
		poly = next
		tPrev = leave
	}
	return -2
}

// Nearest hits agree: both miss, or both hit within 1% (and 1 unit)
func agree(_ a: Double, _ b: Double) -> Bool {
	if (a >= 0) != (b >= 0) { return false }
	return a < 0 || abs(a - b) <= max(0.01 * max(a, b), 1)
}

// MARK: - The run

func grouped(_ n: Double) -> String {
	let f = NumberFormatter()
	f.numberStyle = .decimal
	f.maximumFractionDigits = 0
	f.groupingSeparator = ","
	return f.string(from: NSNumber(value: n)) ?? "\(n)"
}
func pad(_ s: String, _ w: Int, right: Bool = true) -> String {
	s.count >= w ? s : (right ? String(repeating: " ", count: w - s.count) + s : s + String(repeating: " ", count: w - s.count))
}
func percent(_ n: Int, _ of: Int) -> Double { 100 * Double(n) / Double(max(of, 1)) }

struct Row {
	let level: Int
	let fiveD: Int
	let set: Int
	let walkRate: Double, hwRate: Double
	let differ: Double, walkWrong: Double, hwWrong: Double, gaveUp: Double
}
var rows: [Row] = []
let setNames = ["A shadow segments 1-2 WU", "B occlusion 0.5 WU", "C long 16 WU", "D coherent 16 WU"]
let columns = "   " + pad("Ray set", 26, right: false) + pad("walk rays/ms", 13) + pad("hw rays/ms", 12) + pad("hw/walk", 9)
	+ pad("polys/ray", 11) + pad("differ", 9) + pad("walk side", 12) + pad("hw side", 10) + pad("gave up", 9)

say("Trace spike: the engine's map walk against hardware ray tracing on \(device.name), \(grouped(Double(rayCount))) rays a set.")
say("Rates: rays per millisecond of GPU time, median of 10 dispatches. polys/ray: polygons the walk visits.")
say("differ: the tracers disagree. Of those, walk side: the walk gave up at its step limit, a segment ended beyond")
say("the floor or ceiling of the polygon the walk reached (stacked rooms: light_reaches never checks the end's")
say("height), the walk's float arithmetic slipped (a double-precision walk agrees with the hardware), or the ray")
say("passed within the walk's tolerance of a corner or an opening's edge (a strict double walk agrees);")
say("hw side: the rest, where the hardware sees geometry the walk does not (5D space). gave up: step limit hit.")
let started = Date()

for index in levelIndexes {
	guard let level = readLevel(index) else { say("\nLevel \(index): not found"); continue }
	let polys = level.polygons
	let pairs = fiveDPairs(polys)
	let in5D = Set(pairs.flatMap { [$0.0, $0.1] })
	let map = buildMap(polys)
	let (vertices, triPoly) = buildTriangles(polys)
	let triangles = triPoly.count
	let mapBuffer = buffer(map), vertexBuffer = buffer(vertices), triPolyBuffer = buffer(triPoly)

	// The acceleration structure, built for refitting (platforms move)
	let geometry = MTLAccelerationStructureTriangleGeometryDescriptor()
	geometry.vertexBuffer = vertexBuffer
	geometry.vertexStride = 12
	geometry.vertexFormat = .float3
	geometry.triangleCount = triangles
	geometry.opaque = true
	let descriptor = MTLPrimitiveAccelerationStructureDescriptor()
	descriptor.geometryDescriptors = [geometry]
	descriptor.usage = .refit
	let sizes = device.accelerationStructureSizes(descriptor: descriptor)
	guard let accel = device.makeAccelerationStructure(size: sizes.accelerationStructureSize),
		  let scratch = device.makeBuffer(length: max(sizes.buildScratchBufferSize, sizes.refitScratchBufferSize, 16),
										  options: .storageModePrivate) else { fail("acceleration structure") }
	let build = { (cb: MTLCommandBuffer) in
		let enc = cb.makeAccelerationStructureCommandEncoder()!
		enc.build(accelerationStructure: accel, descriptor: descriptor, scratchBuffer: scratch, scratchBufferOffset: 0)
		enc.endEncoding()
	}
	let refit = { (cb: MTLCommandBuffer) in
		let enc = cb.makeAccelerationStructureCommandEncoder()!
		enc.refit(sourceAccelerationStructure: accel, descriptor: descriptor, destinationAccelerationStructure: nil,
				  scratchBuffer: scratch, scratchBufferOffset: 0)
		enc.endEncoding()
	}
	_ = gpuMilliseconds(build)

	let generating = Date()
	let sets = makeRaySets(level, grid: Grid(polys))
	let generation = Date().timeIntervalSince(generating)
	var lines: [String] = []

	for (s, set) in sets.enumerated() {
		let count = set.rays.count / 2
		let rays = buffer(set.rays)
		let walkOut = device.makeBuffer(length: count * 8, options: .storageModeShared)!
		let hwOut = device.makeBuffer(length: count * 8, options: .storageModeShared)!
		let walk = { (cb: MTLCommandBuffer) in
			dispatch(cb, set.nearest ? walkNearest : walkSegments, rays: rays, out: walkOut, count: count) { enc in
				enc.setBuffer(mapBuffer, offset: 0, index: 2)
			}
		}
		let hw = { (cb: MTLCommandBuffer) in
			dispatch(cb, set.nearest ? hwNearest : hwSegments, rays: rays, out: hwOut, count: count) { enc in
				enc.setAccelerationStructure(accel, bufferIndex: 2)
				enc.setBuffer(triPolyBuffer, offset: 0, index: 4)
			}
		}
		// Warm up (the GPU's clocks ramp), then alternate so drift falls on both alike
		let warming = Date()
		repeat { _ = gpuMilliseconds(walk); _ = gpuMilliseconds(hw) } while Date().timeIntervalSince(warming) < 0.3
		var walkTimes: [Double] = [], hwTimes: [Double] = []
		for _ in 0..<10 { walkTimes.append(gpuMilliseconds(walk)); hwTimes.append(gpuMilliseconds(hw)) }
		let walkRate = Double(count) / median(walkTimes), hwRate = Double(count) / median(hwTimes)

		// Compare the answers, and say which one is wrong
		let w = walkOut.contents().bindMemory(to: SIMD2<Float>.self, capacity: count)
		let h = hwOut.contents().bindMemory(to: SIMD2<Float>.self, capacity: count)
		var differ = 0, gaveUp = 0, steps = 0, shown = 0
		var limit = 0, blind = 0, slip = 0, tolerance = 0, hardware = 0, hardware5D = 0
		for k in 0..<count {
			let a = w[k], b = h[k]
			let o4 = set.rays[2 * k], e4 = set.rays[2 * k + 1]
			let start = Int(o4.w)
			var disagree: Bool, walked: Int, last = -1, hwPoly = -1, walkPoly = -1
			if set.nearest {
				walked = Int(a.y / 4096)
				walkPoly = Int(a.y) - 4096 * walked
				if walkPoly >= 4095 { walked += 1; walkPoly -= 4096 }	// a miss (-1) packs as 4096 x steps - 1
				hwPoly = Int(b.y)
				disagree = !agree(Double(a.x), Double(b.x))
			} else {
				walked = Int(a.y) % 32
				last = Int(a.y) / 32 - 1
				if b.y >= 0 { hwPoly = Int(triPoly[Int(b.y)]) }
				disagree = (a.x > 0.5) != (b.x > 0.5)
			}
			let gave = set.nearest ? a.x == -2 : walked > 24
			if gave { gaveUp += 1 }
			steps += min(walked, set.nearest ? 48 : 24)
			guard disagree else { continue }
			differ += 1
			let o = SIMD3<Double>(Double(o4.x), Double(o4.y), Double(o4.z)), e = SIMD3<Double>(Double(e4.x), Double(e4.y), Double(e4.z))
			var cause: String
			if gave {
				limit += 1; cause = "walk gave up"
			} else if !set.nearest && a.x > 0.5 && last >= 0 && (e.z < Double(polys[last].floor) || e.z > Double(polys[last].ceiling)) {
				blind += 1; cause = "walk blind: ends beyond polygon \(last)'s floor or ceiling"
			} else if set.nearest ? agree(walkNearestD(map, start, o, e, Double(e4.w)), Double(b.x))
									: lightReachesD(map, start, o, e) == (b.x > 0.5) {
				slip += 1; cause = "walk precision"
			} else if set.nearest ? agree(walkNearestD(map, start, o, e, Double(e4.w), strict: true), Double(b.x))
									: lightReachesD(map, start, o, e, strict: true) == (b.x > 0.5) {
				tolerance += 1; cause = "walk tolerance"
			} else {
				hardware += 1
				let touches = in5D.contains(start) || in5D.contains(hwPoly) || in5D.contains(walkPoly) || in5D.contains(last)
				if touches { hardware5D += 1 }
				cause = touches ? "hardware, 5D polygon involved" : "hardware, no 5D polygon involved"
			}
			if shown < debugCount && (debugCount >= 1000 || cause.hasPrefix("hardware, no")) {
				shown += 1
				print(String(format: "   %@ ray %d: origin (%.1f, %.1f, %.1f) poly %d  %@ (%.4f, %.4f, %.4f, %.0f)",
							 setNames[s], k, o.x, o.y, o.z, start, set.nearest ? "dir" : "to", e.x, e.y, e.z, e4.w))
				if set.nearest {
					print(String(format: "      walk t %.2f poly %d steps %d | hw t %.2f poly %d | %@", a.x, walkPoly, walked, b.x, hwPoly, cause))
				} else {
					print(String(format: "      walk %@ steps %d last %d | hw %@ poly %d | %@", a.x > 0.5 ? "lit" : "blocked", walked, last,
								 b.x > 0.5 ? "lit" : "blocked", hwPoly, cause))
				}
			}
		}
		if debugCount > 0 && differ > 0 {
			print("   \(setNames[s]): \(differ) differ: walk gave up \(limit), blind \(blind), precision \(slip), tolerance \(tolerance); hardware \(hardware) (\(hardware5D) touching a 5D polygon)")
		}
		let row = Row(level: index, fiveD: pairs.count, set: s, walkRate: walkRate, hwRate: hwRate,
					  differ: percent(differ, count), walkWrong: percent(limit + blind + slip + tolerance, count),
					  hwWrong: percent(hardware, count), gaveUp: percent(gaveUp, count))
		rows.append(row)
		lines.append("   " + pad(set.name, 26, right: false) + pad(grouped(walkRate), 13) + pad(grouped(hwRate), 12)
					 + pad(String(format: "%.1fx", hwRate / walkRate), 9) + pad(String(format: "%.1f", Double(steps) / Double(count)), 11)
					 + pad(String(format: "%.3f%%", row.differ), 9) + pad(String(format: "%.3f%%", row.walkWrong), 12)
					 + pad(String(format: "%.3f%%", row.hwWrong), 10) + pad(String(format: "%.3f%%", row.gaveUp), 9))
	}

	// Build and refit, timed with the GPU warm
	var builds: [Double] = [], refits: [Double] = []
	for k in 0..<12 { let ms = gpuMilliseconds(build); if k >= 2 { builds.append(ms) } }
	for k in 0..<12 { let ms = gpuMilliseconds(refit); if k >= 2 { refits.append(ms) } }

	say()
	say(String(format: "%2d %@: %d polygons, %@ triangles, %d 5D pairs; build %.3f ms, refit %.3f ms, %@ KB",
			   index, level.name, polys.count, grouped(Double(triangles)), pairs.count, median(builds), median(refits),
			   grouped(Double(sizes.accelerationStructureSize) / 1024)))
	if level.adjacencyMismatches > 0 { say("   (\(level.adjacencyMismatches) stored adjacencies differ from the lines' owners)") }
	say(columns)
	for l in lines { say(l) }
	if debugCount > 0 { print(String(format: "   (ray sets built on the CPU in %.1f s)", generation)) }
}

// Summary across the levels run
if !rows.isEmpty {
	say()
	say("Across the levels: geometric mean of the rates; mean disagreement on levels without and with 5D space.")
	say("   " + pad("Ray set", 26, right: false) + pad("walk rays/ms", 13) + pad("hw rays/ms", 12) + pad("hw/walk", 9)
		+ pad("no 5D: differ", 15) + pad("hw side", 10) + pad("5D: differ", 12) + pad("hw side", 10))
	for s in 0..<4 {
		let r = rows.filter { $0.set == s }
		guard !r.isEmpty else { continue }
		let gw = exp(r.map { log($0.walkRate) }.reduce(0, +) / Double(r.count))
		let gh = exp(r.map { log($0.hwRate) }.reduce(0, +) / Double(r.count))
		let clean = r.filter { $0.fiveD == 0 }, fived = r.filter { $0.fiveD > 0 }
		func mean(_ rs: [Row], _ f: (Row) -> Double) -> String {
			rs.isEmpty ? "-" : String(format: "%.3f%%", rs.map(f).reduce(0, +) / Double(rs.count))
		}
		say("   " + pad(setNames[s], 26, right: false) + pad(grouped(gw), 13) + pad(grouped(gh), 12)
			+ pad(String(format: "%.1fx", gh / gw), 9) + pad(mean(clean, \.differ), 15) + pad(mean(clean, \.hwWrong), 10)
			+ pad(mean(fived, \.differ), 12) + pad(mean(fived, \.hwWrong), 10))
	}
}
say()
say(String(format: "Finished in %.0f s.", Date().timeIntervalSince(started)))

try? FileManager.default.createDirectory(at: outDir, withIntermediateDirectories: true)
let outFile = outDir.appendingPathComponent("trace-spike.txt")
do { try report.write(to: outFile, atomically: true, encoding: .utf8); print("Written to \(outFile.path)") }
catch { print("could not write \(outFile.path): \(error)") }

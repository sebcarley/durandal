// Brightness ratio of two frame shots, for finding shading steps: writes
// a grey PNG of (luminance A / luminance B - 1) * gain + 0.5 and prints a
// coarse grid of mean ratios (percent) so a band or rectangle shows up.
//   xcrun swift scripts/ratio.swift <a.png> <b.png> <out.png> [gain] [cols] [rows]
import Foundation
import CoreGraphics
import ImageIO
import UniformTypeIdentifiers

let a = CommandLine.arguments
guard a.count >= 4 else { print("usage: ratio.swift <a.png> <b.png> <out.png> [gain] [cols] [rows]"); exit(1) }
let gain = a.count > 4 ? Double(a[4]) ?? 4.0 : 4.0
let cols = a.count > 5 ? Int(a[5]) ?? 16 : 16
let rows = a.count > 6 ? Int(a[6]) ?? 9 : 9
func load(_ path: String) -> (w: Int, h: Int, px: [UInt8])? {
	guard let src = CGImageSourceCreateWithURL(URL(fileURLWithPath: path) as CFURL, nil),
		  let img = CGImageSourceCreateImageAtIndex(src, 0, nil) else { return nil }
	let w = img.width, h = img.height
	var px = [UInt8](repeating: 0, count: w * h * 4)
	let ctx = CGContext(data: &px, width: w, height: h, bitsPerComponent: 8, bytesPerRow: w * 4,
						space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
	ctx.draw(img, in: CGRect(x: 0, y: 0, width: w, height: h))
	return (w, h, px)
}
guard let A = load(a[1]), let B = load(a[2]), A.w == B.w, A.h == B.h else { print("cannot read, or sizes differ"); exit(1) }
let w = A.w, h = A.h
var out = [UInt8](repeating: 255, count: w * h * 4)
var grid = [Double](repeating: 0, count: cols * rows)
var count = [Int](repeating: 0, count: cols * rows)
func lum(_ p: [UInt8], _ i: Int) -> Double { 0.299 * Double(p[i]) + 0.587 * Double(p[i + 1]) + 0.114 * Double(p[i + 2]) }
for y in 0..<h { for x in 0..<w {
	let i = (y * w + x) * 4
	let la = lum(A.px, i), lb = lum(B.px, i)
	var r = 0.0
	if lb > 8 && la > 8 { r = la / lb - 1 }	// ignore near-black
	let v = UInt8(max(0, min(255, 127.5 + r * gain * 127.5)))
	out[i] = v; out[i + 1] = v; out[i + 2] = v; out[i + 3] = 255
	if lb > 8 && la > 8 {
		let g = min(rows - 1, y * rows / h) * cols + min(cols - 1, x * cols / w)
		grid[g] += r; count[g] += 1
	}
}}
let ctx = CGContext(data: &out, width: w, height: h, bitsPerComponent: 8, bytesPerRow: w * 4,
					space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
if let img = ctx.makeImage(), let dst = CGImageDestinationCreateWithURL(URL(fileURLWithPath: a[3]) as CFURL, UTType.png.identifier as CFString, 1, nil) {
	CGImageDestinationAddImage(dst, img, nil); CGImageDestinationFinalize(dst)
}
print("mean ratio A/B - 1, percent, per block (rows top to bottom):")
for r in 0..<rows {
	var line = ""
	for c in 0..<cols { let g = r * cols + c; line += count[g] > 0 ? String(format: "%6.1f", 100 * grid[g] / Double(count[g])) : "     ." }
	print(line)
}

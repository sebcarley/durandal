// Mean RGB (0-255) of a region of each PNG.
//   xcrun swift scripts/stats.swift <x> <y> <w> <h> <in.png>...
import Foundation
import CoreGraphics
import ImageIO

let a = CommandLine.arguments
guard a.count >= 6, let x = Int(a[1]), let y = Int(a[2]), let w = Int(a[3]), let h = Int(a[4]) else {
	print("usage: stats.swift <x> <y> <w> <h> <in.png>..."); exit(1)
}
for path in a[5...] {
	guard let src = CGImageSourceCreateWithURL(URL(fileURLWithPath: path) as CFURL, nil),
		  let img = CGImageSourceCreateImageAtIndex(src, 0, nil),
		  let crop = img.cropping(to: CGRect(x: x, y: y, width: w, height: h)) else { print("cannot read \(path)"); continue }
	var px = [UInt8](repeating: 0, count: w * h * 4)
	let ctx = CGContext(data: &px, width: w, height: h, bitsPerComponent: 8, bytesPerRow: w * 4,
						space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
	ctx.draw(crop, in: CGRect(x: 0, y: 0, width: w, height: h))
	var sum = [Double](repeating: 0, count: 3)
	for i in 0..<(w * h) { for c in 0..<3 { sum[c] += Double(px[i * 4 + c]) } }
	let n = Double(w * h)
	print(String(format: "%6.1f %6.1f %6.1f  %@", sum[0] / n, sum[1] / n, sum[2] / n, (path as NSString).lastPathComponent))
}

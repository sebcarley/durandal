// Crops PNGs and enlarges them with nearest-neighbour scaling, stacking
// several inputs side by side, for inspecting renders pixel by pixel.
//   xcrun swift scripts/crop.swift <out.png> <x> <y> <w> <h> <scale> <in.png>...
import Foundation
import CoreGraphics
import ImageIO
import UniformTypeIdentifiers

let a = CommandLine.arguments
guard a.count >= 8, let x = Int(a[2]), let y = Int(a[3]), let w = Int(a[4]), let h = Int(a[5]), let s = Int(a[6]) else {
	print("usage: crop.swift <out.png> <x> <y> <w> <h> <scale> <in.png>..."); exit(1)
}
let inputs = Array(a[7...])
let gap = 4
let outW = inputs.count * w * s + (inputs.count - 1) * gap, outH = h * s
let cs = CGColorSpaceCreateDeviceRGB()
guard let ctx = CGContext(data: nil, width: outW, height: outH, bitsPerComponent: 8, bytesPerRow: 0, space: cs,
						  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { exit(1) }
ctx.interpolationQuality = .none
ctx.setFillColor(CGColor(red: 1, green: 0, blue: 1, alpha: 1))
ctx.fill(CGRect(x: 0, y: 0, width: outW, height: outH))
for (i, path) in inputs.enumerated() {
	guard let src = CGImageSourceCreateWithURL(URL(fileURLWithPath: path) as CFURL, nil),
		  let img = CGImageSourceCreateImageAtIndex(src, 0, nil),
		  let crop = img.cropping(to: CGRect(x: x, y: y, width: w, height: h)) else { print("cannot read \(path)"); exit(1) }
	ctx.draw(crop, in: CGRect(x: i * (w * s + gap), y: 0, width: w * s, height: h * s))
}
guard let out = ctx.makeImage(),
	  let dest = CGImageDestinationCreateWithURL(URL(fileURLWithPath: a[1]) as CFURL, UTType.png.identifier as CFString, 1, nil) else { exit(1) }
CGImageDestinationAddImage(dest, out, nil)
CGImageDestinationFinalize(dest)

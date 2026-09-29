// Lays PNGs out in a grid (for reviewing textures), cells scaled to fit.
//   xcrun swift scripts/sheet.swift <out.png> <columns> <cell> <in.png>...
import Foundation
import CoreGraphics
import ImageIO
import UniformTypeIdentifiers

let a = CommandLine.arguments
guard a.count >= 5, let cols = Int(a[2]), let cell = Int(a[3]) else { print("usage"); exit(1) }
let inputs = Array(a[4...])
let rows = (inputs.count + cols - 1) / cols
let gap = 6
let W = cols * (cell + gap), H = rows * (cell + gap)
let ctx = CGContext(data: nil, width: W, height: H, bitsPerComponent: 8, bytesPerRow: 0, space: CGColorSpaceCreateDeviceRGB(),
					bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
ctx.interpolationQuality = .none
ctx.setFillColor(CGColor(red: 1, green: 0, blue: 1, alpha: 1))
ctx.fill(CGRect(x: 0, y: 0, width: W, height: H))
for (i, path) in inputs.enumerated() {
	guard let src = CGImageSourceCreateWithURL(URL(fileURLWithPath: path) as CFURL, nil),
		  let img = CGImageSourceCreateImageAtIndex(src, 0, nil) else { continue }
	let c = i % cols, r = i / cols
	ctx.draw(img, in: CGRect(x: c * (cell + gap), y: H - (r + 1) * (cell + gap) + gap, width: cell, height: cell))
}
let dest = CGImageDestinationCreateWithURL(URL(fileURLWithPath: a[1]) as CFURL, UTType.png.identifier as CFString, 1, nil)!
CGImageDestinationAddImage(dest, ctx.makeImage()!, nil)
CGImageDestinationFinalize(dest)

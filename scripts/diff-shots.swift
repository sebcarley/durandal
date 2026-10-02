// Compares two directories of frame shots (PNGs of the same names): per
// shot the mean absolute difference (0-255, largest channel), the share of
// pixels off by more than 8 and 32, and the largest difference; with a
// third directory, writes each pair's difference x8 there.
//   xcrun swiftc -O scripts/diff-shots.swift -o .deps/diff-shots
//   .deps/diff-shots <dirA> <dirB> [out-dir]
import Foundation
import CoreGraphics
import ImageIO
import UniformTypeIdentifiers
func load(_ p: String) -> (Int, Int, [UInt8])? {
    guard let src = CGImageSourceCreateWithURL(URL(fileURLWithPath: p) as CFURL, nil),
          let img = CGImageSourceCreateImageAtIndex(src, 0, nil) else { return nil }
    let w = img.width, h = img.height
    var px = [UInt8](repeating: 0, count: w * h * 4)
    let ctx = CGContext(data: &px, width: w, height: h, bitsPerComponent: 8, bytesPerRow: w * 4,
                        space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGImageAlphaInfo.noneSkipLast.rawValue)!
    ctx.draw(img, in: CGRect(x: 0, y: 0, width: w, height: h))
    return (w, h, px)
}
let a = CommandLine.arguments
let names = (try? FileManager.default.contentsOfDirectory(atPath: a[1]))?.filter { $0.hasSuffix(".png") }.sorted() ?? []
var total = 0.0, n = 0
for name in names {
    guard let (w, h, pa) = load(a[1] + "/" + name), let (w2, h2, pb) = load(a[2] + "/" + name), w == w2, h == h2 else { continue }
    var sum = 0, over8 = 0, over32 = 0, mx = 0
    var out = [UInt8](repeating: 0, count: w * h * 4)
    for i in 0..<(w * h) {
        var d = 0
        for c in 0..<3 { d = max(d, abs(Int(pa[i * 4 + c]) - Int(pb[i * 4 + c]))) }
        sum += d; if d > 8 { over8 += 1 }; if d > 32 { over32 += 1 }; mx = max(mx, d)
        let v = UInt8(min(255, d * 8)); out[i * 4] = v; out[i * 4 + 1] = v; out[i * 4 + 2] = v; out[i * 4 + 3] = 255
    }
    let mean = Double(sum) / Double(w * h)
    total += mean; n += 1
    print(String(format: "%@  mean %.3f  >8: %.3f%%  >32: %.3f%%  max %d", name, mean,
                 100.0 * Double(over8) / Double(w * h), 100.0 * Double(over32) / Double(w * h), mx))
    if a.count > 3 {
        let ctx = CGContext(data: &out, width: w, height: h, bitsPerComponent: 8, bytesPerRow: w * 4,
                            space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGImageAlphaInfo.noneSkipLast.rawValue)!
        let dst = CGImageDestinationCreateWithURL(URL(fileURLWithPath: a[3] + "/" + name) as CFURL,
                                                  UTType.png.identifier as CFString, 1, nil)!
        CGImageDestinationAddImage(dst, ctx.makeImage()!, nil); CGImageDestinationFinalize(dst)
    }
}
if n > 0 { print(String(format: "average mean diff %.3f over %d shots", total / Double(n), n)) }

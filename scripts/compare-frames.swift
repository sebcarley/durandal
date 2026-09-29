// Compares tickNNNNNN-frame-gl.png with tickNNNNNN-frame-metal.png in a
// directory: mean absolute difference (0-255), % of pixels off by more than
// 16, largest difference; writes tickNNNNNN-frame-diff.png (x4) for each.
import Foundation
import CoreGraphics
import ImageIO
import UniformTypeIdentifiers

func load(_ url: URL) -> (w: Int, h: Int, px: [UInt8])? {
    guard let src = CGImageSourceCreateWithURL(url as CFURL, nil),
          let img = CGImageSourceCreateImageAtIndex(src, 0, nil) else { return nil }
    let w = img.width, h = img.height
    var px = [UInt8](repeating: 0, count: w * h * 4)
    let ctx = CGContext(data: &px, width: w, height: h, bitsPerComponent: 8, bytesPerRow: w * 4,
                        space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGImageAlphaInfo.noneSkipLast.rawValue)!
    ctx.draw(img, in: CGRect(x: 0, y: 0, width: w, height: h))
    return (w, h, px)
}

func save(_ url: URL, _ w: Int, _ h: Int, _ px: inout [UInt8]) {
    let ctx = CGContext(data: &px, width: w, height: h, bitsPerComponent: 8, bytesPerRow: w * 4,
                        space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGImageAlphaInfo.noneSkipLast.rawValue)!
    let img = ctx.makeImage()!
    let dst = CGImageDestinationCreateWithURL(url as CFURL, UTType.png.identifier as CFString, 1, nil)!
    CGImageDestinationAddImage(dst, img, nil)
    CGImageDestinationFinalize(dst)
}

let dir = URL(fileURLWithPath: CommandLine.arguments[1])
let files = (try? FileManager.default.contentsOfDirectory(atPath: dir.path)) ?? []
let ticks = files.filter { $0.hasSuffix("-frame-gl.png") }.map { String($0.prefix(10)) }.sorted()
print("tick, mean diff, % over 16, max")
for t in ticks {
    guard let a = load(dir.appendingPathComponent("\(t)-frame-gl.png")),
          let b = load(dir.appendingPathComponent("\(t)-frame-metal.png")) else { print("\(t): missing pair"); continue }
    if a.w != b.w || a.h != b.h { print("\(t): size \(a.w)x\(a.h) vs \(b.w)x\(b.h)"); continue }
    var total = 0.0, over = 0, maxd = 0
    var diff = [UInt8](repeating: 255, count: a.w * a.h * 4)
    for i in 0..<(a.w * a.h) {
        var worst = 0
        for c in 0..<3 {
            let d = abs(Int(a.px[i * 4 + c]) - Int(b.px[i * 4 + c]))
            total += Double(d); worst = max(worst, d)
        }
        maxd = max(maxd, worst); if worst > 16 { over += 1 }
        let v = UInt8(min(255, worst * 4))
        diff[i * 4] = v; diff[i * 4 + 1] = v; diff[i * 4 + 2] = v
    }
    save(dir.appendingPathComponent("\(t)-frame-diff.png"), a.w, a.h, &diff)
    print(String(format: "%@, %.3f, %.3f, %d", t, total / Double(a.w * a.h * 3), 100.0 * Double(over) / Double(a.w * a.h), maxd))
}

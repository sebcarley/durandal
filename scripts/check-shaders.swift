// Compiles the engine's Metal shader sources (the raw MSL strings in the
// given files) with the Mac's Metal device, as the game does at launch,
// and prints any errors - so a shader mistake is caught without launching
// the game (a failed compile at launch crashes it).
//   xcrun swift scripts/check-shaders.swift <file>...
import Foundation
import Metal

guard let device = MTLCreateSystemDefaultDevice() else { print("no Metal device"); exit(2) }
var failed = false
for path in CommandLine.arguments.dropFirst() {
    guard let text = try? String(contentsOfFile: path, encoding: .utf8) else { print("\(path): unreadable"); failed = true; continue }
    var rest = text[...]
    var index = 0
    while let start = rest.range(of: "R\"MSL(") {
        guard let end = rest.range(of: ")MSL\"", range: start.upperBound..<rest.endIndex) else { break }
        let source = String(rest[start.upperBound..<end.lowerBound])
        let options = MTLCompileOptions()
        options.mathMode = .safe
        do {
            _ = try device.makeLibrary(source: source, options: options)
            print("\(path) #\(index): ok")
        } catch {
            print("\(path) #\(index): \(error.localizedDescription)")
            failed = true
        }
        rest = rest[end.upperBound...]
        index += 1
    }
}
exit(failed ? 1 : 0)

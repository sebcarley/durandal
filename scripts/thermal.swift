// The Mac's thermal pressure as macOS reports it (nominal, fair, serious,
// critical): check it before and after benchmark runs on the fanless Air.
//   xcrun swift scripts/thermal.swift
import Foundation
let names = ["nominal", "fair", "serious", "critical"]
let state = ProcessInfo.processInfo.thermalState.rawValue
print("thermal state: \(names.indices.contains(state) ? names[state] : String(state))")

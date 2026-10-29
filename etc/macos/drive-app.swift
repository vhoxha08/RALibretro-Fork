// Drives a running application for smoke-test.sh: brings it to the front, finds its window, and presses keys in it.
//
// usage: drive-app activate <pid>
//        drive-app window <pid>                   prints the id of its main window (for screencapture -l)
//        drive-app keys <pid> <code:count:delay>...
//
// Keys are macOS virtual key codes (i.e. 53 Escape, 125 Down, 126 Up, 36 Return), each pressed count times with
// delay seconds after each press. Pressing keys needs the Accessibility permission for the terminal running it.
import AppKit
import CoreGraphics

func fail(_ message: String) -> Never {
  FileHandle.standardError.write((message + "\n").data(using: .utf8)!)
  exit(1)
}

let arguments = CommandLine.arguments
guard arguments.count >= 3, let pid = pid_t(arguments[2]) else {
  fail("usage: drive-app activate|window|keys <pid> [code:count:delay...]")
}

switch arguments[1] {
case "activate":
  guard let app = NSRunningApplication(processIdentifier: pid) else { fail("no application with pid \(pid)") }
  app.activate(options: [])

case "window":
  // the largest on-screen window of the process (the main window; dialogs are smaller)
  let windows = CGWindowListCopyWindowInfo([.optionOnScreenOnly, .excludeDesktopElements], kCGNullWindowID)
    as? [[String: Any]] ?? []
  let ownWindows = windows.filter { ($0[kCGWindowOwnerPID as String] as? pid_t) == pid }
  func area(_ window: [String: Any]) -> Double {
    let bounds = window[kCGWindowBounds as String] as? [String: Double] ?? [:]
    return (bounds["Width"] ?? 0) * (bounds["Height"] ?? 0)
  }
  guard let main = ownWindows.max(by: { area($0) < area($1) }),
        let number = main[kCGWindowNumber as String] as? Int else { fail("no window for pid \(pid)") }
  print(number)

case "keys":
  if !CGPreflightPostEventAccess() {
    CGRequestPostEventAccess()
    fail("pressing keys needs the Accessibility permission (System Settings → Privacy & Security → Accessibility)")
  }
  for spec in arguments.dropFirst(3) {
    let parts = spec.split(separator: ":").map(String.init)
    guard parts.count == 3, let code = CGKeyCode(parts[0]), let count = Int(parts[1]), let delay = Double(parts[2])
    else { fail("bad key \(spec): expected code:count:delay") }
    for _ in 0..<count {
      let source = CGEventSource(stateID: .hidSystemState)
      CGEvent(keyboardEventSource: source, virtualKey: code, keyDown: true)?.postToPid(pid)
      usleep(30_000)
      CGEvent(keyboardEventSource: source, virtualKey: code, keyDown: false)?.postToPid(pid)
      usleep(useconds_t(delay * 1_000_000))
    }
  }

default:
  fail("unknown command \(arguments[1])")
}

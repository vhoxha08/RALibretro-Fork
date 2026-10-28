// Makes the macOS application icon (etc/macos/icon.png) from the Windows icon: the artwork on a rounded square
// with a drop shadow, laid out like Apple's icon template (a 824x824 shape centered on a 1024x1024 canvas).
//
// usage: swift etc/macos/make-icon.swift src/retroarch.ico etc/macos/icon.png

import AppKit

let arguments = CommandLine.arguments
guard arguments.count == 3, let source = NSImage(contentsOfFile: arguments[1]) else {
    print("usage: make-icon.swift source.ico target.png")
    exit(1)
}

let canvas = 1024
let shape = CGRect(x: 100, y: 100, width: 824, height: 824)
let cornerRadius: CGFloat = 185

guard let context = CGContext(data: nil, width: canvas, height: canvas, bitsPerComponent: 8, bytesPerRow: 0,
                              space: CGColorSpace(name: CGColorSpace.sRGB)!,
                              bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else {
    exit(1)
}

let path = CGPath(roundedRect: shape, cornerWidth: cornerRadius, cornerHeight: cornerRadius, transform: nil)

// the shape with its shadow
context.saveGState()
context.setShadow(offset: CGSize(width: 0, height: -10), blur: 28, color: CGColor(gray: 0, alpha: 0.35))
context.addPath(path)
context.setFillColor(CGColor(gray: 0.12, alpha: 1))
context.fillPath()
context.restoreGState()

// a dark gradient, lighter at the top
context.saveGState()
context.addPath(path)
context.clip()
let gradient = CGGradient(colorsSpace: CGColorSpace(name: CGColorSpace.sRGB)!,
                          colors: [CGColor(red: 0.20, green: 0.21, blue: 0.24, alpha: 1),
                                   CGColor(red: 0.07, green: 0.07, blue: 0.09, alpha: 1)] as CFArray,
                          locations: [0, 1])!
context.drawLinearGradient(gradient, start: CGPoint(x: 0, y: shape.maxY), end: CGPoint(x: 0, y: shape.minY), options: [])

// the artwork (white on black): screened, so only the white shows
var proposed = CGRect(origin: .zero, size: source.size)
if let artwork = source.cgImage(forProposedRect: &proposed, context: nil, hints: nil) {
    let size: CGFloat = 700
    context.setBlendMode(.screen)
    context.interpolationQuality = .high
    context.draw(artwork, in: CGRect(x: (CGFloat(canvas) - size) / 2, y: (CGFloat(canvas) - size) / 2,
                                     width: size, height: size))
}
context.restoreGState()

guard let image = context.makeImage(),
      let destination = CGImageDestinationCreateWithURL(URL(fileURLWithPath: arguments[2]) as CFURL,
                                                        "public.png" as CFString, 1, nil) else {
    exit(1)
}
CGImageDestinationAddImage(destination, image, nil)
exit(CGImageDestinationFinalize(destination) ? 0 : 1)

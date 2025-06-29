import AlberDriver
import SwiftUI
import MetalKit
import Darwin

final class DrawableSize {
    var width: UInt32 = 0
    var height: UInt32 = 0
    var sizeChanged = false
}

var emulatorLock = NSLock()
var drawableSize = DrawableSize()

class ResizeAwareMTKView: MTKView {
    var onResize: ((CGSize) -> Void)?
    
    override func layoutSubviews() {
        super.layoutSubviews()
        onResize?(self.drawableSize)
    }
}

class DocumentViewController: UIViewController, DocumentDelegate {
    var documentPicker: DocumentPicker!

    override func viewDidLoad() {
        super.viewDidLoad()
        documentPicker = DocumentPicker(presentationController: self, delegate: self)
        /// When the view loads (ie user opens the app) show the file picker
        show()
    }
    
    /// Callback from the document picker
    func didPickDocument(document: Document?) {
        if let pickedDoc = document {
            let fileURL = pickedDoc.fileURL
            
            print("Loading ROM", fileURL)
            emulatorLock.lock()
            print(fileURL.path(percentEncoded: false))
            iosLoadROM(fileURL.path(percentEncoded: false))
            emulatorLock.unlock()
        }
    }

    func show() {
        documentPicker.displayPicker()
    }
}

struct DocumentView: UIViewControllerRepresentable {
    func makeUIViewController(context: Context) -> DocumentViewController {
        DocumentViewController()
    }

    func updateUIViewController(_ uiViewController: DocumentViewController, context: Context) {}
}

struct ContentView: UIViewRepresentable {
    func makeUIView(context: Context) -> ResizeAwareMTKView {
        let mtkView = ResizeAwareMTKView()
        mtkView.preferredFramesPerSecond = 60
        mtkView.enableSetNeedsDisplay = true
        mtkView.isPaused = true
        
        if let metalDevice = MTLCreateSystemDefaultDevice() {
            mtkView.device = metalDevice
        }
        
        mtkView.framebufferOnly = false

        mtkView.onResize = { newDrawableSize in
            let newWidth = UInt32(newDrawableSize.width)
            let newHeight = UInt32(newDrawableSize.height)
            
            emulatorLock.lock()
            if drawableSize.width != newWidth || drawableSize.height != newHeight {
                drawableSize.width = newWidth
                drawableSize.height = newHeight
                drawableSize.sizeChanged = true
            }
            emulatorLock.unlock()
        }

        let dispatchQueue = DispatchQueue(label: "QueueIdentification", qos: .background)
        let metalLayer = mtkView.layer as! CAMetalLayer

        dispatchQueue.async {
            iosCreateEmulator()
            
            while (true) {
                emulatorLock.lock()
                if drawableSize.sizeChanged {
                    drawableSize.sizeChanged = false
                    iosSetOutputSize(drawableSize.width, drawableSize.height)
                }
                
                iosRunFrame(metalLayer)
                emulatorLock.unlock()
            }
        }

        return mtkView
    }
    
    func updateUIView(_ uiView: ResizeAwareMTKView, context: Context) {}
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        DocumentView()
        ContentView()
            .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

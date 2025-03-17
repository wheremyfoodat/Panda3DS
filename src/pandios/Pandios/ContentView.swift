import AlberDriver
import SwiftUI
import MetalKit
import Darwin

var emulatorLock = NSLock()

class DocumentViewController: UIViewController, DocumentDelegate {
    var documentPicker: DocumentPicker!

    override func viewDidLoad() {
        super.viewDidLoad()

        /// set up the document picker
        documentPicker = DocumentPicker(presentationController: self, delegate: self)
        /// When the view loads (ie user opens the app) show the file picker
        show()
    }
    
    /// callback from the document picker
    func didPickDocument(document: Document?) {
        if let pickedDoc = document {
            let fileURL = pickedDoc.fileURL
            
            emulatorLock.lock()
            print("Loading ROM", fileURL)
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
        return DocumentViewController()
    }
    
    func updateUIViewController(_ uiViewController: DocumentViewController, context: Context) {
        // No update needed
    }
}

struct ContentView: UIViewRepresentable {
    @State var showFileImporter = true

    /*
    func makeCoordinator() -> Renderer {
        Renderer(self)
    }
    */
    
    func makeUIView(context: UIViewRepresentableContext<ContentView>) -> MTKView {
        let mtkView = MTKView()
        mtkView.preferredFramesPerSecond = 60
        mtkView.enableSetNeedsDisplay = true
        mtkView.isPaused = true
        
        if let metalDevice = MTLCreateSystemDefaultDevice() {
            mtkView.device = metalDevice
        }
        
        mtkView.framebufferOnly = false
        mtkView.drawableSize = mtkView.frame.size

        let dispatchQueue = DispatchQueue(label: "QueueIdentification", qos: .background)
        let metalLayer = mtkView.layer as! CAMetalLayer;

        dispatchQueue.async{
            iosCreateEmulator()
            
            while (true) {
                emulatorLock.lock()
                iosRunFrame(metalLayer);
                emulatorLock.unlock()
            }
        }

        return mtkView
    }
    
    func updateUIView(_ uiView: MTKView, context: UIViewRepresentableContext<ContentView>) {
        print("Updating MTKView");
    }
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        DocumentView();
        ContentView();
    }
}

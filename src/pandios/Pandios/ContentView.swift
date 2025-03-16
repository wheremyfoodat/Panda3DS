import AlberDriver
import SwiftUI
import MetalKit
import Darwin

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
                iosRunFrame(metalLayer);
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
        ContentView()
    }
}

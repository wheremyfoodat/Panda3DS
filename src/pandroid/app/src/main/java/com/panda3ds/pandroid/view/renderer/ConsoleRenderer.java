package com.panda3ds.pandroid.view.renderer;

import com.panda3ds.pandroid.view.renderer.layout.ConsoleLayout;

public interface ConsoleRenderer {
    void setLayout(ConsoleLayout layout);
    ConsoleLayout getLayout();
    String getBackendName();
}

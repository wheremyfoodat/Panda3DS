package com.panda3ds.pandroid;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import static android.opengl.GLES32.*;

import android.content.res.Resources;
import android.opengl.GLSurfaceView;
import android.util.DisplayMetrics;
import android.util.Log;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

public class PandaGlRenderer implements GLSurfaceView.Renderer {
    int screenWidth, screenHeight;
    int screenTexture;
    public int screenFbo;

    PandaGlRenderer() {
        super();
    }

    @Override
    protected void finalize() throws Throwable {
        if (screenTexture != 0) {
            glDeleteTextures(1, new int[]{screenTexture}, 0);
        }
        if (screenFbo != 0) {
            glDeleteFramebuffers(1, new int[]{screenFbo}, 0);
        }
        super.finalize();
    }
    
    public void onSurfaceCreated(GL10 unused, EGLConfig config) {
        Log.i("pandroid", glGetString(GL_EXTENSIONS));
        Log.w("pandroid", glGetString(GL_VERSION));
        screenWidth = Resources.getSystem().getDisplayMetrics().widthPixels;
        screenHeight = Resources.getSystem().getDisplayMetrics().heightPixels;

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        int[] generateBuffer = new int[1];
        glGenTextures(1, generateBuffer, 0);
        screenTexture = generateBuffer[0];
        glBindTexture(GL_TEXTURE_2D, screenTexture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, screenWidth, screenHeight);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);

        glGenFramebuffers(1, generateBuffer, 0);
        screenFbo = generateBuffer[0];
        glBindFramebuffer(GL_FRAMEBUFFER, screenFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, screenTexture, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        AlberDriver.Initialize();
    }

    public void onDrawFrame(GL10 unused) {
        if (AlberDriver.HasRomLoaded()) {
            AlberDriver.RunFrame(screenFbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, screenFbo);
            glBlitFramebuffer(0, 0, 400, 480, 0, 0, screenWidth, screenHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        }
    }

    public void onSurfaceChanged(GL10 unused, int width, int height) {
        glViewport(0, 0, width, height);
        screenWidth = width;
        screenHeight = height;
        glDisable(GL_SCISSOR_TEST);
    }
}

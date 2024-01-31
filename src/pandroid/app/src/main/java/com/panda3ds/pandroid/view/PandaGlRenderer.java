package com.panda3ds.pandroid.view;

import static android.opengl.GLES32.*;

import android.content.res.Resources;
import android.graphics.Rect;
import android.opengl.GLSurfaceView;
import android.util.Log;
import com.panda3ds.pandroid.AlberDriver;
import com.panda3ds.pandroid.data.config.GlobalConfig;
import com.panda3ds.pandroid.utils.Constants;
import com.panda3ds.pandroid.utils.GameUtils;
import com.panda3ds.pandroid.utils.PerformanceMonitor;
import com.panda3ds.pandroid.view.renderer.ConsoleRenderer;
import com.panda3ds.pandroid.view.renderer.layout.ConsoleLayout;
import com.panda3ds.pandroid.view.renderer.layout.DefaultScreenLayout;
import com.panda3ds.pandroid.data.SMDH;
import com.panda3ds.pandroid.data.game.GameMetadata;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class PandaGlRenderer implements GLSurfaceView.Renderer, ConsoleRenderer {
	private final String romPath;
	private ConsoleLayout displayLayout;
	private int screenWidth, screenHeight;
	private int screenTexture;
	public int screenFbo;

	PandaGlRenderer(String romPath) {
		super();
		this.romPath = romPath;

		screenWidth = Resources.getSystem().getDisplayMetrics().widthPixels;
		screenHeight = Resources.getSystem().getDisplayMetrics().heightPixels;
		setLayout(new DefaultScreenLayout());
	}

	@Override
	protected void finalize() throws Throwable {
		if (screenTexture != 0) {
			glDeleteTextures(1, new int[] {screenTexture}, 0);
		}
		if (screenFbo != 0) {
			glDeleteFramebuffers(1, new int[] {screenFbo}, 0);
		}
		PerformanceMonitor.destroy();
		super.finalize();
	}

	public void onSurfaceCreated(GL10 unused, EGLConfig config) {
		Log.i(Constants.LOG_TAG, glGetString(GL_EXTENSIONS));
		Log.w(Constants.LOG_TAG, glGetString(GL_VERSION));

		int[] version = new int[2];
		glGetIntegerv(GL_MAJOR_VERSION, version, 0);
		glGetIntegerv(GL_MINOR_VERSION, version, 1);

		if (version[0] < 3 || (version[0] == 3 && version[1] < 1)) {
			Log.e(Constants.LOG_TAG, "OpenGL 3.1 or higher is required");
		}

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
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			Log.e(Constants.LOG_TAG, "Framebuffer is not complete");
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		AlberDriver.Initialize();
		AlberDriver.setShaderJitEnabled(GlobalConfig.get(GlobalConfig.KEY_SHADER_JIT));
		AlberDriver.LoadRom(romPath);

		// Load the SMDH
		byte[] smdhData = AlberDriver.GetSmdh();
		if (smdhData.length == 0) {
			Log.w(Constants.LOG_TAG, "Failed to load SMDH");
		} else {
			SMDH smdh = new SMDH(smdhData);
			Log.i(Constants.LOG_TAG, "Loaded rom SDMH");
			Log.i(Constants.LOG_TAG, String.format("Are you playing '%s' published by '%s'", smdh.getTitle(), smdh.getPublisher()));
			GameMetadata game = GameUtils.getCurrentGame();
			GameUtils.removeGame(game);
			GameUtils.addGame(GameMetadata.applySMDH(game, smdh));
		}
		PerformanceMonitor.initialize(getBackendName());
	}

	public void onDrawFrame(GL10 unused) {
		if (AlberDriver.HasRomLoaded()) {
			AlberDriver.RunFrame(screenFbo);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, screenFbo);

			Rect topScreen = displayLayout.getTopDisplayBounds();
			Rect bottomScreen = displayLayout.getBottomDisplayBounds();

			glBlitFramebuffer(
				0, Constants.N3DS_FULL_HEIGHT, Constants.N3DS_WIDTH, Constants.N3DS_HALF_HEIGHT, topScreen.left, screenHeight - topScreen.top,
				topScreen.right, screenHeight - topScreen.bottom, GL_COLOR_BUFFER_BIT, GL_LINEAR
			);

			// Remove the black bars on the bottom screen
			glBlitFramebuffer(
				40, Constants.N3DS_HALF_HEIGHT, Constants.N3DS_WIDTH - 40, 0, bottomScreen.left, screenHeight - bottomScreen.top, bottomScreen.right,
				screenHeight - bottomScreen.bottom, GL_COLOR_BUFFER_BIT, GL_LINEAR
			);
		}
		PerformanceMonitor.runFrame();
	}

	public void onSurfaceChanged(GL10 unused, int width, int height) {
		screenWidth = width;
		screenHeight = height;

		displayLayout.update(screenWidth, screenHeight);
	}

	@Override
	public void setLayout(ConsoleLayout layout) {
		displayLayout = layout;
		displayLayout.setTopDisplaySourceSize(Constants.N3DS_WIDTH, Constants.N3DS_HALF_HEIGHT);
		displayLayout.setBottomDisplaySourceSize(Constants.N3DS_WIDTH - 40 - 40, Constants.N3DS_HALF_HEIGHT);
		displayLayout.update(screenWidth, screenHeight);
	}

	@Override
	public ConsoleLayout getLayout() {
		return displayLayout;
	}

	@Override
	public String getBackendName() {
		return "OpenGL";
	}
}
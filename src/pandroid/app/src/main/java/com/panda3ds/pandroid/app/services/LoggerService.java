package com.panda3ds.pandroid.app.services;

import android.app.Service;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.os.Build;
import android.os.IBinder;
import android.util.Log;

import androidx.annotation.Nullable;

import com.panda3ds.pandroid.lang.PipeStreamTask;
import com.panda3ds.pandroid.lang.Task;
import com.panda3ds.pandroid.utils.Constants;
import com.panda3ds.pandroid.utils.FileUtils;

import java.io.OutputStream;
import java.util.Arrays;

public class LoggerService extends Service {
    private static final long MAX_LOG_SIZE = 1024 * 1024 * 4; // 4MB

    private PipeStreamTask errorTask;
    private PipeStreamTask outputTask;
    private Process logcat;

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        try {
            Runtime.getRuntime().exec(new String[]{"logcat", "-c"}).waitFor();
            logcat = Runtime.getRuntime().exec(new String[]{"logcat"});
            String logPath = getExternalMediaDirs()[0].getAbsolutePath();
            FileUtils.createDir(logPath, "logs");
            logPath = logPath + "/logs";

            if (FileUtils.exists(logPath + "/last.txt")) {
                FileUtils.delete(logPath + "/last.txt");
            }

            if (FileUtils.exists(logPath + "/current.txt")) {
                FileUtils.rename(logPath + "/current.txt", "last.txt");
            }

            OutputStream stream = FileUtils.getOutputStream(logPath + "/current.txt");
            errorTask = new PipeStreamTask(logcat.getErrorStream(), stream, MAX_LOG_SIZE);
            outputTask = new PipeStreamTask(logcat.getInputStream(), stream, MAX_LOG_SIZE);

            errorTask.start();
            outputTask.start();

            Log.i(Constants.LOG_TAG, "STARTED LOGGER SERVICE");
            printDeviceAbout();
        } catch (Exception e) {
            stopSelf();
            Log.e(Constants.LOG_TAG, "Failed to start LoggerService!");
        }
    }

    private void printDeviceAbout() {
        Log.i(Constants.LOG_TAG, "----------------------");
        Log.i(Constants.LOG_TAG, "Android SDK: " + Build.VERSION.SDK_INT);
        Log.i(Constants.LOG_TAG, "Device: " + Build.DEVICE);
        Log.i(Constants.LOG_TAG, "Model: " + Build.MANUFACTURER + " " + Build.MODEL);
        Log.i(Constants.LOG_TAG, "ABIs: " + Arrays.toString(Build.SUPPORTED_ABIS));
        try {
            PackageInfo info = getPackageManager().getPackageInfo(getPackageName(), 0);
            Log.i(Constants.LOG_TAG, "");
            Log.i(Constants.LOG_TAG, "Package: " + info.packageName);
            Log.i(Constants.LOG_TAG, "Install location: " + info.installLocation);
            Log.i(Constants.LOG_TAG, "App version: " + info.versionName + " (" + info.versionCode + ")");
        } catch (Exception e) {
            Log.e(Constants.LOG_TAG, "Error on obtain package info: " + e);
        }
        Log.i(Constants.LOG_TAG, "----------------------");
    }

    @Override
    public void onTaskRemoved(Intent rootIntent) {
        stopSelf();
        //This is a time for app save save log file
        try {
            Thread.sleep(1000);
        } catch (Exception e) {}
        super.onTaskRemoved(rootIntent);
    }

    @Override
    public void onDestroy() {
        Log.i(Constants.LOG_TAG, "FINISHED LOGGER SERVICE");
        errorTask.close();
        outputTask.close();
        try {
            logcat.destroy();
        } catch (Throwable t) {}
        super.onDestroy();
    }
}

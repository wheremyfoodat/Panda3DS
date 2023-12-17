package com.panda3ds.pandroid.utils;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import androidx.documentfile.provider.DocumentFile;

import com.panda3ds.pandroid.app.PandroidApplication;

public class FileUtils {

    public static final String MODE_READ = "r";

    private static Uri parseUri(String value) {
        return Uri.parse(value);
    }

    private static Context getContext() {
        return PandroidApplication.getAppContext();
    }

    public static String getName(String path) {
        DocumentFile file = DocumentFile.fromSingleUri(getContext(), parseUri(path));
        return file.getName();
    }

    public static long getSize(String path) {
        return DocumentFile.fromSingleUri(getContext(), parseUri(path)).length();
    }


    public static String getCacheDir() {
        return getContext().getCacheDir().getAbsolutePath();
    }

    public static void makeUriPermanent(String uri, String mode) {

        int flags = Intent.FLAG_GRANT_READ_URI_PERMISSION;
        if (mode.toLowerCase().contains("w"))
            flags &= Intent.FLAG_GRANT_WRITE_URI_PERMISSION;

        getContext().getContentResolver().takePersistableUriPermission(parseUri(uri), flags);
    }

    public static int openContentUri(String path, String mode) {
        try {
            Uri uri = parseUri(path);
            ParcelFileDescriptor descriptor = getContext().getContentResolver().openFileDescriptor(uri, mode);
            int fd = descriptor.getFd();
            descriptor.detachFd();
            descriptor.close();
            return fd;
        } catch (Exception e) {
            Log.e(Constants.LOG_TAG, "Error on openContentUri: " + e);
        }

        return -1;
    }
    
}

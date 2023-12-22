package com.panda3ds.pandroid.utils;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.util.Log;

import androidx.documentfile.provider.DocumentFile;

import com.panda3ds.pandroid.app.PandroidApplication;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;

public class FileUtils {
    public static final String MODE_READ = "r";

    private static DocumentFile parseFile(String path) {
        if (path.startsWith("/")) {
            return DocumentFile.fromFile(new File(path));
        }
        Uri uri = Uri.parse(path);
        return DocumentFile.fromSingleUri(getContext(), uri);
    }

    private static Context getContext() {
        return PandroidApplication.getAppContext();
    }

    public static String getName(String path) {
        return parseFile(path).getName();
    }

    public static String getPrivatePath() {
        File file = getContext().getFilesDir();
        if (!file.exists()) {
            file.mkdirs();
        }
        return file.getAbsolutePath();
    }

    public static String getConfigPath() {
        File file = new File(getPrivatePath(), "config");
        if (!file.exists()) {
            file.mkdirs();
        }
        return file.getAbsolutePath();
    }

    public static boolean exists(String path) {
        return parseFile(path).exists();
    }

    public static boolean createDir(String path, String name) {
        DocumentFile folder = parseFile(path);
        if (folder.findFile(name) != null)
            return true;
        return folder.createDirectory(name) != null;
    }

    public static boolean createFile(String path, String name) {
        DocumentFile folder = parseFile(path);
        if (folder.findFile(name) != null)
            return true;
        return folder.createFile("", name) != null;
    }

    public static boolean writeTextFile(String path, String name, String content) {
        try {
            if (!exists(path + "/" + name)) {
                createFile(path, name);
            }
            OutputStream stream = getOutputStream(path + "/" + name);
            stream.write(content.getBytes(StandardCharsets.UTF_8));
            stream.flush();
            stream.close();
        } catch (Exception e) {
            Log.e(Constants.LOG_TAG, "Error on write text file: ", e);
            return false;
        }
        return true;
    }

    public static String readTextFile(String path) {
        if (!exists(path))
            return null;

        try {
            InputStream stream = getInputStream(path);
            ByteArrayOutputStream output = new ByteArrayOutputStream();

            int len;
            byte[] buffer = new byte[1024 * 8];
            while ((len = stream.read(buffer)) != -1)
                output.write(buffer, 0, len);

            stream.close();
            output.flush();
            output.close();

            byte[] data = output.toByteArray();

            return new String(data, 0, data.length);
        } catch (Exception e) {
            return null;
        }
    }

    public static InputStream getInputStream(String path) throws FileNotFoundException {
        return getContext().getContentResolver().openInputStream(parseFile(path).getUri());
    }

    public static OutputStream getOutputStream(String path) throws FileNotFoundException {
        return getContext().getContentResolver().openOutputStream(parseFile(path).getUri());
    }

    public static void makeUriPermanent(String uri, String mode) {
        int flags = Intent.FLAG_GRANT_READ_URI_PERMISSION;
        if (mode.toLowerCase().contains("w"))
            flags &= Intent.FLAG_GRANT_WRITE_URI_PERMISSION;

        getContext().getContentResolver().takePersistableUriPermission(Uri.parse(uri), flags);
    }
}

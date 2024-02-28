package com.panda3ds.pandroid.utils;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.system.Os;
import android.util.Log;

import androidx.documentfile.provider.DocumentFile;

import com.panda3ds.pandroid.app.PandroidApplication;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.Objects;

public class FileUtils {
    public static final String MODE_READ = "r";
    public static final int CANONICAL_SEARCH_DEEP = 8;

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

    public static String getResourcesPath() {
        File file = new File(getPrivatePath(), "config/resources");
        if (!file.exists()) {
            file.mkdirs();
        }

        return file.getAbsolutePath();
    }

    public static String getResourcePath(String name) {
        File file = new File(getResourcesPath(), name);
        file.mkdirs();

        return file.getAbsolutePath();
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

    public static String parseNativeMode(String mode) {
        mode = mode.toLowerCase();
        switch (mode) {
            case "r":
            case "rb":
                return "r";
            case "r+":
            case "r+b":
            case "rb+":
                return "rw";
            case "w+":
                return "rwt";
            case "w":
            case "wb":
                return "wt";
            case "wa":
                return "wa";
        }

        throw new IllegalArgumentException("Invalid file mode: "+mode);
    }

    public static boolean exists(String path) {
        return parseFile(path).exists();
    }

    public static void rename(String path, String newName) {
        parseFile(path).renameTo(newName);
    }

    public static void delete(String path) {
        DocumentFile file = parseFile(path);

        if (file.exists()) {
            if (file.isDirectory()) {
                String[] children = listFiles(path);
                for (String child : children) {
                    delete(path + "/" + child);
                }
            }

            file.delete();
        }
    }

    public static boolean createDir(String path, String name) {
        DocumentFile folder = parseFile(path);
        if (folder.findFile(name) != null) {
            return true;
        }
    
        return folder.createDirectory(name) != null;
    }

    public static boolean createFile(String path, String name) {
        DocumentFile folder = parseFile(path);
        if (folder.findFile(name) != null) {
            folder.findFile(name).delete();
        }

        return folder.createFile("", name) != null;
    }

    public static boolean writeTextFile(String path, String name, String content) {
        try {
            createFile(path, name);
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
        if (!exists(path)) {
            return null;
        }

        try {
            InputStream stream = getInputStream(path);
            ByteArrayOutputStream output = new ByteArrayOutputStream();

            int len;
            byte[] buffer = new byte[1024 * 8];
            while ((len = stream.read(buffer)) != -1) {
                output.write(buffer, 0, len);
            }

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
        if (mode.toLowerCase().contains("w")) {
            flags &= Intent.FLAG_GRANT_WRITE_URI_PERMISSION;
        }

        getContext().getContentResolver().takePersistableUriPermission(Uri.parse(uri), flags);
    }

    /**
     * When call ContentProvider.openFileDescriptor() android opens a file descriptor
     * on app process in /proc/self/fd/[file descriptor id] this is a link to real file path
     * can use File.getCanonicalPath() for get a link origin, but in some android version
     * need use Os.readlink(path) to get a real path.
     */
    public static String obtainRealPath(String uri) {
        try {
            ParcelFileDescriptor parcelDescriptor = getContext().getContentResolver().openFileDescriptor(Uri.parse(uri), "r");
            int fd = parcelDescriptor.getFd();
            File file = new File("/proc/self/fd/" + fd).getAbsoluteFile();
        
            for (int i = 0; i < CANONICAL_SEARCH_DEEP; i++) {
                try {
                    String canonical = file.getCanonicalPath();
                    if (!Objects.equals(canonical, file.getAbsolutePath())) {
                        file = new File(canonical).getAbsoluteFile();
                    }
                } catch (Exception x) {
                    break;
                }
            }

            if (!file.getAbsolutePath().startsWith("/proc/self/")) {
                parcelDescriptor.close();
                return file.getAbsolutePath();
            }

            String path = Os.readlink(file.getAbsolutePath());
            parcelDescriptor.close();

            if (new File(path).exists()) {
                return path;
            }
    
            return null;
        } catch (Exception e) {
            return null;
        }
    }

    public static void updateFile(String path) {
        DocumentFile file = parseFile(path);
        Uri uri = file.getUri();
        
        switch (uri.getScheme()) {
            case "file": {
                new File(uri.getPath()).setLastModified(System.currentTimeMillis());
                break;
            }

            case "content": {
                getContext().getContentResolver().update(uri, null, null, null);
                break;
            }

            default: {
                Log.w(Constants.LOG_TAG, "Cannot update file from scheme: " + uri.getScheme());
                break;
            }
        }
    }

    public static long getLastModified(String path) {
        return parseFile(path).lastModified();
    }

    public static String[] listFiles(String path) {
        DocumentFile folder = parseFile(path);
        DocumentFile[] files = folder.listFiles();

        String[] result = new String[files.length];
        for (int i = 0; i < result.length; i++) {
            result[i] = files[i].getName();
        }
        
        return result;
    }

    public static Uri obtainUri(String path) {
        return parseFile(path).getUri();
    }

    public static String extension(String uri) {
        String name = getName(uri);
        if (!name.contains(".")) {
            return name.toLowerCase();
        }
        String[] parts = name.split("\\.");
        
        return parts[parts.length-1].toLowerCase();
    }

    public static boolean copyFile(String source, String path, String name) {
        try {
            String fullPath = path + "/" + name;
            if (!FileUtils.exists(fullPath)) {
                FileUtils.delete(fullPath);
            }
            FileUtils.createFile(path, name);
            InputStream in = getInputStream(source);
            OutputStream out = getOutputStream(fullPath);
            byte[] buffer = new byte[1024 * 128]; //128 KB
            int length;
            while ((length = in.read(buffer)) != -1) {
                out.write(buffer, 0, length);
            }
            out.flush();
            out.close();
            in.close();
        } catch (Exception e) {
            Log.e(Constants.LOG_TAG, "ERROR ON COPY FILE", e);
            return false;
        }
        return true;
    }
}

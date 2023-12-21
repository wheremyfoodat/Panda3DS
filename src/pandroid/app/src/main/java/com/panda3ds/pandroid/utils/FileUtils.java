package com.panda3ds.pandroid.utils;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import androidx.documentfile.provider.DocumentFile;

import com.panda3ds.pandroid.app.PandroidApplication;

import java.io.File;

public class FileUtils {
    public static final String MODE_READ = "r";
    private static DocumentFile parseFile(String path){
        if (path.startsWith("/")){
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

    public static boolean createFolder(String path, String name){
        DocumentFile folder = parseFile(path);

        if (folder.findFile(name) != null){
            return true;
        }

        return folder.createDirectory(name) != null;
    }

    public static String getPrivatePath(){
        return getContext().getFilesDir().getAbsolutePath();
    }

    public static void makeUriPermanent(String uri, String mode) {

        int flags = Intent.FLAG_GRANT_READ_URI_PERMISSION;
        if (mode.toLowerCase().contains("w"))
            flags &= Intent.FLAG_GRANT_WRITE_URI_PERMISSION;

        getContext().getContentResolver().takePersistableUriPermission(Uri.parse(uri), flags);
    }
}

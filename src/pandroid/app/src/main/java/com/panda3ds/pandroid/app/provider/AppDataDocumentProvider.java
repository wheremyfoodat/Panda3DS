package com.panda3ds.pandroid.app.provider;

import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract.Document;
import android.provider.DocumentsContract.Root;
import android.provider.DocumentsProvider;

import androidx.annotation.Nullable;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.PandroidApplication;
import com.panda3ds.pandroid.utils.FileUtils;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Objects;

public class AppDataDocumentProvider extends DocumentsProvider {
    private static final String ROOT_ID = "root";
    private static final String[] DEFAULT_ROOT_PROJECTION = new String[]{
            Root.COLUMN_ROOT_ID,
            Root.COLUMN_MIME_TYPES,
            Root.COLUMN_FLAGS,
            Root.COLUMN_ICON,
            Root.COLUMN_TITLE,
            Root.COLUMN_SUMMARY,
            Root.COLUMN_DOCUMENT_ID,
            Root.COLUMN_AVAILABLE_BYTES
    };

    private static final String[] DEFAULT_DOCUMENT_PROJECTION = new String[]{
            Document.COLUMN_DOCUMENT_ID,
            Document.COLUMN_DISPLAY_NAME,
            Document.COLUMN_MIME_TYPE,
            Document.COLUMN_LAST_MODIFIED,
            Document.COLUMN_FLAGS,
            Document.COLUMN_SIZE
    };

    private String obtainDocumentId(File file) {
        String basePath = baseDirectory().getAbsolutePath();
        String fullPath = file.getAbsolutePath();
        return (ROOT_ID + "/" + fullPath.substring(basePath.length())).replaceAll("//", "/");
    }

    private File obtainFile(String documentId) {
        if (documentId.startsWith(ROOT_ID)) {
            return new File(baseDirectory(), documentId.substring(ROOT_ID.length()));
        }
        throw new IllegalArgumentException("Invalid document id: " + documentId);
    }

    private Context context() {
        return PandroidApplication.getAppContext();
    }

    private File baseDirectory() {
        return context().getFilesDir();
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public Cursor queryRoots(String[] projection) throws FileNotFoundException {
        MatrixCursor cursor = new MatrixCursor(projection == null ? DEFAULT_ROOT_PROJECTION : projection);
        cursor.newRow()
                .add(Root.COLUMN_ROOT_ID, ROOT_ID)
                .add(Root.COLUMN_SUMMARY, null)
                .add(Root.COLUMN_FLAGS, Root.FLAG_SUPPORTS_IS_CHILD | Root.FLAG_SUPPORTS_CREATE)
                .add(Root.COLUMN_DOCUMENT_ID, ROOT_ID + "/")
                .add(Root.COLUMN_AVAILABLE_BYTES, baseDirectory().getFreeSpace())
                .add(Root.COLUMN_TITLE, context().getString(R.string.app_name))
                .add(Root.COLUMN_MIME_TYPES, "*/*")
                .add(Root.COLUMN_ICON, R.mipmap.ic_launcher);

        return cursor;
    }

    @Override
    public Cursor queryDocument(String documentId, String[] projection) throws FileNotFoundException {
        File file = obtainFile(documentId);
        MatrixCursor cursor = new MatrixCursor(projection == null ? DEFAULT_DOCUMENT_PROJECTION : projection);
        includeFile(cursor, file);
        
        return cursor;
    }

    private void includeFile(MatrixCursor cursor, File file) {
        int flags = 0;
        if (file.isDirectory()) {
            flags = Document.FLAG_DIR_SUPPORTS_CREATE | Document.FLAG_SUPPORTS_DELETE;
        } else {
            flags = Document.FLAG_SUPPORTS_WRITE | Document.FLAG_SUPPORTS_REMOVE | Document.FLAG_SUPPORTS_DELETE;
        }
        cursor.newRow()
                .add(Document.COLUMN_DOCUMENT_ID, obtainDocumentId(file))
                .add(Document.COLUMN_MIME_TYPE, file.isDirectory() ? Document.MIME_TYPE_DIR : "application/octet-stream")
                .add(Document.COLUMN_FLAGS, flags)
                .add(Document.COLUMN_LAST_MODIFIED, file.lastModified())
                .add(Document.COLUMN_DISPLAY_NAME, file.getName())
                .add(Document.COLUMN_SIZE, file.length());

    }

    @Override
    public Cursor queryChildDocuments(String parentDocumentId, String[] projection, String sortOrder) throws FileNotFoundException {
        File file = obtainFile(parentDocumentId);
        MatrixCursor cursor = new MatrixCursor(projection == null ? DEFAULT_DOCUMENT_PROJECTION : projection);
        File[] children = file.listFiles();
        if (children != null) {
            for (File child : children) {
                includeFile(cursor, child);
            }
        }

        return cursor;
    }

    @Override
    public String createDocument(String parentDocumentId, String mimeType, String displayName) throws FileNotFoundException {
        File parent = obtainFile(parentDocumentId);
        File file = new File(parent, displayName);
        if (!parent.exists()) {
            throw new FileNotFoundException("Parent doesn't exist");
        }

        if (Objects.equals(mimeType, Document.MIME_TYPE_DIR)) {
            if (!file.mkdirs()) {
                throw new FileNotFoundException("Error while creating directory");
            }
        } else {
            try {
                if (!file.createNewFile()) {
                    throw new Exception("Error while creating file");
                }
            } catch (Exception e) {
                throw new FileNotFoundException(e.getMessage());
            }
        }
        return obtainDocumentId(file);
    }

    @Override
    public void deleteDocument(String documentId) throws FileNotFoundException {
        File file = obtainFile(documentId);
        if (file.exists()) {
            FileUtils.delete(file.getAbsolutePath());
        } else {
            throw new FileNotFoundException("File not exists");
        }
    }

    @Override
    public ParcelFileDescriptor openDocument(String documentId, String mode, @Nullable CancellationSignal signal) throws FileNotFoundException {
        return ParcelFileDescriptor.open(obtainFile(documentId), ParcelFileDescriptor.parseMode(mode));
    }
}

package com.panda3ds.pandroid.view.code.syntax;

public abstract class CodeSyntax {
    public abstract void apply(byte[] syntaxBuffer, final CharSequence text);

    // Get syntax highlighting data for a file based on its filename, by looking at the extension
    public static CodeSyntax getFromFilename(String name) {
        name = name.trim().toLowerCase();
        String[] parts = name.split("\\.");
        if (parts.length == 0)
            return null;

        // Get syntax based on file extension
        switch (parts[parts.length - 1]) {
            case "lua":
                return new LuaSyntax();
            default:
                return null;
        }
    }
}

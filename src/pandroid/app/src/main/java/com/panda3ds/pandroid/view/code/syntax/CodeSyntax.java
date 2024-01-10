package com.panda3ds.pandroid.view.code.syntax;

public abstract class CodeSyntax {
    public abstract void apply(byte[] syntaxBuffer, final CharSequence text);

    public static CodeSyntax obtainByFileName(String name) {
        name = name.trim().toLowerCase();
        String[] parts = name.split("\\.");
        if (parts.length == 0)
            return null;
        switch (parts[parts.length - 1]) {
            case "lua":
                return new LuaSyntax();
            default:
                return null;
        }
    }
}

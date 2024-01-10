package com.panda3ds.pandroid.view.code.syntax;

import com.panda3ds.pandroid.view.code.EditorColors;

import java.util.Arrays;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

class LuaSyntax extends CodeSyntax {
    public static final Pattern comment = Pattern.compile("(\\-\\-.*)");

    public static final Pattern keywords = PatternUtils.buildGenericKeywords(
            "and", "break", "do", "else", "elseif", "end", "false", "for", "function", "if", "in",
            "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while");

    public static final Pattern identifiers = PatternUtils.buildGenericKeywords(
            "assert", "collectgarbage", "dofile", "error", "getmetatable", "ipairs", "loadfile", "load", "loadstring", "next", "pairs", "pcall", "print", "rawequal", "rawlen", "rawget", "rawset",
            "select", "setmetatable", "tonumber", "tostring", "type", "xpcall", "_G", "_VERSION", "arshift", "band", "bnot", "bor", "bxor", "btest", "extract", "lrotate", "lshift", "replace",
            "rrotate", "rshift", "create", "resume", "running", "status", "wrap", "yield", "isyieldable", "debug", "getuservalue", "gethook", "getinfo", "getlocal", "getregistry", "getmetatable",
            "getupvalue", "upvaluejoin", "upvalueid", "setuservalue", "sethook", "setlocal", "setmetatable", "setupvalue", "traceback", "close", "flush", "input", "lines", "open", "output", "popen",
            "read", "tmpfile", "type", "write", "close", "flush", "lines", "read", "seek", "setvbuf", "write", "__gc", "__tostring", "abs", "acos", "asin", "atan", "ceil", "cos", "deg", "exp", "tointeger",
            "floor", "fmod", "ult", "log", "max", "min", "modf", "rad", "random", "randomseed", "sin", "sqrt", "string", "tan", "type", "atan2", "cosh", "sinh", "tanh",
            "pow", "frexp", "ldexp", "log10", "pi", "huge", "maxinteger", "mininteger", "loadlib", "searchpath", "seeall", "preload", "cpath", "path", "searchers", "loaded", "module", "require", "clock",
            "date", "difftime", "execute", "exit", "getenv", "remove", "rename", "setlocale", "time", "tmpname", "byte", "char", "dump", "find", "format", "gmatch", "gsub", "len", "lower", "match", "rep",
            "reverse", "sub", "upper", "pack", "packsize", "unpack", "concat", "maxn", "insert", "pack", "unpack", "remove", "move", "sort", "offset", "codepoint", "char", "len", "codes", "charpattern",
            "coroutine", "table", "io", "os", "string", "uint8_t", "bit32", "math", "debug", "package");

    public static final Pattern string = Pattern.compile("((\")(.*?)([^\\\\]\"))|((\")(.+))|((')(.?)('))");
    public static final Pattern symbols = Pattern.compile("([.!&?:;*+/{}()\\]\\[,=-])");
    public static final Pattern numbers = Pattern.compile("\\b((\\d*[.]?\\d+([Ee][+-]?[\\d]+)?[LlfFdD]?)|(0[xX][0-9a-zA-Z]+)|(0[bB][0-1]+)|(0[0-7]+))\\b");


    @Override
    public void apply(byte[] syntaxBuffer, CharSequence text) {
        for (Matcher matcher = keywords.matcher(text); matcher.find(); ) {
            Arrays.fill(syntaxBuffer, matcher.start(), matcher.end(), EditorColors.COLOR_KEYWORDS);
        }

        for (Matcher matcher = identifiers.matcher(text); matcher.find(); ) {
            Arrays.fill(syntaxBuffer, matcher.start(), matcher.end(), EditorColors.COLOR_FIELDS);
        }

        for (Matcher matcher = symbols.matcher(text); matcher.find(); ) {
            Arrays.fill(syntaxBuffer, matcher.start(), matcher.end(), EditorColors.COLOR_SYMBOLS);
        }

        for (Matcher matcher = numbers.matcher(text); matcher.find(); ) {
            Arrays.fill(syntaxBuffer, matcher.start(), matcher.end(), EditorColors.COLOR_NUMBERS);
        }

        for (Matcher matcher = string.matcher(text); matcher.find(); ) {
            Arrays.fill(syntaxBuffer, matcher.start(), matcher.end(), EditorColors.COLOR_STRING);
        }

        for (Matcher matcher = comment.matcher(text); matcher.find(); ) {
            Arrays.fill(syntaxBuffer, matcher.start(), matcher.end(), EditorColors.COLOR_COMMENT);
        }
    }
}

# Pandroid Proguard Rules
# For more details, see
#   http://developer.android.com/guide/developing/tools/proguard.html

# Keep all JNI and C++ related classes and methods
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep all native libraries and their methods
-keep class * {
    native <methods>;
}

# Keep all classes in the specified package and its subpackages
-keep class com.panda3ds.pandroid.** {*;}

# Uncomment this to preserve the line number information for
# debugging stack traces.
#-keepattributes SourceFile,LineNumberTable

# If you keep the line number information, uncomment this to
# hide the original source file name.
#-renamesourcefileattribute SourceFile

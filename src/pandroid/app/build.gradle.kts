plugins {
    id("com.android.application")
}

android {
    namespace = "com.panda3ds.pandroid"
    compileSdk = 33

    defaultConfig {
        applicationId = "com.panda3ds.pandroid"
        minSdk = 24
        targetSdk = 33
        versionCode = getVersionCode()
        versionName = getVersionName()

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        ndk {
            abiFilters += listOf("x86_64", "arm64-v8a")
        }
    }

    buildTypes {
        getByName("release") {
            isMinifyEnabled = true
            isShrinkResources = true
            isDebuggable = false
            signingConfig = signingConfigs.getByName("debug")
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
        getByName("debug") {
            isMinifyEnabled = false
            isShrinkResources = false
            isDebuggable = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }
}

dependencies {
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("com.google.android.material:material:1.8.0")
    implementation("androidx.preference:preference:1.2.1")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    implementation("com.google.code.gson:gson:2.10.1")
}

fun getVersionName(): String {
    var tag = "1.0"
    try {
        val process = Runtime.getRuntime().exec("git describe --tags --abbrev=0")
        tag = process.inputStream.bufferedReader().readText().trim()
        if (tag.startsWith("v")) {
            tag = tag.substring(1)
        }
    } catch (e: Exception) {
        println("Failed to get latest Git tag: ${e.message}")
    }
    return tag
}

fun getVersionCode(): Int {
    var versionCode = 1
    val tag = getVersionName()
    if (tag.isNotEmpty() && tag[0].isDigit()) {
        versionCode = tag[0].toString().toInt()
    }
    return versionCode
}

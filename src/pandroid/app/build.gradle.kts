import java.lang.ProcessBuilder.Redirect

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
        versionCode = getGitVersionCode()
        versionName = getGitVersionName()

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        ndk {
            abiFilters += listOf("x86_64", "arm64-v8a")
        }
    }

    buildTypes {
        getByName("release") {
            isMinifyEnabled = false
            isShrinkResources = false
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

/**
 * Returns the version name based on the current git state
 * If HEAD is a tag, the tag name is used as the version name
 * e.g. `1.0.0`
 * If HEAD is not a tag, the tag name, the branch name and the short commit hash are used
 * e.g. `1.0.0-master-ab00cd11`
 */
fun getGitVersionName(): String {
    var versionName = "0.0.0"

    try {
        val process = ProcessBuilder("git", "describe", "--abbrev=0")
            .directory(project.rootDir)
            .redirectOutput(Redirect.PIPE)
            .start()
        val tag = process.inputStream.bufferedReader().readText().trim()
        if (!tag.isEmpty())
            versionName = tag

        versionName += "-" + getGitBranch() + "-" + getGitShortHash()
    } catch (e: Exception) {
        logger.quiet("$e: defaulting to dummy version number $versionName")
    }

    logger.quiet("Version name: $versionName")
    return versionName
}


/**
 * Returns the number of commits until the last tag
 */
fun getGitVersionCode(): Int {
    var versionCode = 1

    try {
        val process = ProcessBuilder("git", "rev-list", "--first-parent", "--count", "--tags")
            .directory(project.rootDir)
            .redirectOutput(Redirect.PIPE)
            .start()
        val output = process.inputStream.bufferedReader().readText().toInt()
        versionCode = maxOf(output, versionCode)
    } catch (e: Exception) {
        logger.error("$e: defaulting to dummy version code $versionCode")
    }

    logger.quiet("Version code: $versionCode")
    return versionCode
}

/**
 * Returns the short commit hash
 */
fun getGitShortHash(): String {
    var gitHash = "0"

    try {
        val process = ProcessBuilder("git", "rev-parse", "--short", "HEAD")
            .directory(project.rootDir)
            .redirectOutput(Redirect.PIPE)
            .start()
        gitHash = process.inputStream.bufferedReader().readText().trim()
    } catch (e: Exception) {
        logger.error("$e: defaulting to dummy build hash $gitHash")
    }

    return gitHash
}

/**
 * Returns the current branch name
 */
fun getGitBranch(): String {
    var branch = "unk"

    try {
        val process = ProcessBuilder("git", "rev-parse", "--abbrev-ref", "HEAD")
            .directory(project.rootDir)
            .redirectOutput(Redirect.PIPE)
            .start()
        branch = process.inputStream.bufferedReader().readText().trim()
    } catch (e: Exception) {
        logger.error("$e: defaulting to dummy branch $branch")
    }

    return branch
}

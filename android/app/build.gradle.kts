plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

val spotifyClientId: String = providers.environmentVariable("SPOTIFY_CLIENT_ID").orNull
    ?.takeIf { it.isNotBlank() }
    ?: providers.gradleProperty("spotifyClientId").getOrElse("")

android {
    namespace = "com.theblitzteam.blitzdj"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.theblitzteam.blitzdj"
        minSdk = 24
        targetSdk = 35
        versionCode = 3
        versionName = "0.3.0"

        buildConfigField("String", "SPOTIFY_CLIENT_ID", "\"$spotifyClientId\"")

        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a")
        }

        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DANDROID_STL=c++_static",
                    "-DBLITZDJ_ENABLE_BLUETOOTH=OFF",
                    "-DBLITZDJ_BUILD_CLI=OFF",
                    "-DBLITZDJ_INSTALL_ASSETS=OFF"
                )
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../../CMakeLists.txt")
        }
    }

    buildFeatures {
        buildConfig = true
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            signingConfig = signingConfigs.getByName("debug")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    lint {
        abortOnError = false
        checkReleaseBuilds = false
    }

    packaging {
        jniLibs {
            useLegacyPackaging = false
        }
    }
}

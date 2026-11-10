import javax.xml.parsers.DocumentBuilderFactory

plugins { id("com.android.application"); id("org.jetbrains.kotlin.plugin.compose") }
val versionDocument=DocumentBuilderFactory.newInstance().apply {
    setFeature("http://apache.org/xml/features/disallow-doctype-decl",true)
}.newDocumentBuilder().parse(rootProject.file("../../Directory.Build.props"))
val camberVersion=versionDocument.getElementsByTagName("Version").item(0).textContent
val camberCode=versionDocument.getElementsByTagName("AndroidVersionCode").item(0).textContent.toInt()
if(providers.gradleProperty("camberBundle").isPresent)layout.buildDirectory.set(file("build-bundle"))
android {
    namespace = "camera.camber"
    compileSdk = 36
    defaultConfig { applicationId = "camera.camber"; minSdk = 29; targetSdk = 36; versionCode = camberCode; versionName = camberVersion }
    val releaseStore=System.getenv("CAMBER_ANDROID_KEYSTORE")
    signingConfigs {
        if(releaseStore!=null) create("beta") {
            storeFile=file(releaseStore);storePassword=System.getenv("CAMBER_ANDROID_STORE_PASSWORD")
            keyAlias=System.getenv("CAMBER_ANDROID_KEY_ALIAS");keyPassword=System.getenv("CAMBER_ANDROID_KEY_PASSWORD")
        }
    }
    buildTypes { getByName("release") {
        isDebuggable=false;isMinifyEnabled=true;isShrinkResources=true
        proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"),"proguard-rules.pro")
        if(releaseStore!=null) signingConfig=signingConfigs.getByName("beta")
    }
    create("internal") {
        initWith(getByName("release"))
        signingConfig=signingConfigs.getByName("debug")
        matchingFallbacks.add("release")
    } }
    splits { abi { isEnable=!providers.gradleProperty("camberBundle").isPresent;reset();include("armeabi-v7a","arm64-v8a","x86","x86_64");isUniversalApk=true } }
    buildFeatures { compose = true }
    androidResources { noCompress += listOf("task", "tflite") }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
}
dependencies {
    implementation(platform("androidx.compose:compose-bom:2025.04.01"))
    implementation("androidx.activity:activity-compose:1.10.1")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.9.0")
    implementation("io.github.webrtc-sdk:android:150.7871.04")
    implementation("com.google.mediapipe:tasks-vision:0.10.35")
    // Replace vulnerable MediaPipe transitives; keep Android-specific Guava.
    implementation("com.google.guava:guava:33.7.2-android")
    implementation("com.google.protobuf:protobuf-javalite:4.28.2")
    implementation("com.squareup.okhttp3:okhttp:4.12.0")
    implementation("com.journeyapps:zxing-android-embedded:4.3.0")
    testImplementation("junit:junit:4.13.2")
}

// AGP 9 defaults to testing only the debug variant; beta verifies both.
androidComponents { beforeVariants(selector().all()) { variant ->
    variant.hostTests[com.android.build.api.variant.HostTestBuilder.UNIT_TEST_TYPE]?.enable = true
} }
dependencyLocking { lockAllConfigurations() }

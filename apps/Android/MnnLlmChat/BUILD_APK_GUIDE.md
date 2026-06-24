# 构建 APK 指南

本文档说明如何在开发完成后构建 MnnLlmChat 的 APK 安装包。

## 环境要求

| 工具 | 版本要求 | 说明 |
|------|----------|------|
| Android Studio | Ladybug (2024.2) 或更高 | 推荐使用 IDE 构建，也可纯命令行 |
| JDK | 17+ | Gradle 8.9 要求 |
| Android SDK | compileSdk 35 | 通过 SDK Manager 安装 |
| Android NDK | 27.2.12479018 | 项目指定版本，通过 SDK Manager 安装 |
| CMake | 3.22.1 | 通过 SDK Manager 安装 |
| Gradle | 8.9 | 由 `gradlew` 自动下载，无需手动安装 |

## 前置步骤：编译 MNN 原生库

App 的 C++ 层依赖预编译的 `libMNN.so`，位于 `project/android/build_64/lib/`。如果该目录不存在或需要重新编译：

```bash
# 1. 确保 ANDROID_NDK 环境变量已设置
export ANDROID_NDK=/path/to/android-ndk

# 2. 进入 Android 构建目录
cd project/android
mkdir -p build_64 && cd build_64

# 3. 执行构建脚本（arm64-v8a, Release）
bash ../build_64.sh

# 构建完成后 lib/ 目录下会生成 libMNN.so
```

> 如果只修改了 Java/Kotlin 层代码（如本次 DSL 预览功能），且原生库已经编译过，可跳过此步。

## 构建 APK

### 方式一：Android Studio（推荐）

1. 用 Android Studio 打开 `apps/Android/MnnLlmChat/` 目录
2. 等待 Gradle Sync 完成
3. 菜单栏 **Build → Build Bundle(s) / APK(s) → Build APK(s)**
4. 或选择 Build Variant 后点击 Run

Build Variant 说明：

| Variant | 说明 |
|---------|------|
| `standardDebug` | 标准版 Debug，可直接安装调试 |
| `standardRelease` | 标准版 Release，需签名配置 |
| `googleplayDebug` | Google Play 版 Debug |
| `googleplayRelease` | Google Play 版 Release |

> 日常开发调试使用 `standardDebug` 即可。

### 方式二：命令行

在 `apps/Android/MnnLlmChat/` 目录下执行：

```bash
# Debug APK（无需签名，可直接安装）
./gradlew assembleStandardDebug

# Release APK（需要签名配置，见下方说明）
./gradlew assembleStandardRelease
```

#### Windows 下使用

```bat
gradlew.bat assembleStandardDebug
```

### 产物位置

```
app/build/outputs/apk/
├── standard/
│   ├── debug/
│   │   └── app-standard-debug.apk        ← Debug 包
│   └── release/
│       └── app-standard-release.apk       ← Release 包
└── googleplay/
    ├── debug/
    └── release/
```

## Release 签名配置

Release 构建通过环境变量配置签名：

```bash
export KEYSTORE_FILE=/path/to/your-keystore.jks
export KEYSTORE_PASSWORD=your_store_password
export KEY_ALIAS=your_key_alias
export KEY_PASSWORD=your_key_password

./gradlew assembleStandardRelease
```

如果没有 keystore，先生成一个：

```bash
keytool -genkey -v -keystore my-release-key.jks \
  -keyalg RSA -keysize 2048 -validity 10000 \
  -alias my-key-alias
```

> 未配置签名环境变量时，Release 构建会使用 debug 签名。

## 安装到设备

```bash
# 通过 adb 安装 Debug 包
adb install app/build/outputs/apk/standard/debug/app-standard-debug.apk

# 安装并替换已有版本
adb install -r app/build/outputs/apk/standard/debug/app-standard-debug.apk
```

## 常见问题

### NDK 版本不匹配

项目要求 NDK `27.2.12479018`。通过 Android Studio 的 **SDK Manager → SDK Tools → NDK (Side by side)** 安装对应版本，或设置 `ANDROID_NDK_HOME` 环境变量指向正确路径。

### libMNN.so 找不到

构建时报错 `libMNN.so` 不存在，说明原生库未编译。按"前置步骤"编译 MNN 原生库，确保 `project/android/build_64/lib/libMNN.so` 存在。

### Sherpa MNN 下载失败

首次构建会自动从 CDN 下载 `libsherpa-mnn-jni.so`（TTS/ASR 依赖）。如果网络不通，手动下载 `https://meta.alicdn.com/data/mnn/libs/libsherpa-mnn-jni-16k.zip`，解压到 `app/src/main/jniLibs/arm64-v8a/`。

### Markwon 依赖拉取失败

默认从 JitPack 拉取定制版 Markwon。如果 JitPack 不可达，可设置 `USE_LOCAL_MARKWON=true` 并在本地编译 Markwon fork（详见 `app/build.gradle` 中的 `localMarkwonDir` 配置）。

### Firebase 相关错误

项目默认禁用 Firebase。如果不需要 Crashlytics，忽略相关警告即可。如需启用，在 `app/` 目录放置 `google-services.json` 并添加 Gradle 参数 `-PENABLE_FIREBASE=true`。

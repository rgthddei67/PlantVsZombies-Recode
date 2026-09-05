param(
    [string]$AndroidRoot = 'D:\Android',
    [string]$VcpkgRoot = 'D:\PVZ\vcpkg-master',
    [string]$VmaInclude = 'D:\VulkanSDK\Include',
    [string]$SdlSource = '',
    [switch]$PackageOnly
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$sdk = Join-Path $AndroidRoot 'sdk'
$ndk = Join-Path $sdk 'ndk/27.2.12479018'
$jdk = Get-ChildItem (Join-Path $AndroidRoot 'jdk') -Directory | Select-Object -First 1
if (!$jdk) { throw 'AndroidRoot/jdk 下需要 JDK 17' }
$env:JAVA_HOME = $jdk.FullName
$env:ANDROID_HOME = $sdk
$env:ANDROID_NDK_HOME = $ndk
$env:GRADLE_USER_HOME = Join-Path $AndroidRoot 'gradle-cache'
# VS 环境用于 vcpkg 的 Windows host 工具，不把 clang-cl 用于 Android 目标。
$env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;" + $env:PATH
$vs = & vswhere -latest -property installationPath
cmd /c "`"$vs\Common7\Tools\VsDevCmd.bat`" -arch=x64 -no_logo && set" |
    ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }
$env:PATH = "$env:JAVA_HOME\bin;" + $env:PATH
$native = Join-Path $repo 'build/android-arm64'
$stage = Join-Path $repo 'build/android-package'
$triplet = 'arm64-pvz-android'
if (!$PackageOnly) {
    & cmake -S $repo -B $native -G Ninja `
        "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot/scripts/buildsystems/vcpkg.cmake" `
        "-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$ndk/build/cmake/android.toolchain.cmake" `
        "-DVCPKG_OVERLAY_TRIPLETS=$repo/cmake/triplets" `
        "-DVCPKG_OVERLAY_PORTS=$repo/cmake/vcpkg-ports" `
        "-DVCPKG_TARGET_TRIPLET=$triplet" '-DVCPKG_HOST_TRIPLET=x64-windows-static' `
        '-DANDROID_ABI=arm64-v8a' '-DANDROID_PLATFORM=android-28' `
        '-DANDROID_STL=c++_shared' '-DCMAKE_BUILD_TYPE=Release' "-DPVZ_VMA_INCLUDE_DIR=$VmaInclude"
    if ($LASTEXITCODE) { throw 'Android CMake configure failed' }
    & cmake --build $native --parallel 8
    if ($LASTEXITCODE) { throw 'Android native build failed' }
}
# Java 桥必须与 vcpkg 本次编译使用的 SDL 源码完全一致。
if (!$SdlSource) {
    $sources = @(Get-ChildItem "$VcpkgRoot/buildtrees/sdl2/src" -Directory |
        Where-Object { Test-Path (Join-Path $_.FullName 'android-project/app/src/main/java/org/libsdl/app/SDLActivity.java') })
    if ($sources.Count -ne 1) { throw '存在多个 SDL 源码版本，请用 -SdlSource 指定本次 vcpkg 使用的目录' }
    $SdlSource = $sources[0].FullName
}
$libs = Join-Path $stage 'jniLibs/arm64-v8a'
$assets = Join-Path $stage 'assets'
$java = Join-Path $stage 'sdl-java'
New-Item -ItemType Directory -Force $libs, $assets, $java | Out-Null
Copy-Item "$native/libmain.so" $libs
Copy-Item "$native/vcpkg_installed/$triplet/lib/libSDL2.so" $libs
Copy-Item "$ndk/toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" $libs
Copy-Item "$SdlSource/android-project/app/src/main/java/*" $java -Recurse -Force
# 镜像会删除暂存目录中过期文件；先验证所有目标都落在本仓库固定暂存根下。
$stageFull = [IO.Path]::GetFullPath($stage).TrimEnd('\') + '\'
foreach ($target in @("$assets/resources", "$assets/font", "$assets/Shader/opengl")) {
    $targetFull = [IO.Path]::GetFullPath($target)
    if (!$targetFull.StartsWith($stageFull, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe staging destination: $targetFull"
    }
    if ((Test-Path -LiteralPath $targetFull) -and
        ((Get-Item -LiteralPath $targetFull).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Staging destination must not be a directory link: $targetFull"
    }
}
# 每次从权威目录同步，移除的资产不会滞留 APK；只镜像本任务固定暂存目录。
foreach ($name in @('resources', 'font')) {
    & robocopy "$repo/build/clang-release/$name" "$assets/$name" /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -gt 7) { throw "Asset staging failed: $name" }
}
& robocopy "$repo/PlantVsZombies/Shader/opengl" "$assets/Shader/opengl" /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -gt 7) { throw 'Shader staging failed' }
& cmake "-DRES_DIR=$assets/resources" "-DRES_PARENT=$assets" "-DOUT=$assets/resources/manifest.txt" -P "$repo/cmake/gen_manifest.cmake"
if ($LASTEXITCODE) { throw 'Manifest generation failed' }
& cmake "-DSHARE_DIR=$native/vcpkg_installed/$triplet/share" `
    "-DVMA_HEADER=$VmaInclude/vma/vk_mem_alloc.h" `
    "-DOUT=$assets/THIRD-PARTY-LICENSES.txt" -P "$repo/cmake/gen_third_party_licenses.cmake"
if ($LASTEXITCODE) { throw 'Third-party license generation failed' }
$keyStore = Join-Path $AndroidRoot 'debug.keystore'
if (!(Test-Path -LiteralPath $keyStore)) {
    & "$env:JAVA_HOME/bin/keytool.exe" -genkeypair -keystore $keyStore -storepass android `
        -alias androiddebugkey -keypass android -dname 'CN=Android Debug,O=Android,C=US' `
        -keyalg RSA -keysize 2048 -validity 10000
    if ($LASTEXITCODE) { throw 'Debug keystore generation failed' }
}
& "$AndroidRoot/gradle/gradle-8.9/bin/gradle.bat" -p $PSScriptRoot --no-daemon `
    "-PpvzDebugKeystore=$keyStore" assembleDebug
if ($LASTEXITCODE) { throw 'APK packaging failed' }
Write-Output "APK: $PSScriptRoot/app/build/outputs/apk/debug/app-debug.apk"

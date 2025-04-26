# STATIC Qt build settings
$qtVersion = "6.9.0"
$qtDir = "C:\Qt"
$installDir = "$qtDir\qt-static"
$buildBase = "$qtDir\build-static-qtbase"
$buildTools = "$qtDir\build-static-qttools"
$srcZip = "$qtDir\qt-everywhere-src-$qtVersion.zip"
$srcExtracted = "$qtDir\qt-everywhere"

# Cleanup
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $installDir, $buildBase, $buildTools, $srcExtracted

# Download
$qtDownloadUrl = "https://download.qt.io/official_releases/qt/6.9/$qtVersion/submodules/qt-everywhere-src-$qtVersion.zip"
Invoke-WebRequest -Uri $qtDownloadUrl -OutFile $srcZip

# Unzip
Expand-Archive -Path $srcZip -DestinationPath $qtDir

# Configure qtbase for static build
cmake -S "$srcExtracted" -B $buildBase -GNinja `
  -DCMAKE_INSTALL_PREFIX=$installDir `
  -DCMAKE_BUILD_TYPE=Release `
  -DFEATURE_static=ON `
  -DBUILD_SHARED_LIBS=OFF `
  -DFEATURE_developer_build=OFF `
  -DQT_BUILD_EXAMPLES=OFF `
  -DQT_BUILD_TESTS=OFF `
  -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded" # /MT static CRT

# Build and install qtbase
cmake --build $buildBase
cmake --install $buildBase

# Configure and install qttools
cmake -S "$srcExtracted\qttools" -B $buildTools -GNinja `
  -DCMAKE_PREFIX_PATH=$installDir `
  -DCMAKE_INSTALL_PREFIX=$installDir `
  -DCMAKE_BUILD_TYPE=Release `
  -DBUILD_SHARED_LIBS=OFF `
  -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded"

cmake --build $buildTools
cmake --install $buildTools

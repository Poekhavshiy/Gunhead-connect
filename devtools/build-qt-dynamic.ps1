# Variables
$qtVersion = "6.9.0"
$qtDir = "C:\Qt"
$installDir = "$qtDir\qt-install-dynamic"
$buildBase = "$qtDir\build-qtbase"
$buildTools = "$qtDir\build-qttools"
$srcZip = "$qtDir\qt-everywhere-src-$qtVersion.zip"
$srcExtracted = "$qtDir\qt-everywhere"

# Clean previous install and build dirs
Write-Host "Cleaning previous build and install directories..."
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $installDir, $buildBase, $buildTools, $srcExtracted

# Download latest Qt 6 release source
$qtDownloadUrl = "https://download.qt.io/official_releases/qt/6.9/$qtVersion/submodules/qt-everywhere-src-$qtVersion.zip"
Write-Host "Downloading Qt $qtVersion source..."
Invoke-WebRequest -Uri $qtDownloadUrl -OutFile $srcZip

# Unzip Qt source
Write-Host "Unzipping Qt source..."
Expand-Archive -Path $srcZip -DestinationPath $qtDir

# CMake configure qtbase
Write-Host "Configuring qtbase..."
cmake -S "$srcExtracted" -B $buildBase -GNinja `
  -DCMAKE_INSTALL_PREFIX=$installDir `
  -DCMAKE_BUILD_TYPE=Release `
  -DFEATURE_developer_build=ON `
  -DQT_BUILD_EXAMPLES=OFF `
  -DQT_BUILD_TESTS=OFF

# Build and install qtbase
Write-Host "Building qtbase..."
cmake --build $buildBase
Write-Host "Installing qtbase..."
cmake --install $buildBase

# CMake configure qttools
Write-Host "Configuring qttools..."
cmake -S "$srcExtracted\qttools" -B $buildTools -GNinja `
  -DCMAKE_PREFIX_PATH=$installDir `
  -DCMAKE_INSTALL_PREFIX=$installDir `
  -DCMAKE_BUILD_TYPE=Release

# Build and install qttools
Write-Host "Building qttools..."
cmake --build $buildTools
Write-Host "Installing qttools..."
cmake --install $buildTools

Write-Host "`n✅ Qt $qtVersion built and installed to $installDir"

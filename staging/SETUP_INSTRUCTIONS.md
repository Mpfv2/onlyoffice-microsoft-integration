# Setup Instructions (run on Arch Linux)

## Step 1: Fork repos on GitHub
- https://github.com/ONLYOFFICE/DesktopEditors → Fork
- https://github.com/ONLYOFFICE/sdkjs → Fork

## Step 2: Add submodules
```bash
cd ~/onlyoffice-mscollab
git submodule add https://github.com/YOUR_USERNAME/DesktopEditors DesktopEditors
git submodule add https://github.com/YOUR_USERNAME/sdkjs sdkjs
git submodule update --init --recursive
```

## Step 3: Copy staged files into submodules
```bash
# C++ source and tests
cp -r staging/DesktopEditors/src/mscollab/ DesktopEditors/src/mscollab/
cp -r staging/DesktopEditors/tests/mscollab/ DesktopEditors/tests/mscollab/

# JavaScript bridge shim
mkdir -p sdkjs/common
cp staging/sdkjs/common/mscollab-shim.js sdkjs/common/mscollab-shim.js
```

## Step 3b: Register the JS shim
In `sdkjs/common/loader.js` (or the equivalent bundle entry point), add:
```html
<script src="mscollab-shim.js"></script>
```
This must load after the main sdkjs bundle so `window.asc_docs_api` is defined.

## Step 4: Wire mscollab.pri into DesktopEditors/desktop-sdk.pro
Add this line after the last existing `include(...)` in desktop-sdk.pro:
```qmake
include(src/mscollab/mscollab.pri)
```

## Step 5: Install build dependencies
```bash
sudo pacman -S --needed \
  qt5-base qt5-webengine qt5-tools \
  libcurl-gnutls libsecret libzip \
  cmake make gcc python3 git \
  gtest pkgconf openssl
```

## Step 6: Run unit tests (before full build)
```bash
cd ~/onlyoffice-mscollab/DesktopEditors/tests/mscollab
cmake -B build && cmake --build build
./build/mscollab_tests          # GTest suite (all tests)
./build/mscollab_fsshttpb_tests # standalone binary encoder tests
```

## Step 7: Verify full build
```bash
cd ~/onlyoffice-mscollab/DesktopEditors
python3 build_tools/build.py --module desktop --platform linux_64 2>&1 | tail -20
```

## Step 8: Register Azure app
1. Go to https://portal.azure.com → Azure Active Directory → App registrations → New registration
2. Name: `OnlyOffice-MsCollab`, Supported account types: Single tenant
3. Redirect URI: `http://localhost` (Public client / native)
4. Copy the Application (client) ID into `src/mscollab/config.h` as `CLIENT_ID`
5. Under API permissions, add: `Files.ReadWrite`, `Sites.ReadWrite.All`, `offline_access`
6. Grant admin consent if required by the school tenant

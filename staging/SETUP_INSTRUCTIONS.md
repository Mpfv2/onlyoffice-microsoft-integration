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
cp -r staging/DesktopEditors/src/mscollab/ DesktopEditors/src/mscollab/
cp -r staging/DesktopEditors/tests/mscollab/ DesktopEditors/tests/mscollab/
```

## Step 4: Wire mscollab.pri into DesktopEditors/desktop-sdk.pro
Add this line after the last existing `include(...)` in desktop-sdk.pro:
```qmake
include(src/mscollab/mscollab.pri)
```

## Step 5: Install build dependencies
```bash
sudo pacman -S --needed \
  qt5-base qt5-webengine qt5-tools \
  libcurl-gnutls libxml2 libsecret \
  cmake make gcc python3 git
```

## Step 6: Verify build
```bash
cd ~/onlyoffice-mscollab/DesktopEditors
python3 build_tools/build.py --module desktop --platform linux_64 2>&1 | tail -20
```

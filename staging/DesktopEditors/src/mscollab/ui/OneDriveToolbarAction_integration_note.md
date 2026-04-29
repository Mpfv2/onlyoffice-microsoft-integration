# Wiring the "Open from OneDrive" Toolbar Button

After cloning the DesktopEditors fork, add the toolbar button to the main window.

## 1. Find the toolbar definition

Run:
```bash
grep -rn "addAction\|QToolBar\|toolbar\|Toolbar" \
    src/applicationmanager/ src/windows/ --include="*.cpp" | grep -i "open\|file" | head -10
```

## 2. Add the action

In the file that defines the main toolbar (typically the main window class), add:

```cpp
#include "mscollab/ui/OneDriveDialog.h"

// In the toolbar setup method:
QAction* openOneDriveAction = toolbar->addAction(
    QIcon(":/icons/onedrive.png"), "Open from OneDrive");
connect(openOneDriveAction, &QAction::triggered, this, [this]() {
    OneDriveDialog dlg([this]() {
        return m_mscollabBridge.authModule().accessToken();
    }, this);
    if (dlg.exec() == QDialog::Accepted) {
        // Open the downloaded file in OnlyOffice
        openLocalFile(dlg.selectedLocalPath());
        // Start co-authoring: pass webUrl (for FSSHTTP) AND localPath (for OOXML loading)
        m_mscollabBridge.openFromOneDrive(
            dlg.selectedWebUrl().toStdString(),
            dlg.selectedLocalPath().toStdString());
    }
});
```

## 3. Icon

Place a 24×24 OneDrive icon at:
`resources/icons/onedrive.png`

Or use a fallback Qt stock icon until a proper icon is sourced:
```cpp
QIcon::fromTheme("folder-remote")
```

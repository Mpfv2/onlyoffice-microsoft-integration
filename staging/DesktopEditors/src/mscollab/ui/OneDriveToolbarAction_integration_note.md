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
    // Trigger the browser-based OAuth flow before opening the dialog so the
    // user doesn't see an empty file picker on first launch.  authenticate()
    // is a no-op if there's already a valid refresh token in libsecret.
    auto& auth = m_mscollabBridge.authModule();
    if (!auth.isAuthenticated() && !auth.authenticate()) {
        QMessageBox::warning(this, "OneDrive",
            "Sign-in failed.  Check your network and try again.");
        return;
    }

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

# CAscApplicationManager Integration Instructions

After cloning the DesktopEditors fork, apply these changes to wire IntegrationBridge
into the application manager.

## In `src/applicationmanager/CAscApplicationManager.h`

Add at top of file:
```cpp
#include "mscollab/bridge/IntegrationBridge.h"
```

Add as a private member:
```cpp
IntegrationBridge m_mscollabBridge;
```

## In `src/applicationmanager/CAscApplicationManager.cpp`

### 1. Find the document open handler
Run: `grep -n "OpenDocument\|openDocument\|documentOpen\|OpenLocalFile" src/applicationmanager/CAscApplicationManager.cpp | head -10`
Add at the end of the open handler:
```cpp
m_mscollabBridge.onDocumentOpened(sFilePath.toStdString());
```

### 2. Find the document close handler
Add at the end of the close handler:
```cpp
m_mscollabBridge.onDocumentClosed(sFilePath.toStdString());
```

### 3. Find the JS->native message dispatcher
Run: `grep -n "sendToNative\|NativeMessage\|onNativeMessage" src/applicationmanager/CAscApplicationManager.cpp | head -10`
Add a new message handler:
```cpp
if (sMessageName == "onOutgoingChange") {
    m_mscollabBridge.onOutgoingChange(sMessageData.toStdString());
}
```

### 4. Find the JS executor and set sendToJs
Run: `grep -rn "executeJS\|evaluateJavaScript\|CallJSMethod\|ExecuteJavaScript" src/applicationmanager/ | head -10`
Add after the bridge is constructed (e.g., in the constructor or init method):
```cpp
// Option A (CEF-based):
m_mscollabBridge.sendToJs = [this](const std::string& delta) {
    GetFrame()->ExecuteJavaScript(
        "window.MsCollab.applyRemoteChange(" + delta + ")", "", 0);
};

// Option B (Qt WebChannel):
m_mscollabBridge.sendToJs = [this](const std::string& delta) {
    emit jsChannelMessage("applyRemoteChange", QString::fromStdString(delta));
};
```
Use whichever pattern matches the grep output.

# OnlyOffice MS-FSSHTTP Co-Authoring Integration

**Date:** 2026-04-29
**Status:** Approved
**Target platform:** Arch Linux
**Goal:** Fork OnlyOffice Desktop Editors to support real-time co-authoring with Microsoft Word desktop users via the MS-FSSHTTP protocol and Microsoft 365 (OneDrive, school tenant: techcollege.dk)

---

## 1. Scope

- **In scope:** `.docx` co-authoring with Word desktop users via OneDrive (Microsoft 365 Education tenant)
- **Out of scope (phase 2):** Excel, PowerPoint, comments/track changes sync, equation co-authoring (OMML format)
- **Out of scope (forever):** SharePoint on-premise, consumer Microsoft accounts

---

## 2. Architecture

Two upstream repos are forked and submoduled into a new top-level repo `onlyoffice-mscollab`:

- `ONLYOFFICE/DesktopEditors` — Qt/C++ shell (primary changes)
- `ONLYOFFICE/sdkjs` — JavaScript document engine (minimal shim only)

Four new components are added inside `DesktopEditors`:

```
src/
  mscollab/
    auth/         # OAuth 2.0 / MSAL (AuthModule)
    fsshttp/      # MS-FSSHTTP client (MSFSSHTTPClient)
    bridge/       # Integration bridge between Qt shell and sdkjs
    merge/        # Conflict resolution logic
```

The sdkjs shim lives in `sdkjs/common/mscollab-shim.js`.

---

## 3. Authentication (AuthModule)

- **Protocol:** OAuth 2.0 Authorization Code Flow with PKCE
- **Library:** `libcurl` for HTTP + manual token management (avoids MSAL's heavy dependency footprint on Linux)
- **Tenant:** Configurable; defaults to `techcollege.dk` tenant (Azure AD)
- **Token storage:** Encrypted local keyring via `libsecret` (standard on Linux desktop)
- **Scopes required:** `Files.ReadWrite`, `offline_access`
- **Flow:**
  1. User clicks "Open from OneDrive" or opens a file from a detected OneDrive path
  2. AuthModule opens a local loopback server (`localhost:PORT`) and launches the system browser to the Microsoft login page
  3. On redirect, captures the auth code, exchanges for access + refresh tokens
  4. Tokens are stored in keyring and refreshed automatically before expiry

---

## 4. MS-FSSHTTP Client (MSFSSHTTPClient)

**Reference:** [MS-FSSHTTP Open Specification](https://docs.microsoft.com/en-us/openspecs/sharepoint_protocols/ms-fsshttp/)

### 4.1 Session Lifecycle

| Step | SubRequest type | Trigger |
|------|----------------|---------|
| Join | `CoauthoringSubRequest` (Join) | Document open |
| Heartbeat | `CoauthoringSubRequest` (Refresh) | Every 30 seconds |
| Exit | `CoauthoringSubRequest` (Exit) | Document close |

### 4.2 Change Flow

```
OnlyOffice (us)            OneDrive Server           Word (classmate)
     │                           │                          │
     │──JoinCoauthoringSession──►│◄─JoinCoauthoringSession──│
     │◄──────────session token───│──────session token──────►│
     │                           │                          │
     │  (user types)             │                          │
     │──CellSubRequest(delta)───►│                          │
     │                           │──────notify─────────────►│
     │                           │◄────CellSubRequest(delta)─│
     │◄──────notify──────────────│                          │
     │  (apply delta to doc)     │                          │
```

### 4.3 SubRequest types implemented

- `CoauthoringSubRequest` — session join/refresh/exit
- `CellSubRequest` — document delta transmission (FsshttpCellStorageData format)
- `SchemaLockSubRequest` — prevents incompatible clients from corrupting the file

### 4.4 Delta Format

Microsoft uses `FsshttpCellStorageData` — a binary format encoding document cell store changes. We implement:
- A serializer: converts OnlyOffice change events → `FsshttpCellStorageData`
- A deserializer: converts incoming `FsshttpCellStorageData` → OnlyOffice `asc_ApplyChanges` calls

### 4.5 Error Handling

| Condition | Behavior |
|-----------|----------|
| Auth failure | Fall back to local-only mode; show non-blocking notification |
| Session dropped | Auto-rejoin with exponential backoff (max 3 attempts, then notify user) |
| Unresolvable merge conflict | Keep both versions as tracked changes |
| Network loss | Queue outgoing deltas locally; flush on reconnect |

---

## 5. Integration Bridge

### 5.1 C++ side

- Subclass `CAscApplicationManager` to intercept document open/save
- Detect OneDrive file paths (contains `/OneDrive/` or is a resolved OneDrive URL)
- Add message types to the existing Qt ↔ sdkjs event bus:
  - `onOutgoingChange` — fired by sdkjs when user edits; consumed by MSFSSHTTPClient
  - `onIncomingChange` — fired by MSFSSHTTPClient when remote delta arrives; consumed by sdkjs

### 5.2 JS side (sdkjs shim — ~50 lines)

```js
// sdkjs/common/mscollab-shim.js
window.MsCollab = {
  onLocalChange: (delta) => {
    window.AscDesktopEditor.sendToNative('onOutgoingChange', JSON.stringify(delta));
  },
  applyRemoteChange: (delta) => {
    asc_docs_api.asc_ApplyChanges(delta);
  }
};
```

### 5.3 "Open from OneDrive" UI

New toolbar button added to the Qt shell. On click:
1. Triggers AuthModule OAuth flow if not already authenticated
2. Opens a file picker showing the user's OneDrive root via Graph API (`/me/drive/root/children`)
3. Downloads selected file to a temp path, opens in OnlyOffice
4. Initiates coauthoring session immediately after load

---

## 6. Merge / Conflict Resolution

- **Strategy:** Paragraph-level last-write-wins for text edits
- **Tie-breaking:** Server assigns priority at session join; lower priority client yields on conflict
- **Unresolvable conflicts** (e.g., simultaneous structural changes to same paragraph): both versions preserved as tracked changes, user prompted to resolve

---

## 7. Build System

**Repo structure:**
```
onlyoffice-mscollab/
  DesktopEditors/     # submodule (our fork)
  sdkjs/              # submodule (our fork)
  build.py            # wraps upstream build_tools with our additions
  README.md
```

**New dependencies:**
- `libcurl` — HTTP for auth + SOAP requests
- `libxml2` — SOAP/XML serialization
- `libsecret` — token storage (standard on Linux)

**Build on Arch:**
```bash
# Install deps
sudo pacman -S qt5-base libcurl-gnutls libxml2 libsecret

# Build
python3 build.py --module desktop --platform linux_64
```

---

## 8. Known Limitations (Phase 1)

- Equation co-authoring not supported (OMML is a separate binary format; addressed in phase 2)
- Comments and track changes do not sync in real time
- Excel and PowerPoint co-authoring not included
- First implementation will have higher merge conflict rate than native Word due to incomplete FsshttpCellStorageData coverage — this improves iteratively

---

## 9. Success Criteria

- User can open a `.docx` from OneDrive within OnlyOffice on Arch Linux
- User and a classmate using Word desktop can edit the same document simultaneously
- Changes from Word appear in OnlyOffice within ~5 seconds
- Changes from OnlyOffice appear in Word within ~5 seconds
- No file corruption on merge conflict

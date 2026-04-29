# onlyoffice-mscollab

A fork of [OnlyOffice Desktop Editors](https://github.com/ONLYOFFICE/DesktopEditors) that adds real-time co-authoring with Microsoft Word desktop users via the MS-FSSHTTP protocol and Microsoft 365 OneDrive.

**Target platform:** Arch Linux  
**School tenant:** techcollege.dk (Microsoft 365 Education)

---

## What this does

OnlyOffice is open-source but has no native Microsoft 365 co-authoring. This project implements the MS-FSSHTTP coauthoring protocol so that a student running OnlyOffice on Linux can edit the same `.docx` file simultaneously with classmates using Word on Windows — through their shared school OneDrive.

## Status

**Phase 1 — all staging files complete.** Set up on Arch Linux to build:

| Component | Status |
|-----------|--------|
| OAuth 2.0 PKCE auth (Microsoft 365) | ✅ Written |
| Token storage (libsecret keyring) | ✅ Written |
| MS-FSSHTTP session management | ✅ Written |
| SOAP/XML serializer | ✅ Written |
| Conflict merge engine | ✅ Written |
| sdkjs JS bridge shim | ✅ Written |
| Integration bridge | ✅ Written |
| Graph API URL resolver | ✅ Written |
| "Open from OneDrive" UI dialog | ✅ Written |

**Phase 2** (after Phase 1 is working):
- `FsshttpCellStorageData` binary delta encoding (true keystroke-level sync)
- Equation (OMML) co-authoring
- Comments and tracked changes sync
- Excel / PowerPoint support

## Architecture

```
OnlyOffice Desktop (forked)
├── src/mscollab/
│   ├── auth/          # OAuth 2.0 PKCE + libsecret token storage
│   ├── fsshttp/       # MS-FSSHTTP client (session, serializer, HTTP transport)
│   ├── merge/         # Paragraph-level conflict resolution
│   ├── bridge/        # IntegrationBridge — wires everything into OnlyOffice
│   └── config.h       # Azure app client ID + tenant
└── (sdkjs fork)
    └── common/
        └── mscollab-shim.js   # JS bridge: local changes → native, remote changes → doc
```

The Integration Bridge hooks into `CAscApplicationManager` to intercept document open/close events. When a OneDrive file is opened, it authenticates via OAuth, joins an MS-FSSHTTP coauthoring session on OneDrive, and routes incoming/outgoing change events between the document engine and the protocol layer.

## Setup (Arch Linux)

See [`staging/SETUP_INSTRUCTIONS.md`](staging/SETUP_INSTRUCTIONS.md) for the full walkthrough:

1. Fork `ONLYOFFICE/DesktopEditors` and `ONLYOFFICE/sdkjs` on GitHub
2. Add as submodules and copy staged files in
3. Register an Azure app at [portal.azure.com](https://portal.azure.com) and set your client ID in `staging/DesktopEditors/src/mscollab/config.h`
4. Install deps: `sudo pacman -S qt5-base qt5-webengine libcurl-gnutls libxml2 libsecret cmake`
5. Build: `python3 build_tools/build.py --module desktop --platform linux_64`

## Running tests

```bash
cd DesktopEditors/tests/mscollab
cmake -B build && cmake --build build
./build/mscollab_tests
```

17 tests covering TokenStore, AuthModule (PKCE), FsshttpSerializer, FsshttpSession, and MergeEngine.

## Azure app registration

You need to register an application in your school's Azure AD tenant:

1. Go to [portal.azure.com](https://portal.azure.com) → Azure Active Directory → App Registrations → New Registration
2. Name: `OnlyOffice MSCollab`, accounts: single tenant (`techcollege.dk`)
3. Redirect URI: `http://localhost` (Public client / native)
4. API Permissions → Microsoft Graph → Delegated: `Files.ReadWrite`, `offline_access`
5. Copy the **Application (client) ID** into `config.h`

## MS-FSSHTTP reference

Microsoft's co-authoring protocol specification:  
[MS-FSSHTTP Open Specification](https://docs.microsoft.com/en-us/openspecs/sharepoint_protocols/ms-fsshttp/)

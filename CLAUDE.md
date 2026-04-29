# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Fork of OnlyOffice Desktop Editors adding real-time co-authoring with Microsoft Word desktop via the MS-FSSHTTP protocol and Microsoft 365 (OneDrive). Target platform: Arch Linux. School tenant: techcollege.dk.

## Development Workflow

**We are currently on Windows.** The actual submodules (DesktopEditors/, sdkjs/) have not been cloned yet — they get set up on Arch Linux. All code is written to `staging/` and copied into the submodules at setup time.

- Write all new C++ files to `staging/DesktopEditors/src/mscollab/`
- Write all new test files to `staging/DesktopEditors/tests/mscollab/`
- Write all new JS files to `staging/sdkjs/common/`
- When a new file is added, also add it to `staging/DesktopEditors/src/mscollab/mscollab.pri`
- Push to GitHub after every logical chunk of work — token budget is finite

## Repo Structure

```
onlyoffice-mscollab/
  staging/
    DesktopEditors/
      src/mscollab/         # ALL our new C++ code lives here
        auth/               # OAuth 2.0 PKCE + libsecret token storage
        fsshttp/            # MS-FSSHTTP client (session, serializer, transport)
        graph/              # Microsoft Graph API client (URL resolver, file picker)
        bridge/             # IntegrationBridge — wires everything into OnlyOffice
        merge/              # Conflict resolution engine
        ui/                 # Qt dialogs (OneDrive file picker)
        config.h            # Azure app client ID + tenant
        mscollab.pri        # qmake module file — list new files here
      tests/mscollab/       # GTest unit tests
        CMakeLists.txt
    sdkjs/
      common/
        mscollab-shim.js    # JS bridge shim
  docs/superpowers/
    specs/                  # Design specs
    plans/                  # Implementation plans
  CLAUDE.md
  README.md
```

## Build (on Arch Linux after submodule setup)

```bash
sudo pacman -S qt5-base qt5-webengine qt5-tools libcurl-gnutls libxml2 libsecret libzip cmake
python3 build_tools/build.py --module desktop --platform linux_64
```

## Running Tests

```bash
cd staging/DesktopEditors/tests/mscollab
cmake -B build && cmake --build build
./build/mscollab_tests
```

## Architecture

**C++ modules in `src/mscollab/`:**

| Module | Responsibility |
|--------|---------------|
| `auth/AuthModule` | OAuth 2.0 PKCE flow, browser login, token refresh |
| `auth/TokenStore` | libsecret keyring persistence |
| `fsshttp/FsshttpClient` | Top-level MS-FSSHTTP client; 30s heartbeat + 5s GetChanges; delta queue; rejoin backoff |
| `fsshttp/FsshttpSerializer` | SOAP/XML encode/decode for Coauth and Cell SubRequests |
| `fsshttp/FsshttpSession` | Session state machine (Disconnected/Joining/Joined/Exiting) |
| `fsshttp/FsshttpBinaryEncoder` | MS-FSSHTTPB binary encoder/decoder (CompactUint, ExGUID, StreamObjects) |
| `fsshttp/FsshttpCellSubRequest` | Builds PutChanges/GetChanges FSSHTTPB binary payloads |
| `fsshttp/OoxmlDocument` | Loads word/document.xml from .docx (libzip), applies/parses paragraph deltas |
| `graph/GraphApiClient` | Microsoft Graph API — resolves OneDrive paths → SharePoint URLs, lists files |
| `bridge/IntegrationBridge` | Owns all components; MergeEngine conflict resolution on incoming deltas |
| `merge/MergeEngine` | Paragraph-level last-write-wins, structural conflicts → tracked changes |
| `ui/OneDriveDialog` | Qt file picker dialog, driven by GraphApiClient |

**JS side (`sdkjs/common/mscollab-shim.js`):**
- `window.MsCollab.onLocalChange(delta)` — fires when user edits, routes to native
- `window.MsCollab.applyRemoteChange(deltaJson)` — called from native to apply remote delta

**Integration notes for CAscApplicationManager** are in:
`staging/DesktopEditors/src/mscollab/bridge/CAscApplicationManager_integration_note.md`

## Key Interfaces (exact signatures — must match across files)

```cpp
// AuthModule
AuthModule(const std::string& tenantDomain, const std::string& clientId)
std::string accessToken()  // non-const, may refresh silently
bool isAuthenticated() const
bool authenticate()        // opens browser, blocks until done

// FsshttpClient
FsshttpClient(fileUrl, clientId, std::function<std::string()> tokenProvider)
bool joinSession()
void exitSession()
void sendDelta(const std::string& deltaJson)
bool isJoined() const
std::function<void(const std::string&)> onRemoteDelta
std::function<void()> onSessionDropped

// GraphApiClient
GraphApiClient(std::function<std::string()> tokenProvider)
std::string resolveWebUrl(const std::string& localPath, const std::string& onedriveFolderName) const
std::vector<GraphApiClient::DriveItem> listFolder(const std::string& folderPath) const
// DriveItem: { string id, string name, string webUrl, string downloadUrl, bool isFolder }

// MergeEngine
MergeResult merge(const Delta& local, const Delta& remote) const
// Delta: { int paragraphIndex, string content }
// MergeResult: { Action action, int paragraphIndex, string content }
// Action: Apply | TrackedChange | Discard
```

## Phase Status

**Phase 1 — COMPLETE (all staging files written):**
- ✅ TokenStore, AuthModule, FsshttpSerializer, FsshttpSession, FsshttpClient, MergeEngine, mscollab-shim.js, config.h
- ✅ GraphApiClient (`graph/GraphApiClient.h/.cpp`) — resolves local OneDrive paths → SharePoint webUrls via Graph API; lists folder contents
- ✅ OneDriveDialog (`ui/OneDriveDialog.h/.cpp`) — Qt QDialog with lazy-loading OneDrive tree, downloads selected .docx to temp dir
- ✅ IntegrationBridge — wired to GraphApiClient for URL resolution; exposes `authModule()` for toolbar action
- ✅ Toolbar wiring guide (`ui/OneDriveToolbarAction_integration_note.md`)

**Remaining before Arch Linux build:**
- Register Azure app at portal.azure.com, paste client ID into `config.h`
- Clone submodules on Arch Linux and run `staging/SETUP_INSTRUCTIONS.md`
- Wire toolbar button into `CAscApplicationManager` (see the integration note)

**Phase 2 — in progress:**
- ✅ FsshttpBinaryEncoder: MS-FSSHTTPB CompactUint, ExGUID, CellID, StreamObjectHeader (16/32-bit), base64
- ✅ FsshttpCellSubRequest: buildPutChanges, buildGetChanges, parseGetChangesResponse
- ✅ FsshttpSerializer: encodeCellPutChanges, encodeCellGetChanges, decodeCellSubResponse
- ✅ OoxmlDocument: libzip-based .docx loader, XML-escaped paragraph apply/serialize/parse
- ✅ FsshttpClient: sendDelta uses OOXML; 5s GetChanges poll loop; delta queue; rejoin backoff
- ✅ IntegrationBridge: openFromOneDrive, MergeEngine conflict resolution in remote delta path
- ✅ JS shim: paragraph-level apply via asc_SetSelectionRange + asc_ReplaceText; TRACKED change support
- ✅ OoxmlDocument unit tests (5 GTest tests, no real .docx required)
- ✅ GraphApiClient: uploadFile() — PUT /root:/{path}:/content; called from onDocumentClosed as safety-net sync
- ✅ GetChanges knowledge tracking: `CurrentKnowledge` attribute threaded through FsshttpSerializer ↔ FsshttpClient; knowledge cleared on (re)join
- ✅ Comments sync: OoxmlDocument::parseCommentDeltas; FsshttpClient routes comment blobs; JS shim applies via asc_AddComment
- 🔲 OMML equation co-authoring
- 🔲 outgoing comment sync (currently only receives comments, does not send)
- 🔲 Excel / PowerPoint support

## Key Constraints

- Phase 1: `.docx` only
- MS-FSSHTTP spec: https://docs.microsoft.com/en-us/openspecs/sharepoint_protocols/ms-fsshttp/
- Graph API base URL: `https://graph.microsoft.com/v1.0/me/drive`
- Azure tenant: `techcollege.dk`
- Push to GitHub after every logical chunk — token budget is finite

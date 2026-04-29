# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Fork of OnlyOffice Desktop Editors adding real-time co-authoring with Microsoft Word desktop via the MS-FSSHTTP protocol and Microsoft 365 (OneDrive). Target platform: Arch Linux.

## Repo Structure

```
onlyoffice-mscollab/
  DesktopEditors/     # submodule — forked ONLYOFFICE/DesktopEditors (Qt/C++)
  sdkjs/              # submodule — forked ONLYOFFICE/sdkjs (JS document engine)
  docs/superpowers/specs/  # design specs
  build.py            # wraps upstream build_tools with our additions
```

Our new code lives entirely in `DesktopEditors/src/mscollab/` and `sdkjs/common/mscollab-shim.js`. Do not modify other upstream files unless necessary for a hook point.

## Build

```bash
# Install deps (Arch)
sudo pacman -S qt5-base libcurl-gnutls libxml2 libsecret

# Build desktop app
python3 build.py --module desktop --platform linux_64
```

The upstream build system is `qmake` + `build_tools/`. Our module is registered in `DesktopEditors/desktop-sdk.pro`.

## Architecture

Four components in `DesktopEditors/src/mscollab/`:

- `auth/` — OAuth 2.0 PKCE flow via `libcurl`, token storage via `libsecret`. Triggered when user opens a OneDrive file or clicks "Open from OneDrive".
- `fsshttp/` — MS-FSSHTTP client. SOAP/XML over HTTPS via `libcurl` + `libxml2`. Implements `CoauthoringSubRequest`, `CellSubRequest`, `SchemaLockSubRequest`. See spec for session lifecycle.
- `bridge/` — Subclasses `CAscApplicationManager` to intercept open/save. Adds `onOutgoingChange` and `onIncomingChange` message types to the Qt ↔ sdkjs event bus.
- `merge/` — Paragraph-level last-write-wins conflict resolution. Unresolvable conflicts become tracked changes.

The sdkjs shim (`mscollab-shim.js`) exposes `window.MsCollab.onLocalChange` and `window.MsCollab.applyRemoteChange` — these are the only JS-side touch points.

## Key Constraints

- Phase 1 covers `.docx` only — no Excel, PowerPoint, equations (OMML), or comments sync.
- MS-FSSHTTP spec reference: https://docs.microsoft.com/en-us/openspecs/sharepoint_protocols/ms-fsshttp/
- School tenant: `techcollege.dk` (Azure AD / Microsoft 365 Education)

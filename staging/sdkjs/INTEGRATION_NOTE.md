# sdkjs Integration Note

After cloning the sdkjs fork and copying `common/mscollab-shim.js` into place,
add the shim to the document editor's HTML loader.

Find the script includes in:
  `web-apps/apps/documenteditor/main/index.html`

Add after the last `<script>` include from `common/`:
  `<script type="text/javascript" src="../../../common/mscollab-shim.js"></script>`

To verify the shim loaded, open the OnlyOffice desktop app DevTools (F12 in the web view) and run:
  `typeof window.MsCollab`
Expected output: `"object"`

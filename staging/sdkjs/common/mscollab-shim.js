(function () {
    'use strict';

    if (typeof window === 'undefined' || typeof window.AscDesktopEditor === 'undefined') {
        return; // not running inside OnlyOffice desktop shell
    }

    window.MsCollab = {
        // Called by sdkjs when the user makes a local edit.
        // delta: serialized change event from asc_docs_api
        onLocalChange: function (delta) {
            window.AscDesktopEditor.sendToNative(
                'onOutgoingChange',
                typeof delta === 'string' ? delta : JSON.stringify(delta)
            );
        },

        // Called from native (C++) when a remote delta arrives.
        // delta: JSON string of a change event
        applyRemoteChange: function (deltaJson) {
            try {
                var delta = typeof deltaJson === 'string'
                    ? JSON.parse(deltaJson) : deltaJson;
                if (window.Asc && window.Asc.plugin) return; // ignore if in plugin context
                window.asc_docs_api && window.asc_docs_api.asc_ApplyChanges(delta);
            } catch (e) {
                console.error('[MsCollab] applyRemoteChange failed:', e);
            }
        }
    };

    // Hook into the document change event emitted by the editing engine.
    // The event name may vary by OnlyOffice version — verify against sdkjs source.
    document.addEventListener('asc_onDocumentContentReady', function () {
        if (!window.asc_docs_api) return;
        window.asc_docs_api.asc_registerCallback('asc_onDocumentChanged', function (data) {
            window.MsCollab.onLocalChange(data);
        });
    });
}());

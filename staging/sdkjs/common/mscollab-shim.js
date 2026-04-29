(function () {
    'use strict';

    if (typeof window === 'undefined' || typeof window.AscDesktopEditor === 'undefined') {
        return; // not running inside OnlyOffice desktop shell
    }

    // ---------------------------------------------------------------------------
    // applyParagraphContent: navigates to paragraph N and replaces its text.
    // Uses the OnlyOffice Collaboration API.
    // ---------------------------------------------------------------------------
    function applyParagraphContent(paragraphIndex, text, asTrackedChange) {
        var api = window.asc_docs_api;
        if (!api) return;

        try {
            // Select the entire paragraph at paragraphIndex.
            // AscFonts.g_font_manager must be ready; this fires only after
            // asc_onDocumentContentReady so we are safe here.
            var range = new Asc.SelectionRange();
            range.StartRow    = paragraphIndex;
            range.StartColumn = 0;
            range.EndRow      = paragraphIndex;
            range.EndColumn   = -1; // -1 = select to end of paragraph

            api.asc_SetSelectionRange(range);

            if (asTrackedChange) {
                api.asc_BeginUndoGrouping && api.asc_BeginUndoGrouping();
                api.asc_SetTrackChanges && api.asc_SetTrackChanges(true);
            }

            api.asc_ReplaceText(text);

            if (asTrackedChange) {
                api.asc_SetTrackChanges && api.asc_SetTrackChanges(false);
                api.asc_EndUndoGrouping && api.asc_EndUndoGrouping();
            }
        } catch (e) {
            console.error('[MsCollab] applyParagraphContent error:', e);
        }
    }

    window.MsCollab = {
        // Called by sdkjs when the user makes a local edit.
        // data: {paragraphIndex: N, content: "text"} or sdkjs change event
        onLocalChange: function (data) {
            window.AscDesktopEditor.sendToNative(
                'onOutgoingChange',
                typeof data === 'string' ? data : JSON.stringify(data)
            );
        },

        // Called from C++ IntegrationBridge via the native→JS bridge.
        // deltaJson: JSON string "{paragraphIndex:N, content:\"text\"}"
        // If content starts with "TRACKED:" the change is applied as a tracked change.
        applyRemoteChange: function (deltaJson) {
            try {
                var delta = typeof deltaJson === 'string'
                    ? JSON.parse(deltaJson) : deltaJson;

                if (typeof delta.paragraphIndex !== 'number') return;

                var isTracked = typeof delta.content === 'string' &&
                                delta.content.indexOf('TRACKED:') === 0;
                var text = isTracked
                    ? delta.content.substring(8)
                    : (delta.content || '');

                applyParagraphContent(delta.paragraphIndex, text, isTracked);
            } catch (e) {
                console.error('[MsCollab] applyRemoteChange failed:', e);
            }
        }
    };

    // Hook into sdkjs document-ready event to register the outgoing-change listener.
    // "asc_onDocumentContentReady" fires after the document is fully loaded.
    document.addEventListener('asc_onDocumentContentReady', function () {
        if (!window.asc_docs_api) return;
        window.asc_docs_api.asc_registerCallback(
            'asc_onDocumentChanged',
            function (data) {
                // Convert sdkjs change event to {paragraphIndex, content} format.
                // data.para is the 0-based paragraph index; data.text is the new content.
                // If the sdkjs format differs, adapt here.
                var delta = {
                    paragraphIndex: (data && typeof data.para === 'number') ? data.para : 0,
                    content:        (data && typeof data.text === 'string') ? data.text : ''
                };
                window.MsCollab.onLocalChange(delta);
            }
        );
    });
}());

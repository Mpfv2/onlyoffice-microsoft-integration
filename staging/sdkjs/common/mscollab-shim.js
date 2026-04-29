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

    // ---------------------------------------------------------------------------
    // applyRemoteComment: inserts or updates a comment from a remote co-author.
    // Uses the OnlyOffice Collaboration API if available.
    // ---------------------------------------------------------------------------
    function applyRemoteComment(delta) {
        var api = window.asc_docs_api;
        if (!api) return;
        try {
            // asc_AddComment accepts a comment descriptor object.
            // The exact API shape depends on the sdkjs version; fall back silently.
            if (typeof api.asc_AddComment === 'function') {
                var comment = new Asc.asc_CComment();
                if (comment.put_Text)   comment.put_Text(delta.text   || '');
                if (comment.put_Author) comment.put_Author(delta.author || '');
                if (comment.put_Time)   comment.put_Time(delta.date   || '');
                api.asc_AddComment(comment);
            }
        } catch (e) {
            console.error('[MsCollab] applyRemoteComment error:', e);
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
        // deltaJson: JSON string for a paragraph change or a comment addition.
        //
        // Paragraph delta:  {paragraphIndex:N, content:"text"}
        //   If content starts with "TRACKED:" the change is applied as a tracked change.
        //
        // Comment delta:  {type:"comment", commentId:N, author:"...", date:"...", text:"..."}
        applyRemoteChange: function (deltaJson) {
            try {
                var delta = typeof deltaJson === 'string'
                    ? JSON.parse(deltaJson) : deltaJson;

                if (delta.type === 'comment') {
                    applyRemoteComment(delta);
                    return;
                }

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

    // Hook into sdkjs document-ready event to register outgoing-change listeners.
    // "asc_onDocumentContentReady" fires after the document is fully loaded.
    document.addEventListener('asc_onDocumentContentReady', function () {
        var api = window.asc_docs_api;
        if (!api) return;

        // Paragraph changes
        api.asc_registerCallback('asc_onDocumentChanged', function (data) {
            var delta = {
                paragraphIndex: (data && typeof data.para === 'number') ? data.para : 0,
                content:        (data && typeof data.text === 'string') ? data.text : ''
            };
            window.MsCollab.onLocalChange(delta);
        });

        // Comment additions
        if (typeof api.asc_registerCallback === 'function') {
            api.asc_registerCallback('asc_onAddComment', function (comment) {
                try {
                    var delta = {
                        type:      'comment',
                        commentId: (comment && typeof comment.get_Id === 'function')
                                       ? (comment.get_Id() | 0) : 0,
                        author:    (comment && typeof comment.get_Author === 'function')
                                       ? (comment.get_Author() || '') : '',
                        date:      (comment && typeof comment.get_Time === 'function')
                                       ? (comment.get_Time() || '') : '',
                        text:      (comment && typeof comment.get_Text === 'function')
                                       ? (comment.get_Text() || '') : ''
                    };
                    window.MsCollab.onLocalChange(delta);
                } catch (e) {
                    console.error('[MsCollab] asc_onAddComment handler error:', e);
                }
            });
        }
    });
}());

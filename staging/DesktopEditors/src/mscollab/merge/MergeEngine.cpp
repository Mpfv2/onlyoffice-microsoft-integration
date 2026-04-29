#include "MergeEngine.h"

bool MergeEngine::isStructural(const std::string& content) {
    return content.find("STRUCTURAL_") == 0;
}

MergeEngine::MergeResult MergeEngine::merge(const Delta& local,
                                            const Delta& remote) const {
    MergeResult r;
    r.paragraphIndex = remote.paragraphIndex;
    r.content = remote.content;

    if (local.paragraphIndex != remote.paragraphIndex) {
        r.action = Action::Apply;
        return r;
    }
    // Same paragraph — structural conflict becomes tracked change
    if (isStructural(local.content) && isStructural(remote.content)) {
        r.action = Action::TrackedChange;
        return r;
    }
    // Text conflict — remote wins (last-write-wins)
    r.action = Action::Apply;
    return r;
}

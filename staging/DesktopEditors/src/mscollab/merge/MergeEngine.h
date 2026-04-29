#pragma once
#include <string>

class MergeEngine {
public:
    struct Delta {
        int         paragraphIndex = -1;
        std::string content;
    };

    enum class Action { Apply, TrackedChange, Discard };

    struct MergeResult {
        Action      action = Action::Apply;
        int         paragraphIndex = -1;
        std::string content;
    };

    // Returns what to do with the remote delta given a simultaneous local delta.
    MergeResult merge(const Delta& local, const Delta& remote) const;

private:
    static bool isStructural(const std::string& content);
};

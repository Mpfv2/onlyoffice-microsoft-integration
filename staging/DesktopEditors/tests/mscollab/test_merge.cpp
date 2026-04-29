#include <gtest/gtest.h>
#include "merge/MergeEngine.h"

TEST(MergeEngine, NoConflictReturnsRemoteDelta) {
    MergeEngine m;
    MergeEngine::Delta local  = {1, "hello world"};
    MergeEngine::Delta remote = {2, "foo bar"};
    auto result = m.merge(local, remote);
    EXPECT_EQ(result.action, MergeEngine::Action::Apply);
    EXPECT_EQ(result.paragraphIndex, 2);
}

TEST(MergeEngine, SameParagraphRemoteWins) {
    MergeEngine m;
    MergeEngine::Delta local  = {3, "local text"};
    MergeEngine::Delta remote = {3, "remote text"};
    auto result = m.merge(local, remote);
    EXPECT_EQ(result.action, MergeEngine::Action::Apply);
    EXPECT_EQ(result.content, "remote text");
}

TEST(MergeEngine, StructuralConflictBecomesTrackedChange) {
    MergeEngine m;
    MergeEngine::Delta local  = {5, "STRUCTURAL_DELETE"};
    MergeEngine::Delta remote = {5, "STRUCTURAL_INSERT"};
    auto result = m.merge(local, remote);
    EXPECT_EQ(result.action, MergeEngine::Action::TrackedChange);
}

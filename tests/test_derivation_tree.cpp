/***************************************************************************
 *            tests/test_derivation_tree.cpp
 *
 *  Copyright  2026  Luca Geretti
 *
 ****************************************************************************/

/*
 *  This file is part of Ariadne Tree.
 *
 *  Ariadne Tree is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Ariadne Tree is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Ariadne Tree. If not, see <https://www.gnu.org/licenses/>.
 */

/** @file tests/test_derivation_tree.cpp
 *  @brief Check genealogy, stable storage, ownership and exception safety.
 */

#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "ariadne-testing.hpp"
#include "ariadne-tree.hpp"

using namespace Ariadne;

namespace {

struct TrajectoryMetadata {
    bool complete;
    std::size_t origin;
};

// Noncopyable, nonmovable payloads ensure that node growth cannot relocate
// them.
struct CheckedValue {
    static inline int alive = 0;
    int value;

    explicit CheckedValue(int v) : value(v) {
        if (v < 0) {
            throw std::runtime_error("Requested payload construction failure");
        }
        ++alive;
    }
    CheckedValue(const CheckedValue &) = delete;
    CheckedValue(CheckedValue &&) = delete;
    ~CheckedValue() { --alive; }
};

using Tree = DerivationTree<int, std::string>;
static_assert(!std::is_copy_constructible_v<Tree>);
static_assert(!std::is_copy_assignable_v<Tree>);
static_assert(std::is_move_constructible_v<Tree>);
static_assert(std::is_same_v<
              decltype(std::declval<const Tree &>().object(Tree::NodeId())),
              const int &>);
static_assert(std::is_same_v<
              decltype(std::declval<const Tree &>().metadata(Tree::NodeId())),
              const std::string &>);
static_assert(std::is_same_v<
              decltype(std::declval<const Tree &>().children(Tree::NodeId())),
              std::span<const Tree::NodeId>>);

} // namespace

class TestDerivationTree : public Testing::ContextFixture {
  public:
    using ContextFixture::ContextFixture;

    void test_root_and_genealogy() {
        Tree tree;
        ARIADNE_TESTING_PROVE(tree.empty());
        ARIADNE_TESTING_EXPECT_EQUAL(tree.size(), 0u);
        ARIADNE_TESTING_PROVE(!tree.root());
        const auto root = tree.create_root(0, "root");
        ARIADNE_TESTING_PROVE(!tree.empty());
        ARIADNE_TESTING_PROVE(tree.contains(root));
        ARIADNE_TESTING_EXPECT_EQUAL(*tree.root(), root);
        ARIADNE_TESTING_PROVE(!tree.parent(root));
        ARIADNE_TESTING_PROVE(tree.ancestors(root).empty());
        ARIADNE_TESTING_PROVE(tree.descendants(root).empty());
        ARIADNE_TESTING_EXPECT_THROWS(std::logic_error,
                                      tree.create_root(9, "duplicate"));
        ARIADNE_TESTING_EXPECT_EQUAL(tree.size(), 1u);

        const auto first = tree.add_child(root, 1, "first");
        const auto second = tree.add_child(root, 2, "second");
        const auto grandchild = tree.add_child(first, 3, "grandchild");
        const auto later = tree.add_child(root, 4, "later");
        const auto &view = std::as_const(tree);
        ARIADNE_TESTING_EXPECT_EQUAL(*view.parent(grandchild), first);
        ARIADNE_TESTING_EXPECT_EQUAL(*view.parent(first), root);
        ARIADNE_TESTING_EXPECT_EQUAL(view.children(root).size(), 3u);
        ARIADNE_TESTING_EXPECT_EQUAL(view.children(root)[0], first);
        ARIADNE_TESTING_EXPECT_EQUAL(view.children(root)[1], second);
        ARIADNE_TESTING_EXPECT_EQUAL(view.children(root)[2], later);
        const std::vector<Tree::NodeId> ancestors{first, root};
        const std::vector<Tree::NodeId> descendants{first, grandchild, second,
                                                    later};
        ARIADNE_TESTING_PROVE(view.ancestors(grandchild) == ancestors);
        ARIADNE_TESTING_PROVE(view.descendants(root) == descendants);
        ARIADNE_TESTING_PROVE(view.children(grandchild).empty());
        ARIADNE_TESTING_EXPECT_EQUAL(view.object(grandchild), 3);
        ARIADNE_TESTING_EXPECT_EQUAL(view.metadata(grandchild),
                                     std::string("grandchild"));
    }

    void test_parent_progress_and_local_completion() {
        DerivationTree<std::vector<int>, TrajectoryMetadata> tree;
        const auto root = tree.create_root(std::vector<int>{10, 20},
                                           TrajectoryMetadata{false, 0});
        auto &trajectory = tree.object(root);
        auto &metadata = tree.metadata(root);
        const auto first = tree.add_child(root, std::vector<int>{21},
                                          TrajectoryMetadata{false, 1});
        trajectory.push_back(30);
        const auto second = tree.add_child(root, std::vector<int>{31},
                                           TrajectoryMetadata{false, 2});
        metadata.complete = true;
        ARIADNE_TESTING_EXPECT_EQUAL(trajectory.size(), 3u);
        ARIADNE_TESTING_EXPECT_EQUAL(trajectory.back(), 30);
        ARIADNE_TESTING_EXPECT_EQUAL(tree.children(root).size(), 2u);
        ARIADNE_TESTING_PROVE(tree.metadata(root).complete);
        ARIADNE_TESTING_PROVE(!tree.metadata(first).complete);
        ARIADNE_TESTING_PROVE(!tree.metadata(second).complete);
        ARIADNE_TESTING_EXPECT_EQUAL(tree.object(root)[tree.metadata(first).origin],
                                     20);
        ARIADNE_TESTING_EXPECT_EQUAL(
            tree.object(root)[tree.metadata(second).origin], 30);
        ARIADNE_TESTING_EXPECT_EQUAL(tree.object(first).size(), 1u);
    }

    void test_wide_growth_keeps_payloads_stable() {
        ARIADNE_TESTING_EXPECT_EQUAL(CheckedValue::alive, 0);
        {
            DerivationTree<CheckedValue, CheckedValue> tree;
            const auto root = tree.create_root(10, 20);
            auto *object = &tree.object(root);
            auto *metadata = &tree.metadata(root);
            const auto first = tree.add_child(root, 0, 0);
            auto *first_object = &tree.object(first);
            for (int i = 1; i < 4096; ++i) {
                tree.add_child(root, i, i);
            }
            ARIADNE_TESTING_PROVE(&tree.object(root) == object);
            ARIADNE_TESTING_PROVE(&tree.metadata(root) == metadata);
            ARIADNE_TESTING_PROVE(&tree.object(first) == first_object);
            ARIADNE_TESTING_EXPECT_EQUAL(tree.size(), 4097u);
            ARIADNE_TESTING_EXPECT_EQUAL(tree.children(root).size(), 4096u);
            ARIADNE_TESTING_EXPECT_EQUAL(tree.children(root).front(), first);
            for (std::size_t i = 0; i < tree.children(root).size(); ++i) {
                const auto child = tree.children(root)[i];
                ARIADNE_TESTING_EXPECT_EQUAL(tree.object(child).value,
                                             static_cast<int>(i));
                ARIADNE_TESTING_EXPECT_EQUAL(*tree.parent(child), root);
            }
            ARIADNE_TESTING_EXPECT_EQUAL(tree.prune(root), 4096u);
            ARIADNE_TESTING_EXPECT_EQUAL(CheckedValue::alive, 2);
            ARIADNE_TESTING_PROVE(&tree.object(root) == object);
            ARIADNE_TESTING_PROVE(&tree.metadata(root) == metadata);
            ARIADNE_TESTING_PROVE(!tree.contains(first));
            const auto regrown = tree.add_child(root, 0, 0);
            ARIADNE_TESTING_PROVE(regrown != first);
        }
        ARIADNE_TESTING_EXPECT_EQUAL(CheckedValue::alive, 0);
    }

    void test_deep_traversal_and_destruction() {
        DerivationTree<int, int> tree;
        const auto root = tree.create_root(0, 0);
        auto last = root;
        for (int i = 1; i <= 20000; ++i) {
            last = tree.add_child(last, i, i);
        }
        const auto descendants = tree.descendants(root);
        const auto ancestors = tree.ancestors(last);
        ARIADNE_TESTING_EXPECT_EQUAL(descendants.size(), 20000u);
        ARIADNE_TESTING_EXPECT_EQUAL(ancestors.size(), 20000u);
        ARIADNE_TESTING_EXPECT_EQUAL(descendants.back(), last);
        ARIADNE_TESTING_EXPECT_EQUAL(ancestors.back(), root);
        for (std::size_t i = 0; i < descendants.size(); ++i) {
            ARIADNE_TESTING_EXPECT_EQUAL(tree.object(descendants[i]),
                                         static_cast<int>(i + 1));
        }
        ARIADNE_TESTING_EXPECT_EQUAL(tree.prune(root), 20000u);
        ARIADNE_TESTING_EXPECT_EQUAL(tree.size(), 1u);
        ARIADNE_TESTING_PROVE(!tree.contains(last));
        const auto regrown = tree.add_child(root, 1, 1);
        ARIADNE_TESTING_PROVE(regrown != descendants.front());
        ARIADNE_TESTING_EXPECT_EQUAL(*tree.parent(regrown), root);
        // Flat storage and iterative traversal also avoid recursive destruction.
    }

    void test_invalid_and_foreign_ids() {
        Tree tree;
        Tree other;
        const Tree::NodeId invalid;
        const auto foreign = other.create_root(1, "foreign");
        const auto root = tree.create_root(0, "root");
        for (const auto id: {invalid, foreign}) {
            ARIADNE_TESTING_PROVE(!tree.contains(id));
            ARIADNE_TESTING_EXPECT_THROWS(std::out_of_range, tree.object(id));
            ARIADNE_TESTING_EXPECT_THROWS(std::out_of_range, tree.metadata(id));
            ARIADNE_TESTING_EXPECT_THROWS(std::out_of_range, tree.parent(id));
            ARIADNE_TESTING_EXPECT_THROWS(std::out_of_range, tree.children(id));
            ARIADNE_TESTING_EXPECT_THROWS(std::out_of_range, tree.ancestors(id));
            ARIADNE_TESTING_EXPECT_THROWS(std::out_of_range, tree.descendants(id));
            ARIADNE_TESTING_EXPECT_THROWS(std::out_of_range,
                                          tree.add_child(id, 2, "invalid"));
            ARIADNE_TESTING_EXPECT_THROWS(std::out_of_range, tree.prune(id));
        }
        DerivationTree<std::string, int> different_type;
        const auto cross_type = different_type.create_root("different type", 0);
        ARIADNE_TESTING_PROVE(!tree.contains(cross_type));
        ARIADNE_TESTING_PROVE(cross_type != root);
        Tree::NodeId expired;
        {
            Tree temporary;
            expired = temporary.create_root(0, "expired");
        }
        Tree replacement;
        replacement.create_root(0, "new owner");
        ARIADNE_TESTING_PROVE(!replacement.contains(expired));
        ARIADNE_TESTING_EXPECT_EQUAL(tree.size(), 1u);
        ARIADNE_TESTING_PROVE(tree.children(root).empty());
    }

    void test_moves_and_move_only_payloads() {
        using MoveTree = DerivationTree<std::unique_ptr<int>, std::unique_ptr<int>>;
        MoveTree source;
        const auto root =
            source.create_root(std::make_unique<int>(1), std::make_unique<int>(2));
        const auto child = source.add_child(root, std::make_unique<int>(3),
                                            std::make_unique<int>(4));
        auto *object = &source.object(root);
        MoveTree moved(std::move(source));
        ARIADNE_TESTING_PROVE(source.empty());
        ARIADNE_TESTING_PROVE(!source.contains(root));
        ARIADNE_TESTING_PROVE(moved.contains(root));
        ARIADNE_TESTING_PROVE(&moved.object(root) == object);
        const auto new_source_root =
            source.create_root(std::make_unique<int>(5), std::make_unique<int>(6));
        ARIADNE_TESTING_PROVE(new_source_root != root);
        ARIADNE_TESTING_PROVE(!moved.contains(new_source_root));
        MoveTree destination;
        const auto old = destination.create_root(std::make_unique<int>(7),
                                                 std::make_unique<int>(8));
        destination = std::move(moved);
        ARIADNE_TESTING_PROVE(moved.empty());
        ARIADNE_TESTING_PROVE(!destination.contains(old));
        ARIADNE_TESTING_PROVE(!moved.contains(child));
        ARIADNE_TESTING_PROVE(&destination.object(root) == object);
        ARIADNE_TESTING_EXPECT_EQUAL(*destination.object(child), 3);
        ARIADNE_TESTING_EXPECT_EQUAL(*destination.metadata(child), 4);
        ARIADNE_TESTING_EXPECT_EQUAL(*destination.parent(child), root);
        auto *same = &destination;
        destination = std::move(*same);
        ARIADNE_TESTING_PROVE(destination.contains(root));
    }

    void test_failed_construction_preserves_genealogy() {
        ARIADNE_TESTING_EXPECT_EQUAL(CheckedValue::alive, 0);
        {
            DerivationTree<CheckedValue, CheckedValue> tree;
            ARIADNE_TESTING_EXPECT_THROWS(std::runtime_error,
                                          tree.create_root(-1, 2));
            ARIADNE_TESTING_PROVE(tree.empty());
            ARIADNE_TESTING_EXPECT_THROWS(std::runtime_error,
                                          tree.create_root(1, -2));
            ARIADNE_TESTING_PROVE(!tree.root());
            ARIADNE_TESTING_EXPECT_EQUAL(CheckedValue::alive, 0);
            const auto root = tree.create_root(1, 2);
            ARIADNE_TESTING_EXPECT_THROWS(std::runtime_error,
                                          tree.add_child(root, -3, 4));
            ARIADNE_TESTING_EXPECT_THROWS(std::runtime_error,
                                          tree.add_child(root, 3, -4));
            ARIADNE_TESTING_EXPECT_EQUAL(CheckedValue::alive, 2);
            ARIADNE_TESTING_EXPECT_EQUAL(tree.size(), 1u);
            ARIADNE_TESTING_PROVE(tree.children(root).empty());
            const auto child = tree.add_child(root, 3, 4);
            ARIADNE_TESTING_EXPECT_EQUAL(CheckedValue::alive, 4);
            // Exercise failure after children already exist as well.
            ARIADNE_TESTING_EXPECT_THROWS(std::runtime_error,
                                          tree.add_child(root, 5, -6));
            ARIADNE_TESTING_EXPECT_EQUAL(tree.size(), 2u);
            ARIADNE_TESTING_EXPECT_EQUAL(tree.children(root).size(), 1u);
            ARIADNE_TESTING_EXPECT_EQUAL(tree.children(root).front(), child);
            ARIADNE_TESTING_EXPECT_EQUAL(*tree.parent(child), root);
            ARIADNE_TESTING_EXPECT_EQUAL(CheckedValue::alive, 4);
        }
        ARIADNE_TESTING_EXPECT_EQUAL(CheckedValue::alive, 0);
    }

    void test_prune_retains_boundary_and_other_branches() {
        Tree tree;
        const auto root = tree.create_root(0, "root");
        const auto boundary = tree.add_child(root, 1, "checkpoint");
        const auto sibling = tree.add_child(root, 2, "unaffected");
        const auto first = tree.add_child(boundary, 3, "first");
        const auto second = tree.add_child(boundary, 4, "second");
        const auto grandchild = tree.add_child(first, 5, "grandchild");
        const auto cousin = tree.add_child(sibling, 6, "cousin");
        auto *boundary_object = &tree.object(boundary);
        auto *boundary_metadata = &tree.metadata(boundary);
        auto *sibling_object = &tree.object(sibling);
        const auto unaffected_children = tree.children(root);
        ARIADNE_TESTING_EXPECT_EQUAL(tree.prune(boundary), 3u);
        ARIADNE_TESTING_EXPECT_EQUAL(tree.size(), 4u);
        ARIADNE_TESTING_PROVE(&tree.object(boundary) == boundary_object);
        ARIADNE_TESTING_PROVE(&tree.metadata(boundary) == boundary_metadata);
        ARIADNE_TESTING_EXPECT_EQUAL(*boundary_object, 1);
        ARIADNE_TESTING_EXPECT_EQUAL(*boundary_metadata, std::string("checkpoint"));
        ARIADNE_TESTING_PROVE(&tree.object(sibling) == sibling_object);
        ARIADNE_TESTING_EXPECT_EQUAL(*tree.parent(boundary), root);
        ARIADNE_TESTING_EXPECT_EQUAL(unaffected_children[0], boundary);
        ARIADNE_TESTING_EXPECT_EQUAL(unaffected_children[1], sibling);
        ARIADNE_TESTING_EXPECT_EQUAL(tree.children(sibling).front(), cousin);
        ARIADNE_TESTING_PROVE(tree.children(boundary).empty());
        ARIADNE_TESTING_EXPECT_EQUAL(tree.prune(boundary), 0u);
        const std::vector<Tree::NodeId> removed{first, second, grandchild};
        for (const auto old_id: removed) {
            ARIADNE_TESTING_PROVE(!tree.contains(old_id));
            ARIADNE_TESTING_EXPECT_THROWS(std::out_of_range, tree.object(old_id));
            ARIADNE_TESTING_EXPECT_THROWS(std::out_of_range, tree.metadata(old_id));
            ARIADNE_TESTING_EXPECT_THROWS(std::out_of_range, tree.parent(old_id));
            ARIADNE_TESTING_EXPECT_THROWS(std::out_of_range, tree.children(old_id));
            ARIADNE_TESTING_EXPECT_THROWS(std::out_of_range, tree.ancestors(old_id));
            ARIADNE_TESTING_EXPECT_THROWS(std::out_of_range, tree.descendants(old_id));
            ARIADNE_TESTING_EXPECT_THROWS(std::out_of_range, tree.prune(old_id));
            ARIADNE_TESTING_EXPECT_THROWS(std::out_of_range, tree.add_child(old_id, 9, "stale"));
        }
        const auto regrown_first = tree.add_child(boundary, 7, "regrown first");
        const auto regrown_second = tree.add_child(boundary, 8, "regrown second");
        const auto regrown_grandchild = tree.add_child(regrown_first, 9, "regrown grandchild");
        for (const auto old_id: removed) {
            ARIADNE_TESTING_PROVE(!tree.contains(old_id));
            ARIADNE_TESTING_PROVE(old_id != regrown_first && old_id != regrown_second && old_id != regrown_grandchild);
        }
        const std::vector<Tree::NodeId> expected{boundary, regrown_first, regrown_grandchild, regrown_second, sibling, cousin};
        ARIADNE_TESTING_PROVE(tree.descendants(root) == expected);
        ARIADNE_TESTING_EXPECT_EQUAL(tree.size(), 7u);
    }

    void test_prune_root_leaf_and_metadata() {
        DerivationTree<std::vector<int>, TrajectoryMetadata> tree;
        const auto root = tree.create_root(std::vector<int>{10, 20}, TrajectoryMetadata{true, 0});
        auto &history = tree.object(root);
        ARIADNE_TESTING_EXPECT_EQUAL(tree.prune(root), 0u);
        const auto child = tree.add_child(root, std::vector<int>{21}, TrajectoryMetadata{false, 1});
        const auto grandchild = tree.add_child(child, std::vector<int>{22}, TrajectoryMetadata{false, 0});
        ARIADNE_TESTING_EXPECT_EQUAL(tree.prune(root), 2u);
        ARIADNE_TESTING_PROVE(!tree.empty());
        ARIADNE_TESTING_EXPECT_EQUAL(*tree.root(), root);
        ARIADNE_TESTING_PROVE(!tree.parent(root));
        ARIADNE_TESTING_EXPECT_EQUAL(history.size(), 2u);
        ARIADNE_TESTING_PROVE(tree.metadata(root).complete);
        ARIADNE_TESTING_PROVE(!tree.contains(child));
        ARIADNE_TESTING_PROVE(!tree.contains(grandchild));
        // Completion and trajectory suffixes are consumer-controlled payloads.
        tree.metadata(root).complete = false;
        history.push_back(30);
        const auto new_child = tree.add_child(root, std::vector<int>{31}, TrajectoryMetadata{false, 2});
        ARIADNE_TESTING_EXPECT_EQUAL(tree.object(root)[tree.metadata(new_child).origin], 30);
        ARIADNE_TESTING_EXPECT_EQUAL(tree.prune(new_child), 0u);
        ARIADNE_TESTING_PROVE(tree.contains(new_child));
    }

    void test_recycled_slots_release_payloads_and_rollback_failures() {
        ARIADNE_TESTING_EXPECT_EQUAL(CheckedValue::alive, 0);
        {
            DerivationTree<CheckedValue, CheckedValue> tree;
            const auto root = tree.create_root(1, 2);
            auto *root_object = &tree.object(root);
            const auto child = tree.add_child(root, 3, 4);
            const auto grandchild = tree.add_child(child, 5, 6);
            ARIADNE_TESTING_EXPECT_EQUAL(CheckedValue::alive, 6);
            ARIADNE_TESTING_EXPECT_EQUAL(tree.prune(root), 2u);
            ARIADNE_TESTING_EXPECT_EQUAL(CheckedValue::alive, 2);
            ARIADNE_TESTING_EXPECT_THROWS(std::runtime_error, tree.add_child(root, -7, 8));
            ARIADNE_TESTING_EXPECT_THROWS(std::runtime_error, tree.add_child(root, 7, -8));
            ARIADNE_TESTING_EXPECT_EQUAL(CheckedValue::alive, 2);
            ARIADNE_TESTING_EXPECT_EQUAL(tree.size(), 1u);
            ARIADNE_TESTING_PROVE(tree.children(root).empty());
            const auto next_child = tree.add_child(root, 7, 8);
            const auto next_grandchild = tree.add_child(next_child, 9, 10);
            ARIADNE_TESTING_EXPECT_EQUAL(CheckedValue::alive, 6);
            ARIADNE_TESTING_PROVE(!tree.contains(child));
            ARIADNE_TESTING_PROVE(!tree.contains(grandchild));
            ARIADNE_TESTING_EXPECT_EQUAL(*tree.parent(next_grandchild), next_child);
            ARIADNE_TESTING_EXPECT_EQUAL(tree.prune(root), 2u);
            ARIADNE_TESTING_PROVE(!tree.contains(next_child));
            ARIADNE_TESTING_PROVE(!tree.contains(next_grandchild));
            std::vector<DerivationTreeNodeId> prior_ids;
            for (int cycle = 0; cycle < 200; ++cycle) {
                const auto current_id = tree.add_child(root, cycle, cycle);
                for (const auto prior_id: prior_ids) {
                    ARIADNE_TESTING_PROVE(!tree.contains(prior_id));
                    ARIADNE_TESTING_PROVE(prior_id != current_id);
                }
                prior_ids.push_back(current_id);
                ARIADNE_TESTING_EXPECT_EQUAL(CheckedValue::alive, 4);
                ARIADNE_TESTING_EXPECT_EQUAL(tree.prune(root), 1u);
                ARIADNE_TESTING_EXPECT_EQUAL(CheckedValue::alive, 2);
            }
            ARIADNE_TESTING_PROVE(&tree.object(root) == root_object);
        }
        ARIADNE_TESTING_EXPECT_EQUAL(CheckedValue::alive, 0);
    }

    void test_pruned_storage_survives_moves() {
        using MoveTree = DerivationTree<std::unique_ptr<int>, int>;
        MoveTree source;
        const auto root = source.create_root(std::make_unique<int>(1), 1);
        const auto branch = source.add_child(root, std::make_unique<int>(2), 2);
        const auto removed = source.add_child(branch, std::make_unique<int>(3), 3);
        auto *retained = &source.object(branch);
        ARIADNE_TESTING_EXPECT_EQUAL(source.prune(branch), 1u);
        MoveTree moved(std::move(source));
        ARIADNE_TESTING_PROVE(source.empty());
        ARIADNE_TESTING_PROVE(!source.contains(root));
        ARIADNE_TESTING_PROVE(!moved.contains(removed));
        ARIADNE_TESTING_EXPECT_EQUAL(moved.size(), 2u);
        ARIADNE_TESTING_PROVE(&moved.object(branch) == retained);
        const auto regrown = moved.add_child(branch, std::make_unique<int>(4), 4);
        ARIADNE_TESTING_PROVE(regrown != removed);
        MoveTree destination;
        const auto old_destination = destination.create_root(std::make_unique<int>(5), 5);
        ARIADNE_TESTING_EXPECT_EQUAL(moved.prune(branch), 1u);
        destination = std::move(moved);
        ARIADNE_TESTING_PROVE(moved.empty());
        ARIADNE_TESTING_PROVE(!destination.contains(old_destination));
        ARIADNE_TESTING_PROVE(!destination.contains(removed));
        ARIADNE_TESTING_PROVE(!destination.contains(regrown));
        ARIADNE_TESTING_PROVE(&destination.object(branch) == retained);
        const auto after_assignment = destination.add_child(branch, std::make_unique<int>(6), 6);
        ARIADNE_TESTING_PROVE(after_assignment != removed && after_assignment != regrown);
        ARIADNE_TESTING_EXPECT_EQUAL(*destination.object(after_assignment), 6);
        ARIADNE_TESTING_EXPECT_EQUAL(destination.prune(root), 2u);
        ARIADNE_TESTING_EXPECT_EQUAL(destination.size(), 1u);
    }
};

int main(int argc, char **argv) {
    ARIADNE_TESTING_SUITE("test_derivation_tree");
    ARIADNE_TESTING_ADD_TEST(TestDerivationTree, test_root_and_genealogy);
    ARIADNE_TESTING_ADD_TEST(TestDerivationTree,
                             test_parent_progress_and_local_completion);
    ARIADNE_TESTING_ADD_TEST(TestDerivationTree,
                             test_wide_growth_keeps_payloads_stable);
    ARIADNE_TESTING_ADD_TEST(TestDerivationTree,
                             test_deep_traversal_and_destruction);
    ARIADNE_TESTING_ADD_TEST(TestDerivationTree, test_invalid_and_foreign_ids);
    ARIADNE_TESTING_ADD_TEST(TestDerivationTree,
                             test_moves_and_move_only_payloads);
    ARIADNE_TESTING_ADD_TEST(TestDerivationTree,
                             test_failed_construction_preserves_genealogy);
    ARIADNE_TESTING_ADD_TEST(TestDerivationTree, test_prune_retains_boundary_and_other_branches);
    ARIADNE_TESTING_ADD_TEST(TestDerivationTree, test_prune_root_leaf_and_metadata);
    ARIADNE_TESTING_ADD_TEST(TestDerivationTree, test_recycled_slots_release_payloads_and_rollback_failures);
    ARIADNE_TESTING_ADD_TEST(TestDerivationTree, test_pruned_storage_survives_moves);
    return ARIADNE_TESTING_RUN_TESTS_WITH_ARGS(argc, argv);
}

/***************************************************************************
 *            tree/derivation_tree.hpp
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

/** @file tree/derivation_tree.hpp
 *  @brief Append-only genealogy of application-defined objects and metadata.
 */

#ifndef ARIADNE_TREE_DERIVATION_TREE_HPP
#define ARIADNE_TREE_DERIVATION_TREE_HPP

#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <ostream>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace Ariadne {

/** @brief An opaque identifier scoped to its owning derivation tree.
 *
 * Equality compares both the ownership token and the node index. Default
 * construction creates an invalid identifier. Copying an identifier does not
 * extend the lifetime of its tree. Tree moves transfer ownership of its IDs.
 * @see DerivationTree::contains
 */
class DerivationTreeNodeId {
    template<class Object, class Metadata>
    friend class DerivationTree;

    std::uint64_t _owner = 0; ///< Non-recycled ownership token; zero denotes an invalid ID.
    std::size_t _index = 0;   ///< Position in the owning tree's append-only storage.

    /** @brief Construct an identifier for an internal storage position.
     * @param owner Ownership token allocated to the tree.
     * @param index Existing or about-to-be-published node position.
     */
    DerivationTreeNodeId(std::uint64_t owner, std::size_t index) noexcept
        : _owner(owner), _index(index) {}

  public:
    /** @brief Construct an invalid identifier rejected by tree containment checks. */
    DerivationTreeNodeId() noexcept = default;

    /** @brief Compare the owner and index, including for invalid identifiers.
     * @return True if both identifiers name the same position in the same owner.
     */
    friend bool operator==(DerivationTreeNodeId,
                           DerivationTreeNodeId) noexcept = default;

    /** @brief Write a diagnostic owner:index representation.
     * @param os Destination stream.
     * @param id Identifier to print, valid or invalid.
     * @return The destination stream.
     * @note The representation is process-local, not a persistence format.
     */
    friend std::ostream &operator<<(std::ostream &os, DerivationTreeNodeId id) {
        return os << id._owner << ':' << id._index;
    }
};

/** @brief Internal support for derivation-tree ownership. */
namespace detail {

/** @brief Allocate an ownership token shared across template instantiations.
 * @return A nonzero token that has not previously been issued.
 * @throws std::overflow_error If the ownership-token space is exhausted.
 * @note Token allocation is thread-safe; operations on a tree are sequential.
 */
inline std::uint64_t next_derivation_tree_owner() {
    static std::atomic<std::uint64_t> next{1};
    auto owner = next.load(std::memory_order_relaxed);
    for (;;) {
        if (owner == std::numeric_limits<std::uint64_t>::max()) {
            throw std::overflow_error("DerivationTree owner identifiers exhausted");
        }
        if (next.compare_exchange_weak(owner, owner + 1,
                                       std::memory_order_relaxed)) {
            return owner;
        }
    }
}

} // namespace detail

/** @brief A sequential n-ary genealogy with mutable objects and metadata.
 * @tparam Object Application-defined object stored in each node.
 * @tparam Metadata Application-defined node metadata, including any completion state.
 *
 * Nodes live in a deque: growth never moves an existing object or metadata.
 * IDs are tree-qualified indices, checked in constant time. Children retain
 * insertion order. Completion and scheduling, if needed, belong in Metadata.
 *
 * Object/metadata references survive insertion and tree moves. Children spans
 * may be invalidated by an attempted insertion under that parent, including
 * one whose object construction throws. All borrowed views/references expire
 * when their owning nodes are destroyed. Structural mutation currently supports
 * growth only; pruning with subsequent regrowth is planned. Inserting intermediate
 * nodes and reparenting existing nodes are outside the model. Copying is disabled.
 * Operations must not be called reentrantly from
 * payload constructors and require external synchronization for shared access.
 */
template<class Object, class Metadata>
class DerivationTree {
  public:
    /** @brief Stable, opaque handle to a node owned by this tree. */
    using NodeId = DerivationTreeNodeId;
    /** @brief Unsigned node-count and storage-index type. */
    using size_type = std::size_t;

  private:
    /** @brief Internal sentinel used for the root's absent parent. */
    static constexpr size_type no_parent = std::numeric_limits<size_type>::max();

    /** @brief Owned payload and non-owning genealogical links for one node. */
    struct Node {
        size_type parent;             ///< Parent index, or no_parent for the root.
        Object object;                ///< Mutable application payload.
        Metadata metadata;            ///< Mutable application metadata.
        std::vector<NodeId> children; ///< Child identifiers in insertion order.

        /** @brief Construct payloads directly in their stable storage location.
         * @tparam O Forwarded object argument type.
         * @tparam M Forwarded metadata argument type.
         * @param parent_index Parent storage index, or no_parent.
         * @param value Argument forwarded to the Object constructor.
         * @param data Argument forwarded to the Metadata constructor.
         * @note Exceptions thrown by payload constructors propagate to the caller.
         */
        template<class O, class M>
        Node(size_type parent_index, O &&value, M &&data)
            : parent(parent_index), object(std::forward<O>(value)),
              metadata(std::forward<M>(data)) {}
    };

    std::uint64_t _owner = detail::next_derivation_tree_owner(); ///< Current tree ownership token.
    std::deque<Node> _nodes;                                     ///< Flat ownership with stable payload addresses during growth.

    /** @brief Build an internal handle without checking whether the node exists.
     * @param index Existing or about-to-be-published storage position.
     * @return A handle qualified with this tree's ownership token.
     */
    NodeId _id(size_type index) const noexcept { return NodeId(_owner, index); }

    /** @brief Validate an identifier and access its node for reading.
     * @param id Identifier to resolve.
     * @return The corresponding internal node.
     * @throws std::out_of_range If id is invalid or belongs to another owner.
     */
    const Node &_node(NodeId id) const {
        if (!contains(id)) {
            throw std::out_of_range(
                "Invalid or foreign DerivationTree node identifier");
        }
        return _nodes[id._index];
    }

    /** @brief Validate an identifier and access its mutable node.
     * @param id Identifier to resolve.
     * @return The corresponding internal node.
     * @throws std::out_of_range If id is invalid or belongs to another owner.
     */
    Node &_node(NodeId id) {
        return const_cast<Node &>(std::as_const(*this)._node(id));
    }

    /** @brief Exchange storage and ownership tokens without moving payloads.
     * @param other Tree whose storage and identity are exchanged with this tree.
     */
    void _swap(DerivationTree &other) noexcept {
        std::swap(_owner, other._owner);
        _nodes.swap(other._nodes);
    }

  public:
    /** @brief Construct an empty tree with an independent ownership token.
     * @throws std::overflow_error If ownership tokens are exhausted.
     * @throws std::bad_alloc If allocating empty storage fails.
     */
    DerivationTree() = default;
    /** @brief Copy construction is disabled to avoid duplicating node identities. */
    DerivationTree(const DerivationTree &) = delete;
    /** @brief Copy assignment is disabled to avoid duplicating node identities. */
    DerivationTree &operator=(const DerivationTree &) = delete;

    /** @brief Transfer nodes and IDs, leaving an empty independent source.
     * @param other Tree from which ownership is transferred.
     * @throws std::overflow_error If the replacement ownership token is exhausted.
     * @throws std::bad_alloc If allocating replacement empty storage fails.
     * @note Existing payload references and IDs now refer to this tree. Failure
     * to construct replacement storage leaves other unchanged.
     */
    DerivationTree(DerivationTree &&other) : DerivationTree() { _swap(other); }

    /** @brief Replace this tree by transferring another tree's nodes and IDs.
     * @param other Tree from which ownership is transferred.
     * @return This tree after assignment.
     * @throws std::overflow_error If the replacement ownership token is exhausted.
     * @throws std::bad_alloc If allocating replacement empty storage fails.
     * @note Old destination references and IDs are invalidated. Source references
     * remain valid and its IDs belong to this tree. Self-move has no effect;
     * allocation failure leaves both trees unchanged. Cost includes destruction
     * of the old destination's payloads.
     */
    DerivationTree &operator=(DerivationTree &&other) {
        if (this != &other) {
            DerivationTree replacement(std::move(other));
            _swap(replacement);
        }
        return *this;
    }

    /** @brief Test whether this tree has no root or nodes.
     * @return True exactly when size() is zero. Runs in O(1).
     */
    bool empty() const noexcept { return _nodes.empty(); }
    /** @brief Return the number of stored nodes in O(1).
     * @return Node count, including the root if present.
     */
    size_type size() const noexcept { return _nodes.size(); }

    /** @brief Test in O(1) whether an identifier currently belongs to this tree.
     * @param id Any identifier, including default, foreign or expired IDs.
     * @return True if both the owner and stored index are valid here.
     */
    bool contains(NodeId id) const noexcept {
        return id._owner == _owner && id._index < _nodes.size();
    }

    /** @brief Access the root's identifier in O(1).
     * @return The root ID, or std::nullopt for an empty tree.
     */
    std::optional<NodeId> root() const noexcept {
        return empty() ? std::nullopt : std::optional<NodeId>(_id(0));
    }

    /** @brief Construct the only root from forwarded payload arguments.
     * @tparam O Forwarded object argument type.
     * @tparam M Forwarded metadata argument type.
     * @param object Argument used to construct the root's Object.
     * @param metadata Argument used to construct the root's Metadata.
     * @return The new root's identifier.
     * @throws std::logic_error If a root already exists.
     * @throws std::bad_alloc If node allocation fails.
     * @note Exceptions thrown by payload constructors propagate to the caller.
     * @note Failure leaves the logical tree unchanged; it does not undo side effects on
     * forwarded arguments. Runs in O(1), excluding payload construction.
     */
    template<class O, class M>
        requires std::constructible_from<Object, O &&> &&
                 std::constructible_from<Metadata, M &&>
    NodeId create_root(O &&object, M &&metadata) {
        if (!empty()) {
            throw std::logic_error("DerivationTree already has a root");
        }
        _nodes.emplace_back(no_parent, std::forward<O>(object),
                            std::forward<M>(metadata));
        return _id(0);
    }

    /** @brief Append an ordered child while retaining the parent and its payload.
     * @tparam O Forwarded object argument type.
     * @tparam M Forwarded metadata argument type.
     * @param parent Identifier of the new child's parent.
     * @param object Argument used to construct the child's Object.
     * @param metadata Argument used to construct the child's Metadata.
     * @return The new child's stable identifier.
     * @throws std::out_of_range If parent is invalid or foreign.
     * @throws std::length_error If child or node storage cannot grow further.
     * @throws std::bad_alloc If storage allocation fails.
     * @note Exceptions thrown by payload constructors propagate to the caller.
     * @note Runs in amortized O(1), excluding payload construction. Existing
     * object/metadata references remain valid. Failure preserves the logical
     * tree but can invalidate this parent's children spans through reservation.
     * Side effects on forwarded arguments are not rolled back.
     */
    template<class O, class M>
        requires std::constructible_from<Object, O &&> &&
                 std::constructible_from<Metadata, M &&>
    NodeId add_child(NodeId parent, O &&object, M &&metadata) {
        auto &children = _node(parent).children;
        // Reserve geometrically before constructing a node, so publishing its
        // trivial ID cannot throw. Never reserve size()+1 on every insertion.
        if (children.size() == children.capacity()) {
            const auto child_capacity = children.capacity();
            const auto child_limit = children.max_size();
            if (child_capacity == child_limit) {
                throw std::length_error("Too many DerivationTree children");
            }
            children.reserve(child_capacity == 0                ? 1
                             : child_capacity > child_limit / 2 ? child_limit
                                                                : child_capacity * 2);
        }
        const auto child = _id(_nodes.size());
        _nodes.emplace_back(parent._index, std::forward<O>(object),
                            std::forward<M>(metadata));
        children.push_back(child);
        return child;
    }

    /** @brief Access a mutable application object in O(1).
     * @param id Identifier of the object to access.
     * @return A borrowed reference stable across node insertion and tree moves.
     * @throws std::out_of_range If id is invalid or foreign.
     */
    Object &object(NodeId id) { return _node(id).object; }
    /** @brief Access an application object for reading in O(1).
     * @param id Identifier of the object to access.
     * @return A borrowed const reference stable across insertion and tree moves.
     * @throws std::out_of_range If id is invalid or foreign.
     */
    const Object &object(NodeId id) const { return _node(id).object; }
    /** @brief Access mutable application metadata in O(1).
     * @param id Identifier of the metadata to access.
     * @return A borrowed reference stable across node insertion and tree moves.
     * @throws std::out_of_range If id is invalid or foreign.
     */
    Metadata &metadata(NodeId id) { return _node(id).metadata; }
    /** @brief Access application metadata for reading in O(1).
     * @param id Identifier of the metadata to access.
     * @return A borrowed const reference stable across insertion and tree moves.
     * @throws std::out_of_range If id is invalid or foreign.
     */
    const Metadata &metadata(NodeId id) const { return _node(id).metadata; }

    /** @brief Access a node's immediate parent in O(1).
     * @param id Identifier of the child, or of the root.
     * @return The parent's ID, or std::nullopt for the root.
     * @throws std::out_of_range If id is invalid or foreign.
     */
    std::optional<NodeId> parent(NodeId id) const {
        const auto index = _node(id).parent;
        return index == no_parent ? std::nullopt
                                  : std::optional<NodeId>(_id(index));
    }

    /** @brief Borrow a read-only view of child IDs in insertion order in O(1).
     * @param id Identifier of the parent.
     * @return A possibly empty span that does not own its storage.
     * @throws std::out_of_range If id is invalid or foreign.
     * @note Reacquire the view after attempted insertion under this parent,
     * including insertion that throws. The view does not permit topology edits.
     */
    std::span<const NodeId> children(NodeId id) const {
        return _node(id).children;
    }

    /** @brief Collect strict ancestors, starting with the immediate parent.
     * @param id Identifier of the node whose ancestry is requested.
     * @return Ancestor IDs ordered from parent to root; empty for the root.
     * @throws std::out_of_range If id is invalid or foreign.
     * @throws std::bad_alloc If result allocation fails.
     * @note Uses O(depth) time and result storage, with no recursive calls.
     */
    std::vector<NodeId> ancestors(NodeId id) const {
        std::vector<NodeId> result;
        auto index = _node(id).parent;
        while (index != no_parent) {
            result.push_back(_id(index));
            index = _nodes[index].parent;
        }
        return result;
    }

    /** @brief Collect strict descendants in insertion-ordered depth-first preorder.
     * @param id Identifier of the node whose descendants are requested.
     * @return Descendant IDs, excluding id itself; empty for a leaf.
     * @throws std::out_of_range If id is invalid or foreign.
     * @throws std::bad_alloc If traversal or result allocation fails.
     * @note Time and auxiliary/result storage are O(visited nodes). Traversal
     * is iterative and does not consume call-stack space proportional to depth.
     */
    std::vector<NodeId> descendants(NodeId id) const {
        const auto &first = _node(id).children;
        std::vector<NodeId> pending(first.rbegin(), first.rend());
        std::vector<NodeId> result;
        while (!pending.empty()) {
            const auto next = pending.back();
            pending.pop_back();
            result.push_back(next);
            const auto &children = _nodes[next._index].children;
            pending.insert(pending.end(), children.rbegin(), children.rend());
        }
        return result;
    }
};

} // namespace Ariadne

#endif // ARIADNE_TREE_DERIVATION_TREE_HPP

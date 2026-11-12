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

/** @brief An opaque, tree-qualified node identifier. Default construction is
 * invalid. */
class DerivationTreeNodeId {
    template<class Object, class Metadata>
    friend class DerivationTree;

    std::uint64_t _owner = 0;
    std::size_t _index = 0;

    DerivationTreeNodeId(std::uint64_t owner, std::size_t index) noexcept
        : _owner(owner), _index(index) {}

  public:
    DerivationTreeNodeId() noexcept = default;

    friend bool operator==(DerivationTreeNodeId,
                           DerivationTreeNodeId) noexcept = default;

    friend std::ostream &operator<<(std::ostream &os, DerivationTreeNodeId id) {
        return os << id._owner << ':' << id._index;
    }
};

namespace detail {

// Shared by all instantiations. Only owner allocation is synchronized; trees
// are sequential.
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
 *
 * Nodes live in a deque: growth never moves an existing object or metadata.
 * IDs are tree-qualified indices, checked in constant time. Children retain
 * insertion order. Completion and scheduling, if needed, belong in Metadata.
 *
 * Object/metadata references survive insertion and tree moves. Children spans
 * may be invalidated by an attempted insertion under that parent, including
 * one whose object construction throws. All borrowed views/references expire
 * when their owning nodes are destroyed. Copying, removal and reparenting are
 * deliberately unsupported. Operations must not be called reentrantly from
 * payload constructors and require external synchronization for shared access.
 */
template<class Object, class Metadata>
class DerivationTree {
  public:
    using NodeId = DerivationTreeNodeId;
    using size_type = std::size_t;

  private:
    static constexpr size_type no_parent = std::numeric_limits<size_type>::max();

    struct Node {
        size_type parent;
        Object object;
        Metadata metadata;
        std::vector<NodeId> children;

        template<class O, class M>
        Node(size_type parent_index, O &&value, M &&data)
            : parent(parent_index), object(std::forward<O>(value)),
              metadata(std::forward<M>(data)) {}
    };

    std::uint64_t _owner = detail::next_derivation_tree_owner();
    std::deque<Node> _nodes;

    NodeId _id(size_type index) const noexcept { return NodeId(_owner, index); }

    const Node &_node(NodeId id) const {
        if (!contains(id)) {
            throw std::out_of_range(
                "Invalid or foreign DerivationTree node identifier");
        }
        return _nodes[id._index];
    }

    Node &_node(NodeId id) {
        return const_cast<Node &>(std::as_const(*this)._node(id));
    }

    void _swap(DerivationTree &other) noexcept {
        std::swap(_owner, other._owner);
        _nodes.swap(other._nodes);
    }

  public:
    DerivationTree() = default;
    DerivationTree(const DerivationTree &) = delete;
    DerivationTree &operator=(const DerivationTree &) = delete;

    /** @brief Transfer nodes and IDs; the source becomes an empty, independent
     * tree. */
    DerivationTree(DerivationTree &&other) : DerivationTree() { _swap(other); }

    /** @brief Transfer nodes and IDs, invalidating the previous destination's IDs
     * and references. */
    DerivationTree &operator=(DerivationTree &&other) {
        if (this != &other) {
            DerivationTree replacement(std::move(other));
            _swap(replacement);
        }
        return *this;
    }

    bool empty() const noexcept { return _nodes.empty(); }
    size_type size() const noexcept { return _nodes.size(); }

    bool contains(NodeId id) const noexcept {
        return id._owner == _owner && id._index < _nodes.size();
    }

    std::optional<NodeId> root() const noexcept {
        return empty() ? std::nullopt : std::optional<NodeId>(_id(0));
    }

    /** @brief Create the only root; duplicate creation throws std::logic_error.
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

    /** @brief Append a child without consuming its parent; failed construction
     * adds no node. */
    template<class O, class M>
        requires std::constructible_from<Object, O &&> &&
                 std::constructible_from<Metadata, M &&>
    NodeId add_child(NodeId parent, O &&object, M &&metadata) {
        auto &children = _node(parent).children;
        // Reserve geometrically before constructing a node, so publishing its
        // trivial ID cannot throw. Never reserve size()+1 on every insertion.
        if (children.size() == children.capacity()) {
            const auto capacity = children.capacity();
            const auto maximum = children.max_size();
            if (capacity == maximum) {
                throw std::length_error("Too many DerivationTree children");
            }
            children.reserve(capacity == 0            ? 1
                             : capacity > maximum / 2 ? maximum
                                                      : capacity * 2);
        }
        const auto child = _id(_nodes.size());
        _nodes.emplace_back(parent._index, std::forward<O>(object),
                            std::forward<M>(metadata));
        children.push_back(child);
        return child;
    }

    Object &object(NodeId id) { return _node(id).object; }
    const Object &object(NodeId id) const { return _node(id).object; }
    Metadata &metadata(NodeId id) { return _node(id).metadata; }
    const Metadata &metadata(NodeId id) const { return _node(id).metadata; }

    std::optional<NodeId> parent(NodeId id) const {
        const auto index = _node(id).parent;
        return index == no_parent ? std::nullopt
                                  : std::optional<NodeId>(_id(index));
    }

    /** @brief Borrow the ordered child IDs; topology cannot be mutated through
     * this view. */
    std::span<const NodeId> children(NodeId id) const {
        return _node(id).children;
    }

    /** @brief Return strict ancestors, starting with the immediate parent. */
    std::vector<NodeId> ancestors(NodeId id) const {
        std::vector<NodeId> result;
        auto index = _node(id).parent;
        while (index != no_parent) {
            result.push_back(_id(index));
            index = _nodes[index].parent;
        }
        return result;
    }

    /** @brief Return strict descendants in insertion-ordered depth-first
     * preorder. */
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

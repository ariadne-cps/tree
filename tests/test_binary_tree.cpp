/***************************************************************************
 *            test_binary_tree.cpp
 *
 *  Copyright  2008-20  Ivan S. Zapreev, Pieter Collins
 *
 ****************************************************************************/

/*
 *  This file is part of Ariadne.
 *
 *  Ariadne is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Ariadne is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Ariadne.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "utility/stlio.hpp"

#include "utility/macros.hpp"
#include "foundation/logical.hpp"

#include "tree/binary_tree.hpp"

#include "ariadne-testing.hpp"

using namespace Ariadne;
using namespace std;

class TestBinaryTree : public Ariadne::Testing::ContextFixture {
  public:
    using ContextFixture::ContextFixture;

    Void test_binary_tree() {
    const BinaryTreeNode * binary_tree_node_null_pointer = nullptr;

    // !!!
    ARIADNE_TESTING_NOTE("Allocate an ebabled node and manipulate it");

    BinaryTreeNode theBinaryTreeRoot(true);
    ARIADNE_TESTING_NOTE("Initial tree node");
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeRoot.is_leaf(), true );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeRoot.is_enabled(), true );

    theBinaryTreeRoot.set_disabled();
    ARIADNE_TESTING_NOTE("Disable the node");
    BinaryTreeNode expected_node1(false);
    ARIADNE_TESTING_EXPECT_EQUAL(expected_node1, theBinaryTreeRoot);

    ARIADNE_TESTING_NOTE("Making the leaf node intermediate should cause the IsALeafNodeException exception");
    ARIADNE_TESTING_EXPECT_THROWS(IsALeafNodeException, theBinaryTreeRoot.set_unknown());

    ARIADNE_TESTING_NOTE("The node still have to be disabled");
    ARIADNE_TESTING_EXPECT_EQUAL(expected_node1, theBinaryTreeRoot);

    ARIADNE_TESTING_NOTE("Enable the node");
    theBinaryTreeRoot.set_enabled();
    BinaryTreeNode expected_node2(true);
    ARIADNE_TESTING_EXPECT_EQUAL(expected_node2, theBinaryTreeRoot);

    ARIADNE_TESTING_NOTE("Compare unequal non-null subtrees and null/non-null nodes");
    BinaryTreeNode unequal_tree_one(false);
    unequal_tree_one.split();
    BinaryTreeNode unequal_tree_two(unequal_tree_one);
    unequal_tree_one.left_node()->set_enabled();
    ARIADNE_TESTING_EXPECT_EQUAL(BinaryTreeNode::is_equal_nodes(&unequal_tree_one,&unequal_tree_two),false);
    ARIADNE_TESTING_EXPECT_EQUAL(BinaryTreeNode::is_equal_nodes(nullptr,&unequal_tree_one),false);
    ARIADNE_TESTING_EXPECT_EQUAL(BinaryTreeNode::is_equal_nodes(&unequal_tree_one,nullptr),false);

    BinaryTreeNode unequal_right_one(false);
    unequal_right_one.split();
    BinaryTreeNode unequal_right_two(unequal_right_one);
    unequal_right_one.right_node()->set_enabled();
    ARIADNE_TESTING_EXPECT_EQUAL(unequal_right_one==unequal_right_two,false);

    BinaryTreeNode unknown_node(false);
    unknown_node.split();
    BinaryTreeNode enabled_leaf(true);
    ARIADNE_TESTING_EXPECT_EQUAL(unknown_node==enabled_leaf,false);

    BooleanArray truncated_tree(1), truncated_leaves(1);
    truncated_tree[0]=true;
    truncated_leaves[0]=false;
    // Even a failed reconstruction must release all previously created subnodes.
    ARIADNE_TESTING_EXPECT_EXCEPTION(BinaryTreeNode(truncated_tree,truncated_leaves));
    BooleanArray nested_truncated_tree = {true,true,false};
    BooleanArray nested_truncated_leaves = {true};
    ARIADNE_TESTING_EXPECT_EXCEPTION(BinaryTreeNode(nested_truncated_tree,nested_truncated_leaves));

    BinaryTreeNode contract_node(false);
    ARIADNE_TESTING_EXPECT_EXCEPTION(BinaryTreeNode::restrict(nullptr,&contract_node));
    ARIADNE_TESTING_EXPECT_EXCEPTION(BinaryTreeNode::restrict(&contract_node,nullptr));
    ARIADNE_TESTING_EXPECT_EXCEPTION(BinaryTreeNode::remove(nullptr,&contract_node));
    ARIADNE_TESTING_EXPECT_EXCEPTION(BinaryTreeNode::remove(&contract_node,nullptr));

    // !!!
    ARIADNE_TESTING_NOTE("Split the enabled node");
    ARIADNE_TESTING_NOTE("Should mark the node as intermediate and create two enabled subnodes");
    theBinaryTreeRoot.split();
    ARIADNE_TESTING_PROVE(theBinaryTreeRoot.left_node() != binary_tree_node_null_pointer);
    ARIADNE_TESTING_PROVE(theBinaryTreeRoot.right_node() != binary_tree_node_null_pointer);
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeRoot.is_enabled(), false );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeRoot.is_disabled(), false );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeRoot.left_node()->is_leaf(), true );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeRoot.right_node()->is_leaf(), true );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeRoot.left_node()->is_enabled(), true );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeRoot.right_node()->is_enabled(), true );

    // !!!
    ARIADNE_TESTING_NOTE("Split a splited node");
    ARIADNE_TESTING_NOTE("We expect an unchanged tree, since it is already split");
    theBinaryTreeRoot.mince(1);
    ARIADNE_TESTING_PROVE(theBinaryTreeRoot.left_node() != binary_tree_node_null_pointer);
    ARIADNE_TESTING_PROVE(theBinaryTreeRoot.right_node() != binary_tree_node_null_pointer);
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeRoot.is_enabled(), false );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeRoot.is_disabled(), false );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeRoot.left_node()->is_leaf(), true );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeRoot.right_node()->is_leaf(), true );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeRoot.left_node()->is_enabled(), true );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeRoot.right_node()->is_enabled(), true );

    // !!!
    ARIADNE_TESTING_NOTE("Copy the node by using the copy constructor");
    ARIADNE_TESTING_NOTE("The entire subtree should be copied");
    BinaryTreeNode theBinaryTreeCopy( theBinaryTreeRoot );
    ARIADNE_TESTING_PROVE(theBinaryTreeCopy.left_node() != binary_tree_node_null_pointer);
    ARIADNE_TESTING_PROVE(theBinaryTreeCopy.right_node() != binary_tree_node_null_pointer);
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeCopy.is_enabled(), false );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeCopy.is_disabled(), false );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeCopy.left_node()->is_leaf(), true );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeCopy.right_node()->is_leaf(), true );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeCopy.left_node()->is_enabled(), true );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeCopy.right_node()->is_enabled(), true );

    // !!!
    ARIADNE_TESTING_NOTE("Recombine the new tree and see what happens");
    ARIADNE_TESTING_NOTE("The new tree should be reduced to a node");
    ARIADNE_TESTING_NOTE("The old tree should remain unchanged");
    theBinaryTreeCopy.recombine();
    ARIADNE_TESTING_NOTE( "Reduced tree" );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeCopy.is_leaf(), true );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeCopy.is_enabled(), true );
    ARIADNE_TESTING_NOTE( "The original tree" );
    ARIADNE_TESTING_PROVE(theBinaryTreeRoot.left_node() != binary_tree_node_null_pointer);
    ARIADNE_TESTING_PROVE(theBinaryTreeRoot.right_node() != binary_tree_node_null_pointer);
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeRoot.is_enabled(), false );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeRoot.is_disabled(), false );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeRoot.left_node()->is_leaf(), true );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeRoot.right_node()->is_leaf(), true );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeRoot.left_node()->is_enabled(), true );
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeRoot.right_node()->is_enabled(), true );

    // !!!
    ARIADNE_TESTING_NOTE("Disable the left tree leaf, recombine the tree and split the disabled leaf to level 3");
    ARIADNE_TESTING_NOTE("A tree with disabled left node");
    theBinaryTreeRoot.left_node()->set_disabled();
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeRoot.left_node()->is_disabled(), true );

    ARIADNE_TESTING_NOTE("A tree after recombination" );
    theBinaryTreeRoot.recombine();
    ARIADNE_TESTING_PROVE(theBinaryTreeRoot.left_node() != binary_tree_node_null_pointer);
    ARIADNE_TESTING_PROVE(theBinaryTreeRoot.right_node() != binary_tree_node_null_pointer);

    ARIADNE_TESTING_NOTE( "A tree after splitting the disabled left node" );
    theBinaryTreeRoot.left_node()->mince(3);
    ARIADNE_TESTING_EXPECT_EQUAL( theBinaryTreeRoot.left_node()->is_leaf(), true );

    ARIADNE_TESTING_NOTE("A tree after splitting the root node" );
    theBinaryTreeRoot.mince(3);
    BooleanArray treeArray(9), leafArray(5);
    treeArray[0] = true;
    treeArray[1] = false; leafArray[0] = false;
    treeArray[2] = true;
    treeArray[3] = true;
    treeArray[4] = false; leafArray[1] = true;
    treeArray[5] = false; leafArray[2] = true;
    treeArray[6] = true;
    treeArray[7] = false; leafArray[3] = true;
    treeArray[8] = false; leafArray[4] = true;
    BinaryTreeNode expected_binary_tree( treeArray , leafArray );
    ARIADNE_TESTING_EXPECT_EQUAL(expected_binary_tree, theBinaryTreeRoot);

    ARIADNE_TESTING_NOTE("Testing the depth of the binary tree");
    ARIADNE_TESTING_EXPECT_EQUAL(theBinaryTreeRoot.depth(), 3u);

    // !!!
    ARIADNE_TESTING_NOTE("Disable the left most enabled leaf, recombine the tree");
    theBinaryTreeRoot.right_node()->left_node()->left_node()->set_disabled();
    ARIADNE_TESTING_NOTE("A tree with enabled left-most leaf");
    leafArray[1] = false;
    BinaryTreeNode expected_binary_tree1( treeArray , leafArray );
    ARIADNE_TESTING_EXPECT_EQUAL(expected_binary_tree1, theBinaryTreeRoot);

    ARIADNE_TESTING_NOTE("A tree after recombination");
    theBinaryTreeRoot.recombine();
    expected_binary_tree1.right_node()->right_node()->make_leaf(true);
    ARIADNE_TESTING_EXPECT_EQUAL(expected_binary_tree1, theBinaryTreeRoot);

    // !!!
    ARIADNE_TESTING_NOTE("Testing the add_enabled( BinaryWord& ) method");
    ARIADNE_TESTING_NOTE("Adding enabled node to an enabled node, the former one is defined by an empty path");

    BinaryTreeNode theNewBinaryTreeRoot(true);
    BinaryWord binaryWord;
    theNewBinaryTreeRoot.add_enabled( binaryWord );
    ARIADNE_TESTING_NOTE("The binary tree should stay intact, i.e. consist of one enabled node");
    BinaryTreeNode expected_binary_tree2( true );
    ARIADNE_TESTING_EXPECT_EQUAL(expected_binary_tree2, theNewBinaryTreeRoot);

    ARIADNE_TESTING_NOTE("Adding enabled sub node, path: (true,false) to a disabled node");
    theNewBinaryTreeRoot.set_disabled();
    binaryWord.push_back(true);
    binaryWord.push_back(false);
    theNewBinaryTreeRoot.add_enabled( binaryWord );
    ARIADNE_TESTING_NOTE("The binary tree have five nodes, four leafs and one (true, false) is enabled");
    expected_binary_tree2.set_disabled();
    expected_binary_tree2.split();
    expected_binary_tree2.right_node()->split();
    expected_binary_tree2.right_node()->left_node()->set_enabled();
    ARIADNE_TESTING_EXPECT_EQUAL(expected_binary_tree2, theNewBinaryTreeRoot);

    ARIADNE_TESTING_NOTE("Adding enabled sub node, path: (true) to a disabled node");
    binaryWord = BinaryWord();
    binaryWord.push_back(true);
    theNewBinaryTreeRoot.add_enabled( binaryWord );
    ARIADNE_TESTING_NOTE("The binary tree have three nodes, two leafs and one (true) is enabled");
    expected_binary_tree2.right_node()->make_leaf(true);
    ARIADNE_TESTING_EXPECT_EQUAL(expected_binary_tree2, theNewBinaryTreeRoot);

    ARIADNE_TESTING_NOTE("Adding a deeper path below an already enabled leaf changes nothing");
    BinaryWord deeperEnabledPath;
    deeperEnabledPath.push_back(true);
    deeperEnabledPath.push_back(false);
    theNewBinaryTreeRoot.add_enabled(deeperEnabledPath);
    ARIADNE_TESTING_EXPECT_EQUAL(expected_binary_tree2, theNewBinaryTreeRoot);

    ARIADNE_TESTING_NOTE("Adding enabled sub node, path: (false, true) to a disabled node");
    binaryWord = BinaryWord();
    binaryWord.push_back(false);
    binaryWord.push_back(true);
    theNewBinaryTreeRoot.add_enabled( binaryWord );
    ARIADNE_TESTING_NOTE("The binary tree have five nodes, four leafs and two (false, true) and (true) are enabled");
    expected_binary_tree2.left_node()->split();
    expected_binary_tree2.left_node()->right_node()->set_enabled();
    ARIADNE_TESTING_EXPECT_EQUAL(expected_binary_tree2, theNewBinaryTreeRoot);

    ARIADNE_TESTING_NOTE("Testing add_enabled(subtree,path) early enabled and left path");
    BinaryTreeNode source_subtree(true);
    BinaryWord subtreePath("0");
    ARIADNE_TESTING_EXPECT_EQUAL(subtreePath.size(),1u);
    ARIADNE_TESTING_EXPECT_EQUAL(source_subtree.is_enabled(),true);

    BinaryTreeNode enabled_destination(true);
    enabled_destination.add_enabled(&source_subtree,subtreePath);
    ARIADNE_TESTING_EXPECT_EQUAL(enabled_destination.is_enabled(),true);

    BinaryTreeNode left_destination(false);
    ARIADNE_TESTING_EXPECT_EQUAL(left_destination.is_leaf(),true);
    ARIADNE_TESTING_EXPECT_EQUAL(left_destination.is_disabled(),true);
    left_destination.add_enabled(&source_subtree,subtreePath);
    ARIADNE_TESTING_EXPECT_EQUAL(left_destination.is_enabled(subtreePath),true);

    ARIADNE_TESTING_NOTE("Adding an enabled subtree along a right-hand path");
    BinaryWord rightSubtreePath;
    rightSubtreePath.push_back(true);
    BinaryTreeNode right_destination(false);
    right_destination.add_enabled(&source_subtree,rightSubtreePath);
    ARIADNE_TESTING_EXPECT_EQUAL(right_destination.is_leaf(),false);
    ARIADNE_TESTING_EXPECT_EQUAL(right_destination.left_node()->is_disabled(),true);
    ARIADNE_TESTING_EXPECT_EQUAL(right_destination.right_node()->is_enabled(),true);

    ARIADNE_TESTING_NOTE("Testing intersect/subset short-circuit outcomes");
    BinaryTreeNode disabled_subtree(false);
    disabled_subtree.split();
    BinaryTreeNode enabled_subtree(false);
    enabled_subtree.split();
    enabled_subtree.left_node()->set_enabled();
    BinaryTreeNode enabled_leaf_for_relation(true);
    BinaryTreeNode disabled_leaf_for_relation(false);

    ARIADNE_TESTING_EXPECT_EQUAL(BinaryTreeNode::intersect(&disabled_subtree,&enabled_leaf_for_relation),false);
    ARIADNE_TESTING_EXPECT_EQUAL(BinaryTreeNode::intersect(&enabled_leaf_for_relation,&disabled_subtree),false);
    ARIADNE_TESTING_EXPECT_EQUAL(BinaryTreeNode::subset(&enabled_subtree,&disabled_leaf_for_relation),false);

    ARIADNE_TESTING_NOTE("An entirely disabled subtree is a subset of a disabled leaf");
    ARIADNE_TESTING_EXPECT_EQUAL(disabled_subtree.is_leaf(),false);
    ARIADNE_TESTING_EXPECT_EQUAL(disabled_subtree.has_enabled(),false);
    ARIADNE_TESTING_EXPECT_EQUAL(BinaryTreeNode::subset(&disabled_subtree,&disabled_leaf_for_relation),true);
}


Void test_copy_and_reset() {
    BinaryTreeNode empty(BooleanArray{},BooleanArray{});
    BinaryTreeNode no_leaves(BooleanArray{false},BooleanArray{});
    ARIADNE_TESTING_PROVE(empty.is_disabled());
    ARIADNE_TESTING_PROVE(no_leaves.is_disabled());
    ARIADNE_TESTING_PROVE(empty.node_to_string().find("BinaryTreeNode")!=String::npos);

    BinaryTreeNode branch(false);
    branch.split();
    branch.left_node()->set_enabled();
    ARIADNE_TESTING_EXPECT_THROWS(NotALeafNodeException, branch.set_enabled());
    BinaryTreeNode copied(false);
    copied=branch;
    ARIADNE_TESTING_PROVE(copied==branch);
    copied.right_node()->set_enabled();
    ARIADNE_TESTING_PROVE(branch.right_node()->is_disabled());
    const BinaryTreeNode& alias=copied;
    copied=alias;
    ARIADNE_TESTING_PROVE(copied.all_enabled());
    copied=empty;
    ARIADNE_TESTING_PROVE(copied.is_leaf());
    ARIADNE_TESTING_PROVE(copied.is_disabled());
    copied=branch;
    copied.copy_from(&empty);
    ARIADNE_TESTING_PROVE(copied.is_leaf());
    copied.copy_from(&branch);
    copied.copy_from(&copied);
    ARIADNE_TESTING_PROVE(copied==branch);
    copied.copy_from(nullptr);
    ARIADNE_TESTING_PROVE(copied==branch);
    copied.set(true);
    ARIADNE_TESTING_PROVE(copied.is_leaf());
    ARIADNE_TESTING_PROVE(copied.left_node()==nullptr);
    ARIADNE_TESTING_PROVE(copied.right_node()==nullptr);
    copied.set(false);
    ARIADNE_TESTING_PROVE(copied.is_disabled());
    copied.split();
    copied.left_node()->set_enabled();
    ARIADNE_TESTING_PROVE(copied.has_enabled());
}

    Void test_independent_binary_prefix_semantics() {
        // No Grid or paving object is involved: only abstract binary paths.
        BinaryTreeNode full(true);
        BinaryTreeNode split_full(true);
        split_full.split();
        ARIADNE_TESTING_PROVE(!(full==split_full)); // Different structure.
        ARIADNE_TESTING_PROVE(BinaryTreeNode::subset(&full,&split_full));
        ARIADNE_TESTING_PROVE(BinaryTreeNode::subset(&split_full,&full));
        split_full.recombine();
        ARIADNE_TESTING_PROVE(full==split_full);

        BinaryTreeNode prefixes(false);
        prefixes.add_enabled(BinaryWord("01"));
        ARIADNE_TESTING_PROVE(prefixes.is_enabled(BinaryWord("01")));
        ARIADNE_TESTING_PROVE(prefixes.is_enabled(BinaryWord("0101")));
        ARIADNE_TESTING_PROVE(!prefixes.is_enabled(BinaryWord("00")));
        ARIADNE_TESTING_PROVE(!prefixes.is_enabled(BinaryWord("1")));
        prefixes.add_enabled(BinaryWord("0100")); // Already covered by "01".
        ARIADNE_TESTING_PROVE(prefixes.is_enabled(BinaryWord("0100")));
        ARIADNE_TESTING_PROVE(BinaryTreeNode::subset(&prefixes,&full));
        ARIADNE_TESTING_PROVE(!BinaryTreeNode::subset(&full,&prefixes));

        BinaryTreeNode disjoint(false);
        disjoint.add_enabled(BinaryWord("1"));
        ARIADNE_TESTING_PROVE(!BinaryTreeNode::intersect(&prefixes,&disjoint));
        disjoint.add_enabled(BinaryWord("011"));
        ARIADNE_TESTING_PROVE(BinaryTreeNode::intersect(&prefixes,&disjoint));
    }

    Void test_child_node_streaming_and_leaf_guards() {
        BinaryTreeNode node(false);
        node.split();
        ARIADNE_TESTING_NOTE("child_node selects the left or right child; on a leaf it returns nullptr");
        ARIADNE_TESTING_PROVE(node.child_node(false) == node.left_node());
        ARIADNE_TESTING_PROVE(node.child_node(true) == node.right_node());
        BinaryTreeNode leaf(true);
        ARIADNE_TESTING_PROVE(leaf.child_node(false) == nullptr);
        ARIADNE_TESTING_PROVE(leaf.child_node(true) == nullptr);

        ARIADNE_TESTING_NOTE("make_leaf on an existing leaf only sets the state");
        leaf.make_leaf(false);
        ARIADNE_TESTING_PROVE(leaf.is_leaf());
        ARIADNE_TESTING_PROVE(leaf.is_disabled());

        ARIADNE_TESTING_NOTE("set_disabled on a non-leaf node throws NotALeafNodeException");
        ARIADNE_TESTING_EXPECT_THROWS(NotALeafNodeException, node.set_disabled());

        ARIADNE_TESTING_NOTE("Streaming a node prints its structure and leaf words");
        node.left_node()->set_enabled();
        std::ostringstream stream;
        stream << node;
        ARIADNE_TESTING_EXPECT_EQUAL(stream.str(), String("BinaryTreeNode( Tree: 100, Leaves: 10)"));
    }

    Void test_tree_to_binary_words_and_leaf_count() {
        BinaryTreeNode root(false);
        root.split();
        root.left_node()->set_enabled();
        BinaryWord tree, leaves;
        root.tree_to_binary_words(tree, leaves);
        ARIADNE_TESTING_NOTE("A non-leaf serializes as 1 followed by the DFS words of its children");
        ARIADNE_TESTING_EXPECT_EQUAL(tree, BinaryWord("100"));
        ARIADNE_TESTING_EXPECT_EQUAL(leaves, BinaryWord("10"));

        BinaryTreeNode enabled_leaf(true);
        BinaryWord leaf_tree, leaf_leaves;
        enabled_leaf.tree_to_binary_words(leaf_tree, leaf_leaves);
        ARIADNE_TESTING_EXPECT_EQUAL(leaf_tree, BinaryWord("0"));
        ARIADNE_TESTING_EXPECT_EQUAL(leaf_leaves, BinaryWord("1"));

        ARIADNE_TESTING_EXPECT_EQUAL(BinaryTreeNode::count_enabled_leaf_nodes(&root), 1u);
        ARIADNE_TESTING_EXPECT_EQUAL(BinaryTreeNode::count_enabled_leaf_nodes(&enabled_leaf), 1u);
        BinaryTreeNode disabled_leaf(false);
        ARIADNE_TESTING_EXPECT_EQUAL(BinaryTreeNode::count_enabled_leaf_nodes(&disabled_leaf), 0u);
    }

    Void test_is_enabled_at_internal_node() {
        BinaryTreeNode root(false);
        root.split();
        root.left_node()->split();
        root.left_node()->left_node()->set_enabled();
        ARIADNE_TESTING_NOTE("A path ending at an internal node is decided by all_enabled() of that subtree");
        ARIADNE_TESTING_PROVE(!root.is_enabled(BinaryWord("0")));
        ARIADNE_TESTING_PROVE(!root.is_enabled(BinaryWord()));
        ARIADNE_TESTING_PROVE(!root.all_enabled());
        root.left_node()->right_node()->set_enabled();
        root.right_node()->set_enabled();
        ARIADNE_TESTING_PROVE(root.all_enabled());
        ARIADNE_TESTING_PROVE(root.is_enabled(BinaryWord("0")));
        ARIADNE_TESTING_PROVE(root.is_enabled(BinaryWord()));
    }

    Void test_restrict_and_remove() {
        BinaryTreeNode enabled_leaf(true);
        BinaryTreeNode disabled_leaf(false);

        ARIADNE_TESTING_NOTE("restrict: two leaves are AND-ed");
        BinaryTreeNode a(true);
        BinaryTreeNode::restrict(&a, &disabled_leaf);
        ARIADNE_TESTING_PROVE(a.is_disabled());
        BinaryTreeNode b(true), c(true);
        BinaryTreeNode::restrict(&b, &c);
        ARIADNE_TESTING_PROVE(b.is_enabled());

        ARIADNE_TESTING_NOTE("restrict: non-leaf against a disabled leaf becomes a disabled leaf");
        BinaryTreeNode d(false); d.split();
        BinaryTreeNode::restrict(&d, &disabled_leaf);
        ARIADNE_TESTING_PROVE(d.is_leaf());
        ARIADNE_TESTING_PROVE(d.is_disabled());

        ARIADNE_TESTING_NOTE("restrict: non-leaf against an enabled leaf is unchanged");
        BinaryTreeNode e(false); e.split();
        BinaryTreeNode::restrict(&e, &enabled_leaf);
        ARIADNE_TESTING_PROVE(!e.is_leaf());
        ARIADNE_TESTING_PROVE(e.left_node()->is_disabled());

        ARIADNE_TESTING_NOTE("restrict: enabled leaf against a non-leaf copies the non-leaf");
        BinaryTreeNode f(true);
        BinaryTreeNode g(false); g.split(); g.left_node()->set_enabled();
        BinaryTreeNode::restrict(&f, &g);
        ARIADNE_TESTING_PROVE(f == g);

        ARIADNE_TESTING_NOTE("restrict: disabled leaf against a non-leaf is unchanged");
        BinaryTreeNode h(false);
        BinaryTreeNode::restrict(&h, &g);
        ARIADNE_TESTING_PROVE(h.is_leaf());
        ARIADNE_TESTING_PROVE(h.is_disabled());

        ARIADNE_TESTING_NOTE("restrict: two non-leaves recurse");
        BinaryTreeNode i(false); i.split(); i.left_node()->set_enabled();
        BinaryTreeNode j(false); j.split();
        BinaryTreeNode::restrict(&i, &j);
        ARIADNE_TESTING_PROVE(!i.is_leaf());
        ARIADNE_TESTING_PROVE(i.left_node()->is_disabled());
        ARIADNE_TESTING_PROVE(i.right_node()->is_disabled());

        ARIADNE_TESTING_NOTE("remove: two enabled leaves, the first becomes disabled");
        BinaryTreeNode k(true);
        BinaryTreeNode::remove(&k, &enabled_leaf);
        ARIADNE_TESTING_PROVE(k.is_disabled());

        ARIADNE_TESTING_NOTE("remove: the other leaf combinations leave the node unchanged");
        BinaryTreeNode l(true);
        BinaryTreeNode::remove(&l, &disabled_leaf);
        ARIADNE_TESTING_PROVE(l.is_enabled());
        BinaryTreeNode m(false);
        BinaryTreeNode::remove(&m, &enabled_leaf);
        ARIADNE_TESTING_PROVE(m.is_disabled());

        ARIADNE_TESTING_NOTE("remove: non-leaf against an enabled leaf becomes a disabled leaf");
        BinaryTreeNode n(false); n.split();
        BinaryTreeNode::remove(&n, &enabled_leaf);
        ARIADNE_TESTING_PROVE(n.is_leaf());
        ARIADNE_TESTING_PROVE(n.is_disabled());

        ARIADNE_TESTING_NOTE("remove: non-leaf against a disabled leaf is unchanged");
        BinaryTreeNode o(false); o.split();
        BinaryTreeNode::remove(&o, &disabled_leaf);
        ARIADNE_TESTING_PROVE(!o.is_leaf());

        ARIADNE_TESTING_NOTE("remove: enabled leaf against a non-leaf is split and recursed");
        BinaryTreeNode p(true);
        BinaryTreeNode q(false); q.split();
        BinaryTreeNode::remove(&p, &q);
        ARIADNE_TESTING_PROVE(!p.is_leaf());
        ARIADNE_TESTING_PROVE(p.left_node()->is_enabled());
        ARIADNE_TESTING_PROVE(p.right_node()->is_enabled());

        ARIADNE_TESTING_NOTE("remove: disabled leaf against a non-leaf returns unchanged");
        BinaryTreeNode r(false);
        BinaryTreeNode::remove(&r, &q);
        ARIADNE_TESTING_PROVE(r.is_leaf());
        ARIADNE_TESTING_PROVE(r.is_disabled());

        ARIADNE_TESTING_NOTE("remove: two non-leaves recurse; enabled leaves survive a disabled other");
        BinaryTreeNode s(false); s.split(); s.left_node()->set_enabled();
        BinaryTreeNode t(false); t.split();
        BinaryTreeNode::remove(&s, &t);
        ARIADNE_TESTING_PROVE(!s.is_leaf());
        ARIADNE_TESTING_PROVE(s.left_node()->is_enabled());
    }

    Void test_add_enabled_static_overloads() {
        BinaryTreeNode enabled_leaf(true);
        BinaryTreeNode disabled_leaf(false);

        ARIADNE_TESTING_NOTE("Adding to an enabled leaf changes nothing");
        BinaryTreeNode to1(true);
        to1.add_enabled(&to1, &enabled_leaf);
        ARIADNE_TESTING_PROVE(to1.is_leaf());
        ARIADNE_TESTING_PROVE(to1.is_enabled());

        ARIADNE_TESTING_NOTE("Adding an enabled leaf to a disabled leaf enables it");
        BinaryTreeNode to2(false);
        to2.add_enabled(&to2, &enabled_leaf);
        ARIADNE_TESTING_PROVE(to2.is_enabled());

        ARIADNE_TESTING_NOTE("Adding a disabled leaf to a disabled leaf changes nothing");
        BinaryTreeNode to3(false);
        to3.add_enabled(&to3, &disabled_leaf);
        ARIADNE_TESTING_PROVE(to3.is_disabled());

        ARIADNE_TESTING_NOTE("Adding a non-leaf to a disabled leaf copies the subtree");
        BinaryTreeNode from_subtree(false); from_subtree.split(); from_subtree.left_node()->set_enabled();
        BinaryTreeNode to4(false);
        to4.add_enabled(&to4, &from_subtree);
        ARIADNE_TESTING_PROVE(to4 == from_subtree);
        ARIADNE_TESTING_PROVE(to4.left_node()->is_enabled());

        ARIADNE_TESTING_NOTE("Adding an enabled leaf to a non-leaf makes it an enabled leaf");
        BinaryTreeNode to5(false); to5.split();
        to5.add_enabled(&to5, &enabled_leaf);
        ARIADNE_TESTING_PROVE(to5.is_leaf());
        ARIADNE_TESTING_PROVE(to5.is_enabled());

        ARIADNE_TESTING_NOTE("Adding a disabled leaf to a non-leaf changes nothing");
        BinaryTreeNode to6(false); to6.split();
        to6.add_enabled(&to6, &disabled_leaf);
        ARIADNE_TESTING_PROVE(!to6.is_leaf());
        ARIADNE_TESTING_PROVE(to6.left_node()->is_disabled());

        ARIADNE_TESTING_NOTE("Adding a non-leaf to a non-leaf recurses into the children");
        BinaryTreeNode to7(false); to7.split();
        to7.add_enabled(&to7, &from_subtree);
        ARIADNE_TESTING_PROVE(!to7.is_leaf());
        ARIADNE_TESTING_PROVE(to7.left_node()->is_enabled());
        ARIADNE_TESTING_PROVE(to7.right_node()->is_disabled());
    }

    Void test_prepend_tree() {
        ARIADNE_TESTING_NOTE("prepend_tree with a one-bit path grafts the old root on that side");
        BinaryTreeNode* left_graft = BinaryTreeNode::prepend_tree(BinaryWord("0"), new BinaryTreeNode(true));
        ARIADNE_TESTING_PROVE(left_graft->left_node()->is_enabled());
        ARIADNE_TESTING_PROVE(left_graft->right_node()->is_disabled());
        ARIADNE_TESTING_PROVE(left_graft->depth() == 1u);
        delete left_graft;

        BinaryTreeNode* right_graft = BinaryTreeNode::prepend_tree(BinaryWord("1"), new BinaryTreeNode(true));
        ARIADNE_TESTING_PROVE(right_graft->right_node()->is_enabled());
        ARIADNE_TESTING_PROVE(right_graft->left_node()->is_disabled());
        delete right_graft;

        ARIADNE_TESTING_NOTE("prepend_tree with a two-bit path walks the loop and grafts at the end");
        BinaryTreeNode* deep = BinaryTreeNode::prepend_tree(BinaryWord("10"), new BinaryTreeNode(true));
        ARIADNE_TESTING_PROVE(deep->depth() == 2u);
        ARIADNE_TESTING_PROVE(deep->is_enabled(BinaryWord("10")));
        ARIADNE_TESTING_PROVE(!deep->is_enabled(BinaryWord("11")));
        delete deep;

        BinaryTreeNode* deep_alt = BinaryTreeNode::prepend_tree(BinaryWord("01"), new BinaryTreeNode(true));
        ARIADNE_TESTING_PROVE(deep_alt->is_enabled(BinaryWord("01")));
        ARIADNE_TESTING_PROVE(!deep_alt->is_enabled(BinaryWord("00")));
        delete deep_alt;
    }

    Void test_intersect_and_subset_full_matrix() {
        BinaryTreeNode enabled_leaf(true);
        BinaryTreeNode disabled_leaf(false);
        BinaryTreeNode partial(false); partial.split(); partial.left_node()->set_enabled();
        BinaryTreeNode full(false); full.split(); full.left_node()->set_enabled(); full.right_node()->set_enabled();

        ARIADNE_TESTING_NOTE("intersect: both leaves require both enabled");
        ARIADNE_TESTING_PROVE(BinaryTreeNode::intersect(&enabled_leaf, &enabled_leaf));
        ARIADNE_TESTING_PROVE(!BinaryTreeNode::intersect(&enabled_leaf, &disabled_leaf));
        ARIADNE_TESTING_PROVE(!BinaryTreeNode::intersect(&disabled_leaf, &enabled_leaf));

        ARIADNE_TESTING_NOTE("intersect: one leaf and one non-leaf");
        ARIADNE_TESTING_PROVE(BinaryTreeNode::intersect(&partial, &enabled_leaf));
        ARIADNE_TESTING_PROVE(!BinaryTreeNode::intersect(&partial, &disabled_leaf));
        ARIADNE_TESTING_PROVE(BinaryTreeNode::intersect(&enabled_leaf, &partial));
        ARIADNE_TESTING_PROVE(!BinaryTreeNode::intersect(&disabled_leaf, &partial));

        ARIADNE_TESTING_NOTE("subset: both leaves");
        ARIADNE_TESTING_PROVE(BinaryTreeNode::subset(&disabled_leaf, &enabled_leaf));
        ARIADNE_TESTING_PROVE(BinaryTreeNode::subset(&enabled_leaf, &enabled_leaf));
        ARIADNE_TESTING_PROVE(!BinaryTreeNode::subset(&enabled_leaf, &disabled_leaf));
        ARIADNE_TESTING_PROVE(BinaryTreeNode::subset(&disabled_leaf, &disabled_leaf));

        ARIADNE_TESTING_NOTE("subset: leaf against non-leaf uses all_enabled of the non-leaf");
        ARIADNE_TESTING_PROVE(BinaryTreeNode::subset(&enabled_leaf, &full));
        ARIADNE_TESTING_PROVE(!BinaryTreeNode::subset(&enabled_leaf, &partial));
        ARIADNE_TESTING_PROVE(BinaryTreeNode::subset(&disabled_leaf, &partial));

        ARIADNE_TESTING_NOTE("subset: two non-leaves recurse");
        ARIADNE_TESTING_PROVE(BinaryTreeNode::subset(&partial, &full));
        ARIADNE_TESTING_PROVE(!BinaryTreeNode::subset(&full, &partial));
        ARIADNE_TESTING_PROVE(BinaryTreeNode::subset(&partial, &partial));
        BinaryTreeNode empty_split(false); empty_split.split();
        ARIADNE_TESTING_PROVE(!BinaryTreeNode::subset(&partial, &empty_split));
    }

};

int main(int argc, char** argv) {
    ARIADNE_TESTING_SUITE("test_binary_tree");
    ARIADNE_TESTING_ADD_TEST(TestBinaryTree, test_independent_binary_prefix_semantics);
    ARIADNE_TESTING_ADD_TEST(TestBinaryTree, test_copy_and_reset);
    ARIADNE_TESTING_ADD_TEST(TestBinaryTree, test_child_node_streaming_and_leaf_guards);
    ARIADNE_TESTING_ADD_TEST(TestBinaryTree, test_tree_to_binary_words_and_leaf_count);
    ARIADNE_TESTING_ADD_TEST(TestBinaryTree, test_is_enabled_at_internal_node);
    ARIADNE_TESTING_ADD_TEST(TestBinaryTree, test_restrict_and_remove);
    ARIADNE_TESTING_ADD_TEST(TestBinaryTree, test_add_enabled_static_overloads);
    ARIADNE_TESTING_ADD_TEST(TestBinaryTree, test_prepend_tree);
    ARIADNE_TESTING_ADD_TEST(TestBinaryTree, test_intersect_and_subset_full_matrix);
    ARIADNE_TESTING_ADD_TEST(TestBinaryTree, test_binary_tree);
    return ARIADNE_TESTING_RUN_TESTS_WITH_ARGS(argc, argv);
}

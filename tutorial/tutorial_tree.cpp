#include <iostream>
#include "ariadne-tree.hpp"
using namespace Ariadne;
int main() {
    BinaryTreeNode prefixes(false);
    prefixes.add_enabled(BinaryWord("01"));
    if(!prefixes.is_enabled(BinaryWord("010")) || prefixes.is_enabled(BinaryWord("11"))) return 1;
    BinaryTreeNode all(true);
    all.split();
    if(!BinaryTreeNode::subset(&prefixes,&all) || BinaryTreeNode::subset(&all,&prefixes)) return 2;
    all.recombine();
    if(!all.is_leaf() || !all.is_enabled()) return 3;
    std::cout << "Ariadne Tree tutorial passed.\n";
    return 0;
}

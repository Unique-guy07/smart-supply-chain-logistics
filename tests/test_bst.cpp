#include "test_utils.h"
#include "inventory/BST.h"
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Test: Empty tree state and empty traversal snapshots
// ---------------------------------------------------------------------------
void testEmptyTree()
{
    BST tree;
    TEST_CHECK(tree.isEmpty());
    TEST_CHECK(tree.getCount() == 0);
    TEST_CHECK(!tree.contains("SKU-001"));

    const std::vector<std::string> empty;
    TEST_CHECK(tree.inorder() == empty);
    TEST_CHECK(tree.preorder() == empty);
    TEST_CHECK(tree.postorder() == empty);
}

// ---------------------------------------------------------------------------
// Test: Single insertion and search
// ---------------------------------------------------------------------------
void testSingleInsertionAndSearch()
{
    BST tree;

    TEST_CHECK(tree.insert("SKU-001"));
    TEST_CHECK(!tree.isEmpty());
    TEST_CHECK(tree.getCount() == 1);
    TEST_CHECK(tree.contains("SKU-001"));
    TEST_CHECK(!tree.contains("SKU-999"));

    const std::vector<std::string> expected{"SKU-001"};
    TEST_CHECK(tree.inorder() == expected);
    TEST_CHECK(tree.preorder() == expected);
    TEST_CHECK(tree.postorder() == expected);
}

// ---------------------------------------------------------------------------
// Test: Multiple insertion and all traversal orders
// ---------------------------------------------------------------------------
void testMultipleInsertionsAndTraversals()
{
    BST tree;
    TEST_CHECK(tree.insert("M"));
    TEST_CHECK(tree.insert("C"));
    TEST_CHECK(tree.insert("T"));
    TEST_CHECK(tree.insert("A"));
    TEST_CHECK(tree.insert("E"));
    TEST_CHECK(tree.insert("R"));
    TEST_CHECK(tree.insert("Z"));

    TEST_CHECK(tree.getCount() == 7);
    TEST_CHECK(tree.contains("A"));
    TEST_CHECK(tree.contains("E"));
    TEST_CHECK(tree.contains("R"));
    TEST_CHECK(tree.contains("Z"));
    TEST_CHECK(!tree.contains("Q"));

    const std::vector<std::string> expectedInorder{
        "A", "C", "E", "M", "R", "T", "Z"};
    const std::vector<std::string> expectedPreorder{
        "M", "C", "A", "E", "T", "R", "Z"};
    const std::vector<std::string> expectedPostorder{
        "A", "E", "C", "R", "Z", "T", "M"};

    TEST_CHECK(tree.inorder() == expectedInorder);
    TEST_CHECK(tree.preorder() == expectedPreorder);
    TEST_CHECK(tree.postorder() == expectedPostorder);
}

// ---------------------------------------------------------------------------
// Test: Duplicate IDs are rejected without changing count
// ---------------------------------------------------------------------------
void testDuplicateInsertion()
{
    BST tree;
    TEST_CHECK(tree.insert("SKU-001"));
    TEST_CHECK(!tree.insert("SKU-001"));
    TEST_CHECK(tree.getCount() == 1);
    TEST_CHECK(tree.contains("SKU-001"));
}

// ---------------------------------------------------------------------------
// Test: Leaf deletion
// ---------------------------------------------------------------------------
void testLeafDeletion()
{
    BST tree;
    tree.insert("M");
    tree.insert("C");
    tree.insert("T");
    tree.insert("A");

    TEST_CHECK(tree.remove("A"));
    TEST_CHECK(tree.getCount() == 3);
    TEST_CHECK(!tree.contains("A"));
    TEST_CHECK(tree.contains("C"));
    TEST_CHECK(tree.contains("M"));
    TEST_CHECK(tree.contains("T"));

    const std::vector<std::string> expected{"C", "M", "T"};
    TEST_CHECK(tree.inorder() == expected);
}

// ---------------------------------------------------------------------------
// Test: One-child deletion
// ---------------------------------------------------------------------------
void testOneChildDeletion()
{
    BST tree;
    tree.insert("M");
    tree.insert("C");
    tree.insert("A");

    TEST_CHECK(tree.remove("C"));
    TEST_CHECK(tree.getCount() == 2);
    TEST_CHECK(!tree.contains("C"));
    TEST_CHECK(tree.contains("A"));
    TEST_CHECK(tree.contains("M"));

    const std::vector<std::string> expected{"A", "M"};
    TEST_CHECK(tree.inorder() == expected);
}

// ---------------------------------------------------------------------------
// Test: Two-child deletion uses an in-order successor
// ---------------------------------------------------------------------------
void testTwoChildDeletion()
{
    BST tree;
    tree.insert("M");
    tree.insert("C");
    tree.insert("T");
    tree.insert("A");
    tree.insert("E");
    tree.insert("R");
    tree.insert("Z");

    TEST_CHECK(tree.remove("C"));
    TEST_CHECK(tree.getCount() == 6);
    TEST_CHECK(!tree.contains("C"));
    TEST_CHECK(tree.contains("A"));
    TEST_CHECK(tree.contains("E"));
    TEST_CHECK(tree.contains("M"));
    TEST_CHECK(tree.contains("R"));
    TEST_CHECK(tree.contains("T"));
    TEST_CHECK(tree.contains("Z"));

    const std::vector<std::string> expected{"A", "E", "M", "R", "T", "Z"};
    TEST_CHECK(tree.inorder() == expected);
}

// ---------------------------------------------------------------------------
// Test: Root deletion
// ---------------------------------------------------------------------------
void testRootDeletion()
{
    BST tree;
    tree.insert("M");
    tree.insert("C");
    tree.insert("T");
    tree.insert("A");
    tree.insert("E");
    tree.insert("R");
    tree.insert("Z");

    TEST_CHECK(tree.remove("M"));
    TEST_CHECK(tree.getCount() == 6);
    TEST_CHECK(!tree.contains("M"));
    TEST_CHECK(tree.contains("A"));
    TEST_CHECK(tree.contains("C"));
    TEST_CHECK(tree.contains("E"));
    TEST_CHECK(tree.contains("R"));
    TEST_CHECK(tree.contains("T"));
    TEST_CHECK(tree.contains("Z"));

    const std::vector<std::string> expected{"A", "C", "E", "R", "T", "Z"};
    TEST_CHECK(tree.inorder() == expected);
}

// ---------------------------------------------------------------------------
// Test: Missing deletion leaves the tree unchanged
// ---------------------------------------------------------------------------
void testMissingDeletion()
{
    BST tree;
    tree.insert("B");
    tree.insert("A");
    tree.insert("C");

    const std::vector<std::string> before = tree.inorder();
    TEST_CHECK(!tree.remove("X"));
    TEST_CHECK(tree.getCount() == 3);
    TEST_CHECK(tree.inorder() == before);
}

// ---------------------------------------------------------------------------
// Test: Destructor safely cleans up all owned nodes
// ---------------------------------------------------------------------------
void testDestructorCleanup()
{
    {
        BST tree;
        tree.insert("M");
        tree.insert("C");
        tree.insert("T");
        tree.insert("A");
        tree.insert("E");
        tree.insert("R");
        tree.insert("Z");
    }

    TEST_CHECK(true);
}

// ---------------------------------------------------------------------------
// Test: Deletion of a node with only a right child
// ---------------------------------------------------------------------------
void testDeletionNodeWithOnlyRightChild()
{
    BST tree;
    // Construct BST:
    //         M
    //        / \
    //       C   T
    //        \
    //         E
    // Node 'C' is a non-root node with no left child and only a right child 'E'.
    tree.insert("M");
    tree.insert("C");
    tree.insert("T");
    tree.insert("E");

    TEST_CHECK(tree.getCount() == 4);
    TEST_CHECK(tree.contains("C"));
    TEST_CHECK(tree.contains("E"));

    TEST_CHECK(tree.remove("C"));
    TEST_CHECK(tree.getCount() == 3);
    TEST_CHECK(!tree.contains("C"));
    TEST_CHECK(tree.contains("E"));
    TEST_CHECK(tree.contains("M"));
    TEST_CHECK(tree.contains("T"));

    const std::vector<std::string> expected{"E", "M", "T"};
    TEST_CHECK(tree.inorder() == expected);
}

// ---------------------------------------------------------------------------
// Test: Two-child deletion where the inorder successor has a right child
// ---------------------------------------------------------------------------
void testTwoChildDeletionSuccessorWithRightChild()
{
    // Case 1: Root deletion with two children where successor has a right child
    {
        BST tree;
        // Tree structure:
        //              M
        //           /     \
        //          D       T
        //         / \     / \
        //        B   F   P   X
        //                 \
        //                  R
        // Node to delete: "M" (two children: "D" and "T")
        // Successor in right subtree: "P" (left of "T", no left child)
        // Successor "P" is not a leaf; it has right child "R".
        tree.insert("M");
        tree.insert("D");
        tree.insert("T");
        tree.insert("B");
        tree.insert("F");
        tree.insert("P");
        tree.insert("X");
        tree.insert("R");

        TEST_CHECK(tree.getCount() == 8);
        TEST_CHECK(tree.contains("M"));
        TEST_CHECK(tree.contains("P"));
        TEST_CHECK(tree.contains("R"));

        TEST_CHECK(tree.remove("M"));
        TEST_CHECK(tree.getCount() == 7);
        TEST_CHECK(!tree.contains("M"));

        TEST_CHECK(tree.contains("P"));
        TEST_CHECK(tree.contains("R"));
        TEST_CHECK(tree.contains("B"));
        TEST_CHECK(tree.contains("D"));
        TEST_CHECK(tree.contains("F"));
        TEST_CHECK(tree.contains("T"));
        TEST_CHECK(tree.contains("X"));

        const std::vector<std::string> expected{
            "B", "D", "F", "P", "R", "T", "X"};
        TEST_CHECK(tree.inorder() == expected);
    }

    // Case 2: Non-root node deletion with two children where successor has a right child
    {
        BST tree;
        // Tree structure:
        //              ROOT (R)
        //             /        \
        //            G          T
        //          /   \
        //         B     L
        //              / \
        //             I   P
        //              \
        //               J
        // Node to delete: "G" (two children: "B" and "L")
        // Inorder successor of "G" is "I" (minimum in "L"'s subtree).
        // "I" is not a leaf; it has right child "J".
        tree.insert("R");
        tree.insert("G");
        tree.insert("T");
        tree.insert("B");
        tree.insert("L");
        tree.insert("I");
        tree.insert("P");
        tree.insert("J");

        TEST_CHECK(tree.getCount() == 8);
        TEST_CHECK(tree.contains("G"));
        TEST_CHECK(tree.contains("I"));
        TEST_CHECK(tree.contains("J"));

        TEST_CHECK(tree.remove("G"));
        TEST_CHECK(tree.getCount() == 7);
        TEST_CHECK(!tree.contains("G"));

        TEST_CHECK(tree.contains("I"));
        TEST_CHECK(tree.contains("J"));
        TEST_CHECK(tree.contains("B"));
        TEST_CHECK(tree.contains("L"));
        TEST_CHECK(tree.contains("P"));
        TEST_CHECK(tree.contains("R"));
        TEST_CHECK(tree.contains("T"));

        const std::vector<std::string> expected{
            "B", "I", "J", "L", "P", "R", "T"};
        TEST_CHECK(tree.inorder() == expected);
    }
}

// ---------------------------------------------------------------------------
// Test: Root deletion with zero or one child
// ---------------------------------------------------------------------------
void testRootDeletionZeroOrOneChild()
{
    // Root is the only node (zero children)
    {
        BST tree;
        tree.insert("M");
        TEST_CHECK(tree.getCount() == 1);
        TEST_CHECK(tree.contains("M"));

        TEST_CHECK(tree.remove("M"));
        TEST_CHECK(tree.isEmpty());
        TEST_CHECK(tree.getCount() == 0);
        TEST_CHECK(!tree.contains("M"));

        const std::vector<std::string> empty;
        TEST_CHECK(tree.inorder() == empty);
    }

    // Root has exactly one child (left child only)
    {
        BST tree;
        tree.insert("M");
        tree.insert("C");
        tree.insert("A");

        TEST_CHECK(tree.getCount() == 3);
        TEST_CHECK(tree.contains("M"));

        TEST_CHECK(tree.remove("M"));
        TEST_CHECK(tree.getCount() == 2);
        TEST_CHECK(!tree.contains("M"));
        TEST_CHECK(tree.contains("C"));
        TEST_CHECK(tree.contains("A"));

        const std::vector<std::string> expected{"A", "C"};
        TEST_CHECK(tree.inorder() == expected);
    }

    // Root has exactly one child (right child only)
    {
        BST tree;
        tree.insert("M");
        tree.insert("T");
        tree.insert("Z");

        TEST_CHECK(tree.getCount() == 3);
        TEST_CHECK(tree.contains("M"));

        TEST_CHECK(tree.remove("M"));
        TEST_CHECK(tree.getCount() == 2);
        TEST_CHECK(!tree.contains("M"));
        TEST_CHECK(tree.contains("T"));
        TEST_CHECK(tree.contains("Z"));

        const std::vector<std::string> expected{"T", "Z"};
        TEST_CHECK(tree.inorder() == expected);
    }
}

// ---------------------------------------------------------------------------
// M5 Test: Iterative traversals on empty and single-element trees
// ---------------------------------------------------------------------------
void testIterativeTraversalsEmptyAndSingle()
{
    // Empty tree
    {
        BST tree;
        const std::vector<std::string> empty;
        TEST_CHECK(tree.inorderIterative() == empty);
        TEST_CHECK(tree.preorderIterative() == empty);
        TEST_CHECK(tree.inorderIterative() == tree.inorder());
        TEST_CHECK(tree.preorderIterative() == tree.preorder());
    }

    // Single element tree
    {
        BST tree;
        tree.insert("SKU-001");
        const std::vector<std::string> expected{"SKU-001"};
        TEST_CHECK(tree.inorderIterative() == expected);
        TEST_CHECK(tree.preorderIterative() == expected);
        TEST_CHECK(tree.inorderIterative() == tree.inorder());
        TEST_CHECK(tree.preorderIterative() == tree.preorder());
    }
}

// ---------------------------------------------------------------------------
// M5 Test: Iterative traversals on balanced and skewed trees
// ---------------------------------------------------------------------------
void testIterativeTraversalsBalancedAndSkewed()
{
    // Balanced tree
    {
        BST tree;
        tree.insert("M");
        tree.insert("C");
        tree.insert("T");
        tree.insert("A");
        tree.insert("E");
        tree.insert("R");
        tree.insert("Z");

        TEST_CHECK(tree.inorderIterative() == tree.inorder());
        TEST_CHECK(tree.preorderIterative() == tree.preorder());

        const std::vector<std::string> expectedIn{"A", "C", "E", "M", "R", "T", "Z"};
        const std::vector<std::string> expectedPre{"M", "C", "A", "E", "T", "R", "Z"};
        TEST_CHECK(tree.inorderIterative() == expectedIn);
        TEST_CHECK(tree.preorderIterative() == expectedPre);
    }

    // Skewed left tree: E -> D -> C -> B -> A
    {
        BST tree;
        tree.insert("E");
        tree.insert("D");
        tree.insert("C");
        tree.insert("B");
        tree.insert("A");

        TEST_CHECK(tree.inorderIterative() == tree.inorder());
        TEST_CHECK(tree.preorderIterative() == tree.preorder());
        const std::vector<std::string> expectedIn{"A", "B", "C", "D", "E"};
        const std::vector<std::string> expectedPre{"E", "D", "C", "B", "A"};
        TEST_CHECK(tree.inorderIterative() == expectedIn);
        TEST_CHECK(tree.preorderIterative() == expectedPre);
    }

    // Skewed right tree: A -> B -> C -> D -> E
    {
        BST tree;
        tree.insert("A");
        tree.insert("B");
        tree.insert("C");
        tree.insert("D");
        tree.insert("E");

        TEST_CHECK(tree.inorderIterative() == tree.inorder());
        TEST_CHECK(tree.preorderIterative() == tree.preorder());
        const std::vector<std::string> expectedIn{"A", "B", "C", "D", "E"};
        const std::vector<std::string> expectedPre{"A", "B", "C", "D", "E"};
        TEST_CHECK(tree.inorderIterative() == expectedIn);
        TEST_CHECK(tree.preorderIterative() == expectedPre);
    }
}

// ---------------------------------------------------------------------------
// M5 Test: Tree serialization and deserialization round-trip
// ---------------------------------------------------------------------------
void testSerializationAndDeserializationRoundTrip()
{
    // Empty tree round trip
    {
        BST tree;
        TEST_CHECK(tree.serialize().empty());
        TEST_CHECK(tree.deserialize(""));
        TEST_CHECK(tree.isEmpty());
        TEST_CHECK(tree.getCount() == 0);
    }

    // Balanced tree round trip
    {
        BST tree;
        tree.insert("M");
        tree.insert("C");
        tree.insert("T");
        tree.insert("A");
        tree.insert("E");
        tree.insert("R");
        tree.insert("Z");

        std::string serialized = tree.serialize();
        TEST_CHECK(serialized == "1:M;1:C;1:A;1:E;1:T;1:R;1:Z;");

        BST tree2;
        TEST_CHECK(tree2.deserialize(serialized));
        TEST_CHECK(tree2.getCount() == 7);
        TEST_CHECK(tree2.inorder() == tree.inorder());
        TEST_CHECK(tree2.preorder() == tree.preorder());
        TEST_CHECK(tree2.postorder() == tree.postorder());
        TEST_CHECK(tree2.inorderIterative() == tree.inorderIterative());
        TEST_CHECK(tree2.preorderIterative() == tree.preorderIterative());

        // Verify key searchability
        TEST_CHECK(tree2.contains("M"));
        TEST_CHECK(tree2.contains("A"));
        TEST_CHECK(tree2.contains("Z"));
        TEST_CHECK(!tree2.contains("Q"));
    }

    // Product IDs containing commas, spaces, tabs, newlines, colons, and semicolons
    {
        BST tree;
        TEST_CHECK(tree.insert("SKU,1"));
        TEST_CHECK(tree.insert("SKU 2"));
        TEST_CHECK(tree.insert("SKU\t3"));
        TEST_CHECK(tree.insert("SKU\n4"));
        TEST_CHECK(tree.insert("SKU:5;XYZ"));

        std::string serialized = tree.serialize();
        BST tree2;
        TEST_CHECK(tree2.deserialize(serialized));
        TEST_CHECK(tree2.getCount() == 5);
        TEST_CHECK(tree2.inorder() == tree.inorder());
        TEST_CHECK(tree2.preorder() == tree.preorder());
        TEST_CHECK(tree2.postorder() == tree.postorder());

        TEST_CHECK(tree2.contains("SKU,1"));
        TEST_CHECK(tree2.contains("SKU 2"));
        TEST_CHECK(tree2.contains("SKU\t3"));
        TEST_CHECK(tree2.contains("SKU\n4"));
        TEST_CHECK(tree2.contains("SKU:5;XYZ"));
    }

    // Empty product ID round-trip
    {
        BST tree;
        TEST_CHECK(tree.insert(""));
        TEST_CHECK(tree.insert("B"));
        TEST_CHECK(tree.insert("A"));

        std::string serialized = tree.serialize();
        TEST_CHECK(serialized == "0:;1:B;1:A;");

        BST tree2;
        TEST_CHECK(tree2.deserialize(serialized));
        TEST_CHECK(tree2.getCount() == 3);
        TEST_CHECK(tree2.contains(""));
        TEST_CHECK(tree2.contains("B"));
        TEST_CHECK(tree2.contains("A"));
        TEST_CHECK(tree2.inorder() == tree.inorder());
        TEST_CHECK(tree2.preorder() == tree.preorder());
    }
}

// ---------------------------------------------------------------------------
// M5 Test: Deserialization validation and error handling
// ---------------------------------------------------------------------------
void testDeserializationValidationRejection()
{
    // 1. Out-of-order pre-order sequence violating BST ordering:
    // In "1:M;1:T;1:C;", "C" appears after "T", but all nodes with key < "M" must appear
    // before any node with key > "M".
    {
        BST tree;
        tree.insert("PRESERVE_ME");
        TEST_CHECK(!tree.deserialize("1:M;1:T;1:C;"));
        // Strong guarantee: tree is untouched on error
        TEST_CHECK(tree.getCount() == 1);
        TEST_CHECK(tree.contains("PRESERVE_ME"));
    }

    // 2. Duplicate key in sequence
    {
        BST tree;
        tree.insert("PRESERVE_ME");
        TEST_CHECK(!tree.deserialize("1:M;1:C;1:M;"));
        TEST_CHECK(tree.getCount() == 1);
        TEST_CHECK(tree.contains("PRESERVE_ME"));
    }

    // 3. Truncated or malformed framing delimiters
    {
        BST tree;
        tree.insert("PRESERVE_ME");

        // Payload shorter than declared length
        TEST_CHECK(!tree.deserialize("5:ABC;"));
        // Missing trailing delimiter
        TEST_CHECK(!tree.deserialize("3:ABC"));
        // Non-digit length prefix
        TEST_CHECK(!tree.deserialize("abc:M;"));
        TEST_CHECK(!tree.deserialize(":M;"));
        TEST_CHECK(!tree.deserialize("-1:M;"));
        // Missing colon
        TEST_CHECK(!tree.deserialize("1M;"));
        // Trailing garbage
        TEST_CHECK(!tree.deserialize("1:M;junk"));
        // Old comma-separated format must be rejected safely
        TEST_CHECK(!tree.deserialize("M,C,T"));
        TEST_CHECK(!tree.deserialize(",,"));

        // Verify original tree untouched after all malformed attempts
        TEST_CHECK(tree.getCount() == 1);
        TEST_CHECK(tree.contains("PRESERVE_ME"));
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    TEST_RUN(testEmptyTree);
    TEST_RUN(testSingleInsertionAndSearch);
    TEST_RUN(testMultipleInsertionsAndTraversals);
    TEST_RUN(testDuplicateInsertion);
    TEST_RUN(testLeafDeletion);
    TEST_RUN(testOneChildDeletion);
    TEST_RUN(testDeletionNodeWithOnlyRightChild);
    TEST_RUN(testTwoChildDeletion);
    TEST_RUN(testTwoChildDeletionSuccessorWithRightChild);
    TEST_RUN(testRootDeletion);
    TEST_RUN(testRootDeletionZeroOrOneChild);
    TEST_RUN(testMissingDeletion);
    TEST_RUN(testDestructorCleanup);
    // M5 Additions
    TEST_RUN(testIterativeTraversalsEmptyAndSingle);
    TEST_RUN(testIterativeTraversalsBalancedAndSkewed);
    TEST_RUN(testSerializationAndDeserializationRoundTrip);
    TEST_RUN(testDeserializationValidationRejection);
    TEST_REPORT();
}

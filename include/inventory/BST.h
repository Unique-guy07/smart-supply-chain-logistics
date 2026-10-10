#pragma once

#include <string>
#include <vector>

/// Unbalanced binary search tree used as the ordered Product-ID index.
///
/// The tree owns only copied product IDs. Product records remain owned by
/// HashTable, which is the inventory subsystem's primary lookup index.
/// IDs use normal lexicographic std::string ordering.
class BST
{
private:
    struct Node
    {
        std::string productId;
        Node* left;
        Node* right;

        explicit Node(const std::string& id);
    };

    Node* root;
    int count;

    Node* remove(Node* node, const std::string& productId, bool& removed);
    Node* detachMin(Node*& node);
    static void clear(Node* node);

    void inorder(const Node* node, std::vector<std::string>& result) const;
    void preorder(const Node* node, std::vector<std::string>& result) const;
    void postorder(const Node* node, std::vector<std::string>& result) const;

    static Node* buildFromPreorder(const std::vector<std::string>& tokens,
                                   std::size_t& index,
                                   const std::string* minBound,
                                   const std::string* maxBound,
                                   int& nodeCount);

public:
    BST();
    ~BST();

    // Non-copyable, non-movable because the tree owns raw Node pointers.
    BST(const BST&) = delete;
    BST& operator=(const BST&) = delete;
    BST(BST&&) = delete;
    BST& operator=(BST&&) = delete;

    /// Insert an ID. Returns false when the ID already exists.
    bool insert(const std::string& productId);

    /// Search for an ID in the ordered index.
    bool contains(const std::string& productId) const;

    /// Remove an ID. Returns false when the ID does not exist.
    bool remove(const std::string& productId);

    int getCount() const;
    bool isEmpty() const;

    /// Return traversal snapshots of the product-ID index.
    std::vector<std::string> inorder() const;
    std::vector<std::string> preorder() const;
    std::vector<std::string> postorder() const;

    /// Non-recursive in-order traversal using custom LIFO Stack.
    std::vector<std::string> inorderIterative() const;

    /// Non-recursive pre-order traversal using custom LIFO Stack.
    std::vector<std::string> preorderIterative() const;

    /// Serializes BST state into an unambiguous length-prefixed pre-order string (<len>:<key>;).
    std::string serialize() const;

    /// Deserializes and validates a BST from pre-order serialized data.
    /// Replaces the current tree on success; leaves current tree untouched on failure.
    /// @return true if valid and successfully reconstructed; false if malformed or duplicate.
    bool deserialize(const std::string& data);
};

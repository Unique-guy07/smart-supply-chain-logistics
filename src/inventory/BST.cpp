#include "inventory/BST.h"
#include "order/Stack.h"
#include <algorithm>
#include <cctype>

// ---------------------------------------------------------------------------
// Node and lifecycle
// ---------------------------------------------------------------------------

BST::Node::Node(const std::string& id)
    : productId(id)
    , left(nullptr)
    , right(nullptr)
{
}

BST::BST()
    : root(nullptr)
    , count(0)
{
}

BST::~BST()
{
    clear(root);
    root = nullptr;
    count = 0;
}

// ---------------------------------------------------------------------------
// Insert and search
// ---------------------------------------------------------------------------

bool BST::insert(const std::string& productId)
{
    Node** current = &root;

    while (*current != nullptr) {
        if (productId == (*current)->productId) {
            return false;
        }

        if (productId < (*current)->productId) {
            current = &((*current)->left);
        } else {
            current = &((*current)->right);
        }
    }

    *current = new Node(productId);
    ++count;
    return true;
}

bool BST::contains(const std::string& productId) const
{
    const Node* current = root;

    while (current != nullptr) {
        if (productId == current->productId) {
            return true;
        }

        current = productId < current->productId
            ? current->left
            : current->right;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Deletion
// ---------------------------------------------------------------------------

bool BST::remove(const std::string& productId)
{
    bool removed = false;
    root = remove(root, productId, removed);

    if (removed) {
        --count;
    }

    return removed;
}

BST::Node* BST::remove(Node* node,
                        const std::string& productId,
                        bool& removed)
{
    if (node == nullptr) {
        return nullptr;
    }

    if (productId < node->productId) {
        node->left = remove(node->left, productId, removed);
        return node;
    }

    if (productId > node->productId) {
        node->right = remove(node->right, productId, removed);
        return node;
    }

    removed = true;

    if (node->left == nullptr) {
        Node* rightChild = node->right;
        delete node;
        return rightChild;
    }

    if (node->right == nullptr) {
        Node* leftChild = node->left;
        delete node;
        return leftChild;
    }

    // Transplant the in-order successor instead of copying its string key.
    // This keeps the two-child case allocation-free and preserves ownership.
    Node* successor = detachMin(node->right);
    successor->left = node->left;
    successor->right = node->right;
    delete node;
    return successor;
}

BST::Node* BST::detachMin(Node*& node)
{
    if (node->left == nullptr) {
        Node* minimum = node;
        node = minimum->right;
        minimum->right = nullptr;
        return minimum;
    }

    return detachMin(node->left);
}

// ---------------------------------------------------------------------------
// Cleanup and traversals
// ---------------------------------------------------------------------------

void BST::clear(Node* node)
{
    if (node == nullptr) {
        return;
    }

    clear(node->left);
    clear(node->right);
    delete node;
}

int BST::getCount() const
{
    return count;
}

bool BST::isEmpty() const
{
    return count == 0;
}

std::vector<std::string> BST::inorder() const
{
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(count));
    inorder(root, result);
    return result;
}

void BST::inorder(const Node* node, std::vector<std::string>& result) const
{
    if (node == nullptr) {
        return;
    }

    inorder(node->left, result);
    result.push_back(node->productId);
    inorder(node->right, result);
}

std::vector<std::string> BST::preorder() const
{
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(count));
    preorder(root, result);
    return result;
}

void BST::preorder(const Node* node, std::vector<std::string>& result) const
{
    if (node == nullptr) {
        return;
    }

    result.push_back(node->productId);
    preorder(node->left, result);
    preorder(node->right, result);
}

std::vector<std::string> BST::postorder() const
{
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(count));
    postorder(root, result);
    return result;
}

void BST::postorder(const Node* node, std::vector<std::string>& result) const
{
    if (node == nullptr) {
        return;
    }

    postorder(node->left, result);
    postorder(node->right, result);
    result.push_back(node->productId);
}

// ---------------------------------------------------------------------------
// M5 Iterative Traversals (using custom Stack<T>)
// ---------------------------------------------------------------------------

std::vector<std::string> BST::inorderIterative() const
{
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(count));

    Stack<const Node*> stack;
    const Node* current = root;

    while (current != nullptr || !stack.empty()) {
        while (current != nullptr) {
            stack.push(current);
            current = current->left;
        }

        current = stack.top();
        stack.pop();
        result.push_back(current->productId);
        current = current->right;
    }

    return result;
}

std::vector<std::string> BST::preorderIterative() const
{
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(count));

    if (root == nullptr) {
        return result;
    }

    Stack<const Node*> stack;
    stack.push(root);

    while (!stack.empty()) {
        const Node* current = stack.top();
        stack.pop();
        result.push_back(current->productId);

        // Push right child first so left child is popped and processed first
        if (current->right != nullptr) {
            stack.push(current->right);
        }
        if (current->left != nullptr) {
            stack.push(current->left);
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// M5 Tree Serialization & Deserialization
// ---------------------------------------------------------------------------

std::string BST::serialize() const
{
    std::vector<std::string> pre = preorderIterative();
    if (pre.empty()) {
        return "";
    }

    std::string result;
    for (const auto& key : pre) {
        result += std::to_string(key.length());
        result += ':';
        result += key;
        result += ';';
    }
    return result;
}

BST::Node* BST::buildFromPreorder(const std::vector<std::string>& tokens,
                                  std::size_t& index,
                                  const std::string* minBound,
                                  const std::string* maxBound,
                                  int& nodeCount)
{
    if (index >= tokens.size()) {
        return nullptr;
    }

    const std::string& key = tokens[index];

    // Enforce strict BST ordering: every key must be within (minBound, maxBound)
    if (minBound != nullptr && key <= *minBound) {
        return nullptr;
    }
    if (maxBound != nullptr && key >= *maxBound) {
        return nullptr;
    }

    // Key is within valid range for this subtree: consume it
    Node* node = new Node(key);
    ++nodeCount;
    ++index;

    try {
        node->left = buildFromPreorder(tokens, index, minBound, &key, nodeCount);
        node->right = buildFromPreorder(tokens, index, &key, maxBound, nodeCount);
    } catch (...) {
        // Safe post-order cleanup on allocation failure
        clear(node->left);
        clear(node->right);
        delete node;
        throw;
    }

    return node;
}

bool BST::deserialize(const std::string& data)
{
    // Empty data represents an empty tree
    if (data.empty()) {
        clear(root);
        root = nullptr;
        count = 0;
        return true;
    }

    // Parse length-prefixed tokens (<len>:<key>;)
    std::vector<std::string> tokens;
    std::size_t pos = 0;
    const std::size_t totalLen = data.length();

    while (pos < totalLen) {
        std::size_t colonPos = data.find(':', pos);
        if (colonPos == std::string::npos || colonPos == pos) {
            return false; // Missing colon or empty length prefix
        }

        // Validate that length prefix contains only ASCII digits
        for (std::size_t i = pos; i < colonPos; ++i) {
            if (!std::isdigit(static_cast<unsigned char>(data[i]))) {
                return false;
            }
        }

        std::string lenStr = data.substr(pos, colonPos - pos);
        // Prevent std::stoull overflow or excessive lengths
        if (lenStr.length() > 10) {
            return false;
        }

        std::size_t tokenLen = 0;
        try {
            tokenLen = static_cast<std::size_t>(std::stoull(lenStr));
        } catch (...) {
            return false;
        }

        std::size_t payloadStart = colonPos + 1;
        // Verify we have tokenLen characters plus the trailing ';' framing delimiter
        if (payloadStart + tokenLen >= totalLen) {
            return false; // Truncated payload or missing ';'
        }

        if (data[payloadStart + tokenLen] != ';') {
            return false; // Missing framing delimiter ';'
        }

        tokens.push_back(data.substr(payloadStart, tokenLen));
        pos = payloadStart + tokenLen + 1;
    }

    if (tokens.empty()) {
        clear(root);
        root = nullptr;
        count = 0;
        return true;
    }

    // Reconstruct into candidate tree with strict BST invariant bounds
    std::size_t index = 0;
    int candidateCount = 0;
    Node* candidateRoot = nullptr;

    try {
        candidateRoot = buildFromPreorder(tokens, index, nullptr, nullptr, candidateCount);
    } catch (...) {
        clear(candidateRoot);
        return false;
    }

    // Validation: all tokens must be consumed and candidate root non-null
    if (index != tokens.size() || candidateRoot == nullptr) {
        clear(candidateRoot);
        return false;
    }

    // Success: atomically swap with current tree (strong exception guarantee)
    clear(root);
    root = candidateRoot;
    count = candidateCount;
    return true;
}

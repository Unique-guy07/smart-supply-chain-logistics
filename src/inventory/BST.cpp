#include "inventory/BST.h"

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

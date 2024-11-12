#include "scenenode.hpp"

namespace syzygy
{
auto SceneNode::parent() -> std::optional<std::reference_wrapper<SceneNode>>
{
    if (m_parent == nullptr)
    {
        return std::nullopt;
    }

    return *m_parent;
}
auto SceneNode::hasChildren() const -> bool { return !m_children.empty(); }
auto SceneNode::children() -> std::span<std::shared_ptr<SceneNode> const>
{
    return m_children;
}

auto SceneNode::childrenCount() const -> size_t { return m_children.size(); }

auto SceneNode::childAt(size_t index) const -> SceneNode const&
{
    auto const& pChild{m_children.at(index)};

    // pChild being null indicates a bug elsewhere in SceneNode
    assert(pChild != nullptr && "SceneNode had NULL child pointer.");

    return *pChild;
}

auto SceneNode::descendent(SceneNode const* const ancestor) const -> bool
{
    SceneNode const* pNode{this};
    size_t iterations{0};
    size_t constexpr MAX_ITERATIONS{100ULL};
    while (pNode != nullptr)
    {
        // Avoid hanging, just terminate if the tree is too large or has a cycle
        assert(
            iterations < MAX_ITERATIONS
            && "Max SceneNode ancestor traversal limit hit - Either there is a "
               "cycle or tree is too large."
        );
        if (pNode == ancestor)
        {
            return true;
        }
        pNode = pNode->m_parent;
        iterations++;
    }
    return false;
}

auto SceneNode::create(std::string&& name) -> std::unique_ptr<SceneNode>
{
    auto result{std::make_unique<SceneNode>()};
    result->m_name = std::move(name);

    return result;
}

auto SceneNode::createChild(std::string&& name) -> SceneNode&
{
    m_children.emplace_back(std::make_unique<SceneNode>());

    SceneNode& newChild{*m_children.back()};

    newChild.m_name = std::move(name);
    newChild.m_parent = this;

    return newChild;
}

void SceneNode::appendChild(std::shared_ptr<SceneNode> newChild)
{
    assert(newChild != nullptr && "Null SceneNode child passed as argument.");
    assert(
        !descendent(newChild.get())
        && "This node has the child in its parent chain."
    );
    assert(
        newChild->m_parent != this
        && "NewChild already has this node as a parent."
    );

    if (newChild->m_parent != nullptr)
    {
        newChild->m_parent->removeChild(*newChild);
    }

    newChild->m_parent = this;
    m_children.emplace_back(newChild);
}

auto SceneNode::removeChild(SceneNode& child) -> std::shared_ptr<SceneNode>
{
    assert(
        child.m_parent == this && "Child to remove is not this node's child."
    );

    auto& siblings{m_children};
    auto childIt = std::stable_partition(
        siblings.begin(),
        siblings.end(),
        [&](std::shared_ptr<SceneNode> const& ptr)
    { return ptr.get() != &child; }
    );
    assert(
        childIt != siblings.end()
        && "SceneNode was not in its parent's children."
    );
    assert(
        std::distance(childIt, siblings.end()) < 2
        && "SceneNode parent had duplicated child."
    );

    std::shared_ptr<SceneNode> removedChild{*childIt};

    // siblings.erase(childIt, siblings.end());
    siblings.pop_back();

    removedChild->m_parent = nullptr;
    return removedChild;
}

// The following try methods can return nullptr since to return a shared pointer
// we need access to the parent which holds the shared pointer.
// Also these operations do not make sense if there is no parent, since that
// means this node is the root of a tree.

auto SceneNode::tryRemoveSelf() -> std::shared_ptr<SceneNode>
{
    if (m_parent != nullptr)
    {
        return m_parent->removeChild(*this);
    }
    return nullptr;
}

auto SceneNode::tryExtractSelf() -> std::shared_ptr<SceneNode>
{
    if (m_parent != nullptr)
    {
        return m_parent->extractChild(*this);
    }
    return nullptr;
}

auto SceneNode::extractChild(SceneNode& child) -> std::shared_ptr<SceneNode>
{
    assert(
        child.m_parent == this && "Child to extract is not this node's child."
    );

    std::shared_ptr<SceneNode> removedChild{removeChild(child)};
    auto newChildStartIt = m_children.insert(
        m_children.end(),
        std::make_move_iterator(removedChild->m_children.begin()),
        std::make_move_iterator(removedChild->m_children.end())
    );
    for (; newChildStartIt < m_children.end(); newChildStartIt++)
    {
        (*newChildStartIt)->m_parent = this;
    }

    removedChild->m_children.clear();

    return removedChild;
}

auto SceneNode::depth() const -> size_t
{
    size_t result{0};

    SceneNode* node{m_parent};
    while (node != nullptr)
    {
        result++;
        node = node->m_parent;
    }

    return result;
}

auto SceneNode::transformToRoot() const -> glm::mat4x4
{
    glm::mat4x4 result{transform.toMatrix()};

    SceneNode* node{m_parent};
    while (node != nullptr)
    {
        result = node->transform.toMatrix() * result;
        node = node->m_parent;
    }

    return result;
}

auto SceneNode::name() const -> std::string const& { return m_name; }

auto SceneNode::accessMesh()
    -> std::optional<std::reference_wrapper<MeshInstanced>>
{
    if (m_mesh == nullptr)
    {
        return std::nullopt;
    }

    return *m_mesh;
}

auto SceneNode::accessMesh() const
    -> std::optional<std::reference_wrapper<MeshInstanced const>>
{
    if (m_mesh == nullptr)
    {
        return std::nullopt;
    }
    return *m_mesh;
}

auto SceneNode::swapMesh(std::unique_ptr<MeshInstanced> newMesh)
    -> std::unique_ptr<MeshInstanced>
{
    m_mesh.swap(newMesh);

    return newMesh;
}

auto SceneNode::begin() -> SceneIterator { return SceneIterator{this}; }

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto SceneNode::end() -> SceneIterator { return SceneIterator{nullptr}; }

SceneIterator::SceneIterator(pointer ptr)
    : m_ptr(ptr)
{
}

auto SceneIterator::operator*() const -> SceneIterator::reference
{
    return *m_ptr;
}

auto SceneIterator::operator++() -> SceneIterator&
{
    // Depth first iteration

    // Go to the first child, and if not possible, go to next sibling. Go up
    // parents to find a sibling to go to.
    if (m_ptr->hasChildren())
    {
        m_ptr = m_ptr->children()[0].get();

        m_path.push(m_siblingIndex);
        m_siblingIndex = 0;
    }
    else
    {
        auto pParent{m_ptr->parent()};

        if (!pParent.has_value())
        {
            m_ptr = nullptr;
            return *this;
        }

        std::reference_wrapper<SceneNode> parent{pParent.value()};

        while (m_siblingIndex + 1 == parent.get().children().size())
        {
            m_ptr = &parent.get();

            pParent = m_ptr->parent();
            if (!pParent.has_value() || m_path.empty())
            {
                m_ptr = nullptr;
                return *this;
            }
            parent = pParent.value();

            m_siblingIndex = m_path.top();
            m_path.pop();
        }

        m_siblingIndex++;
        m_ptr = parent.get().children()[m_siblingIndex].get();
    }

    return *this;
}
auto SceneIterator::operator++(int) -> SceneIterator
{
    SceneIterator tmp{*this};
    ++(*this);
    return tmp;
}
auto SceneIterator::operator==(SceneIterator const& other) const -> bool
{
    return m_ptr == other.m_ptr;
}
} // namespace syzygy

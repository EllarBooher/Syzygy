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
auto SceneNode::children() -> std::span<std::unique_ptr<SceneNode> const>
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
    while (pNode != nullptr)
    {
        if (pNode == ancestor)
        {
            return true;
        }
        pNode = pNode->m_parent;
    }
}

auto SceneNode::create(std::string&& name) -> std::unique_ptr<SceneNode>
{
    auto result{std::make_unique<SceneNode>()};
    result->m_name = std::move(name);

    return result;
}

auto SceneNode::appendChild(std::string&& name) -> SceneNode&
{
    m_children.emplace_back(std::make_unique<SceneNode>());

    SceneNode& newChild{*m_children.back()};

    newChild.m_name = std::move(name);
    newChild.m_parent = this;

    return newChild;
}

void SceneNode::reparent(SceneNode& newParent)
{
    if (m_parent == &newParent)
    {
        return;
    }

    newParent.m_children.push_back(removeFromParent());
    m_parent = &newParent;
}

auto SceneNode::removeFromParent() -> std::unique_ptr<SceneNode>
{
    if (m_parent != nullptr)
    {
        auto& siblings{m_parent->m_children};
        auto childIt = std::stable_partition(
            siblings.begin(),
            siblings.end(),
            [this](std::unique_ptr<SceneNode> const& ptr)
        { return ptr.get() != this; }
        );
        assert(
            childIt != siblings.end()
            && "SceneNode was not in its parent's children."
        );
        assert(
            std::distance(childIt, siblings.end()) < 2
            && "SceneNode parent had duplicated child."
        );

        // Resize the parent's children, without destructing this node
        (void)childIt->release();

        // siblings.erase(childIt, siblings.end());
        siblings.pop_back();
    }

    return std::unique_ptr<SceneNode>{this};
}

auto SceneNode::extract() -> std::unique_ptr<SceneNode>
{
    if (m_parent != nullptr)
    {
        auto& siblings{m_parent->m_children};

        for (auto& child : m_children)
        {
            child->m_parent = m_parent;
        }

        auto newSiblingIt = siblings.insert(
            siblings.end(),
            std::make_move_iterator(m_children.begin()),
            std::make_move_iterator(m_children.end())
        );

        m_children.clear();
    }

    return removeFromParent();
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

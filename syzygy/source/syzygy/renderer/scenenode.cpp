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

#include "scenenode.hpp"

namespace syzygy
{
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

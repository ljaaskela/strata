#ifndef VELK_API_NODE_H
#define VELK_API_NODE_H

#include <velk/api/container.h>

namespace velk {

/**
 * @brief Convenience wrapper around a ClassId::Node object.
 *
 * Inherits all container operations from Container. The constructor
 * rejects objects that are not NodeImpl instances (ClassId::Node).
 */
class Node : public Container
{
public:
    Node() = default;

    explicit Node(IObject::Ptr obj)
        : Container(obj && obj->get_class_uid() == ClassId::Node ? std::move(obj) : IObject::Ptr{})
    {}
};

/** @brief Creates a new ClassId::Node instance. */
inline Node create_node()
{
    return Node(instance().create<IObject>(ClassId::Node));
}

} // namespace velk

#endif // VELK_API_NODE_H

#ifndef NODE_H
#define NODE_H

#include <velk/ext/object.h>
#include <velk/interface/intf_container.h>

namespace velk {

/**
 * @brief IContainer implementation that lazy-creates a ContainerImpl attachment on itself.
 *
 * NodeImpl implements IContainer but delegates storage to a ContainerImpl that is
 * auto-created as an attachment on the first mutating call. Read methods return
 * empty/zero when no ContainerImpl exists yet.
 */
class NodeImpl final : public ext::Object<NodeImpl, IContainer>
{
public:
    VELK_CLASS_UID(ClassId::Node);

    ReturnValue add(const IObject::Ptr& child) override;
    ReturnValue remove(const IObject::Ptr& child) override;
    ReturnValue insert(size_t index, const IObject::Ptr& child) override;
    ReturnValue replace(size_t index, const IObject::Ptr& child) override;
    IObject::Ptr get_at(size_t index) const override;
    vector<IObject::Ptr> get_all() const override;
    size_t size() const override;
    void clear() override;

private:
    IContainer* ensure() const;
    IContainer* find() const;

    mutable IContainer* container_ = nullptr;
};

} // namespace velk

#endif // NODE_H

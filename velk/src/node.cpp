#include "node.h"

#include <velk/api/attachment.h>

namespace velk {

namespace detail {
static constexpr AttachmentQuery container_query_{{}, ClassId::Container};
}

IContainer* NodeImpl::ensure() const
{
    if (!container_) {
        container_ = interface_cast<IContainer>(
            const_cast<NodeImpl*>(this)->find_attachment(detail::container_query_, Resolve::Create));
    }
    return container_;
}

IContainer* NodeImpl::find() const
{
    if (!container_) {
        container_ = interface_cast<IContainer>(
            const_cast<NodeImpl*>(this)->find_attachment(detail::container_query_, Resolve::Existing));
    }
    return container_;
}

ReturnValue NodeImpl::add(const IObject::Ptr& child)
{
    auto* c = ensure();
    return c ? c->add(child) : ReturnValue::Fail;
}

ReturnValue NodeImpl::remove(const IObject::Ptr& child)
{
    auto* c = find();
    return c ? c->remove(child) : ReturnValue::NothingToDo;
}

ReturnValue NodeImpl::insert(size_t index, const IObject::Ptr& child)
{
    auto* c = ensure();
    return c ? c->insert(index, child) : ReturnValue::Fail;
}

ReturnValue NodeImpl::replace(size_t index, const IObject::Ptr& child)
{
    auto* c = find();
    return c ? c->replace(index, child) : ReturnValue::InvalidArgument;
}

IObject::Ptr NodeImpl::get_at(size_t index) const
{
    auto* c = find();
    return c ? c->get_at(index) : IObject::Ptr{};
}

vector<IObject::Ptr> NodeImpl::get_all() const
{
    auto* c = find();
    return c ? c->get_all() : vector<IObject::Ptr>{};
}

size_t NodeImpl::size() const
{
    auto* c = find();
    return c ? c->size() : 0;
}

void NodeImpl::clear()
{
    auto* c = find();
    if (c) {
        c->clear();
    }
}

} // namespace velk

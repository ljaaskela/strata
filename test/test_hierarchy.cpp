#include <velk/api/hierarchy.h>
#include <velk/api/velk.h>
#include <velk/ext/object.h>
#include <velk/interface/intf_hierarchy.h>

#include <gtest/gtest.h>

using namespace velk;

class IHierarchyTest : public Interface<IHierarchyTest>
{
public:
    VELK_INTERFACE(
        (PROP, int, value, 0)
    )
};

class HierarchyTestObj : public ext::Object<HierarchyTestObj, IHierarchyTest>
{};

struct ListenerState
{
    bool allow_join = true;
    bool allow_leave = true;
    int joined_count = 0;
    int left_count = 0;
    IObject::Ptr last_join_parent;
};

class ITestListener : public Interface<ITestListener>
{
public:
    virtual ListenerState& state() = 0;
};

class ListenerObj : public ext::Object<ListenerObj, IHierarchyTest, IHierarchyAware, ITestListener>
{
public:
    ListenerState state_;

    ListenerState& state() override { return state_; }

    bool on_hierarchy_joining(const IHierarchy::Ptr&, const IObject::Ptr& parent) override
    {
        state_.last_join_parent = parent;
        return state_.allow_join;
    }

    bool on_hierarchy_leaving(const IHierarchy::Ptr&) override
    {
        return state_.allow_leave;
    }

    void on_hierarchy_joined(const IHierarchy::Ptr&, const IObject::Ptr&) override
    {
        state_.joined_count++;
    }

    void on_hierarchy_left(const IHierarchy::Ptr&) override
    {
        state_.left_count++;
    }
};

class HierarchyTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        instance().type_registry().register_type<HierarchyTestObj>();
        instance().type_registry().register_type<ListenerObj>();
    }

    IObject::Ptr make_obj()
    {
        return instance().create<IObject>(HierarchyTestObj::class_id());
    }
};

TEST_F(HierarchyTest, CreateHierarchy)
{
    auto h = create_hierarchy();
    EXPECT_TRUE(h);
    EXPECT_EQ(h.size(), 0u);
    EXPECT_TRUE(h.empty());
}

TEST_F(HierarchyTest, SetRoot)
{
    auto h = create_hierarchy();
    auto root = make_obj();

    EXPECT_TRUE(succeeded(h.set_root(root)));
    EXPECT_EQ(h.root(), root);
    EXPECT_EQ(h.size(), 1u);
    EXPECT_FALSE(h.empty());
}

TEST_F(HierarchyTest, SetRootNullFails)
{
    auto h = create_hierarchy();
    EXPECT_EQ(h.set_root({}), ReturnValue::InvalidArgument);
}

TEST_F(HierarchyTest, SetRootClearsExisting)
{
    auto h = create_hierarchy();
    auto root1 = make_obj();
    auto root2 = make_obj();
    auto child = make_obj();

    h.set_root(root1);
    h.add(root1, child);
    EXPECT_EQ(h.size(), 2u);

    h.set_root(root2);
    EXPECT_EQ(h.size(), 1u);
    EXPECT_EQ(h.root(), root2);
    EXPECT_FALSE(h.contains(root1));
    EXPECT_FALSE(h.contains(child));
}

TEST_F(HierarchyTest, AddChildren)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();

    h.set_root(root);
    EXPECT_TRUE(succeeded(h.add(root, child1)));
    EXPECT_TRUE(succeeded(h.add(root, child2)));

    EXPECT_EQ(h.size(), 3u);
    EXPECT_EQ(h.child_count(root), 2u);
    EXPECT_TRUE(h.contains(child1));
    EXPECT_TRUE(h.contains(child2));
}

TEST_F(HierarchyTest, AddToNonMemberParentFails)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto orphan = make_obj();
    auto child = make_obj();

    h.set_root(root);
    EXPECT_EQ(h.add(orphan, child), ReturnValue::InvalidArgument);
}

TEST_F(HierarchyTest, AddDuplicateFails)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();

    h.set_root(root);
    h.add(root, child);
    EXPECT_EQ(h.add(root, child), ReturnValue::InvalidArgument);
}

TEST_F(HierarchyTest, AddNullFails)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    h.set_root(root);

    EXPECT_EQ(h.add(root, {}), ReturnValue::InvalidArgument);
    EXPECT_EQ(h.add({}, root), ReturnValue::InvalidArgument);
}

TEST_F(HierarchyTest, ParentOfRoot)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    h.set_root(root);

    EXPECT_FALSE(h.parent_of(root));
}

TEST_F(HierarchyTest, ParentOfChild)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();

    h.set_root(root);
    h.add(root, child);

    EXPECT_EQ(h.parent_of(child), root);
}

TEST_F(HierarchyTest, ChildrenOf)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();

    h.set_root(root);
    h.add(root, child1);
    h.add(root, child2);

    auto children = h.children_of(root);
    EXPECT_EQ(children.size(), 2u);
    EXPECT_EQ(children[0], child1);
    EXPECT_EQ(children[1], child2);
}

TEST_F(HierarchyTest, InsertAtIndex)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();
    auto child3 = make_obj();

    h.set_root(root);
    h.add(root, child1);
    h.add(root, child3);

    EXPECT_TRUE(succeeded(h.insert(root, 1, child2)));

    auto children = h.children_of(root);
    EXPECT_EQ(children.size(), 3u);
    EXPECT_EQ(children[0], child1);
    EXPECT_EQ(children[1], child2);
    EXPECT_EQ(children[2], child3);
}

TEST_F(HierarchyTest, InsertOutOfBoundsFails)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();

    h.set_root(root);
    EXPECT_EQ(h.insert(root, 5, child), ReturnValue::InvalidArgument);
}

TEST_F(HierarchyTest, RemoveSubtree)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto parent = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();

    h.set_root(root);
    h.add(root, parent);
    h.add(parent, child1);
    h.add(parent, child2);
    EXPECT_EQ(h.size(), 4u);

    EXPECT_TRUE(succeeded(h.remove(parent)));
    EXPECT_EQ(h.size(), 1u);
    EXPECT_FALSE(h.contains(parent));
    EXPECT_FALSE(h.contains(child1));
    EXPECT_FALSE(h.contains(child2));
    EXPECT_EQ(h.child_count(root), 0u);
}

TEST_F(HierarchyTest, RemoveRoot)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();

    h.set_root(root);
    h.add(root, child);

    EXPECT_TRUE(succeeded(h.remove(root)));
    EXPECT_EQ(h.size(), 0u);
    EXPECT_FALSE(h.root());
}

TEST_F(HierarchyTest, RemoveNonMember)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto orphan = make_obj();

    h.set_root(root);
    EXPECT_EQ(h.remove(orphan), ReturnValue::NothingToDo);
}

TEST_F(HierarchyTest, ReplaceChild)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();
    auto replacement = make_obj();

    h.set_root(root);
    h.add(root, child1);
    h.add(root, child2);

    EXPECT_TRUE(succeeded(h.replace(child1, replacement)));
    EXPECT_FALSE(h.contains(child1));
    EXPECT_TRUE(h.contains(replacement));
    EXPECT_EQ(h.parent_of(replacement), root);

    auto children = h.children_of(root);
    EXPECT_EQ(children[0], replacement);
    EXPECT_EQ(children[1], child2);
}

TEST_F(HierarchyTest, ReplacePreservesChildren)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto parent = make_obj();
    auto grandchild = make_obj();
    auto replacement = make_obj();

    h.set_root(root);
    h.add(root, parent);
    h.add(parent, grandchild);

    h.replace(parent, replacement);

    EXPECT_EQ(h.child_count(replacement), 1u);
    EXPECT_EQ(h.parent_of(grandchild), replacement);
}

TEST_F(HierarchyTest, ReplaceRoot)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();
    auto new_root = make_obj();

    h.set_root(root);
    h.add(root, child);

    h.replace(root, new_root);

    EXPECT_EQ(h.root(), new_root);
    EXPECT_EQ(h.child_count(new_root), 1u);
    EXPECT_EQ(h.parent_of(child), new_root);
}

TEST_F(HierarchyTest, ReplaceNonMemberFails)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto orphan = make_obj();
    auto replacement = make_obj();

    h.set_root(root);
    EXPECT_EQ(h.replace(orphan, replacement), ReturnValue::InvalidArgument);
}

TEST_F(HierarchyTest, ReplaceDuplicateFails)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();

    h.set_root(root);
    h.add(root, child);

    // Can't replace child with root (root is already in hierarchy)
    EXPECT_EQ(h.replace(child, root), ReturnValue::InvalidArgument);
}

TEST_F(HierarchyTest, NodeOfReturnsSnapshot)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();

    h.set_root(root);
    h.add(root, child1);
    h.add(root, child2);

    auto node = h.node_of(root);
    EXPECT_TRUE(node);
    EXPECT_EQ(node, root);
    EXPECT_FALSE(node.has_parent());
    EXPECT_EQ(node.child_count(), 2u);
    EXPECT_TRUE(node.hierarchy());

    auto childNode = h.node_of(child1);
    EXPECT_TRUE(childNode);
    EXPECT_EQ(childNode, child1);
    EXPECT_EQ(childNode.get_parent(), root);
    EXPECT_TRUE(childNode.has_parent());
    EXPECT_EQ(childNode.child_count(), 0u);
}

TEST_F(HierarchyTest, NodeOfNonMemberReturnsEmpty)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto orphan = make_obj();

    h.set_root(root);
    auto node = h.node_of(orphan);
    EXPECT_FALSE(node);
}

TEST_F(HierarchyTest, ContainsAndSize)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();

    EXPECT_EQ(h.size(), 0u);
    EXPECT_FALSE(h.contains(root));

    h.set_root(root);
    EXPECT_TRUE(h.contains(root));
    EXPECT_FALSE(h.contains(child));
    EXPECT_EQ(h.size(), 1u);

    h.add(root, child);
    EXPECT_TRUE(h.contains(child));
    EXPECT_EQ(h.size(), 2u);
}

TEST_F(HierarchyTest, Clear)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();

    h.set_root(root);
    h.add(root, child);

    h.clear();
    EXPECT_EQ(h.size(), 0u);
    EXPECT_TRUE(h.empty());
    EXPECT_FALSE(h.root());
    EXPECT_FALSE(h.contains(root));
}

TEST_F(HierarchyTest, ChildrenOfReturnsNodes)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();

    h.set_root(root);
    h.add(root, child1);
    h.add(root, child2);

    auto children = h.children_of(root);
    EXPECT_EQ(children.size(), 2u);
    // Each child is a Node that can be used as an Object
    EXPECT_EQ(children[0].class_uid(), HierarchyTestObj::class_id());
    EXPECT_EQ(children[1].class_uid(), HierarchyTestObj::class_id());
}

TEST_F(HierarchyTest, ForEachChild)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();

    h.set_root(root);
    h.add(root, child1);
    h.add(root, child2);

    // Set different values on children via state
    interface_cast<IHierarchyTest>(child1)->value().set_value(10);
    interface_cast<IHierarchyTest>(child2)->value().set_value(20);

    int sum = 0;
    h.for_each_child<IHierarchyTest>(root, [&](IHierarchyTest& obj) {
        sum += obj.value().get_value();
    });
    EXPECT_EQ(sum, 30);
}

TEST_F(HierarchyTest, ForEachChildEarlyStop)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();

    h.set_root(root);
    h.add(root, child1);
    h.add(root, child2);

    int count = 0;
    h.for_each_child<IHierarchyTest>(root, [&](IHierarchyTest&) -> bool {
        count++;
        return false; // stop after first
    });
    EXPECT_EQ(count, 1);
}

TEST_F(HierarchyTest, NullHierarchySafe)
{
    Hierarchy h;
    EXPECT_FALSE(h);
    EXPECT_EQ(h.size(), 0u);
    EXPECT_TRUE(h.empty());
    EXPECT_FALSE(h.root());
    EXPECT_EQ(h.set_root({}), ReturnValue::InvalidArgument);
    EXPECT_EQ(h.add({}, {}), ReturnValue::InvalidArgument);
    EXPECT_EQ(h.insert({}, 0, {}), ReturnValue::InvalidArgument);
    EXPECT_EQ(h.remove({}), ReturnValue::InvalidArgument);
    EXPECT_EQ(h.replace({}, {}), ReturnValue::InvalidArgument);
    h.clear(); // should not crash
    EXPECT_FALSE(h.parent_of({}));
    EXPECT_EQ(h.children_of({}).size(), 0u);
    EXPECT_EQ(h.child_count({}), 0u);
    EXPECT_FALSE(h.contains({}));
}

TEST_F(HierarchyTest, RootReturnsNode)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();

    h.set_root(root);
    h.add(root, child);

    auto node = h.root();
    EXPECT_TRUE(node);
    EXPECT_EQ(node, root);
    EXPECT_FALSE(node.has_parent());
    EXPECT_EQ(node.child_count(), 1u);
    EXPECT_EQ(node.child_at(0), child);
    EXPECT_TRUE(node.hierarchy());
}

TEST_F(HierarchyTest, HierarchyClassId)
{
    auto h = create_hierarchy();
    EXPECT_EQ(h.class_uid(), ClassId::Hierarchy);
}

TEST_F(HierarchyTest, GetHierarchyInterface)
{
    auto h = create_hierarchy();
    auto iface = h.get_hierarchy_interface();
    EXPECT_TRUE(iface);
}

TEST_F(HierarchyTest, NodeInheritsObject)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    h.set_root(root);

    // Set a property value on the object
    interface_cast<IHierarchyTest>(root)->value().set_value(42);

    // Access through Node's inherited Object methods
    auto node = h.root();
    EXPECT_EQ(node.class_uid(), HierarchyTestObj::class_id());
    auto* iht = node.as<IHierarchyTest>();
    ASSERT_NE(iht, nullptr);
    EXPECT_EQ(iht->value().get_value(), 42);
}

TEST_F(HierarchyTest, NodeGetChildren)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();

    h.set_root(root);
    h.add(root, child1);
    h.add(root, child2);

    auto node = h.root();
    auto children = node.get_children();
    EXPECT_EQ(children.size(), 2u);
    EXPECT_EQ(children[0], child1);
    EXPECT_EQ(children[1], child2);
}

TEST_F(HierarchyTest, NodeChildAt)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();

    h.set_root(root);
    h.add(root, child);

    auto node = h.root();
    EXPECT_EQ(node.child_at(0), child);
    EXPECT_FALSE(node.child_at(1)); // out of range
}

TEST_F(HierarchyTest, NodeTypedChildAt)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();

    h.set_root(root);
    h.add(root, child);

    auto node = h.root();
    auto typed = node.child_at<IHierarchyTest>(0);
    EXPECT_TRUE(typed);
    EXPECT_FALSE(node.child_at<IHierarchyTest>(5));
}

TEST_F(HierarchyTest, NodeImplicitConversion)
{
    auto h = create_hierarchy();
    auto root_obj = make_obj();
    auto child_obj = make_obj();

    h.set_root(root_obj);

    // Node converts to IObject::Ptr, so it can be used directly with Hierarchy methods
    auto root_node = h.root();
    EXPECT_TRUE(succeeded(h.add(root_node, child_obj)));
    EXPECT_EQ(h.child_count(root_node), 1u);
    EXPECT_TRUE(h.contains(root_node));
}

TEST_F(HierarchyTest, NodeForEachChild)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();

    h.set_root(root);
    h.add(root, child1);
    h.add(root, child2);

    interface_cast<IHierarchyTest>(child1)->value().set_value(10);
    interface_cast<IHierarchyTest>(child2)->value().set_value(20);

    auto node = h.root();
    int sum = 0;
    node.for_each_child<IHierarchyTest>([&](IHierarchyTest& obj) {
        sum += obj.value().get_value();
    });
    EXPECT_EQ(sum, 30);
}

TEST_F(HierarchyTest, NodeForEachChildEarlyStop)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();

    h.set_root(root);
    h.add(root, child1);
    h.add(root, child2);

    auto node = h.root();
    int count = 0;
    node.for_each_child<IHierarchyTest>([&](IHierarchyTest&) -> bool {
        count++;
        return false;
    });
    EXPECT_EQ(count, 1);
}

TEST_F(HierarchyTest, NodeDefaultConstructedIsEmpty)
{
    ::velk::Node node;
    EXPECT_FALSE(node);
    EXPECT_FALSE(node.object());
    EXPECT_FALSE(node.get_parent());
    EXPECT_FALSE(node.has_parent());
    EXPECT_EQ(node.child_count(), 0u);
    EXPECT_FALSE(node.child_at(0));
    EXPECT_EQ(node.get_children().size(), 0u);
}

TEST_F(HierarchyTest, NodeHierarchyNodeAccess)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    h.set_root(root);

    auto node = h.root();
    auto& hn = node.hierarchy_node();
    EXPECT_EQ(hn.object, root);
}

TEST_F(HierarchyTest, NodeReflectsLiveState)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();

    h.set_root(root);
    h.add(root, child1);

    // Get node before mutation
    auto node = h.root();
    EXPECT_EQ(node.child_count(), 1u);

    // Mutate hierarchy after node was created
    auto child2 = make_obj();
    h.add(root, child2);

    // Node queries hierarchy on demand, so it sees the new child
    EXPECT_EQ(node.child_count(), 2u);
    EXPECT_EQ(node.child_at(1), child2);
}

// Listener tests

TEST_F(HierarchyTest, ListenerJoinedCalledOnAdd)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = instance().create<IObject>(ListenerObj::class_id());
    auto* listener = interface_cast<ITestListener>(child);

    h.set_root(root);
    h.add(root, child);

    EXPECT_EQ(listener->state().joined_count, 1);
    EXPECT_EQ(listener->state().last_join_parent, root);
}

TEST_F(HierarchyTest, ListenerJoinedCalledOnInsert)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = instance().create<IObject>(ListenerObj::class_id());
    auto* listener = interface_cast<ITestListener>(child);

    h.set_root(root);
    h.insert(root, 0, child);

    EXPECT_EQ(listener->state().joined_count, 1);
}

TEST_F(HierarchyTest, ListenerJoinedCalledOnSetRoot)
{
    auto h = create_hierarchy();
    auto root = instance().create<IObject>(ListenerObj::class_id());
    auto* listener = interface_cast<ITestListener>(root);

    h.set_root(root);

    EXPECT_EQ(listener->state().joined_count, 1);
    EXPECT_FALSE(listener->state().last_join_parent); // root has no parent
}

TEST_F(HierarchyTest, ListenerLeftCalledOnRemove)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = instance().create<IObject>(ListenerObj::class_id());
    auto* listener = interface_cast<ITestListener>(child);

    h.set_root(root);
    h.add(root, child);
    h.remove(child);

    EXPECT_EQ(listener->state().left_count, 1);
}

TEST_F(HierarchyTest, ListenerLeftCalledOnSubtreeRemove)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto parent = make_obj();
    auto child = instance().create<IObject>(ListenerObj::class_id());
    auto* listener = interface_cast<ITestListener>(child);

    h.set_root(root);
    h.add(root, parent);
    h.add(parent, child);

    // Remove parent (subtree removal, no veto for descendants)
    h.remove(parent);
    EXPECT_EQ(listener->state().left_count, 1);
}

TEST_F(HierarchyTest, ListenerLeftCalledOnClear)
{
    auto h = create_hierarchy();
    auto root = instance().create<IObject>(ListenerObj::class_id());
    auto child = instance().create<IObject>(ListenerObj::class_id());
    auto* rootListener = interface_cast<ITestListener>(root);
    auto* childListener = interface_cast<ITestListener>(child);

    h.set_root(root);
    h.add(root, child);
    h.clear();

    EXPECT_EQ(rootListener->state().left_count, 1);
    EXPECT_EQ(childListener->state().left_count, 1);
}

TEST_F(HierarchyTest, ListenerLeftCalledOnSetRootReplace)
{
    auto h = create_hierarchy();
    auto root1 = instance().create<IObject>(ListenerObj::class_id());
    auto child = instance().create<IObject>(ListenerObj::class_id());
    auto root2 = make_obj();
    auto* root1Listener = interface_cast<ITestListener>(root1);
    auto* childListener = interface_cast<ITestListener>(child);

    h.set_root(root1);
    h.add(root1, child);
    h.set_root(root2);

    EXPECT_EQ(root1Listener->state().left_count, 1);
    EXPECT_EQ(childListener->state().left_count, 1);
}

TEST_F(HierarchyTest, ListenerReplaceNotifiesBoth)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto old_child = instance().create<IObject>(ListenerObj::class_id());
    auto new_child = instance().create<IObject>(ListenerObj::class_id());
    auto* oldListener = interface_cast<ITestListener>(old_child);
    auto* newListener = interface_cast<ITestListener>(new_child);

    h.set_root(root);
    h.add(root, old_child);
    h.replace(old_child, new_child);

    EXPECT_EQ(oldListener->state().left_count, 1);
    EXPECT_EQ(newListener->state().joined_count, 1);
    EXPECT_EQ(newListener->state().last_join_parent, root);
}

TEST_F(HierarchyTest, ListenerRefuseJoin)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = instance().create<IObject>(ListenerObj::class_id());
    auto* listener = interface_cast<ITestListener>(child);
    listener->state().allow_join = false;

    h.set_root(root);
    EXPECT_EQ(h.add(root, child), ReturnValue::Refused);
    EXPECT_FALSE(h.contains(child));
    EXPECT_EQ(listener->state().joined_count, 0);
}

TEST_F(HierarchyTest, ListenerRefuseJoinOnInsert)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = instance().create<IObject>(ListenerObj::class_id());
    auto* listener = interface_cast<ITestListener>(child);
    listener->state().allow_join = false;

    h.set_root(root);
    EXPECT_EQ(h.insert(root, 0, child), ReturnValue::Refused);
    EXPECT_FALSE(h.contains(child));
}

TEST_F(HierarchyTest, ListenerRefuseJoinOnSetRoot)
{
    auto h = create_hierarchy();
    auto root = instance().create<IObject>(ListenerObj::class_id());
    auto* listener = interface_cast<ITestListener>(root);
    listener->state().allow_join = false;

    EXPECT_EQ(h.set_root(root), ReturnValue::Refused);
    EXPECT_TRUE(h.empty());
}

TEST_F(HierarchyTest, ListenerRefuseLeave)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = instance().create<IObject>(ListenerObj::class_id());
    auto* listener = interface_cast<ITestListener>(child);

    h.set_root(root);
    h.add(root, child);

    listener->state().allow_leave = false;
    EXPECT_EQ(h.remove(child), ReturnValue::Refused);
    EXPECT_TRUE(h.contains(child)); // still in hierarchy
}

TEST_F(HierarchyTest, ListenerRefuseLeaveDoesNotAffectSubtreeRemove)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto parent = make_obj();
    auto child = instance().create<IObject>(ListenerObj::class_id());
    auto* listener = interface_cast<ITestListener>(child);
    listener->state().allow_leave = false;

    h.set_root(root);
    h.add(root, parent);
    h.add(parent, child);

    // Subtree removal: child doesn't get a veto
    EXPECT_TRUE(succeeded(h.remove(parent)));
    EXPECT_FALSE(h.contains(child));
    EXPECT_EQ(listener->state().left_count, 1);
}

TEST_F(HierarchyTest, ListenerRefuseLeaveDoesNotAffectClear)
{
    auto h = create_hierarchy();
    auto root = instance().create<IObject>(ListenerObj::class_id());
    auto* listener = interface_cast<ITestListener>(root);
    listener->state().allow_leave = false;

    h.set_root(root);
    h.clear(); // no veto on clear
    EXPECT_TRUE(h.empty());
    EXPECT_EQ(listener->state().left_count, 1);
}

TEST_F(HierarchyTest, ListenerRefuseJoinOnReplace)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto old_child = make_obj();
    auto new_child = instance().create<IObject>(ListenerObj::class_id());
    auto* listener = interface_cast<ITestListener>(new_child);
    listener->state().allow_join = false;

    h.set_root(root);
    h.add(root, old_child);

    EXPECT_EQ(h.replace(old_child, new_child), ReturnValue::Refused);
    EXPECT_TRUE(h.contains(old_child)); // old child still there
    EXPECT_FALSE(h.contains(new_child));
}

TEST_F(HierarchyTest, NonListenerObjectsUnaffected)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();

    h.set_root(root);
    EXPECT_TRUE(succeeded(h.add(root, child)));
    EXPECT_TRUE(succeeded(h.remove(child)));
}

// ============================================================
// Event tests (on_changing / on_changed)
// ============================================================

struct ChangeRecord
{
    HierarchyChange change;
    bool is_pre{};
};

class EventHierarchyTest : public HierarchyTest
{
protected:
    std::vector<ChangeRecord> records;

    void subscribe(Hierarchy& h)
    {
        auto* ih = interface_cast<IHierarchy>(h.get());
        ih->on_changing().add_handler(
            create_callback([this](const HierarchyChange& change) { records.push_back({change, true}); }));
        ih->on_changed().add_handler(
            create_callback([this](const HierarchyChange& change) { records.push_back({change, false}); }));
    }
};

TEST_F(EventHierarchyTest, OnChangedFiresOnSetRoot)
{
    auto h = create_hierarchy();
    subscribe(h);
    auto root = make_obj();

    h.set_root(root);

    ASSERT_EQ(records.size(), 2u);
    EXPECT_TRUE(records[0].is_pre);
    EXPECT_EQ(records[0].change.type, HierarchyChange::Type::SetRoot);
    EXPECT_EQ(records[0].change.child, root);
    EXPECT_FALSE(records[1].is_pre);
    EXPECT_EQ(records[1].change.type, HierarchyChange::Type::SetRoot);
    EXPECT_EQ(records[1].change.child, root);
}

TEST_F(EventHierarchyTest, OnChangedFiresOnAdd)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    h.set_root(root);
    subscribe(h);

    auto child = make_obj();
    h.add(root, child);

    ASSERT_EQ(records.size(), 2u);
    EXPECT_TRUE(records[0].is_pre);
    EXPECT_EQ(records[0].change.type, HierarchyChange::Type::Add);
    EXPECT_EQ(records[0].change.parent, root);
    EXPECT_EQ(records[0].change.child, child);
    EXPECT_FALSE(records[1].is_pre);
    EXPECT_EQ(records[1].change.type, HierarchyChange::Type::Add);
}

TEST_F(EventHierarchyTest, OnChangedFiresOnInsert)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    h.set_root(root);
    subscribe(h);

    auto child = make_obj();
    h.insert(root, 0, child);

    ASSERT_EQ(records.size(), 2u);
    EXPECT_TRUE(records[0].is_pre);
    EXPECT_EQ(records[0].change.type, HierarchyChange::Type::Insert);
    EXPECT_EQ(records[0].change.parent, root);
    EXPECT_EQ(records[0].change.child, child);
    EXPECT_EQ(records[0].change.index, 0u);
    EXPECT_FALSE(records[1].is_pre);
}

TEST_F(EventHierarchyTest, OnChangedFiresOnRemove)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();
    h.set_root(root);
    h.add(root, child);
    subscribe(h);

    h.remove(child);

    ASSERT_EQ(records.size(), 2u);
    EXPECT_TRUE(records[0].is_pre);
    EXPECT_EQ(records[0].change.type, HierarchyChange::Type::Remove);
    EXPECT_EQ(records[0].change.parent, root);
    EXPECT_EQ(records[0].change.child, child);
    EXPECT_FALSE(records[1].is_pre);
    EXPECT_EQ(records[1].change.type, HierarchyChange::Type::Remove);
}

TEST_F(EventHierarchyTest, OnChangedFiresOnReplace)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto old_child = make_obj();
    auto new_child = make_obj();
    h.set_root(root);
    h.add(root, old_child);
    subscribe(h);

    h.replace(old_child, new_child);

    ASSERT_EQ(records.size(), 2u);
    EXPECT_TRUE(records[0].is_pre);
    EXPECT_EQ(records[0].change.type, HierarchyChange::Type::Replace);
    EXPECT_EQ(records[0].change.parent, root);
    EXPECT_EQ(records[0].change.child, new_child);
    EXPECT_EQ(records[0].change.old_child, old_child);
    EXPECT_FALSE(records[1].is_pre);
    EXPECT_EQ(records[1].change.type, HierarchyChange::Type::Replace);
}

TEST_F(EventHierarchyTest, OnChangedFiresOnClear)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    h.set_root(root);
    subscribe(h);

    h.clear();

    ASSERT_EQ(records.size(), 2u);
    EXPECT_TRUE(records[0].is_pre);
    EXPECT_EQ(records[0].change.type, HierarchyChange::Type::Clear);
    EXPECT_FALSE(records[1].is_pre);
    EXPECT_EQ(records[1].change.type, HierarchyChange::Type::Clear);
}

TEST_F(EventHierarchyTest, OnChangingFiresBeforeMutation)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    h.set_root(root);

    auto* ih = interface_cast<IHierarchy>(h.get());
    bool child_was_absent = false;
    ih->on_changing().add_handler([&](FnArgs) -> IAny::Ptr {
        // During on_changing, the child should not yet be in the hierarchy
        child_was_absent = (h.size() == 1);
        return nullptr;
    });

    auto child = make_obj();
    h.add(root, child);

    EXPECT_TRUE(child_was_absent);
    EXPECT_EQ(h.size(), 2u);
}

TEST_F(EventHierarchyTest, NoEventOnVetoedOperation)
{
    auto h = create_hierarchy();
    subscribe(h);

    auto root_obj = instance().create<IObject>(ListenerObj::class_id());
    auto* root_listener = interface_cast<ITestListener>(root_obj);
    root_listener->state().allow_join = false;

    h.set_root(root_obj);

    EXPECT_EQ(records.size(), 0u);
}

TEST_F(EventHierarchyTest, RemoveFiresOnceForSubtree)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto parent = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();
    h.set_root(root);
    h.add(root, parent);
    h.add(parent, child1);
    h.add(parent, child2);
    subscribe(h);

    h.remove(parent);

    // Should fire once for the subtree root, not per descendant
    ASSERT_EQ(records.size(), 2u);
    EXPECT_EQ(records[0].change.type, HierarchyChange::Type::Remove);
    EXPECT_EQ(records[0].change.child, parent);
    EXPECT_EQ(records[1].change.type, HierarchyChange::Type::Remove);
    EXPECT_EQ(records[1].change.child, parent);
}

TEST_F(EventHierarchyTest, NoEventWhenNoHandlers)
{
    auto h = create_hierarchy();
    auto root = make_obj();

    // No subscribe - events should not be created or fired
    h.set_root(root);
    h.add(root, make_obj());

    // Verify hierarchy works normally
    EXPECT_EQ(h.size(), 2u);
}

TEST_F(EventHierarchyTest, OnChangingBeforeOnChanged)
{
    auto h = create_hierarchy();
    subscribe(h);
    auto root = make_obj();

    h.set_root(root);
    auto child = make_obj();
    h.add(root, child);

    // set_root: pre, post; add: pre, post
    ASSERT_EQ(records.size(), 4u);
    EXPECT_TRUE(records[0].is_pre);   // set_root changing
    EXPECT_FALSE(records[1].is_pre);  // set_root changed
    EXPECT_TRUE(records[2].is_pre);   // add changing
    EXPECT_FALSE(records[3].is_pre);  // add changed
}

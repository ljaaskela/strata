#include "hive.h"

#include <velk/api/velk.h>
#include <velk/interface/intf_metadata.h>

#include <cstdlib>
#include <cstring>
#include <new>

#ifdef _WIN32
#include <malloc.h>
#endif

namespace velk {

namespace {

void* aligned_alloc_impl(size_t alignment, size_t size)
{
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#else
    return std::aligned_alloc(alignment, size);
#endif
}

void aligned_free_impl(void* ptr)
{
#ifdef _WIN32
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
}

size_t align_up(size_t size, size_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

} // anonymous namespace

/**
 * @brief Extended control block for hive-managed objects.
 *
 * Embeds the page/slot context needed by the destroy callback so that
 * the slot can be reclaimed when the last strong reference drops.
 */
struct HiveControlBlock
{
    external_control_block ecb;
    HivePage* page;
    size_t slot_index;
    const IObjectFactory* factory;
    size_t slot_size;
};

/**
 * @brief Shared destroy logic for hive-managed objects.
 *
 * Called from unref() when the last strong reference drops. Recovers the
 * HiveControlBlock context and destroys the object in place, returning
 * the slot to the page freelist (normal mode) or freeing the orphaned
 * page when the last object on it is destroyed (orphan mode).
 */
static void hive_destroy_impl(HiveControlBlock* hcb, bool orphan)
{
    HivePage* page = hcb->page;
    size_t slot_index = hcb->slot_index;
    size_t slot_sz = hcb->slot_size;
    const IObjectFactory* factory = hcb->factory;
    auto* slot = static_cast<char*>(page->slots) + slot_index * slot_sz;

    // Prevent the block from being freed during the destructor chain.
    // ~RefCountedDispatch calls release_ref_counted -> release_control_block
    // -> release_weak(). Our bump keeps the block alive through the destructor.
    hcb->ecb.add_weak();

    // Clear the external tag so that if outstanding weak_ptrs eventually release
    // this block, dealloc_control_block uses the regular (non-external) path.
    // This is safe: HiveControlBlock's trivial destructor means delete via
    // control_block* works correctly (the heap tracks the actual allocation size).
    hcb->ecb.set_ptr(hcb->ecb.get_ptr());

    factory->destroy_in_place(slot);

    // The destructor decremented weak by 1. Our bump kept the block alive.
    // Now release our extra weak. If it's the last, we own the block and delete it.
    if (hcb->ecb.release_weak()) {
        delete hcb;
    }
    // else: outstanding weak_ptrs will dealloc via the regular path.

    // Transition slot to Free.
    page->state[slot_index] = SlotState::Free;
    page->blocks[slot_index] = nullptr;
    if (!orphan) {
        std::memcpy(slot, &page->free_head, sizeof(size_t));
        page->free_head = slot_index;
    }
    --page->live_count;

    if (orphan && page->live_count == 0) {
        delete[] page->state;
        delete[] page->blocks;
        aligned_free_impl(page->slots);
        delete page;
    }
}

static void hive_destroy(external_control_block* ecb)
{
    hive_destroy_impl(static_cast<HiveControlBlock*>(static_cast<void*>(ecb)), false);
}

static void hive_destroy_orphan(external_control_block* ecb)
{
    hive_destroy_impl(static_cast<HiveControlBlock*>(static_cast<void*>(ecb)), true);
}

// --- Hive implementation ---

Hive::~Hive()
{
    for (auto& page_ptr : pages_) {
        auto& page = *page_ptr;
        // First pass: release the hive's strong ref on all Active objects.
        for (size_t i = 0; i < page.capacity; ++i) {
            if (page.state[i] == SlotState::Active) {
                page.state[i] = SlotState::Zombie;
                auto* obj = static_cast<IObject*>(slot_ptr(page, i));
                obj->unref(); // Release hive's strong ref
            }
        }

        // Second pass: check if any zombies remain (external refs still alive).
        bool has_zombies = false;
        for (size_t i = 0; i < page.capacity; ++i) {
            if (page.state[i] == SlotState::Zombie) {
                has_zombies = true;
                break;
            }
        }

        if (has_zombies) {
            // Transfer page ownership to an orphan. The orphan destroy callback
            // will free the page when the last zombie is destroyed.
            for (size_t i = 0; i < page.capacity; ++i) {
                if (page.state[i] == SlotState::Zombie) {
                    page.blocks[i]->ecb.destroy = hive_destroy_orphan;
                }
            }
            // Release unique_ptr ownership — the page now lives until the
            // last orphan destroy callback frees it.
            page_ptr.release();
        } else {
            free_page(page);
        }
    }
}

void Hive::init(Uid classUid)
{
    element_class_uid_ = classUid;
    factory_ = instance().type_registry().find_factory(classUid);
    if (factory_) {
        slot_size_ = align_up(factory_->get_instance_size(), factory_->get_instance_alignment());
        slot_alignment_ = factory_->get_instance_alignment();
    }
}

Uid Hive::get_element_class_uid() const
{
    return element_class_uid_;
}

size_t Hive::size() const
{
    return live_count_;
}

bool Hive::empty() const
{
    return live_count_ == 0;
}

void* Hive::slot_ptr(HivePage& page, size_t index) const
{
    return static_cast<char*>(page.slots) + index * slot_size_;
}

void* Hive::slot_ptr(const HivePage& page, size_t index) const
{
    return static_cast<char*>(page.slots) + index * slot_size_;
}

void Hive::alloc_page(size_t capacity)
{
    auto page = std::make_unique<HivePage>();
    page->capacity = capacity;
    size_t total = align_up(capacity * slot_size_, slot_alignment_);
    page->slots = aligned_alloc_impl(slot_alignment_, total);
    page->state = new SlotState[capacity];
    page->blocks = new HiveControlBlock*[capacity]();

    for (size_t i = 0; i < capacity; ++i) {
        page->state[i] = SlotState::Free;
    }

    // Build intrusive freelist through slot memory.
    for (size_t i = 0; i < capacity - 1; ++i) {
        size_t next = i + 1;
        std::memcpy(static_cast<char*>(page->slots) + i * slot_size_, &next, sizeof(size_t));
    }
    size_t sentinel = HIVE_SENTINEL;
    std::memcpy(static_cast<char*>(page->slots) + (capacity - 1) * slot_size_, &sentinel, sizeof(size_t));
    page->free_head = 0;
    page->live_count = 0;

    pages_.push_back(std::move(page));
}

void Hive::free_page(HivePage& page)
{
    delete[] page.state;
    page.state = nullptr;
    delete[] page.blocks;
    page.blocks = nullptr;
    aligned_free_impl(page.slots);
    page.slots = nullptr;
}

void Hive::push_free(HivePage& page, size_t index, size_t slot_sz)
{
    std::memcpy(static_cast<char*>(page.slots) + index * slot_sz, &page.free_head, sizeof(size_t));
    page.free_head = index;
}

size_t Hive::next_page_capacity() const
{
    switch (pages_.size()) {
    case 0:
        return 16;
    case 1:
        return 64;
    case 2:
        return 256;
    default:
        return 1024;
    }
}

bool Hive::find_slot(const void* obj, size_t& page_idx, size_t& slot_idx) const
{
    auto obj_addr = reinterpret_cast<uintptr_t>(obj);
    for (size_t pi = 0; pi < pages_.size(); ++pi) {
        auto& page = *pages_[pi];
        auto base = reinterpret_cast<uintptr_t>(page.slots);
        auto end = base + page.capacity * slot_size_;
        if (obj_addr >= base && obj_addr < end) {
            size_t offset = static_cast<size_t>(obj_addr - base);
            if (offset % slot_size_ == 0) {
                size_t si = offset / slot_size_;
                if (page.state[si] == SlotState::Active) {
                    page_idx = pi;
                    slot_idx = si;
                    return true;
                }
            }
        }
    }
    return false;
}

IObject::Ptr Hive::add()
{
    if (!factory_) {
        return {};
    }

    // Find a page with free slots.
    HivePage* target = nullptr;
    for (auto& page_ptr : pages_) {
        if (page_ptr->free_head != HIVE_SENTINEL) {
            target = page_ptr.get();
            break;
        }
    }

    if (!target) {
        alloc_page(next_page_capacity());
        target = pages_.back().get();
    }

    // Pop slot from freelist.
    size_t slot_idx = target->free_head;
    size_t next_free;
    std::memcpy(&next_free, static_cast<char*>(target->slots) + slot_idx * slot_size_, sizeof(size_t));
    target->free_head = next_free;
    target->state[slot_idx] = SlotState::Active;
    ++target->live_count;

    // Prepare the HiveControlBlock before construction so the object is
    // born with the correct block (no post-construction swap needed).
    auto* hcb = new HiveControlBlock{};
    hcb->ecb.strong.store(1, std::memory_order_relaxed);
    hcb->ecb.weak.store(1, std::memory_order_relaxed);
    hcb->ecb.destroy = hive_destroy;
    hcb->page = target;
    hcb->slot_index = slot_idx;
    hcb->factory = factory_;
    hcb->slot_size = slot_size_;

    // Placement-construct the object, installing the hive's control block.
    // The factory swaps in our block and returns the auto-allocated one to the pool.
    void* slot = slot_ptr(*target, slot_idx);
    auto* obj = factory_->construct_in_place(slot, &hcb->ecb, ObjectFlags::HiveManaged);

    // Set the self-pointer and external tag on the block.
    hcb->ecb.set_ptr(static_cast<void*>(obj));
    hcb->ecb.set_external_tag();

    // Store the HCB pointer in the page's per-slot array.
    target->blocks[slot_idx] = hcb;

    // The hive owns one strong ref (keeps the object alive while in the hive).
    // The returned shared_ptr will acquire a second strong ref via adopt_ref + ref().
    ++live_count_;

    IObject::Ptr result(obj, &hcb->ecb, adopt_ref);
    obj->ref(); // Hive's strong ref
    return result;
}

ReturnValue Hive::remove(IObject& object)
{
    size_t page_idx, slot_idx;
    if (!find_slot(&object, page_idx, slot_idx)) {
        return ReturnValue::Fail;
    }

    // Transition Active -> Zombie. The object stays alive until external refs drop.
    // When the last strong ref drops, unref() calls hive_destroy which transitions
    // Zombie -> Free.
    pages_[page_idx]->state[slot_idx] = SlotState::Zombie;
    --live_count_;

    // Release the hive's strong ref. If this was the last ref, unref() triggers
    // hive_destroy which reclaims the slot.
    object.unref();

    return ReturnValue::Success;
}

bool Hive::contains(const IObject& object) const
{
    size_t page_idx, slot_idx;
    return find_slot(&object, page_idx, slot_idx);
}

void Hive::for_each(void* context, VisitorFn visitor) const
{
    for (auto& page_ptr : pages_) {
        auto& page = *page_ptr;
        for (size_t i = 0; i < page.capacity; ++i) {
            if (page.state[i] == SlotState::Active) {
                auto* obj = static_cast<IObject*>(slot_ptr(page, i));
                if (!visitor(context, *obj)) {
                    return;
                }
            }
        }
    }
}

} // namespace velk

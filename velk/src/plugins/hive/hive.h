#ifndef VELK_PLUGINS_HIVE_H
#define VELK_PLUGINS_HIVE_H

#include <velk/ext/core_object.h>
#include <velk/interface/hive/intf_hive.h>
#include <velk/interface/intf_object_factory.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace velk {

static constexpr size_t HIVE_SENTINEL = ~size_t(0);

enum class SlotState : uint8_t
{
    Active = 0,
    Zombie = 1,
    Free = 2
};

struct HiveControlBlock;

struct HivePage
{
    void* slots{nullptr};               ///< Aligned contiguous slot memory.
    SlotState* state{nullptr};          ///< Per-slot state array.
    HiveControlBlock** blocks{nullptr}; ///< Per-slot HiveControlBlock pointer array.
    size_t capacity{0};                 ///< Total slots in page.
    size_t free_head{HIVE_SENTINEL};    ///< Intrusive freelist head.
    size_t live_count{0};               ///< Active + Zombie count.
};

/**
 * @brief Concrete implementation of IHive.
 *
 * Stores objects of a single class in cache-friendly contiguous pages using
 * placement-new. Slot reuse is handled via an intrusive per-page free list.
 * Objects remain alive after removal as long as external references exist
 * (zombie state); the slot is reclaimed when the last reference drops.
 */
class Hive final : public ext::ObjectCore<Hive, IHive>
{
public:
    VELK_CLASS_UID(ClassId::Hive);

    Hive() = default;
    ~Hive() override;

    /** @brief Initializes the hive for the given class UID. */
    void init(Uid classUid);

    // IHive overrides
    Uid get_element_class_uid() const override;
    size_t size() const override;
    bool empty() const override;
    IObject::Ptr add() override;
    ReturnValue remove(IObject& object) override;
    bool contains(const IObject& object) const override;
    void for_each(void* context, VisitorFn visitor) const override;

private:
    /** @brief Returns the slot pointer for a given page and slot index. */
    void* slot_ptr(HivePage& page, size_t index) const;
    void* slot_ptr(const HivePage& page, size_t index) const;

    /** @brief Allocates a new page with the given capacity. */
    void alloc_page(size_t capacity);

    /** @brief Frees a page's memory. */
    static void free_page(HivePage& page);

    /** @brief Pushes a slot onto a page's freelist. */
    static void push_free(HivePage& page, size_t index, size_t slot_size);

    /** @brief Returns the next page capacity based on current page count. */
    size_t next_page_capacity() const;

    /** @brief Finds the page and slot index for a given object pointer. Returns false if not found. */
    bool find_slot(const void* obj, size_t& page_idx, size_t& slot_idx) const;

    Uid element_class_uid_;
    const IObjectFactory* factory_{nullptr};
    size_t slot_size_{0};
    size_t slot_alignment_{0};
    size_t live_count_{0};
    std::vector<std::unique_ptr<HivePage>> pages_;
};

} // namespace velk

#endif // VELK_PLUGINS_HIVE_H

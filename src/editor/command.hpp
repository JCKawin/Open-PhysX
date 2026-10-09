#pragma once

#include "core/Types.h"
#include "ecs/scene.hpp"
#include "logic/ISimulation.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace openphysx {

// Authored document. Camera, selection, and the playhead are not part of a partial edit.
struct EditSnapshot
{
    SimulationState state{};
    Scene scene{};
    UUID active = kNullUuid;
};

// One user edit. Do applies it, Undo reverses it, Merge may fold a later edit into this one.
class Command
{
public:
    virtual ~Command() = default;
    virtual void Do(ISimulation& simulation) = 0;
    virtual void Undo(ISimulation& simulation) = 0;
    virtual bool Merge(const Command& other);

    std::uint64_t revision_before = 0;
    std::uint64_t revision_after = 0;
};

// Snapshot of the editor scene plus transport settings. The edit is already applied
// when the stack records it. Redo applies `after`, undo applies `before`.
class SnapshotCommand final : public Command
{
public:
    EditSnapshot before{};
    EditSnapshot after{};
    bool full = false;

    void Do(ISimulation& simulation) override;
    void Undo(ISimulation& simulation) override;
};

class CommandStack
{
public:
    std::uint64_t revision() const { return revision_; }
    void MarkSaved();
    bool IsDirty() const;

    // Drops history and treats the current document as clean. Used by New and Open.
    void Clear();

    // Records an edit that is already visible on `simulation`. No-ops are filtered by the caller.
    void Commit(const EditSnapshot& before, const EditSnapshot& after, bool full);
    void Undo(ISimulation& simulation);
    void Redo(ISimulation& simulation);

    bool CanUndo() const { return !undo_.empty(); }
    bool CanRedo() const { return !redo_.empty(); }

private:
    std::vector<std::unique_ptr<Command>> undo_;
    std::vector<std::unique_ptr<Command>> redo_;
    std::uint64_t revision_ = 0;
    std::uint64_t saved_ = 0;
    std::uint64_t clock_ = 0;
};

// Writes a snapshot back onto the editor scene. A partial edit keeps the current
// playhead. Playback is stopped first so the viewport shows the authored scene.
void ApplySnapshot(ISimulation& simulation, const EditSnapshot& shot, bool full);

} // namespace openphysx

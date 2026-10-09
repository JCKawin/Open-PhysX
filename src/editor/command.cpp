#include "editor/command.hpp"

namespace openphysx {
namespace {

constexpr std::size_t kHistoryLimit = 64;

} // namespace

bool Command::Merge(const Command&)
{
    return false;
}

void ApplySnapshot(ISimulation& simulation, const EditSnapshot& shot, bool full)
{
    const bool want_play = full ? shot.state.playing : simulation.state().playing;
    if (simulation.simulating())
        simulation.stop();

    if (full)
    {
        simulation.state() = shot.state;
    }
    else
    {
        const float time = simulation.state().time;
        simulation.state() = shot.state;
        simulation.state().time = time;
        simulation.state().playing = false;
    }

    simulation.editor_scene() = shot.scene;
    simulation.set_active(shot.active);
    if (want_play)
        simulation.play();
}

void SnapshotCommand::Do(ISimulation& simulation)
{
    ApplySnapshot(simulation, after, full);
}

void SnapshotCommand::Undo(ISimulation& simulation)
{
    ApplySnapshot(simulation, before, full);
}

void CommandStack::MarkSaved()
{
    saved_ = revision_;
}

void CommandStack::MarkDirty()
{
    if (revision_ == saved_)
    {
        ++clock_;
        revision_ = clock_;
    }
}

bool CommandStack::IsDirty() const
{
    return revision_ != saved_;
}

void CommandStack::Clear()
{
    undo_.clear();
    redo_.clear();
    revision_ = 0;
    saved_ = 0;
    clock_ = 0;
}

void CommandStack::Commit(const EditSnapshot& before, const EditSnapshot& after, bool full)
{
    auto command = std::make_unique<SnapshotCommand>();
    command->before = before;
    command->after = after;
    command->full = full;
    command->revision_before = revision_;
    ++clock_;
    revision_ = clock_;
    command->revision_after = revision_;

    undo_.push_back(std::move(command));
    redo_.clear();
    if (undo_.size() > kHistoryLimit)
        undo_.erase(undo_.begin());
}

void CommandStack::Undo(ISimulation& simulation)
{
    if (undo_.empty())
        return;

    std::unique_ptr<Command> command = std::move(undo_.back());
    undo_.pop_back();
    command->Undo(simulation);
    revision_ = command->revision_before;
    redo_.push_back(std::move(command));
}

void CommandStack::Redo(ISimulation& simulation)
{
    if (redo_.empty())
        return;

    std::unique_ptr<Command> command = std::move(redo_.back());
    redo_.pop_back();
    command->Do(simulation);
    revision_ = command->revision_after;
    undo_.push_back(std::move(command));
}

} // namespace openphysx

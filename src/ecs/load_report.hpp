#pragma once

#include <string>
#include <vector>

namespace openphysx {

// Warnings and errors collected while a project is loaded. ECS code records them.
// The UI shows them later. This type does not know about windows or widgets.
struct LoadReport
{
    enum class Severity
    {
        Warning,
        Error,
    };

    struct Entry
    {
        Severity severity = Severity::Warning;
        std::string message;
    };

    std::vector<Entry> entries;

    void warning(std::string message)
    {
        entries.push_back(Entry{Severity::Warning, std::move(message)});
    }

    void error(std::string message)
    {
        entries.push_back(Entry{Severity::Error, std::move(message)});
    }

    bool has_errors() const
    {
        for (const Entry& entry : entries)
        {
            if (entry.severity == Severity::Error)
                return true;
        }
        return false;
    }
};

} // namespace openphysx

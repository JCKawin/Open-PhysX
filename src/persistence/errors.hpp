#pragma once

#include <string>

namespace openphysx {

struct LoadError
{
    enum class Kind
    {
        NotFound,
        Corrupt,
        TooNew,
        Io,
        Schema,
    };

    Kind kind = Kind::Schema;
    std::string message;
};

struct SaveError
{
    enum class Kind
    {
        Io,
        DiskFull,
        Permission,
        Serialize,
    };

    Kind kind = Kind::Serialize;
    std::string message;
};

} // namespace openphysx

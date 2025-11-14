#pragma once

#include "ApiError.h"
#include "core/Cell.h"
#include "core/CommandWithCallback.h"
#include "core/Result.h"
#include <nlohmann/json.hpp>

namespace DirtSim {
namespace Api {

namespace CellGet {

struct Command {
    int x;
    int y;

    nlohmann::json toJson() const;
    static Command fromJson(const nlohmann::json& j);
};

struct Okay {
    Cell cell;

    static constexpr const char* name() { return "cell_get"; }
    nlohmann::json toJson() const;
};

using Response = Result<Okay, ApiError>;
using Cwc = CommandWithCallback<Command, Response>;

} // namespace CellGet
} // namespace Api
} // namespace DirtSim

#pragma once
#include <variant>
#include <string>
#include <vector>
#include <functional> 

using ExecuteResult = std::variant<
    std::monostate,
    std::vector<std::string>,
    std::vector<std::vector<std::string>>
>;
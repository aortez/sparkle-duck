#pragma once

#include "reflect.h"
#include <nlohmann/json.hpp>
#include <string>

/**
 * Generic reflection-based JSON serialization for aggregate types.
 *
 * Uses qlibs/reflect for compile-time introspection and nlohmann/json
 * for JSON generation. Works automatically with any aggregate type.
 *
 * Example:
 *   struct Point { double x = 0.0; double y = 0.0; };
 *   Point p{1.5, 2.5};
 *   auto j = ReflectSerializer::to_json(p);
 *   auto p2 = ReflectSerializer::from_json<Point>(j);
 */
namespace ReflectSerializer {

/**
 * Serialize any aggregate type to nlohmann::json.
 */
template <typename T>
nlohmann::json to_json(const T& obj)
{
    nlohmann::json j;

    // Use qlibs/reflect to iterate over all members.
    reflect::for_each(
        [&](auto I) {
            auto name = std::string(reflect::member_name<I>(obj));
            const auto& value = reflect::get<I>(obj);
            j[name] = value;
        },
        obj);

    return j;
}

/**
 * Deserialize nlohmann::json to any aggregate type.
 */
template <typename T>
T from_json(const nlohmann::json& j)
{
    T obj{};

    // Use qlibs/reflect to iterate over all members.
    reflect::for_each(
        [&](auto I) {
            auto name = std::string(reflect::member_name<I>(obj));
            if (j.contains(name)) {
                using MemberType = std::remove_reference_t<decltype(reflect::get<I>(obj))>;
                reflect::get<I>(obj) = j[name].get<MemberType>();
            }
        },
        obj);

    return obj;
}

} // namespace ReflectSerializer

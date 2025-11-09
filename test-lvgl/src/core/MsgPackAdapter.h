#pragma once

#include "MaterialType.h"
#include <msgpack.hpp>
#include <nlohmann/json.hpp>
#include <variant>

namespace msgpack {
MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS)
{
    namespace adaptor {

    /**
     * @brief MessagePack adapter for nlohmann::json.
     * Allows packing/unpacking nlohmann::json objects.
     */
    template <>
    struct pack<nlohmann::json> {
        template <typename Stream>
        msgpack::packer<Stream>& operator()(
            msgpack::packer<Stream>& o, nlohmann::json const& v) const
        {
            if (v.is_null()) {
                o.pack_nil();
            }
            else if (v.is_boolean()) {
                o.pack(v.get<bool>());
            }
            else if (v.is_number_integer()) {
                o.pack(v.get<int64_t>());
            }
            else if (v.is_number_unsigned()) {
                o.pack(v.get<uint64_t>());
            }
            else if (v.is_number_float()) {
                o.pack(v.get<double>());
            }
            else if (v.is_string()) {
                o.pack(v.get<std::string>());
            }
            else if (v.is_array()) {
                o.pack_array(static_cast<uint32_t>(v.size()));
                for (const auto& elem : v) {
                    o.pack(elem);
                }
            }
            else if (v.is_object()) {
                o.pack_map(static_cast<uint32_t>(v.size()));
                for (auto it = v.begin(); it != v.end(); ++it) {
                    o.pack(it.key());
                    o.pack(it.value());
                }
            }
            return o;
        }
    };

    template <>
    struct convert<nlohmann::json> {
        msgpack::object const& operator()(msgpack::object const& o, nlohmann::json& v) const
        {
            switch (o.type) {
                case msgpack::type::NIL:
                    v = nullptr;
                    break;
                case msgpack::type::BOOLEAN:
                    v = o.as<bool>();
                    break;
                case msgpack::type::POSITIVE_INTEGER:
                    v = o.as<uint64_t>();
                    break;
                case msgpack::type::NEGATIVE_INTEGER:
                    v = o.as<int64_t>();
                    break;
                case msgpack::type::FLOAT32:
                case msgpack::type::FLOAT64:
                    v = o.as<double>();
                    break;
                case msgpack::type::STR:
                    v = o.as<std::string>();
                    break;
                case msgpack::type::ARRAY:
                    v = nlohmann::json::array();
                    for (uint32_t i = 0; i < o.via.array.size; ++i) {
                        nlohmann::json elem;
                        o.via.array.ptr[i].convert(elem);
                        v.push_back(elem);
                    }
                    break;
                case msgpack::type::MAP:
                    v = nlohmann::json::object();
                    for (uint32_t i = 0; i < o.via.map.size; ++i) {
                        std::string key;
                        nlohmann::json val;
                        o.via.map.ptr[i].key.convert(key);
                        o.via.map.ptr[i].val.convert(val);
                        v[key] = val;
                    }
                    break;
                default:
                    throw msgpack::type_error();
            }
            return o;
        }
    };

    } // namespace adaptor
}
} // namespace msgpack

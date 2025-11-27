#pragma once

#include <cmath>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <zpp_bits.h>

namespace DirtSim {

/**
 * Templated 2D vector class supporting int, float, and double types.
 *
 * All operations are inline for maximum performance.
 * Provides type-specific optimizations (e.g., floating-point-only methods).
 */
template <typename T>
struct Vector2 {
    T x = T{};
    T y = T{};

    // Custom zpp_bits serialization (2 fields: x, y).
    using serialize = zpp::bits::members<2>;

    // =================================================================
    // BASIC OPERATIONS (work for all types)
    // =================================================================

    Vector2 add(const Vector2& other) const { return { x + other.x, y + other.y }; }

    Vector2 subtract(const Vector2& other) const { return { x - other.x, y - other.y }; }

    Vector2 times(T scalar) const { return { x * scalar, y * scalar }; }

    T dot(const Vector2& other) const
    {
        if constexpr (std::is_integral_v<T>) {
            return x * other.x + y * other.y;
        }
        else {
            return x * other.x + y * other.y;
        }
    }

    T magnitudeSquared() const { return x * x + y * y; }

    std::string toString() const
    {
        return "(" + std::to_string(x) + ", " + std::to_string(y) + ")";
    }

    // =================================================================
    // MAGNITUDE AND NORMALIZATION
    // =================================================================

    // Magnitude (always returns floating-point for precision).
    auto mag() const
    {
        if constexpr (std::is_integral_v<T>) {
            return std::sqrt(static_cast<double>(x * x + y * y));
        }
        else {
            return std::sqrt(x * x + y * y);
        }
    }

    auto magnitude() const { return mag(); }
    auto length() const { return mag(); }

    // Normalize (returns appropriate type based on T).
    Vector2 normalize() const
    {
        auto magnitude = mag();
        if (magnitude > 0) {
            if constexpr (std::is_integral_v<T>) {
                // For integers, round to nearest int.
                return { static_cast<T>(std::round(x / magnitude)),
                         static_cast<T>(std::round(y / magnitude)) };
            }
            else {
                // For floats/doubles, use direct division.
                return times(T{ 1 } / static_cast<T>(magnitude));
            }
        }
        return *this;
    }

    // =================================================================
    // FLOATING-POINT ONLY OPERATIONS
    // =================================================================

    // These methods are only available for floating-point types.
    template <typename U = T>
    std::enable_if_t<std::is_floating_point_v<U>, Vector2> reflect(const Vector2& normal) const
    {
        // Reflection formula: r = v - 2(v·n)n.
        Vector2 unitNormal = normal.normalize();
        T dotProduct = dot(unitNormal);
        return *this - unitNormal * (T{ 2 } * dotProduct);
    }

    template <typename U = T>
    std::enable_if_t<std::is_floating_point_v<U>, T> angle() const
    {
        // Returns angle in radians from positive x-axis.
        return std::atan2(y, x);
    }

    template <typename U = T>
    std::enable_if_t<std::is_floating_point_v<U>, T> angleTo(const Vector2& other) const
    {
        // Returns angle between this vector and other vector.
        T thisAngle = angle();
        T otherAngle = other.angle();
        T diff = otherAngle - thisAngle;

        // Normalize to [-π, π].
        while (diff > M_PI)
            diff -= T{ 2 } * M_PI;
        while (diff < -M_PI)
            diff += T{ 2 } * M_PI;

        return diff;
    }

    template <typename U = T>
    std::enable_if_t<std::is_floating_point_v<U>, Vector2> rotateBy(T radians) const
    {
        T cosAngle = std::cos(radians);
        T sinAngle = std::sin(radians);
        return { x * cosAngle - y * sinAngle, x * sinAngle + y * cosAngle };
    }

    template <typename U = T>
    std::enable_if_t<std::is_floating_point_v<U>, Vector2> perpendicular() const
    {
        // Returns a vector perpendicular to this one (rotated 90° counterclockwise).
        return { -y, x };
    }

    // =================================================================
    // OPERATOR OVERLOADS
    // =================================================================

    Vector2 operator+(const Vector2& other) const { return add(other); }

    Vector2 operator-(const Vector2& other) const { return subtract(other); }

    Vector2 operator*(T scalar) const { return times(scalar); }

    Vector2 operator/(T scalar) const
    {
        if constexpr (std::is_integral_v<T>) {
            if (scalar == T{ 0 }) {
                throw std::runtime_error("Vector2::operator/: Division by zero");
            }
        }
        else {
            if (scalar == T{ 0 }) {
                throw std::runtime_error("Vector2::operator/: Division by zero");
            }
        }
        return { x / scalar, y / scalar };
    }

    bool operator==(const Vector2& other) const { return x == other.x && y == other.y; }

    bool operator!=(const Vector2& other) const { return !(*this == other); }

    Vector2& operator+=(const Vector2& other)
    {
        x += other.x;
        y += other.y;
        return *this;
    }

    Vector2& operator-=(const Vector2& other)
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    Vector2& operator*=(T scalar)
    {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    Vector2& operator/=(T scalar)
    {
        if constexpr (std::is_integral_v<T>) {
            if (scalar == T{ 0 }) {
                throw std::runtime_error("Vector2::operator/=: Division by zero");
            }
        }
        else {
            if (scalar == T{ 0 }) {
                throw std::runtime_error("Vector2::operator/=: Division by zero");
            }
        }
        x /= scalar;
        y /= scalar;
        return *this;
    }

    Vector2 operator-() const { return { -x, -y }; }

    Vector2 operator+() const { return { x, y }; }

    // =================================================================
    // JSON SERIALIZATION
    // =================================================================

    nlohmann::json toJson() const
    {
        // Use ReflectSerializer for aggregate types.
        return nlohmann::json{ { "x", x }, { "y", y } };
    }

    static Vector2 fromJson(const nlohmann::json& json)
    {
        return { json.at("x").get<T>(), json.at("y").get<T>() };
    }
};

// =================================================================
// TYPE ALIASES (backward compatibility)
// =================================================================

using Vector2d = Vector2<double>;
using Vector2f = Vector2<float>;
using Vector2i = Vector2<int>;

// =================================================================
// NON-MEMBER OPERATORS
// =================================================================

// Scalar multiplication from left side: scalar * vector.
template <typename T>
inline Vector2<T> operator*(T scalar, const Vector2<T>& v)
{
    return v * scalar;
}

// =================================================================
// JSON ADL (Argument-Dependent Lookup) FUNCTIONS
// =================================================================

template <typename T>
inline void to_json(nlohmann::json& j, const Vector2<T>& v)
{
    j = v.toJson();
}

template <typename T>
inline void from_json(const nlohmann::json& j, Vector2<T>& v)
{
    v = Vector2<T>::fromJson(j);
}

} // namespace DirtSim

// =================================================================
// STD::HASH SPECIALIZATION (for unordered containers)
// =================================================================

namespace std {
template <typename T>
struct hash<DirtSim::Vector2<T>> {
    std::size_t operator()(const DirtSim::Vector2<T>& v) const noexcept
    {
        std::size_t h1 = std::hash<T>{}(v.x);
        std::size_t h2 = std::hash<T>{}(v.y);
        return h1 ^ (h2 << 1);
    }
};
} // namespace std

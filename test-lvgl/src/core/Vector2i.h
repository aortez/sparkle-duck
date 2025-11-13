#pragma once

#include <string>

namespace DirtSim {

class Vector2i {
public:
    int x;
    int y;

    Vector2i();
    Vector2i(int x, int y);

    Vector2i add(const Vector2i& other) const;
    Vector2i subtract(const Vector2i& other) const;
    Vector2i times(int scalar) const;
    double mag() const;
    double length() const { return mag(); }
    int dot(const Vector2i& other) const;
    Vector2i normalize() const;
    std::string toString() const;

    Vector2i operator+(const Vector2i& other) const;
    Vector2i operator-(const Vector2i& other) const;
    Vector2i operator*(int scalar) const;
    bool operator==(const Vector2i& other) const;
    Vector2i& operator+=(const Vector2i& other);
    Vector2i& operator-=(const Vector2i& other);
    Vector2i& operator*=(int scalar);
    Vector2i operator/(int scalar) const;
    Vector2i& operator/=(int scalar);

    // Unary operators
    Vector2i operator-() const; // Negation operator
    Vector2i operator+() const; // Unary plus (for completeness)
};

inline Vector2i operator*(int scalar, const Vector2i& v)
{
    return v * scalar;
}

} // namespace DirtSim

// Hash specialization for std::unordered_map/set support.
namespace std {
template <>
struct hash<DirtSim::Vector2i> {
    std::size_t operator()(const DirtSim::Vector2i& v) const noexcept
    {
        // Simple hash combining x and y coordinates.
        std::size_t h1 = std::hash<int>{}(v.x);
        std::size_t h2 = std::hash<int>{}(v.y);
        return h1 ^ (h2 << 1);
    }
};
} // namespace std
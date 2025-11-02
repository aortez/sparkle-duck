#pragma once

#include <nlohmann/json.hpp>
#include <string>

struct Vector2d {
    double x = 0.0;
    double y = 0.0;

    Vector2d add(const Vector2d& other) const;
    Vector2d subtract(const Vector2d& other) const;
    Vector2d times(double scalar) const;
    double mag() const;
    double magnitude() const { return mag(); }
    double length() const { return mag(); }
    double dot(const Vector2d& other) const;
    Vector2d normalize() const;
    std::string toString() const;

    // Collision physics operations
    Vector2d reflect(const Vector2d& normal) const;
    double angle() const;
    double angleTo(const Vector2d& other) const;
    Vector2d rotateBy(double radians) const;
    Vector2d perpendicular() const;

    // JSON serialization.
    nlohmann::json toJson() const;
    static Vector2d fromJson(const nlohmann::json& json);

    // Operator overloads for more natural syntax.
    Vector2d operator+(const Vector2d& other) const;
    Vector2d operator-(const Vector2d& other) const;
    Vector2d operator*(double scalar) const;
    bool operator==(const Vector2d& other) const;
    Vector2d& operator+=(const Vector2d& other);
    Vector2d& operator-=(const Vector2d& other);
    Vector2d& operator*=(double scalar);
    Vector2d operator/(double scalar) const;
    Vector2d& operator/=(double scalar);

    // Unary operators
    Vector2d operator-() const; // Negation operator
    Vector2d operator+() const; // Unary plus (for completeness)
};

/**
 * ADL (Argument-Dependent Lookup) functions for nlohmann::json automatic conversion.
 */
inline void to_json(nlohmann::json& j, const Vector2d& v)
{
    j = v.toJson();
}

inline void from_json(const nlohmann::json& j, Vector2d& v)
{
    v = Vector2d::fromJson(j);
}

// Non-member operator for scalar multiplication from the left side.
inline Vector2d operator*(double scalar, const Vector2d& v)
{
    return v * scalar;
}

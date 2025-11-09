#include "Vector2d.h"

#include <cmath>
#include <stdexcept>
#include <string>

using namespace DirtSim;

Vector2d Vector2d::add(const Vector2d& other) const
{
    return Vector2d{ x + other.x, y + other.y };
}

Vector2d Vector2d::subtract(const Vector2d& other) const
{
    return Vector2d{ x - other.x, y - other.y };
}

Vector2d Vector2d::times(double scalar) const
{
    return Vector2d{ x * scalar, y * scalar };
}

double Vector2d::mag() const
{
    return std::sqrt(x * x + y * y);
}

double Vector2d::dot(const Vector2d& other) const
{
    return x * other.x + y * other.y;
}

Vector2d Vector2d::normalize() const
{
    double magnitude = mag();
    if (magnitude > 0.0) {
        return times(1.0 / magnitude);
    }
    return *this;
}

std::string Vector2d::toString() const
{
    return "(" + std::to_string(x) + ", " + std::to_string(y) + ")";
}

// Operator implementations.
Vector2d Vector2d::operator+(const Vector2d& other) const
{
    return add(other);
}

Vector2d Vector2d::operator-(const Vector2d& other) const
{
    return subtract(other);
}

Vector2d Vector2d::operator*(double scalar) const
{
    return times(scalar);
}

bool Vector2d::operator==(const Vector2d& other) const
{
    return x == other.x && y == other.y;
}

Vector2d& Vector2d::operator+=(const Vector2d& other)
{
    x += other.x;
    y += other.y;
    return *this;
}

Vector2d& Vector2d::operator-=(const Vector2d& other)
{
    x -= other.x;
    y -= other.y;
    return *this;
}

Vector2d& Vector2d::operator*=(double scalar)
{
    x *= scalar;
    y *= scalar;
    return *this;
}

Vector2d Vector2d::operator/(double scalar) const
{
    if (scalar == 0.0) {
        throw std::runtime_error("Vector2d::operator/: Division by zero");
    }
    return Vector2d{ x / scalar, y / scalar };
}

Vector2d& Vector2d::operator/=(double scalar)
{
    if (scalar == 0.0) {
        throw std::runtime_error("Vector2d::operator/=: Division by zero");
    }
    x /= scalar;
    y /= scalar;
    return *this;
}

Vector2d Vector2d::operator-() const
{
    return Vector2d{ -x, -y };
}

Vector2d Vector2d::operator+() const
{
    return Vector2d{ x, y };
}

#include "ReflectSerializer.h"

nlohmann::json Vector2d::toJson() const
{
    return ReflectSerializer::to_json(*this);
}

Vector2d Vector2d::fromJson(const nlohmann::json& json)
{
    return ReflectSerializer::from_json<Vector2d>(json);
}

// Collision physics operations.
Vector2d Vector2d::reflect(const Vector2d& normal) const
{
    // Reflection formula: r = v - 2(v·n)n.
    // where v is incident vector, n is unit normal, r is reflected vector.
    Vector2d unitNormal = normal.normalize();
    double dotProduct = dot(unitNormal);
    return *this - unitNormal * (2.0 * dotProduct);
}

double Vector2d::angle() const
{
    // Returns angle in radians from positive x-axis.
    return std::atan2(y, x);
}

double Vector2d::angleTo(const Vector2d& other) const
{
    // Returns angle between this vector and other vector.
    double thisAngle = angle();
    double otherAngle = other.angle();
    double diff = otherAngle - thisAngle;

    // Normalize to [-π, π]
    while (diff > M_PI)
        diff -= 2.0 * M_PI;
    while (diff < -M_PI)
        diff += 2.0 * M_PI;

    return diff;
}

Vector2d Vector2d::rotateBy(double radians) const
{
    double cosAngle = std::cos(radians);
    double sinAngle = std::sin(radians);

    return Vector2d{ x * cosAngle - y * sinAngle, x * sinAngle + y * cosAngle };
}

Vector2d Vector2d::perpendicular() const
{
    // Returns a vector perpendicular to this one (rotated 90° counterclockwise).
    return Vector2d{ -y, x };
}
#include "Vector2i.h"

#include <cmath>
#include <stdexcept>
#include <string>

using namespace DirtSim;

Vector2i::Vector2i() : x(0), y(0)
{}

Vector2i::Vector2i(int x, int y) : x(x), y(y)
{}

Vector2i Vector2i::add(const Vector2i& other) const
{
    return Vector2i(x + other.x, y + other.y);
}

Vector2i Vector2i::subtract(const Vector2i& other) const
{
    return Vector2i(x - other.x, y - other.y);
}

Vector2i Vector2i::times(int scalar) const
{
    return Vector2i(x * scalar, y * scalar);
}

double Vector2i::mag() const
{
    return std::sqrt(static_cast<double>(x * x + y * y));
}

int Vector2i::dot(const Vector2i& other) const
{
    return x * other.x + y * other.y;
}

Vector2i Vector2i::normalize() const
{
    double magnitude = mag();
    if (magnitude > 0.0) {
        return Vector2i(
            static_cast<int>(std::round(x / magnitude)),
            static_cast<int>(std::round(y / magnitude)));
    }
    return *this;
}

std::string Vector2i::toString() const
{
    return "(" + std::to_string(x) + ", " + std::to_string(y) + ")";
}

Vector2i Vector2i::operator+(const Vector2i& other) const
{
    return add(other);
}

Vector2i Vector2i::operator-(const Vector2i& other) const
{
    return subtract(other);
}

Vector2i Vector2i::operator*(int scalar) const
{
    return times(scalar);
}

bool Vector2i::operator==(const Vector2i& other) const
{
    return x == other.x && y == other.y;
}

Vector2i& Vector2i::operator+=(const Vector2i& other)
{
    x += other.x;
    y += other.y;
    return *this;
}

Vector2i& Vector2i::operator-=(const Vector2i& other)
{
    x -= other.x;
    y -= other.y;
    return *this;
}

Vector2i& Vector2i::operator*=(int scalar)
{
    x *= scalar;
    y *= scalar;
    return *this;
}

Vector2i Vector2i::operator/(int scalar) const
{
    if (scalar == 0) {
        throw std::runtime_error("Vector2i::operator/: Division by zero");
    }
    return Vector2i(x / scalar, y / scalar);
}

Vector2i& Vector2i::operator/=(int scalar)
{
    if (scalar == 0) {
        throw std::runtime_error("Vector2i::operator/=: Division by zero");
    }
    x /= scalar;
    y /= scalar;
    return *this;
}

Vector2i Vector2i::operator-() const
{
    return Vector2i(-x, -y);
}

Vector2i Vector2i::operator+() const
{
    return Vector2i(x, y);
}
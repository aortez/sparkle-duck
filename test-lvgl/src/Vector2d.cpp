#include "Vector2d.h"

#include <cmath>
#include <string>

Vector2d::Vector2d() : x(0.0), y(0.0)
{}

Vector2d::Vector2d(double x, double y) : x(x), y(y)
{}

Vector2d Vector2d::add(const Vector2d& other) const
{
    return Vector2d(x + other.x, y + other.y);
}

Vector2d Vector2d::subtract(const Vector2d& other) const
{
    return Vector2d(x - other.x, y - other.y);
}

Vector2d Vector2d::times(double scalar) const
{
    return Vector2d(x * scalar, y * scalar);
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

// Operator implementations
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
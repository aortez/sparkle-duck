#include "Vector2d.h"

#include <cmath>
#include <stdexcept>
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
    return Vector2d(x / scalar, y / scalar);
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

rapidjson::Value Vector2d::toJson(rapidjson::Document::AllocatorType& allocator) const
{
    rapidjson::Value json(rapidjson::kObjectType);
    json.AddMember("x", x, allocator);
    json.AddMember("y", y, allocator);
    return json;
}

Vector2d Vector2d::fromJson(const rapidjson::Value& json)
{
    if (!json.IsObject()) {
        throw std::runtime_error("Vector2d::fromJson: JSON value must be an object");
    }
    
    if (!json.HasMember("x") || !json.HasMember("y")) {
        throw std::runtime_error("Vector2d::fromJson: JSON object must have 'x' and 'y' members");
    }
    
    if (!json["x"].IsNumber() || !json["y"].IsNumber()) {
        throw std::runtime_error("Vector2d::fromJson: 'x' and 'y' members must be numbers");
    }
    
    return Vector2d(json["x"].GetDouble(), json["y"].GetDouble());
}
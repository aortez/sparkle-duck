#include "Cell.h"
#include <string>

Cell::Cell() : dirty(0.0), com(0.0, 0.0), v(0.0, 0.0) {}

std::string Cell::toString() const {
    return "Cell{dirty=" + std::to_string(dirty) + 
           ", com=" + com.toString() + 
           ", v=" + v.toString() + "}";
} 
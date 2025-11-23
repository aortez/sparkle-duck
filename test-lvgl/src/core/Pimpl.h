#pragma once

#include <memory>
#include <utility>

namespace DirtSim {

/**
 * Generic Pimpl (Pointer to Implementation) wrapper.
 *
 * This template provides a reusable implementation of the Pimpl idiom,
 * which reduces compilation dependencies by hiding implementation details
 * behind a forward-declared pointer.
 *
 * Benefits:
 * - Reduces compilation dependencies (header changes don't ripple to clients).
 * - Hides implementation details from public API.
 * - Enables stable ABI across versions.
 *
 * Trade-offs:
 * - Extra indirection cost (negligible for non-hot-path objects).
 * - Heap allocation (fine for long-lived objects like World).
 *
 * Usage:
 *   // In MyClass.h:
 *   class MyClass {
 *   public:
 *       MyClass(int x, int y);
 *       ~MyClass();
 *       MyClass(MyClass&&) noexcept;
 *       MyClass& operator=(MyClass&&) noexcept;
 *
 *       void doSomething();
 *
 *   private:
 *       struct Impl;
 *       Pimpl<Impl> pImpl;
 *   };
 *
 *   // In MyClass.cpp:
 *   struct MyClass::Impl {
 *       int x_, y_;
 *       HeavyDependency dep_;
 *
 *       Impl(int x, int y) : x_(x), y_(y) {}
 *       void doSomething() { ... }
 *   };
 *
 *   MyClass::MyClass(int x, int y) : pImpl(x, y) {}
 *   MyClass::~MyClass() = default;
 *   MyClass::MyClass(MyClass&&) noexcept = default;
 *   MyClass& MyClass::operator=(MyClass&&) noexcept = default;
 *
 *   void MyClass::doSomething() {
 *       pImpl->doSomething();
 *   }
 */
template <typename T>
class Pimpl {
public:
    // Constructor forwards arguments to T's constructor.
    template <typename... Args>
    Pimpl(Args&&... args) : impl_(std::make_unique<T>(std::forward<Args>(args)...))
    {}

    // Destructor (must be defined in .cpp where T is complete).
    ~Pimpl() = default;

    // Move semantics (move-only by default).
    Pimpl(Pimpl&&) noexcept = default;
    Pimpl& operator=(Pimpl&&) noexcept = default;

    // Delete copy operations (can be customized per-class if deep copy needed).
    Pimpl(const Pimpl&) = delete;
    Pimpl& operator=(const Pimpl&) = delete;

    // Access operators for calling implementation methods.
    T* operator->() { return impl_.get(); }
    const T* operator->() const { return impl_.get(); }

    T& operator*() { return *impl_; }
    const T& operator*() const { return *impl_; }

    // Direct access to pointer (for advanced use cases).
    T* get() { return impl_.get(); }
    const T* get() const { return impl_.get(); }

private:
    std::unique_ptr<T> impl_;
};

} // namespace DirtSim

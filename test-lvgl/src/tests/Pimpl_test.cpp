#include "core/Pimpl.h"

#include <gtest/gtest.h>
#include <string>

using namespace DirtSim;

namespace {

// Test implementation struct for Pimpl.
struct TestImpl {
    int value_;
    std::string name_;

    TestImpl(int value, std::string name) : value_(value), name_(std::move(name)) {}

    int getValue() const { return value_; }
    void setValue(int v) { value_ = v; }
    const std::string& getName() const { return name_; }
};

// Wrapper class using Pimpl.
class TestClass {
public:
    TestClass(int value, std::string name) : pImpl(value, std::move(name)) {}
    ~TestClass() = default;

    // Move semantics.
    TestClass(TestClass&&) noexcept = default;
    TestClass& operator=(TestClass&&) noexcept = default;

    // Copy operations deleted.
    TestClass(const TestClass&) = delete;
    TestClass& operator=(const TestClass&) = delete;

    // Public interface forwarding to Pimpl.
    int getValue() const { return pImpl->getValue(); }
    void setValue(int v) { pImpl->setValue(v); }
    const std::string& getName() const { return pImpl->getName(); }

private:
    Pimpl<TestImpl> pImpl;
};

} // namespace

TEST(PimplTest, ConstructionAndDestruction)
{
    // Test basic construction with forwarded arguments.
    TestClass obj(42, "test");
    EXPECT_EQ(obj.getValue(), 42);
    EXPECT_EQ(obj.getName(), "test");

    // Destruction happens automatically - no leaks.
}

TEST(PimplTest, ArrowOperator)
{
    TestClass obj(10, "arrow");

    // Test non-const arrow operator.
    obj.setValue(20);
    EXPECT_EQ(obj.getValue(), 20);

    // Test const arrow operator.
    const TestClass& constRef = obj;
    EXPECT_EQ(constRef.getValue(), 20);
    EXPECT_EQ(constRef.getName(), "arrow");
}

TEST(PimplTest, DereferenceOperator)
{
    Pimpl<TestImpl> pimpl(15, "deref");

    // Test non-const dereference.
    EXPECT_EQ((*pimpl).getValue(), 15);
    (*pimpl).setValue(25);
    EXPECT_EQ((*pimpl).getValue(), 25);

    // Test const dereference.
    const Pimpl<TestImpl>& constRef = pimpl;
    EXPECT_EQ((*constRef).getValue(), 25);
    EXPECT_EQ((*constRef).getName(), "deref");
}

TEST(PimplTest, MoveConstructor)
{
    TestClass obj1(100, "original");
    EXPECT_EQ(obj1.getValue(), 100);

    // Move construct.
    TestClass obj2(std::move(obj1));
    EXPECT_EQ(obj2.getValue(), 100);
    EXPECT_EQ(obj2.getName(), "original");

    // obj1 is now in moved-from state (valid but unspecified).
}

TEST(PimplTest, MoveAssignment)
{
    TestClass obj1(200, "first");
    TestClass obj2(300, "second");

    EXPECT_EQ(obj1.getValue(), 200);
    EXPECT_EQ(obj2.getValue(), 300);

    // Move assign.
    obj1 = std::move(obj2);
    EXPECT_EQ(obj1.getValue(), 300);
    EXPECT_EQ(obj1.getName(), "second");

    // obj2 is now in moved-from state.
}

TEST(PimplTest, GetMethod)
{
    Pimpl<TestImpl> pimpl(50, "getter");

    // Test non-const get().
    TestImpl* ptr = pimpl.get();
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->getValue(), 50);

    // Test const get().
    const Pimpl<TestImpl>& constRef = pimpl;
    const TestImpl* constPtr = constRef.get();
    ASSERT_NE(constPtr, nullptr);
    EXPECT_EQ(constPtr->getValue(), 50);
}

TEST(PimplTest, ForwardMultipleArguments)
{
    // Test that Pimpl correctly forwards multiple constructor arguments.
    Pimpl<TestImpl> pimpl(999, std::string("forwarded"));
    EXPECT_EQ(pimpl->getValue(), 999);
    EXPECT_EQ(pimpl->getName(), "forwarded");
}

TEST(PimplTest, Modification)
{
    TestClass obj(5, "modify");
    EXPECT_EQ(obj.getValue(), 5);

    // Modify through public interface.
    obj.setValue(10);
    EXPECT_EQ(obj.getValue(), 10);

    obj.setValue(15);
    EXPECT_EQ(obj.getValue(), 15);
}

// Compilation test: verify copy operations are deleted.
// (Uncomment to verify compilation failure)
/*
TEST(PimplTest, CopyConstructorDeleted) {
    TestClass obj1(1, "copy");
    TestClass obj2(obj1);  // Should fail to compile.
}

TEST(PimplTest, CopyAssignmentDeleted) {
    TestClass obj1(1, "copy");
    TestClass obj2(2, "other");
    obj2 = obj1;  // Should fail to compile.
}
*/

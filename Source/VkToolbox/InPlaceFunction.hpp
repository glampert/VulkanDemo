#pragma once

// ================================================================================================
// File: VkToolbox/InPlaceFunction.hpp
// Author: Guilherme R. Lampert
// Created on: 14/05/17
// Brief: std::function-like functor that has an inline buffer to store the
//        callable object. It is guaranteed to never allocate. If the callable is
//        too big, a static_assert is triggered. The size of the functor is defined
//        as a template argument, so it can be of any size. Doesn't use exceptions.
// ================================================================================================

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

// Configurable, but should be a power of two.
#ifndef INPLACE_FUNCTION_ALIGN
    #define INPLACE_FUNCTION_ALIGN 16
#endif // INPLACE_FUNCTION_ALIGN

// Used to trap null function calls. Exceptions are not used.
#ifndef INPLACE_FUNCTION_ASSERT
    #include <cassert>
    #define INPLACE_FUNCTION_ASSERT(expr, message) assert(expr && message)
#endif // INPLACE_FUNCTION_ASSERT

namespace VkToolbox
{

// ========================================================
// class InPlaceFunctionBase:
// ========================================================

template<int, typename>
class InPlaceFunctionBase; // Unimplemented

template<int SizeInBytes, typename ReturnType, typename... ArgTypes>
class InPlaceFunctionBase<SizeInBytes, ReturnType(ArgTypes...)>
{
private:

    struct BaseCallableObject
    {
        virtual ReturnType invoke(ArgTypes... args) const = 0;
        virtual BaseCallableObject * copyConstruct(void * where) const = 0;
        virtual BaseCallableObject * moveConstruct(void * where) = 0;
        virtual ~BaseCallableObject() = default;
    };

    template<typename F>
    struct CallableObjectImpl final
        : public BaseCallableObject
    {
        F m_fn;

        CallableObjectImpl(const F & fn)
            : m_fn{ fn }
        { }
        CallableObjectImpl(F && fn)
            : m_fn{ std::move(fn) }
        { }
        ReturnType invoke(ArgTypes... args) const override
        {
            return m_fn(std::forward<ArgTypes>(args)...);
        }
        BaseCallableObject * copyConstruct(void * where) const override
        {
            return ::new(where) CallableObjectImpl<F>{ m_fn };
        }
        BaseCallableObject * moveConstruct(void * where) override
        {
            return ::new(where) CallableObjectImpl<F>{ std::move(m_fn) };
        }
    };

    enum Constants
    {
        kUserAlign = INPLACE_FUNCTION_ALIGN,
        kMinAlign  = (kUserAlign > alignof(std::max_align_t)) ? kUserAlign : alignof(std::max_align_t),
        kMinSize   = (sizeof(BaseCallableObject *) + (kMinAlign - 1)) & ~(kMinAlign - 1),
        kMaxSize   = SizeInBytes - sizeof(BaseCallableObject *),
    };

    static_assert(SizeInBytes >= kMinSize, "Requested InPlaceFunction size is too small.");

    union Storage
    {
        struct FunctionObject
        {
            // Space for a lambda capture, etc, minus the reserved callable pointer.
            alignas(kMinAlign) std::uint8_t buffer[kMaxSize];

            // One pointer worth of memory is taken from the end for the functor pointer.
            BaseCallableObject * callablePtr;
        } funcObj;

        // In case INPLACE_FUNCTION_ALIGN is less than the minimum, don't rely solely on alignas().
        std::max_align_t maxAlign;
    } m_storage;

protected:

    void * placementBuffer()
    {
        return m_storage.funcObj.buffer;
    }

    BaseCallableObject * callablePtr() const
    {
        return m_storage.funcObj.callablePtr;
    }

    void setCallablePtr(BaseCallableObject * callable)
    {
        m_storage.funcObj.callablePtr = callable;
    }

    template<typename F> void placeFunctionObject(F && fn)
    {
        static_assert(sizeof(CallableObjectImpl<F>) <= kMaxSize, "Function object too big to fit in InPlaceFunction! Use a bigger size.");
        CallableObjectImpl<F> temp{ std::move(fn) };
        setCallablePtr(temp.moveConstruct(placementBuffer()));
    }

    void placeMove(InPlaceFunctionBase * other)
    {
        BaseCallableObject * otherCallable = other->callablePtr();
        if (otherCallable != nullptr)
        {
            setCallablePtr(otherCallable->moveConstruct(placementBuffer()));
            other->setCallablePtr(nullptr);
        }
    }

    void placeCopy(const InPlaceFunctionBase & other)
    {
        const BaseCallableObject * otherCallable = other.callablePtr();
        if (otherCallable != nullptr)
        {
            setCallablePtr(otherCallable->copyConstruct(placementBuffer()));
        }
    }

    void destroy()
    {
        BaseCallableObject * myCallable = callablePtr();
        if (myCallable != nullptr)
        {
            myCallable->~BaseCallableObject();
            setCallablePtr(nullptr);
        }
    }

public:

    InPlaceFunctionBase()
    {
        setCallablePtr(nullptr);
    }

    ~InPlaceFunctionBase()
    {
        destroy();
    }

    bool isNull() const
    {
        return callablePtr() == nullptr;
    }

    ReturnType operator()(ArgTypes... args) const
    {
        INPLACE_FUNCTION_ASSERT(!isNull(), "Invalid InPlaceFunction call!");
        return callablePtr()->invoke(std::forward<ArgTypes>(args)...);
    }
};

// ========================================================
// class InPlaceFunction:
// ========================================================

template<int SizeInBytes, typename FuncType>
class InPlaceFunction final
    : public InPlaceFunctionBase<SizeInBytes, FuncType>
{
public:

    //
    // Constructors:
    //
    InPlaceFunction() = default;
    InPlaceFunction(std::nullptr_t) { } // Implicitly null.
    InPlaceFunction(InPlaceFunction && other)
    {
        placeMove(&other);
    }
    InPlaceFunction(const InPlaceFunction & other)
    {
        placeCopy(other);
    }
    template<typename F> InPlaceFunction(F fn)
    {
        placeFunctionObject(std::move(fn));
    }

    //
    // Assignment:
    //
    InPlaceFunction & operator = (std::nullptr_t)
    {
        destroy();
        return *this;
    }
    InPlaceFunction & operator = (InPlaceFunction && other)
    {
        destroy();
        placeMove(&other);
        return *this;
    }
    InPlaceFunction & operator = (const InPlaceFunction & other)
    {
        destroy();
        placeCopy(other);
        return *this;
    }
    template<typename F> InPlaceFunction & operator = (F fn)
    {
        destroy();
        placeFunctionObject(std::move(fn));
        return *this;
    }

    //
    // Null pointer comparison:
    //
    explicit operator bool() const
    {
        return !isNull();
    }
    bool operator == (std::nullptr_t) const
    {
        return isNull();
    }
    bool operator != (std::nullptr_t) const
    {
        return !isNull();
    }

    // Disallow copying or assigning from a function of different size.
    template<int OtherSize> InPlaceFunction(const InPlaceFunction<OtherSize, FuncType> &) = delete;
    template<int OtherSize> InPlaceFunction & operator = (const InPlaceFunction<OtherSize, FuncType> &) = delete;
};

// ========================================================
// Aliases for common sizes:
// ========================================================

template<typename F> using InPlaceFunction32  = InPlaceFunction< 32, F>;
template<typename F> using InPlaceFunction64  = InPlaceFunction< 64, F>;
template<typename F> using InPlaceFunction128 = InPlaceFunction<128, F>;
template<typename F> using InPlaceFunction256 = InPlaceFunction<256, F>;
template<typename F> using InPlaceFunction512 = InPlaceFunction<512, F>;

static_assert(sizeof(InPlaceFunction32<void()>)  ==  32, "Wrong size for InPlaceFunction32");
static_assert(sizeof(InPlaceFunction64<void()>)  ==  64, "Wrong size for InPlaceFunction64");
static_assert(sizeof(InPlaceFunction128<void()>) == 128, "Wrong size for InPlaceFunction128");
static_assert(sizeof(InPlaceFunction256<void()>) == 256, "Wrong size for InPlaceFunction256");
static_assert(sizeof(InPlaceFunction512<void()>) == 512, "Wrong size for InPlaceFunction512");

// ========================================================
// class MemberFunctionHolder:
// ========================================================

//
// Allows easily creating an InPlaceFunction from a 
// pointer to an object and class member function.
// Use the provided makeInPlaceFunctionFromMember()
// functions to create the wrappers. The function
// type returned will be an InPlaceFunction32. No
// additional memory allocated. Example:
//
//  MyClass obj{};
//  auto memFun = makeInPlaceFunctionFromMember(&obj, &MyClass::someMethod);
//  memFun(); // calls obj->someMethod();
//
template<typename ClassType, typename ReturnType, typename... ArgTypes>
class MemberFunctionHolder final
{
public:
    // This selector will use the const-qualified method if
    // ClassType is const, the non-const one otherwise.
    using MMemFunc    = ReturnType (ClassType::*)(ArgTypes...);
    using CMemFunc    = ReturnType (ClassType::*)(ArgTypes...) const;
    using MemFuncType = typename std::conditional<std::is_const<ClassType>::value, CMemFunc, MMemFunc>::type;

    MemberFunctionHolder(ClassType * obj, MemFuncType pmf)
        : m_pObject(obj), m_pMemFun(pmf)
    {
        INPLACE_FUNCTION_ASSERT(m_pObject != nullptr, "Null this pointer!");
        INPLACE_FUNCTION_ASSERT(m_pMemFun != nullptr, "Null member pointer!");
    }

    ReturnType operator()(ArgTypes... args) const
    {
        return (m_pObject->*m_pMemFun)(std::forward<ArgTypes>(args)...);
    }

private:
    ClassType * m_pObject;
    MemFuncType m_pMemFun;
};

// ========================================================

template<typename ClassType, typename ReturnType, typename... ArgTypes>
inline InPlaceFunction32<ReturnType(ArgTypes...)>
makeInPlaceFunctionFromMember(ClassType * obj, ReturnType (ClassType::*pmf)(ArgTypes...))
{
    return InPlaceFunction32<ReturnType(ArgTypes...)>(
        MemberFunctionHolder<ClassType, ReturnType, ArgTypes...>(obj, pmf));
}

template<typename ClassType, typename ReturnType, typename... ArgTypes>
inline InPlaceFunction32<ReturnType(ArgTypes...)>
makeInPlaceFunctionFromMember(const ClassType * obj, ReturnType (ClassType::*pmf)(ArgTypes...) const)
{
    return InPlaceFunction32<ReturnType(ArgTypes...)>(
        MemberFunctionHolder<const ClassType, ReturnType, ArgTypes...>(obj, pmf));
}

// ========================================================

} // namespace VkToolbox

// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_API_ARGUMENTS_H_
#define V8_API_ARGUMENTS_H_

#include "src/api.h"
#include "src/isolate.h"
#include "src/tracing/trace-event.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {

// Custom arguments replicate a small segment of stack that can be
// accessed through an Arguments object the same way the actual stack
// can.
template <int kArrayLength>
class CustomArgumentsBase : public Relocatable {
 public:
  virtual inline void IterateInstance(ObjectVisitor* v) {
    v->VisitPointers(values_, values_ + kArrayLength);
  }

 protected:
  inline Object** begin() { return values_; }
  explicit inline CustomArgumentsBase(Isolate* isolate)
      : Relocatable(isolate) {}
  Object* values_[kArrayLength];
};

template <typename T>
class CustomArguments : public CustomArgumentsBase<T::kArgsLength> {
 public:
  static const int kReturnValueOffset = T::kReturnValueIndex;

  typedef CustomArgumentsBase<T::kArgsLength> Super;
  ~CustomArguments() {
    this->begin()[kReturnValueOffset] =
        reinterpret_cast<Object*>(kHandleZapValue);
  }

 protected:
  explicit inline CustomArguments(Isolate* isolate) : Super(isolate) {}

  template <typename V>
  Handle<V> GetReturnValue(Isolate* isolate);

  inline Isolate* isolate() {
    return reinterpret_cast<Isolate*>(this->begin()[T::kIsolateIndex]);
  }
};

template <typename T>
template <typename V>
Handle<V> CustomArguments<T>::GetReturnValue(Isolate* isolate) {
  // Check the ReturnValue.
  Object** handle = &this->begin()[kReturnValueOffset];
  // Nothing was set, return empty handle as per previous behaviour.
  if ((*handle)->IsTheHole()) return Handle<V>();
  Handle<V> result = Handle<V>::cast(Handle<Object>(handle));
  result->VerifyApiCallResultType();
  return result;
}

class PropertyCallbackArguments
    : public CustomArguments<PropertyCallbackInfo<Value> > {
 public:
  typedef PropertyCallbackInfo<Value> T;
  typedef CustomArguments<T> Super;
  static const int kArgsLength = T::kArgsLength;
  static const int kThisIndex = T::kThisIndex;
  static const int kHolderIndex = T::kHolderIndex;
  static const int kDataIndex = T::kDataIndex;
  static const int kReturnValueDefaultValueIndex =
      T::kReturnValueDefaultValueIndex;
  static const int kIsolateIndex = T::kIsolateIndex;
  static const int kShouldThrowOnErrorIndex = T::kShouldThrowOnErrorIndex;

  PropertyCallbackArguments(Isolate* isolate, Object* data, Object* self,
                            JSObject* holder, Object::ShouldThrow should_throw)
      : Super(isolate) {
    Object** values = this->begin();
    values[T::kThisIndex] = self;
    values[T::kHolderIndex] = holder;
    values[T::kDataIndex] = data;
    values[T::kIsolateIndex] = reinterpret_cast<Object*>(isolate);
    values[T::kShouldThrowOnErrorIndex] =
        Smi::FromInt(should_throw == Object::THROW_ON_ERROR ? 1 : 0);

    // Here the hole is set as default value.
    // It cannot escape into js as it's remove in Call below.
    values[T::kReturnValueDefaultValueIndex] =
        isolate->heap()->the_hole_value();
    values[T::kReturnValueIndex] = isolate->heap()->the_hole_value();
    DCHECK(values[T::kHolderIndex]->IsHeapObject());
    DCHECK(values[T::kIsolateIndex]->IsSmi());
  }

/*
 * The following Call functions wrap the calling of all callbacks to handle
 * calling either the old or the new style callbacks depending on which one
 * has been registered.
 * For old callbacks which return an empty handle, the ReturnValue is checked
 * and used if it's been set to anything inside the callback.
 * New style callbacks always use the return value.
 */
  Handle<JSObject> Call(IndexedPropertyEnumeratorCallback f);

#define FOR_EACH_CALLBACK_TABLE_MAPPING_1_NAME(F)                  \
  F(AccessorNameGetterCallback, "get", v8::Value, Object)          \
  F(GenericNamedPropertyQueryCallback, "has", v8::Integer, Object) \
  F(GenericNamedPropertyDeleterCallback, "delete", v8::Boolean, Object)

#define WRITE_CALL_1_NAME(Function, type, ApiReturn, InternalReturn)         \
  Handle<InternalReturn> Call(Function f, Handle<Name> name) {               \
    Isolate* isolate = this->isolate();                                      \
    VMState<EXTERNAL> state(isolate);                                        \
    ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f));             \
    PropertyCallbackInfo<ApiReturn> info(begin());                           \
    LOG(isolate,                                                             \
        ApiNamedPropertyAccess("interceptor-named-" type, holder(), *name)); \
    f(v8::Utils::ToLocal(name), info);                                       \
    return GetReturnValue<InternalReturn>(isolate);                          \
  }

  FOR_EACH_CALLBACK_TABLE_MAPPING_1_NAME(WRITE_CALL_1_NAME)

#undef FOR_EACH_CALLBACK_TABLE_MAPPING_1_NAME
#undef WRITE_CALL_1_NAME

#define FOR_EACH_CALLBACK_TABLE_MAPPING_1_INDEX(F)            \
  F(IndexedPropertyGetterCallback, "get", v8::Value, Object)  \
  F(IndexedPropertyQueryCallback, "has", v8::Integer, Object) \
  F(IndexedPropertyDeleterCallback, "delete", v8::Boolean, Object)

#define WRITE_CALL_1_INDEX(Function, type, ApiReturn, InternalReturn)  \
  Handle<InternalReturn> Call(Function f, uint32_t index) {            \
    Isolate* isolate = this->isolate();                                \
    VMState<EXTERNAL> state(isolate);                                  \
    ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f));       \
    PropertyCallbackInfo<ApiReturn> info(begin());                     \
    LOG(isolate, ApiIndexedPropertyAccess("interceptor-indexed-" type, \
                                          holder(), index));           \
    f(index, info);                                                    \
    return GetReturnValue<InternalReturn>(isolate);                    \
  }

  FOR_EACH_CALLBACK_TABLE_MAPPING_1_INDEX(WRITE_CALL_1_INDEX)

#undef FOR_EACH_CALLBACK_TABLE_MAPPING_1_INDEX
#undef WRITE_CALL_1_INDEX

  Handle<Object> Call(GenericNamedPropertySetterCallback f, Handle<Name> name,
                      Handle<Object> value) {
    Isolate* isolate = this->isolate();
    VMState<EXTERNAL> state(isolate);
    ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f));
    PropertyCallbackInfo<v8::Value> info(begin());
    LOG(isolate,
        ApiNamedPropertyAccess("interceptor-named-set", holder(), *name));
    f(v8::Utils::ToLocal(name), v8::Utils::ToLocal(value), info);
    return GetReturnValue<Object>(isolate);
  }

  Handle<Object> Call(IndexedPropertySetterCallback f, uint32_t index,
                      Handle<Object> value) {
    Isolate* isolate = this->isolate();
    VMState<EXTERNAL> state(isolate);
    ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f));
    PropertyCallbackInfo<v8::Value> info(begin());
    LOG(isolate,
        ApiIndexedPropertyAccess("interceptor-indexed-set", holder(), index));
    f(index, v8::Utils::ToLocal(value), info);
    return GetReturnValue<Object>(isolate);
  }

  void Call(AccessorNameSetterCallback f, Handle<Name> name,
            Handle<Object> value) {
    Isolate* isolate = this->isolate();
    VMState<EXTERNAL> state(isolate);
    ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f));
    PropertyCallbackInfo<void> info(begin());
    LOG(isolate,
        ApiNamedPropertyAccess("interceptor-named-set", holder(), *name));
    f(v8::Utils::ToLocal(name), v8::Utils::ToLocal(value), info);
  }

 private:
  inline JSObject* holder() {
    return JSObject::cast(this->begin()[T::kHolderIndex]);
  }
};

class FunctionCallbackArguments
    : public CustomArguments<FunctionCallbackInfo<Value> > {
 public:
  typedef FunctionCallbackInfo<Value> T;
  typedef CustomArguments<T> Super;
  static const int kArgsLength = T::kArgsLength;
  static const int kHolderIndex = T::kHolderIndex;
  static const int kDataIndex = T::kDataIndex;
  static const int kReturnValueDefaultValueIndex =
      T::kReturnValueDefaultValueIndex;
  static const int kIsolateIndex = T::kIsolateIndex;
  static const int kCalleeIndex = T::kCalleeIndex;
  static const int kContextSaveIndex = T::kContextSaveIndex;

  FunctionCallbackArguments(internal::Isolate* isolate, internal::Object* data,
                            internal::HeapObject* callee,
                            internal::Object* holder, internal::Object** argv,
                            int argc, bool is_construct_call)
      : Super(isolate),
        argv_(argv),
        argc_(argc),
        is_construct_call_(is_construct_call) {
    Object** values = begin();
    values[T::kDataIndex] = data;
    values[T::kCalleeIndex] = callee;
    values[T::kHolderIndex] = holder;
    values[T::kContextSaveIndex] = isolate->heap()->the_hole_value();
    values[T::kIsolateIndex] = reinterpret_cast<internal::Object*>(isolate);
    // Here the hole is set as default value.
    // It cannot escape into js as it's remove in Call below.
    values[T::kReturnValueDefaultValueIndex] =
        isolate->heap()->the_hole_value();
    values[T::kReturnValueIndex] = isolate->heap()->the_hole_value();
    DCHECK(values[T::kCalleeIndex]->IsJSFunction() ||
           values[T::kCalleeIndex]->IsFunctionTemplateInfo());
    DCHECK(values[T::kHolderIndex]->IsHeapObject());
    DCHECK(values[T::kIsolateIndex]->IsSmi());
  }

  /*
   * The following Call function wraps the calling of all callbacks to handle
   * calling either the old or the new style callbacks depending on which one
   * has been registered.
   * For old callbacks which return an empty handle, the ReturnValue is checked
   * and used if it's been set to anything inside the callback.
   * New style callbacks always use the return value.
   */
  Handle<Object> Call(FunctionCallback f);

 private:
  internal::Object** argv_;
  int argc_;
  bool is_construct_call_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_API_ARGUMENTS_H_

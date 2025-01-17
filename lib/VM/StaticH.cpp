/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "hermes/VM/Callable.h"
#include "hermes/VM/FastArray.h"
#include "hermes/VM/Interpreter.h"
#include "hermes/VM/JSArray.h"
#include "hermes/VM/JSObject.h"
#include "hermes/VM/JSRegExp.h"
#include "hermes/VM/PropertyAccessor.h"
#include "hermes/VM/SerializedLiteralParser.h"
#include "hermes/VM/StackFrame-inline.h"
#include "hermes/VM/StaticHUtils.h"

#include "JSLib/JSLibInternal.h"

using namespace hermes;
using namespace hermes::vm;

extern "C" void _SH_MODEL(void) {}

/// Convert the given \p cr to a \c CallResult<HermesValue>.
template <typename T>
static inline CallResult<HermesValue> toCallResultHermesValue(
    CallResult<Handle<T>> cr) {
  if (LLVM_UNLIKELY(cr.getStatus() == ExecutionStatus::EXCEPTION)) {
    return ExecutionStatus::EXCEPTION;
  }
  return cr.getValue().getHermesValue();
}

extern "C" SHLegacyValue *
_sh_push_locals(SHRuntime *shr, SHLocals *locals, uint32_t stackSize) {
  // TODO: [SH]: check for native stack overflow.
  Runtime &runtime = getRuntime(shr);

  PinnedHermesValue *savedSP = runtime.getStackPointer();

  // Allocate the registers for the new frame, but only if stack != 0.
  if (stackSize &&
      LLVM_UNLIKELY(!runtime.checkAndAllocStack(
          stackSize, HermesValue::encodeUndefinedValue()))) {
    (void)runtime.raiseStackOverflow(
        Runtime::StackOverflowKind::JSRegisterStack);
    _sh_throw_current(shr);
  }

  locals->prev = runtime.shLocals;
  runtime.shLocals = locals;
  return savedSP;
}

extern "C" SHLegacyValue *
_sh_enter(SHRuntime *shr, SHLocals *locals, uint32_t stackSize) {
  // TODO: [SH]: check for native stack overflow.
  Runtime &runtime = getRuntime(shr);

  PinnedHermesValue *frame = runtime.getStackPointer();
  runtime.setCurrentFrame(StackFramePtr(frame));

  // Allocate the registers for the new frame.
  if (LLVM_UNLIKELY(!runtime.checkAndAllocStack(
          stackSize, HermesValue::encodeUndefinedValue()))) {
    (void)runtime.raiseStackOverflow(
        Runtime::StackOverflowKind::JSRegisterStack);
    _sh_throw_current(shr);
  }

  locals->prev = runtime.shLocals;
  runtime.shLocals = locals;
  return frame;
}

extern "C" void _sh_check_native_stack_overflow(SHRuntime *shr) {
#ifdef HERMES_CHECK_NATIVE_STACK
  Runtime &runtime = getRuntime(shr);
  bool overflowing = runtime.isNativeStackOverflowing();
  if (LLVM_UNLIKELY(overflowing)) {
    (void)runtime.raiseStackOverflow(Runtime::StackOverflowKind::NativeStack);
    _sh_throw_current(shr);
  }
#endif
}

extern "C" void
_sh_pop_locals(SHRuntime *shr, SHLocals *locals, SHLegacyValue *savedSP) {
  Runtime &runtime = getRuntime(shr);
  assert(runtime.shLocals == locals && "Only the current locals can be popped");
  runtime.shLocals = locals->prev;
  runtime.popToSavedStackPointer(toPHV(savedSP));
}

extern "C" void
_sh_leave(SHRuntime *shr, SHLocals *locals, SHLegacyValue *frame) {
  Runtime &runtime = getRuntime(shr);
  assert(runtime.shLocals == locals && "Only the current locals can be popped");
  runtime.shLocals = locals->prev;
  (void)runtime.restoreStackAndPreviousFrame(StackFramePtr(toPHV(frame)));
}

extern "C" SHLegacyValue _sh_ljs_param(SHLegacyValue *frame, uint32_t index) {
  assert(index <= INT32_MAX && "param index should be 31 bits");
  auto framePtr = StackFramePtr(toPHV(frame));
  if (LLVM_LIKELY(index <= framePtr.getArgCount())) {
    // getArgRef() expects `this` to be -1.
    return framePtr.getArgRef((int32_t)index - 1);
  } else {
    return _sh_ljs_undefined();
  }
}

extern "C" SHLegacyValue _sh_ljs_create_this(
    SHRuntime *shr,
    SHLegacyValue *prototype,
    SHLegacyValue *callable) {
  Handle<> callableHandle{toPHV(callable)};
  Runtime &runtime = getRuntime(shr);
  if (LLVM_UNLIKELY(!vmisa<Callable>(*callableHandle))) {
    (void)runtime.raiseTypeError("constructor is not callable");
    _sh_throw_current(shr);
  }
  CallResult<PseudoHandle<JSObject>> res{ExecutionStatus::EXCEPTION};
  {
    GCScopeMarkerRAII marker{runtime};
    res = Callable::newObject(
        Handle<Callable>::vmcast(callableHandle),
        runtime,
        Handle<JSObject>::vmcast(
            toPHV(prototype)->isObject() ? toPHV(prototype)
                                         : &runtime.objectPrototype));
  }
  if (LLVM_UNLIKELY(res == ExecutionStatus::EXCEPTION))
    _sh_throw_current(shr);
  return res->getHermesValue();
}

extern "C" SHLegacyValue _sh_ljs_coerce_this_ns(
    SHRuntime *shr,
    SHLegacyValue value) {
  if (LLVM_LIKELY(_sh_ljs_is_object(value))) {
    return value;
  } else if (_sh_ljs_is_null(value) || _sh_ljs_is_undefined(value)) {
    return getRuntime(shr).global_;
  } else {
    CallResult<HermesValue> res{HermesValue::encodeUndefinedValue()};
    {
      Runtime &runtime = getRuntime(shr);
      GCScopeMarkerRAII marker{runtime};
      res = toObject(
          runtime, runtime.makeHandle(HermesValue::fromRaw(value.raw)));
    }
    if (LLVM_UNLIKELY(res == ExecutionStatus::EXCEPTION))
      _sh_throw_current(shr);
    return *res;
  }
}

static SHLegacyValue getArgumentsPropByVal_RJS(
    SHRuntime *shr,
    SHLegacyValue *frame,
    SHLegacyValue *idx,
    SHLegacyValue *lazyReg,
    bool strictMode) {
  Runtime &runtime = getRuntime(shr);
  StackFramePtr framePtr(toPHV(frame));
  // If the arguments object hasn't been created yet and we have a
  // valid integer index, we use the fast path.
  if (toPHV(lazyReg)->isUndefined()) {
    if (auto index = toArrayIndexFastPath(*toPHV(idx))) {
      // Is this an existing argument?
      if (*index < framePtr.getArgCount())
        return framePtr.getArgRef(*index);
    }
  }
  // Slow path.
  CallResult<PseudoHandle<>> res{ExecutionStatus::EXCEPTION};
  {
    GCScopeMarkerRAII marker{runtime};
    res = Interpreter::getArgumentsPropByValSlowPath_RJS(
        runtime,
        toPHV(lazyReg),
        toPHV(idx),
        framePtr.getCalleeClosureHandleUnsafe(),
        strictMode);
  }
  if (res == ExecutionStatus::EXCEPTION)
    _sh_throw_current(shr);
  return res->getHermesValue();
}

extern "C" SHLegacyValue _sh_ljs_get_arguments_prop_by_val_loose(
    SHRuntime *shr,
    SHLegacyValue *frame,
    SHLegacyValue *idx,
    SHLegacyValue *lazyReg) {
  return getArgumentsPropByVal_RJS(shr, frame, idx, lazyReg, false);
}
extern "C" SHLegacyValue _sh_ljs_get_arguments_prop_by_val_strict(
    SHRuntime *shr,
    SHLegacyValue *frame,
    SHLegacyValue *idx,
    SHLegacyValue *lazyReg) {
  return getArgumentsPropByVal_RJS(shr, frame, idx, lazyReg, true);
}
extern "C" SHLegacyValue _sh_ljs_get_arguments_length(
    SHRuntime *shr,
    SHLegacyValue *frame,
    SHLegacyValue *lazyReg) {
  Runtime &runtime = getRuntime(shr);
  StackFramePtr framePtr(toPHV(frame));
  // If the arguments object hasn't been created yet, use the fast path.
  if (toPHV(lazyReg)->isUndefined())
    return HermesValue::encodeTrustedNumberValue(framePtr.getArgCount());

  CallResult<PseudoHandle<>> res{ExecutionStatus::EXCEPTION};
  {
    GCScopeMarkerRAII marker{runtime};
    // The arguments object has been created, so this is a regular property
    // get.
    auto obj = Handle<JSObject>::vmcast(toPHV(lazyReg));
    res = JSObject::getNamed_RJS(
        obj, runtime, Predefined::getSymbolID(Predefined::length));
  }
  if (res == ExecutionStatus::EXCEPTION)
    _sh_throw_current(shr);
  return res->getHermesValue();
}

static void reifyArguments(
    SHRuntime *shr,
    SHLegacyValue *frame,
    SHLegacyValue *lazyReg,
    bool strictMode) {
  Runtime &runtime = getRuntime(shr);
  StackFramePtr framePtr(toPHV(frame));
  // If the arguments object was already created, do nothing.
  if (!toPHV(lazyReg)->isUndefined()) {
    assert(
        toPHV(lazyReg)->isObject() &&
        "arguments lazy register is not an object");
    return;
  }

  CallResult<HermesValue> res{ExecutionStatus::EXCEPTION};
  {
    GCScopeMarkerRAII marker{runtime};
    res = toCallResultHermesValue(Interpreter::reifyArgumentsSlowPath(
        runtime, framePtr.getCalleeClosureHandleUnsafe(), strictMode));
  }
  if (LLVM_UNLIKELY(res == ExecutionStatus::EXCEPTION))
    _sh_throw_current(shr);
  *lazyReg = *res;
}

extern "C" void _sh_ljs_reify_arguments_loose(
    SHRuntime *shr,
    SHLegacyValue *frame,
    SHLegacyValue *lazyReg) {
  reifyArguments(shr, frame, lazyReg, false);
}
extern "C" void _sh_ljs_reify_arguments_strict(
    SHRuntime *shr,
    SHLegacyValue *frame,
    SHLegacyValue *lazyReg) {
  reifyArguments(shr, frame, lazyReg, true);
}

extern "C" SHLegacyValue _sh_ljs_get_by_val_rjs(
    SHRuntime *shr,
    SHLegacyValue *source,
    SHLegacyValue *key) {
  Handle<> sourceHandle{toPHV(source)}, keyHandle{toPHV(key)};
  Runtime &runtime = getRuntime(shr);
  if (LLVM_LIKELY(sourceHandle->isObject())) {
    CallResult<PseudoHandle<>> res{ExecutionStatus::EXCEPTION};
    {
      GCScopeMarkerRAII marker{runtime};
      res = JSObject::getComputed_RJS(
          Handle<JSObject>::vmcast(sourceHandle), runtime, keyHandle);
    }
    if (LLVM_UNLIKELY(res == ExecutionStatus::EXCEPTION))
      _sh_throw_current(shr);
    return res->getHermesValue();
  }

  // This is the "slow path".
  CallResult<PseudoHandle<>> res{ExecutionStatus::EXCEPTION};
  {
    GCScopeMarkerRAII marker{runtime};
    res = Interpreter::getByValTransient_RJS(runtime, sourceHandle, keyHandle);
  }
  if (LLVM_UNLIKELY(res == ExecutionStatus::EXCEPTION))
    _sh_throw_current(shr);
  return res->getHermesValue();
}

extern "C" SHLegacyValue _sh_catch(
    SHRuntime *shr,
    SHLocals *locals,
    SHLegacyValue *frame,
    uint32_t stackSize) {
  Runtime &runtime = getRuntime(shr);
  runtime.shCurJmpBuf = runtime.shCurJmpBuf->prev;

  runtime.shLocals = locals;
  runtime.popToSavedStackPointer(toPHV(frame + stackSize));
  runtime.setCurrentFrame(StackFramePtr(toPHV(frame)));

  SHLegacyValue res = runtime.getThrownValue();
  runtime.clearThrownValue();
  return res;
}

extern "C" void _sh_throw_current(SHRuntime *shr) {
  Runtime &runtime = getRuntime(shr);
  assert(runtime.shCurJmpBuf && "No SH exception handler installed");
  if (!runtime.shCurJmpBuf) {
    fprintf(stderr, "SH: uncaught exception");
    abort();
  }
  _longjmp(runtime.shCurJmpBuf->buf, 1);
}

extern "C" void _sh_throw(SHRuntime *shr, SHLegacyValue value) {
  Runtime &runtime = getRuntime(shr);
  runtime.setThrownValue(HermesValue::fromRaw(value.raw));
  _sh_throw_current(shr);
}

extern "C" void _sh_throw_type_error(SHRuntime *shr, SHLegacyValue *message) {
  (void)getRuntime(shr).raiseTypeError(Handle<>(toPHV(message)));
  _sh_throw_current(shr);
}

extern "C" void _sh_throw_type_error_ascii(
    SHRuntime *shr,
    const char *message) {
  (void)getRuntime(shr).raiseTypeError(TwineChar16(message));
  _sh_throw_current(shr);
}

extern "C" void _sh_throw_empty(SHRuntime *shr) {
  Runtime &runtime = getRuntime(shr);
  (void)runtime.raiseReferenceError("accessing an uninitialized variable");
  _sh_throw_current(shr);
}

static SHLegacyValue doCall(Runtime &runtime, PinnedHermesValue *callTarget) {
  if (vmisa<NativeJSFunction>(*callTarget)) {
    return NativeJSFunction::_legacyCall(
        getSHRuntime(runtime), vmcast<NativeJSFunction>(*callTarget));
  }

  // FIXME: check for register stack overflow.
  CallResult<PseudoHandle<>> res{ExecutionStatus::EXCEPTION};
  {
    GCScopeMarkerRAII marker{runtime};
    if (vmisa<NativeFunction>(*callTarget)) {
      auto *native = vmcast<NativeFunction>(*callTarget);
      res = NativeFunction::_nativeCall(native, runtime);
    } else if (vmisa<BoundFunction>(*callTarget)) {
      auto *bound = vmcast<BoundFunction>(*callTarget);
      res = BoundFunction::_boundCall(bound, runtime.getCurrentIP(), runtime);
    } else if (vmisa<Callable>(*callTarget)) {
      auto callable = Handle<Callable>::vmcast(callTarget);
      res = callable->call(callable, runtime);
    } else {
      res = runtime.raiseTypeErrorForValue(
          Handle<>(callTarget), " is not a function");
    }
  }

  if (LLVM_UNLIKELY(res == ExecutionStatus::EXCEPTION)) {
    _sh_throw_current(getSHRuntime(runtime));
  }

  return res->getHermesValue();
}

extern "C" SHLegacyValue
_sh_ljs_call(SHRuntime *shr, SHLegacyValue *frame, uint32_t argCount) {
  Runtime &runtime = getRuntime(shr);
  StackFramePtr newFrame(runtime.getStackPointer());
  newFrame.getPreviousFrameRef() = HermesValue::encodeNativePointer(frame);
  newFrame.getSavedIPRef() = HermesValue::encodeNativePointer(nullptr);
  newFrame.getSavedCodeBlockRef() = HermesValue::encodeNativePointer(nullptr);
  newFrame.getSHLocalsRef() = HermesValue::encodeNativePointer(nullptr);
  newFrame.getArgCountRef() = HermesValue::encodeNativeUInt32(argCount);
  return doCall(runtime, &newFrame.getCalleeClosureOrCBRef());
}

extern "C" SHLegacyValue _sh_ljs_call_builtin(
    SHRuntime *shr,
    SHLegacyValue *frame,
    uint32_t argCount,
    uint32_t builtinMethodID) {
  Runtime &runtime = getRuntime(shr);
  StackFramePtr newFrame(runtime.getStackPointer());
  newFrame.getPreviousFrameRef() = HermesValue::encodeNativePointer(frame);
  newFrame.getSavedIPRef() = HermesValue::encodeNativePointer(nullptr);
  newFrame.getSavedCodeBlockRef() = HermesValue::encodeNativePointer(nullptr);
  newFrame.getSHLocalsRef() = HermesValue::encodeNativePointer(nullptr);
  newFrame.getArgCountRef() = HermesValue::encodeNativeUInt32(argCount);
  newFrame.getNewTargetRef() = HermesValue::encodeUndefinedValue();
  newFrame.getCalleeClosureOrCBRef() = HermesValue::encodeObjectValue(
      runtime.getBuiltinCallable(builtinMethodID));
  return doCall(runtime, &newFrame.getCalleeClosureOrCBRef());
}

extern "C" SHLegacyValue _sh_ljs_get_builtin_closure(
    SHRuntime *shr,
    uint32_t builtinMethodID) {
  return HermesValue::encodeObjectValue(
      getRuntime(shr).getBuiltinCallable(builtinMethodID));
}

extern "C" void _sh_ljs_create_environment(
    SHRuntime *shr,
    SHLegacyValue *frame,
    SHLegacyValue *result,
    uint32_t size) {
  Runtime &runtime = getRuntime(shr);
  auto framePtr = StackFramePtr(toPHV(frame));

  auto *parentEnv = framePtr.getCalleeClosureUnsafe()->getEnvironment(runtime);
  // We are not allowed to store null object pointers in locals, so we convert
  // a null pointer to JS Null.
  result->raw = parentEnv
      ? HermesValue::encodeObjectValueUnsafe(parentEnv).getRaw()
      : HermesValue::encodeNullValue().getRaw();

  {
    GCScopeMarkerRAII marker{runtime};
    HermesValue res = Environment::create(
        runtime,
        _sh_ljs_is_null(*result) ? runtime.makeNullHandle<Environment>()
                                 : Handle<Environment>::vmcast(toPHV(result)),
        size);
    result->raw = res.getRaw();
  }
  // #ifdef HERMES_ENABLE_DEBUGGER
  //   framePtr.getDebugEnvironmentRef() = *res;
  // #endif
}

extern "C" SHLegacyValue
_sh_ljs_get_env(SHRuntime *shr, SHLegacyValue *frame, uint32_t level) {
  Runtime &runtime = getRuntime(shr);
  Environment *curEnv = StackFramePtr(toPHV(frame))
                            .getCalleeClosureUnsafe()
                            ->getEnvironment(runtime);
  while (level--) {
    assert(curEnv && "invalid environment relative level");
    curEnv = curEnv->getParentEnvironment(runtime);
  }

  return HermesValue::encodeObjectValue(curEnv);
}

extern "C" void _sh_ljs_store_to_env(
    SHRuntime *shr,
    SHLegacyValue env,
    SHLegacyValue val,
    uint32_t index) {
  vmcast<Environment>(HermesValue::fromRaw(env.raw))
      ->slot(index)
      .set(HermesValue::fromRaw(val.raw), getRuntime(shr).getHeap());
}

extern "C" void _sh_ljs_store_np_to_env(
    SHRuntime *shr,
    SHLegacyValue env,
    SHLegacyValue val,
    uint32_t index) {
  vmcast<Environment>(HermesValue::fromRaw(env.raw))
      ->slot(index)
      .setNonPtr(HermesValue::fromRaw(val.raw), getRuntime(shr).getHeap());
}

static SHLegacyValue createClosure(
    SHRuntime *shr,
    const SHLegacyValue *env,
    SHLegacyValue (*func)(SHRuntime *),
    const SHNativeFuncInfo *funcInfo) {
  Runtime &runtime = getRuntime(shr);
  GCScopeMarkerRAII marker{runtime};

  SHLegacyValue res =
      NativeJSFunction::create(
          runtime,
          Handle<JSObject>::vmcast(&runtime.functionPrototype),
          _sh_ljs_is_null(*env) ? runtime.makeNullHandle<Environment>()
                                : Handle<Environment>::vmcast(toPHV(env)),
          func,
          funcInfo,
          0)
          .getHermesValue();
  return res;
}

extern "C" SHLegacyValue _sh_ljs_create_closure(
    SHRuntime *shr,
    const SHLegacyValue *env,
    SHLegacyValue (*func)(SHRuntime *),
    const SHNativeFuncInfo *funcInfo) {
  return createClosure(shr, env, func, funcInfo);
}

extern "C" SHLegacyValue _sh_ljs_get_global_object(SHRuntime *shr) {
  return getRuntime(shr).global_;
}

extern "C" void _sh_ljs_declare_global_var(SHRuntime *shr, SHSymbolID name) {
  Runtime &runtime = getRuntime(shr);
  {
    GCScopeMarkerRAII mark{runtime};

    DefinePropertyFlags dpf = DefinePropertyFlags::getDefaultNewPropertyFlags();
    dpf.configurable = 0;
    // Do not overwrite existing globals with undefined.
    dpf.setValue = 0;

    auto res = JSObject::defineOwnProperty(
        runtime.getGlobal(),
        runtime,
        SymbolID::unsafeCreate(name),
        dpf,
        Runtime::getUndefinedValue(),
        PropOpFlags().plusThrowOnError());
    if (res != ExecutionStatus::EXCEPTION)
      return;
    assert(
        !runtime.getGlobal()->isProxyObject() &&
        "global can't be a proxy object");
    // If the property already exists, this should be a noop.
    // Instead of incurring the cost to check every time, do it
    // only if an exception is thrown, and swallow the exception
    // if it exists, since we didn't want to make the call,
    // anyway.  This most likely means the property is
    // non-configurable.
    NamedPropertyDescriptor desc;
    auto res1 = JSObject::getOwnNamedDescriptor(
        runtime.getGlobal(), runtime, SymbolID::unsafeCreate(name), desc);
    if (res1) {
      runtime.clearThrownValue();
      return;
    }
    // fall through for exception
  }
  _sh_throw_current(shr);
}

/// Calculate the default property access flags depending on the mode of the
/// \c CodeBlock.
#define DEFAULT_PROP_OP_FLAGS(strictMode) \
  (strictMode ? PropOpFlags().plusThrowOnError() : PropOpFlags())

template <bool tryProp, bool strictMode>
static inline void putById_RJS(
    Runtime &runtime,
    const PinnedHermesValue *target,
    SymbolID symID,
    const PinnedHermesValue *value,
    PropertyCacheEntry *cacheEntry) {
  //++NumPutById;
  if (LLVM_LIKELY(target->isObject())) {
    SmallHermesValue shv = SmallHermesValue::encodeHermesValue(*value, runtime);
    auto *obj = vmcast<JSObject>(*target);

#ifdef HERMESVM_PROFILER_BB
    {
      HERMES_SLOW_ASSERT(
          gcScope.getHandleCountDbg() == KEEP_HANDLES &&
          "unaccounted handles were created");
      auto shvHandle = runtime.makeHandle(shv.toHV(runtime));
      auto cacheHCPtr = vmcast_or_null<HiddenClass>(static_cast<GCCell *>(
          cacheEntry->clazz.get(runtime, runtime.getHeap())));
      CAPTURE_IP(runtime.recordHiddenClass(
          curCodeBlock, ip, ID(idVal), obj->getClass(runtime), cacheHCPtr));
      // shv/obj may be invalidated by recordHiddenClass
      if (shv.isPointer())
        shv.unsafeUpdatePointer(
            static_cast<GCCell *>(shvHandle->getPointer()), runtime);
      obj = vmcast<JSObject>(*target);
    }
    gcScope.flushToSmallCount(KEEP_HANDLES);
#endif
    CompressedPointer clazzPtr{obj->getClassGCPtr()};
    // If we have a cache hit, reuse the cached offset and immediately
    // return the property.
    if (LLVM_LIKELY(cacheEntry->clazz == clazzPtr)) {
      //++NumPutByIdCacheHits;
      JSObject::setNamedSlotValueUnsafe(obj, runtime, cacheEntry->slot, shv);
      return;
    }
    NamedPropertyDescriptor desc;
    OptValue<bool> hasOwnProp =
        JSObject::tryGetOwnNamedDescriptorFast(obj, runtime, symID, desc);
    if (LLVM_LIKELY(hasOwnProp.hasValue() && hasOwnProp.getValue()) &&
        !desc.flags.accessor && desc.flags.writable &&
        !desc.flags.internalSetter) {
      //++NumPutByIdFastPaths;

      // cacheIdx == 0 indicates no caching so don't update the cache in
      // those cases.
      HiddenClass *clazz = vmcast<HiddenClass>(clazzPtr.getNonNull(runtime));
      if (LLVM_LIKELY(!clazz->isDictionary())
          /* && LLVM_LIKELY(cacheIdx != hbc::PROPERTY_CACHING_DISABLED)*/) {
#ifdef HERMES_SLOW_DEBUG
        // if (cacheEntry->clazz && cacheEntry->clazz != clazzPtr)
        //   ++NumPutByIdCacheEvicts;
#else
        //(void)NumPutByIdCacheEvicts;
#endif
        // Cache the class and property slot.
        cacheEntry->clazz = clazzPtr;
        cacheEntry->slot = desc.slot;
      }

      // This must be valid because an own property was already found.
      JSObject::setNamedSlotValueUnsafe(obj, runtime, desc.slot, shv);
      return;
    }

    CallResult<bool> putRes{ExecutionStatus::EXCEPTION};
    {
      GCScopeMarkerRAII marker{runtime};
      const PropOpFlags defaultPropOpFlags = DEFAULT_PROP_OP_FLAGS(strictMode);
      putRes = JSObject::putNamed_RJS(
          Handle<JSObject>::vmcast(target),
          runtime,
          symID,
          Handle<>(value),
          !tryProp ? defaultPropOpFlags : defaultPropOpFlags.plusMustExist());
    }
    if (LLVM_UNLIKELY(putRes == ExecutionStatus::EXCEPTION))
      _sh_throw_current(getSHRuntime(runtime));
  } else {
    //++NumPutByIdTransient;
    assert(!tryProp && "TryPutById can only be used on the global object");
    ExecutionStatus retStatus;
    {
      GCScopeMarkerRAII marker{runtime};
      retStatus = Interpreter::putByIdTransient_RJS(
          runtime, Handle<>(target), symID, Handle<>(value), strictMode);
    }
    if (retStatus == ExecutionStatus::EXCEPTION)
      _sh_throw_current(getSHRuntime(runtime));
  }
}

extern "C" void _sh_ljs_put_by_id_loose_rjs(
    SHRuntime *shr,
    const SHLegacyValue *target,
    SHSymbolID symID,
    const SHLegacyValue *value,
    SHPropertyCacheEntry *propCacheEntry) {
  putById_RJS<false, false>(
      getRuntime(shr),
      toPHV(target),
      SymbolID::unsafeCreate(symID),
      toPHV(value),
      reinterpret_cast<PropertyCacheEntry *>(propCacheEntry));
}

extern "C" void _sh_ljs_put_by_id_strict_rjs(
    SHRuntime *shr,
    const SHLegacyValue *target,
    SHSymbolID symID,
    const SHLegacyValue *value,
    SHPropertyCacheEntry *propCacheEntry) {
  putById_RJS<false, true>(
      getRuntime(shr),
      toPHV(target),
      SymbolID::unsafeCreate(symID),
      toPHV(value),
      reinterpret_cast<PropertyCacheEntry *>(propCacheEntry));
}

extern "C" void _sh_ljs_try_put_by_id_loose_rjs(
    SHRuntime *shr,
    SHLegacyValue *target,
    SHSymbolID symID,
    SHLegacyValue *value,
    SHPropertyCacheEntry *propCacheEntry) {
  putById_RJS<true, true>(
      getRuntime(shr),
      toPHV(target),
      SymbolID::unsafeCreate(symID),
      toPHV(value),
      reinterpret_cast<PropertyCacheEntry *>(propCacheEntry));
}

extern "C" void _sh_ljs_try_put_by_id_strict_rjs(
    SHRuntime *shr,
    SHLegacyValue *target,
    SHSymbolID symID,
    SHLegacyValue *value,
    SHPropertyCacheEntry *propCacheEntry) {
  putById_RJS<true, true>(
      getRuntime(shr),
      toPHV(target),
      SymbolID::unsafeCreate(symID),
      toPHV(value),
      reinterpret_cast<PropertyCacheEntry *>(propCacheEntry));
}

static inline void putByVal_RJS(
    SHRuntime *shr,
    SHLegacyValue *target,
    SHLegacyValue *key,
    SHLegacyValue *value,
    bool strictMode) {
  Handle<> targetHandle{toPHV(target)}, keyHandle{toPHV(key)},
      valueHandle{toPHV(value)};
  Runtime &runtime = getRuntime(shr);
  if (LLVM_LIKELY(targetHandle->isObject())) {
    const PropOpFlags defaultPropOpFlags = DEFAULT_PROP_OP_FLAGS(strictMode);
    CallResult<bool> res{false};
    {
      GCScopeMarkerRAII marker{runtime};
      res = JSObject::putComputed_RJS(
          Handle<JSObject>::vmcast(targetHandle),
          runtime,
          keyHandle,
          valueHandle,
          defaultPropOpFlags);
    }
    if (LLVM_UNLIKELY(res == ExecutionStatus::EXCEPTION))
      _sh_throw_current(shr);
    return;
  }

  // This is the "slow path".
  ExecutionStatus res;
  {
    GCScopeMarkerRAII marker{runtime};
    res = Interpreter::putByValTransient_RJS(
        runtime, targetHandle, keyHandle, valueHandle, strictMode);
  }
  if (LLVM_UNLIKELY(res == ExecutionStatus::EXCEPTION))
    _sh_throw_current(shr);
}

extern "C" void _sh_ljs_put_by_val_loose_rjs(
    SHRuntime *shr,
    SHLegacyValue *target,
    SHLegacyValue *key,
    SHLegacyValue *value) {
  putByVal_RJS(shr, target, key, value, false);
}
extern "C" void _sh_ljs_put_by_val_strict_rjs(
    SHRuntime *shr,
    SHLegacyValue *target,
    SHLegacyValue *key,
    SHLegacyValue *value) {
  putByVal_RJS(shr, target, key, value, true);
}

template <bool tryProp>
static inline HermesValue getById_RJS(
    Runtime &runtime,
    const PinnedHermesValue *source,
    SymbolID symID,
    PropertyCacheEntry *cacheEntry) {
  //++NumGetById;
  // NOTE: it is safe to use OnREG(GetById) here because all instructions
  // have the same layout: opcode, registers, non-register operands, i.e.
  // they only differ in the width of the last "identifier" field.
  if (LLVM_LIKELY(source->isObject())) {
    auto *obj = vmcast<JSObject>(*source);

#ifdef HERMESVM_PROFILER_BB
    {
      HERMES_SLOW_ASSERT(
          gcScope.getHandleCountDbg() == KEEP_HANDLES &&
          "unaccounted handles were created");
      auto objHandle = runtime.makeHandle(obj);
      auto cacheHCPtr = vmcast_or_null<HiddenClass>(static_cast<GCCell *>(
          cacheEntry->clazz.get(runtime, runtime.getHeap())));
      CAPTURE_IP(runtime.recordHiddenClass(
          curCodeBlock, ip, ID(idVal), obj->getClass(runtime), cacheHCPtr));
      // obj may be moved by GC due to recordHiddenClass
      *obj = vmcast<JSObject>(*source);
    }
    gcScope.flushToSmallCount(KEEP_HANDLES);
#endif
    CompressedPointer clazzPtr{obj->getClassGCPtr()};
#ifndef NDEBUG
    // if (vmcast<HiddenClass>(clazzPtr.getNonNull(runtime))->isDictionary())
    //   ++NumGetByIdDict;
#else
    //(void)NumGetByIdDict;
#endif

    // If we have a cache hit, reuse the cached offset and immediately
    // return the property.
    if (LLVM_LIKELY(cacheEntry->clazz == clazzPtr)) {
      //++NumGetByIdCacheHits;
      return JSObject::getNamedSlotValueUnsafe(obj, runtime, cacheEntry->slot)
          .unboxToHV(runtime);
    }
    NamedPropertyDescriptor desc;
    OptValue<bool> fastPathResult =
        JSObject::tryGetOwnNamedDescriptorFast(obj, runtime, symID, desc);
    if (LLVM_LIKELY(fastPathResult.hasValue() && fastPathResult.getValue()) &&
        !desc.flags.accessor) {
      //++NumGetByIdFastPaths;

      // cacheIdx == 0 indicates no caching so don't update the cache in
      // those cases.
      HiddenClass *clazz = vmcast<HiddenClass>(clazzPtr.getNonNull(runtime));
      if (LLVM_LIKELY(!clazz->isDictionaryNoCache())
          /*&& LLVM_LIKELY(cacheIdx != hbc::PROPERTY_CACHING_DISABLED)*/) {
#ifdef HERMES_SLOW_DEBUG
        // if (cacheEntry->clazz && cacheEntry->clazz != clazzPtr)
        //   ++NumGetByIdCacheEvicts;
#else
        //(void)NumGetByIdCacheEvicts;
#endif
        // Cache the class, id and property slot.
        cacheEntry->clazz = clazzPtr;
        cacheEntry->slot = desc.slot;
      }

      assert(
          !obj->isProxyObject() &&
          "tryGetOwnNamedDescriptorFast returned true on Proxy");
      return JSObject::getNamedSlotValueUnsafe(obj, runtime, desc)
          .unboxToHV(runtime);
    }

    // The cache may also be populated via the prototype of the object.
    // This value is only reliable if the fast path was a definite
    // not-found.
    if (fastPathResult.hasValue() && !fastPathResult.getValue() &&
        LLVM_LIKELY(!obj->isProxyObject())) {
      JSObject *parent = obj->getParent(runtime);
      // TODO: This isLazy check is because a lazy object is reported as
      // having no properties and therefore cannot contain the property.
      // This check does not belong here, it should be merged into
      // tryGetOwnNamedDescriptorFast().
      if (parent && cacheEntry->clazz == parent->getClassGCPtr() &&
          LLVM_LIKELY(!obj->isLazy())) {
        //++NumGetByIdProtoHits;
        // We've already checked that this isn't a Proxy.
        return JSObject::getNamedSlotValueUnsafe(
                   parent, runtime, cacheEntry->slot)
            .unboxToHV(runtime);
      }
    }

#ifdef HERMES_SLOW_DEBUG
    // Call to getNamedDescriptorUnsafe is safe because `id` is kept alive
    // by the IdentifierTable.
    // JSObject *propObj = JSObject::getNamedDescriptorUnsafe(
    //    Handle<JSObject>::vmcast(&O2REG(GetById)), runtime, id, desc);
    // if (propObj) {
    //  if (desc.flags.accessor)
    //    ++NumGetByIdAccessor;
    //  else if (propObj != vmcast<JSObject>(O2REG(GetById)))
    //    ++NumGetByIdProto;
    //} else {
    //  ++NumGetByIdNotFound;
    //}
#else
    //(void)NumGetByIdAccessor;
    //(void)NumGetByIdProto;
    //(void)NumGetByIdNotFound;
#endif
#ifdef HERMES_SLOW_DEBUG
    // auto *savedClass = true /*cacheIdx != hbc::PROPERTY_CACHING_DISABLED*/
    //     ? cacheEntry->clazz.get(runtime, runtime.getHeap())
    //     : nullptr;
#endif
    //++NumGetByIdSlow;
    CallResult<PseudoHandle<>> resPH{ExecutionStatus::EXCEPTION};
    {
      GCScopeMarkerRAII marker(runtime);
      const PropOpFlags defaultPropOpFlags = DEFAULT_PROP_OP_FLAGS(false);
      resPH = JSObject::getNamed_RJS(
          Handle<JSObject>::vmcast(source),
          runtime,
          symID,
          !tryProp ? defaultPropOpFlags : defaultPropOpFlags.plusMustExist(),
          true /*cacheIdx != hbc::PROPERTY_CACHING_DISABLED*/ ? cacheEntry
                                                              : nullptr);
    }
    if (LLVM_UNLIKELY(resPH == ExecutionStatus::EXCEPTION))
      _sh_throw_current(getSHRuntime(runtime));
#ifdef HERMES_SLOW_DEBUG
      // if (cacheIdx != hbc::PROPERTY_CACHING_DISABLED && savedClass &&
      //     cacheEntry->clazz.get(runtime, runtime.getHeap()) != savedClass) {
      //   ++NumGetByIdCacheEvicts;
      // }
#endif
    return resPH->get();
  } else {
    //++NumGetByIdTransient;
    assert(!tryProp && "TryGetById can only be used on the global object");
    /* Slow path. */
    CallResult<PseudoHandle<>> resPH{ExecutionStatus::EXCEPTION};
    {
      GCScopeMarkerRAII marker{runtime};
      resPH =
          Interpreter::getByIdTransient_RJS(runtime, Handle<>(source), symID);
    }
    if (LLVM_UNLIKELY(resPH == ExecutionStatus::EXCEPTION))
      _sh_throw_current(getSHRuntime(runtime));
    return resPH->get();
  }
}

extern "C" SHLegacyValue _sh_ljs_try_get_by_id_rjs(
    SHRuntime *shr,
    const SHLegacyValue *source,
    SHSymbolID symID,
    SHPropertyCacheEntry *propCacheEntry) {
  return getById_RJS<true>(
      getRuntime(shr),
      toPHV(source),
      SymbolID::unsafeCreate(symID),
      reinterpret_cast<PropertyCacheEntry *>(propCacheEntry));
}
extern "C" SHLegacyValue _sh_ljs_get_by_id_rjs(
    SHRuntime *shr,
    const SHLegacyValue *source,
    SHSymbolID symID,
    SHPropertyCacheEntry *propCacheEntry) {
  return getById_RJS<false>(
      getRuntime(shr),
      toPHV(source),
      SymbolID::unsafeCreate(symID),
      reinterpret_cast<PropertyCacheEntry *>(propCacheEntry));
}

extern "C" void _sh_ljs_put_own_by_val(
    SHRuntime *shr,
    SHLegacyValue *target,
    SHLegacyValue *key,
    SHLegacyValue *value) {
  Runtime &runtime = getRuntime(shr);
  CallResult<bool> cr{ExecutionStatus::EXCEPTION};
  {
    GCScopeMarkerRAII marker{runtime};
    cr = JSObject::defineOwnComputed(
        Handle<JSObject>::vmcast(toPHV(target)),
        runtime,
        Handle<>(toPHV(key)),
        DefinePropertyFlags::getDefaultNewPropertyFlags(),
        Handle<>(toPHV(value)));
  }
  if (LLVM_UNLIKELY(cr == ExecutionStatus::EXCEPTION))
    _sh_throw_current(shr);
}
extern "C" void _sh_ljs_put_own_ne_by_val(
    SHRuntime *shr,
    SHLegacyValue *target,
    SHLegacyValue *key,
    SHLegacyValue *value) {
  Runtime &runtime = getRuntime(shr);
  CallResult<bool> cr{ExecutionStatus::EXCEPTION};
  {
    GCScopeMarkerRAII marker{runtime};
    cr = JSObject::defineOwnComputed(
        Handle<JSObject>::vmcast(toPHV(target)),
        runtime,
        Handle<>(toPHV(key)),
        DefinePropertyFlags::getNewNonEnumerableFlags(),
        Handle<>(toPHV(value)));
  }
  if (LLVM_UNLIKELY(cr == ExecutionStatus::EXCEPTION))
    _sh_throw_current(shr);
}

extern "C" void _sh_ljs_put_own_by_index(
    SHRuntime *shr,
    SHLegacyValue *target,
    uint32_t key,
    SHLegacyValue *value) {
  Runtime &runtime = getRuntime(shr);
  CallResult<bool> cr{ExecutionStatus::EXCEPTION};
  {
    GCScopeMarkerRAII marker{runtime};
    Handle<> indexHandle{runtime, HermesValue::encodeTrustedNumberValue(key)};
    cr = JSObject::defineOwnComputedPrimitive(
        Handle<JSObject>::vmcast(toPHV(target)),
        runtime,
        indexHandle,
        DefinePropertyFlags::getDefaultNewPropertyFlags(),
        Handle<>(toPHV(value)));
  }
  if (LLVM_UNLIKELY(cr == ExecutionStatus::EXCEPTION))
    _sh_throw_current(shr);
}

/// Put an enumerable property.
extern "C" void _sh_ljs_put_new_own_by_id(
    SHRuntime *shr,
    SHLegacyValue *target,
    uint32_t key,
    SHLegacyValue *value) {
  Runtime &runtime = getRuntime(shr);
  ExecutionStatus cr{ExecutionStatus::EXCEPTION};
  {
    GCScopeMarkerRAII marker{runtime};
    cr = JSObject::defineNewOwnProperty(
        Handle<JSObject>::vmcast(toPHV(target)),
        runtime,
        SymbolID::unsafeCreate(key),
        PropertyFlags::defaultNewNamedPropertyFlags(),
        Handle<>(toPHV(value)));
  }
  if (LLVM_UNLIKELY(cr == ExecutionStatus::EXCEPTION))
    _sh_throw_current(shr);
}

/// Put a non-enumerable property.
extern "C" void _sh_ljs_put_new_own_ne_by_id(
    SHRuntime *shr,
    SHLegacyValue *target,
    uint32_t key,
    SHLegacyValue *value) {
  Runtime &runtime = getRuntime(shr);
  ExecutionStatus cr{ExecutionStatus::EXCEPTION};
  {
    GCScopeMarkerRAII marker{runtime};
    cr = JSObject::defineNewOwnProperty(
        Handle<JSObject>::vmcast(toPHV(target)),
        runtime,
        SymbolID::unsafeCreate(key),
        PropertyFlags::nonEnumerablePropertyFlags(),
        Handle<>(toPHV(value)));
  }
  if (LLVM_UNLIKELY(cr == ExecutionStatus::EXCEPTION))
    _sh_throw_current(shr);
}

/// Put a non-enumerable property.
extern "C" void _sh_ljs_put_own_getter_setter_by_val(
    SHRuntime *shr,
    SHLegacyValue *target,
    SHLegacyValue *key,
    SHLegacyValue *getter,
    SHLegacyValue *setter,
    bool isEnumerable) {
  Runtime &runtime{getRuntime(shr)};

  ExecutionStatus cr = [&runtime, target, key, getter, setter, isEnumerable]() {
    GCScopeMarkerRAII marker{runtime};

    DefinePropertyFlags dpFlags{};
    dpFlags.setConfigurable = 1;
    dpFlags.configurable = 1;
    dpFlags.setEnumerable = 1;
    dpFlags.enumerable = isEnumerable;

    Handle<Callable> getterCallable = runtime.makeNullHandle<Callable>();
    Handle<Callable> setterCallable = runtime.makeNullHandle<Callable>();
    if (LLVM_LIKELY(!toPHV(getter)->isUndefined())) {
      dpFlags.setGetter = 1;
      getterCallable = Handle<Callable>::vmcast(toPHV(getter));
    }
    if (LLVM_LIKELY(!toPHV(setter)->isUndefined())) {
      dpFlags.setSetter = 1;
      setterCallable = Handle<Callable>::vmcast(toPHV(setter));
    }
    assert(
        (dpFlags.setSetter || dpFlags.setGetter) &&
        "No accessor set in PutOwnGetterSetterByVal");

    auto res =
        PropertyAccessor::create(runtime, getterCallable, setterCallable);
    auto accessor = runtime.makeHandle<PropertyAccessor>(res);

    return JSObject::defineOwnComputed(
               Handle<JSObject>::vmcast(toPHV(target)),
               runtime,
               Handle<>(toPHV(key)),
               dpFlags,
               accessor)
        .getStatus();
  }();

  if (LLVM_UNLIKELY(cr == ExecutionStatus::EXCEPTION))
    _sh_throw_current(shr);
}

static HermesValue delById(
    Runtime &runtime,
    PinnedHermesValue *target,
    SymbolID key,
    bool strictMode) {
  const PropOpFlags defaultPropOpFlags = DEFAULT_PROP_OP_FLAGS(strictMode);
  CallResult<bool> status{ExecutionStatus::EXCEPTION};
  if (LLVM_LIKELY(target->isObject())) {
    {
      GCScopeMarkerRAII marker{runtime};
      status = JSObject::deleteNamed(
          Handle<JSObject>::vmcast(toPHV(target)),
          runtime,
          key,
          defaultPropOpFlags);
    }
    if (LLVM_UNLIKELY(status == ExecutionStatus::EXCEPTION)) {
      _sh_throw_current(getSHRuntime(runtime));
    }
    return HermesValue::encodeBoolValue(status.getValue());
  } else {
    // This is the "slow path".
    CallResult<HermesValue> res{ExecutionStatus::EXCEPTION};
    {
      GCScopeMarkerRAII marker{runtime};
      res = toObject(runtime, Handle<>(target));
    }
    if (LLVM_UNLIKELY(res == ExecutionStatus::EXCEPTION)) {
      // If an exception is thrown, likely we are trying to convert
      // undefined/null to an object. Passing over the name of the property
      // so that we could emit more meaningful error messages.
      // CAPTURE_IP(amendPropAccessErrorMsgWithPropName(
      //     runtime, Handle<>(&O2REG(DelById)), "delete", ID(idVal)));
      _sh_throw_current(getSHRuntime(runtime));
    }
    {
      GCScopeMarkerRAII marker{runtime};
      Handle tmpHandle{runtime, res.getValue()};
      status = JSObject::deleteNamed(
          Handle<JSObject>::vmcast(tmpHandle),
          runtime,
          key,
          defaultPropOpFlags);
    }
    if (LLVM_UNLIKELY(status == ExecutionStatus::EXCEPTION)) {
      _sh_throw_current(getSHRuntime(runtime));
    }
    return HermesValue::encodeBoolValue(status.getValue());
  }
}

extern "C" SHLegacyValue _sh_ljs_del_by_id_strict(
    SHRuntime *shr,
    SHLegacyValue *target,
    SHSymbolID key) {
  return delById(
      getRuntime(shr), toPHV(target), SymbolID::unsafeCreate(key), true);
}
extern "C" SHLegacyValue
_sh_ljs_del_by_id_loose(SHRuntime *shr, SHLegacyValue *target, SHSymbolID key) {
  return delById(
      getRuntime(shr), toPHV(target), SymbolID::unsafeCreate(key), false);
}

static HermesValue delByVal(
    Runtime &runtime,
    PinnedHermesValue *target,
    PinnedHermesValue *key,
    bool strictMode) {
  const PropOpFlags defaultPropOpFlags = DEFAULT_PROP_OP_FLAGS(strictMode);
  CallResult<bool> status{ExecutionStatus::EXCEPTION};
  if (LLVM_LIKELY(target->isObject())) {
    {
      GCScopeMarkerRAII marker{runtime};
      status = JSObject::deleteComputed(
          Handle<JSObject>::vmcast(toPHV(target)),
          runtime,
          Handle<>(toPHV(key)),
          defaultPropOpFlags);
    }
    if (LLVM_UNLIKELY(status == ExecutionStatus::EXCEPTION)) {
      _sh_throw_current(getSHRuntime(runtime));
    }
    return HermesValue::encodeBoolValue(status.getValue());
  } else {
    // This is the "slow path".
    CallResult<HermesValue> res{ExecutionStatus::EXCEPTION};
    {
      GCScopeMarkerRAII marker{runtime};
      res = toObject(runtime, Handle<>(target));
    }
    if (LLVM_UNLIKELY(res == ExecutionStatus::EXCEPTION)) {
      // If an exception is thrown, likely we are trying to convert
      // undefined/null to an object. Passing over the name of the property
      // so that we could emit more meaningful error messages.
      // CAPTURE_IP(amendPropAccessErrorMsgWithPropName(
      //     runtime, Handle<>(&O2REG(DelById)), "delete", ID(idVal)));
      _sh_throw_current(getSHRuntime(runtime));
    }
    {
      GCScopeMarkerRAII marker{runtime};
      Handle tmpHandle{runtime, res.getValue()};
      status = JSObject::deleteComputed(
          Handle<JSObject>::vmcast(tmpHandle),
          runtime,
          Handle<>(toPHV(key)),
          defaultPropOpFlags);
    }
    if (LLVM_UNLIKELY(status == ExecutionStatus::EXCEPTION)) {
      _sh_throw_current(getSHRuntime(runtime));
    }
    return HermesValue::encodeBoolValue(status.getValue());
  }
}

extern "C" SHLegacyValue _sh_ljs_del_by_val_strict(
    SHRuntime *shr,
    SHLegacyValue *target,
    SHLegacyValue *key) {
  return delByVal(getRuntime(shr), toPHV(target), toPHV(key), true);
}
extern "C" SHLegacyValue _sh_ljs_del_by_val_loose(
    SHRuntime *shr,
    SHLegacyValue *target,
    SHLegacyValue *key) {
  return delByVal(getRuntime(shr), toPHV(target), toPHV(key), false);
}

extern "C" SHLegacyValue _sh_ljs_get_string(SHRuntime *shr, SHSymbolID symID) {
  NoHandleScope noHandles{getRuntime(shr)};
  return HermesValue::encodeStringValue(
      getRuntime(shr).getStringPrimFromSymbolID(SymbolID::unsafeCreate(symID)));
}

extern "C" SHLegacyValue
_sh_ljs_create_regexp(SHRuntime *shr, SHSymbolID pattern, SHSymbolID flags) {
  Runtime &runtime = getRuntime(shr);
  CallResult<HermesValue> cr =
      [&runtime, pattern, flags]() -> CallResult<HermesValue> {
    GCScopeMarkerRAII marker{runtime};
    Handle<JSRegExp> re = runtime.makeHandle(JSRegExp::create(runtime));
    auto patternHandle = runtime.makeHandle(
        runtime.getStringPrimFromSymbolID(SymbolID::unsafeCreate(pattern)));
    auto flagsHandle = runtime.makeHandle(
        runtime.getStringPrimFromSymbolID(SymbolID::unsafeCreate(flags)));
    if (LLVM_UNLIKELY(
            JSRegExp::initialize(re, runtime, patternHandle, flagsHandle) ==
            ExecutionStatus::EXCEPTION))
      return ExecutionStatus::EXCEPTION;
    return re.getHermesValue();
  }();
  if (LLVM_UNLIKELY(cr == ExecutionStatus::EXCEPTION))
    _sh_throw_current(shr);

  return *cr;
}

extern "C" SHLegacyValue
_sh_ljs_create_bigint(SHRuntime *shr, const uint8_t *value, uint32_t size) {
  Runtime &runtime = getRuntime(shr);
  CallResult<HermesValue> res{ExecutionStatus::EXCEPTION};
  {
    // This function won't allocate handles because the `fromBytes` function
    // is also used in the interpreter.
    NoHandleScope noHandle{runtime};
    res = BigIntPrimitive::fromBytes(runtime, llvh::ArrayRef{value, size});
  }
  if (LLVM_UNLIKELY(res == ExecutionStatus::EXCEPTION))
    _sh_throw_current(shr);
  return *res;
}

extern "C" SHLegacyValue _sh_ljs_new_object(SHRuntime *shr) {
  PseudoHandle<JSObject> result;
  {
    NoHandleScope noHandle{getRuntime(shr)};
    result = JSObject::create(getRuntime(shr));
  }
  return result.getHermesValue();
}
extern "C" SHLegacyValue _sh_ljs_new_object_with_parent(
    SHRuntime *shr,
    const SHLegacyValue *parent) {
  PseudoHandle<JSObject> result;
  {
    NoHandleScope noHandle{getRuntime(shr)};
    result = JSObject::create(
        getRuntime(shr),
        toPHV(parent)->isObject() ? Handle<JSObject>::vmcast(toPHV(parent))
            : toPHV(parent)->isNull()
            ? Runtime::makeNullHandle<JSObject>()
            : Handle<JSObject>::vmcast(&getRuntime(shr).objectPrototype));
  }
  return result.getHermesValue();
}

static Handle<HiddenClass> getHiddenClassForBuffer(
    SHRuntime *shr,
    SHUnit *unit,
    uint32_t literalCacheID,
    uint32_t numLiterals,
    llvh::ArrayRef<unsigned char> keyBuffer,
    uint32_t keyBufferIndex) {
  Runtime &runtime = getRuntime(shr);

  // Check if there is an existing cache entry for this object literal.
  auto *cacheEntry = reinterpret_cast<WeakRoot<HiddenClass> *>(
      &unit->object_literal_class_cache[literalCacheID]);
  if (*cacheEntry)
    return runtime.makeHandle(cacheEntry->get(runtime, runtime.getHeap()));

  MutableHandle<HiddenClass> clazz =
      runtime.makeMutableHandle(*runtime.getHiddenClassForPrototype(
          vmcast<JSObject>(runtime.objectPrototype),
          JSObject::numOverlapSlots<JSObject>()));

  struct {
    void addProperty(SymbolID sym) {
      auto addResult = HiddenClass::addProperty(
          clazz, runtime, sym, PropertyFlags::defaultNewNamedPropertyFlags());
      clazz = addResult->first;
      marker.flush();
    }

    void visitStringID(StringID id) {
      auto sym = SymbolID::unsafeCreate(unit->symbols[id]);
      addProperty(sym);
    }
    void visitNumber(double d) {
      tmpHandleKey = HermesValue::encodeTrustedNumberValue(d);
      // Note that this handle is released in addProperty.
      Handle<SymbolID> symHandle = *valueToSymbolID(runtime, tmpHandleKey);
      addProperty(*symHandle);
    }
    void visitNull() {
      llvm_unreachable("Object literal key cannot be null.");
    }
    void visitBool(bool) {
      llvm_unreachable("Object literal key cannot be a bool.");
    }

    MutableHandle<HiddenClass> &clazz;
    MutableHandle<> tmpHandleKey;
    Runtime &runtime;
    SHUnit *unit;
    GCScopeMarkerRAII marker;
  } v{clazz,
      MutableHandle<>{runtime},
      runtime,
      unit,
      GCScopeMarkerRAII{runtime}};

  SerializedLiteralParser::parse(
      keyBuffer.slice(keyBufferIndex), numLiterals, v);

  if (LLVM_LIKELY(!clazz->isDictionary())) {
    assert(
        numLiterals == clazz->getNumProperties() &&
        "numLiterals should match hidden class property count.");
    assert(
        clazz->getNumProperties() < 256 &&
        "cached hidden class should have property count less than 256");
    cacheEntry->set(runtime, clazz.get());
  }

  return {clazz};
}

extern "C" SHLegacyValue _sh_ljs_new_object_with_buffer(
    SHRuntime *shr,
    SHUnit *unit,
    uint32_t sizeHint,
    uint32_t literalCacheID,
    uint32_t valBufferIndex) {
  Runtime &runtime = getRuntime(shr);
  GCScopeMarkerRAII marker{runtime};
  llvh::ArrayRef keyBuffer{unit->obj_key_buffer, unit->obj_key_buffer_size};
  llvh::ArrayRef valBuffer{unit->obj_val_buffer, unit->obj_val_buffer_size};

  auto [keyBufferIndex, numLiterals] =
      unit->object_literal_key_info[literalCacheID];

  // Create a new object using the built-in constructor or cached hidden class.
  // Note that the built-in constructor is empty, so we don't actually need to
  // call it.
  Handle<HiddenClass> clazz = getHiddenClassForBuffer(
      shr, unit, literalCacheID, numLiterals, keyBuffer, keyBufferIndex);
  Handle<JSObject> obj = runtime.makeHandle(JSObject::create(runtime, clazz));

  struct {
    void visitStringID(StringID id) {
      auto shv = SmallHermesValue::encodeStringValue(
          runtime.getStringPrimFromSymbolID(
              SymbolID::unsafeCreate(unit->symbols[id])),
          runtime);
      JSObject::setNamedSlotValueUnsafe(*obj, runtime, i++, shv);
    }
    void visitNumber(double d) {
      auto shv = SmallHermesValue::encodeNumberValue(d, runtime);
      JSObject::setNamedSlotValueUnsafe(*obj, runtime, i++, shv);
    }
    void visitNull() {
      constexpr auto shv = SmallHermesValue::encodeNullValue();
      JSObject::setNamedSlotValueUnsafe(*obj, runtime, i++, shv);
    }
    void visitBool(bool b) {
      auto shv = SmallHermesValue::encodeBoolValue(b);
      JSObject::setNamedSlotValueUnsafe(*obj, runtime, i++, shv);
    }

    Handle<JSObject> obj;
    Runtime &runtime;
    SHUnit *unit;
    size_t i;
  } v{obj, runtime, unit, 0};

  SerializedLiteralParser::parse(
      valBuffer.slice(valBufferIndex), numLiterals, v);

  return obj.getHermesValue();
}

extern "C" SHLegacyValue _sh_ljs_new_array(SHRuntime *shr, uint32_t sizeHint) {
  Runtime &runtime = getRuntime(shr);

  CallResult<HermesValue> arrayRes = [&runtime, sizeHint]() {
    GCScopeMarkerRAII marker{runtime};
    return toCallResultHermesValue(
        JSArray::create(runtime, sizeHint, sizeHint));
  }();
  if (LLVM_UNLIKELY(arrayRes == ExecutionStatus::EXCEPTION))
    _sh_throw_current(shr);

  return *arrayRes;
}

extern "C" SHLegacyValue _sh_ljs_new_array_with_buffer(
    SHRuntime *shr,
    SHUnit *unit,
    uint32_t numElements,
    uint32_t numLiterals,
    uint32_t arrayBufferIndex) {
  // Create a new array using the built-in constructor, and initialize
  // the elements from a literal array buffer.
  Runtime &runtime = getRuntime(shr);
  llvh::ArrayRef arrayBuffer{unit->array_buffer, unit->array_buffer_size};

  CallResult<HermesValue> arrayRes = [&runtime, numElements]() {
    GCScopeMarkerRAII marker{runtime};
    return toCallResultHermesValue(
        JSArray::create(runtime, numElements, numElements));
  }();
  if (LLVM_UNLIKELY(arrayRes == ExecutionStatus::EXCEPTION))
    _sh_throw_current(shr);

  HermesValue arr = [&runtime,
                     unit,
                     &arrayRes,
                     numElements,
                     numLiterals,
                     arrayBuffer,
                     arrayBufferIndex]() {
    GCScopeMarkerRAII marker{runtime};
    // Resize the array storage in advance.
    Handle<JSArray> arr = runtime.makeHandle(vmcast<JSArray>(*arrayRes));
    JSArray::setStorageEndIndex(arr, runtime, numElements);

    struct {
      void visitStringID(StringID id) {
        auto shv = SmallHermesValue::encodeStringValue(
            runtime.getStringPrimFromSymbolID(
                SymbolID::unsafeCreate(unit->symbols[id])),
            runtime);
        JSArray::unsafeSetExistingElementAt(*arr, runtime, i++, shv);
      }
      void visitNumber(double d) {
        auto shv = SmallHermesValue::encodeNumberValue(d, runtime);
        JSArray::unsafeSetExistingElementAt(*arr, runtime, i++, shv);
      }
      void visitNull() {
        constexpr auto shv = SmallHermesValue::encodeNullValue();
        JSArray::unsafeSetExistingElementAt(*arr, runtime, i++, shv);
      }
      void visitBool(bool b) {
        auto shv = SmallHermesValue::encodeBoolValue(b);
        JSArray::unsafeSetExistingElementAt(*arr, runtime, i++, shv);
      }

      Handle<JSArray> arr;
      Runtime &runtime;
      SHUnit *unit;
      size_t i;
    } v{arr, runtime, unit, 0};

    SerializedLiteralParser::parse(
        arrayBuffer.slice(arrayBufferIndex), numLiterals, v);

    return arr.getHermesValue();
  }();

  return arr;
}

extern "C" SHLegacyValue _sh_new_fastarray(SHRuntime *shr, uint32_t sizeHint) {
  Runtime &runtime = getRuntime(shr);

  CallResult<HermesValue> arrayRes = [&runtime, sizeHint]() {
    GCScopeMarkerRAII marker{runtime};
    return toCallResultHermesValue(FastArray::create(runtime, sizeHint));
  }();
  if (LLVM_UNLIKELY(arrayRes == ExecutionStatus::EXCEPTION))
    _sh_throw_current(shr);

  return *arrayRes;
}

extern "C" SHLegacyValue
_sh_ljs_is_in_rjs(SHRuntime *shr, SHLegacyValue *name, SHLegacyValue *obj) {
  Runtime &runtime = getRuntime(shr);
  CallResult<bool> cr{false};
  {
    GCScopeMarkerRAII marker{runtime};
    if (LLVM_UNLIKELY(!_sh_ljs_is_object(*obj))) {
      (void)runtime.raiseTypeError("right operand of 'in' is not an object");
      cr = ExecutionStatus::EXCEPTION;
    } else {
      cr = JSObject::hasComputed(
          Handle<JSObject>::vmcast(toPHV(obj)), runtime, Handle<>(toPHV(name)));
    }
  }
  if (LLVM_UNLIKELY(cr == ExecutionStatus::EXCEPTION))
    _sh_throw_current(shr);
  return _sh_ljs_bool(*cr);
}

extern "C" SHLegacyValue _sh_ljs_get_pname_list_rjs(
    SHRuntime *shr,
    SHLegacyValue *base,
    SHLegacyValue *index,
    SHLegacyValue *size) {
  Runtime &runtime = getRuntime(shr);
  if (toPHV(base)->isUndefined() || toPHV(base)->isNull()) {
    // Set the iterator to be undefined value.
    return HermesValue::encodeUndefinedValue();
  }

  auto cr = [&runtime, base, index, size]() -> CallResult<HermesValue> {
    GCScopeMarkerRAII marker{runtime};
    Handle baseHandle{toPHV(base)};
    // Convert to object and store it back to the register.
    auto res = toObject(runtime, baseHandle);
    if (LLVM_UNLIKELY(res == ExecutionStatus::EXCEPTION)) {
      return ExecutionStatus::EXCEPTION;
    }
    *base = res.getValue();

    auto obj = runtime.makeMutableHandle(vmcast<JSObject>(res.getValue()));
    uint32_t beginIndex;
    uint32_t endIndex;
    auto cr = getForInPropertyNames(runtime, obj, beginIndex, endIndex);
    if (cr == ExecutionStatus::EXCEPTION) {
      return ExecutionStatus::EXCEPTION;
    }
    auto arr = *cr;
    *index = HermesValue::encodeTrustedNumberValue(beginIndex);
    *size = HermesValue::encodeTrustedNumberValue(endIndex);
    return arr.getHermesValue();
  }();

  if (LLVM_UNLIKELY(cr == ExecutionStatus::EXCEPTION))
    _sh_throw_current(shr);

  return *cr;
}

extern "C" SHLegacyValue _sh_ljs_get_next_pname_rjs(
    SHRuntime *shr,
    SHLegacyValue *props,
    SHLegacyValue *base,
    SHLegacyValue *indexVal,
    SHLegacyValue *sizeVal) {
  Runtime &runtime = getRuntime(shr);
  assert(
      vmisa<BigStorage>(*toPHV(props)) &&
      "GetNextPName's props must be BigStorage");
  Handle<BigStorage> arr = Handle<BigStorage>::vmcast(toPHV(props));
  Handle obj = Handle<JSObject>::vmcast(toPHV(base));
  auto result =
      [&runtime, arr, obj, indexVal, sizeVal]() -> CallResult<HermesValue> {
    GCScopeMarkerRAII marker{runtime};
    uint32_t idx = toPHV(indexVal)->getNumber();
    uint32_t size = toPHV(sizeVal)->getNumber();
    MutableHandle<> tmpHandle{runtime};
    MutableHandle<JSObject> propObj{runtime};
    MutableHandle<SymbolID> tmpPropNameStorage{runtime};
    // Loop until we find a property which is present.
    while (idx < size) {
      tmpHandle = arr->at(runtime, idx);
      ComputedPropertyDescriptor desc;
      ExecutionStatus status = JSObject::getComputedPrimitiveDescriptor(
          obj, runtime, tmpHandle, propObj, tmpPropNameStorage, desc);
      if (LLVM_UNLIKELY(status == ExecutionStatus::EXCEPTION)) {
        return ExecutionStatus::EXCEPTION;
      }
      if (LLVM_LIKELY(propObj))
        break;
      ++idx;
    }
    if (idx < size) {
      // We must return the property as a string
      if (tmpHandle->isNumber()) {
        auto status = toString_RJS(runtime, tmpHandle);
        assert(
            status == ExecutionStatus::RETURNED &&
            "toString on number cannot fail");
        tmpHandle = status->getHermesValue();
      }
      *indexVal = HermesValue::encodeTrustedNumberValue(idx + 1);
      return tmpHandle.get();
    } else {
      return HermesValue::encodeUndefinedValue();
    }
  }();

  if (LLVM_UNLIKELY(result == ExecutionStatus::EXCEPTION))
    _sh_throw_current(shr);

  return *result;
}

extern "C" SHLegacyValue _sh_ljs_direct_eval(
    SHRuntime *shr,
    SHLegacyValue *evalText,
    bool strictCaller) {
  if (!_sh_ljs_is_string(*evalText))
    return *evalText;
  auto result = [&runtime = getRuntime(shr),
                 evalText,
                 strictCaller]() -> CallResult<HermesValue> {
    GCScopeMarkerRAII marker{runtime};

    // Create a dummy scope, so that the local eval executes in its own scope
    // (as per the spec for strict callers, which is the only thing we support).

    ScopeChain scopeChain{};
    scopeChain.functions.emplace_back();

    return vm::directEval(
        runtime,
        Handle<StringPrimitive>::vmcast(toPHV(evalText)),
        strictCaller,
        scopeChain,
        false);
  }();

  if (LLVM_UNLIKELY(result == ExecutionStatus::EXCEPTION))
    _sh_throw_current(shr);

  return *result;
}

extern "C" int32_t _sh_to_int32_double_slow_path(double d) {
  return truncateToInt32SlowPath(d);
}

extern "C" SHLegacyValue
_sh_prload_direct(SHRuntime *shr, SHLegacyValue source, uint32_t propIndex) {
  Runtime &runtime = getRuntime(shr);
  NoAllocScope noAlloc(runtime);
  return JSObject::getNamedSlotValueDirectUnsafe(
             vmcast<JSObject>(*toPHV(&source)), runtime, propIndex)
      .unboxToHV(runtime);
}

extern "C" SHLegacyValue
_sh_prload_indirect(SHRuntime *shr, SHLegacyValue source, uint32_t propIndex) {
  Runtime &runtime = getRuntime(shr);
  NoAllocScope noAlloc(runtime);
  return JSObject::getNamedSlotValueIndirectUnsafe(
             vmcast<JSObject>(*toPHV(&source)), runtime, propIndex)
      .unboxToHV(runtime);
}

extern "C" void _sh_prstore_direct(
    SHRuntime *shr,
    SHLegacyValue *target,
    uint32_t propIndex,
    SHLegacyValue *value) {
  Runtime &runtime = getRuntime(shr);
  SmallHermesValue shv =
      SmallHermesValue::encodeHermesValue(*toPHV(value), runtime);
  JSObject::setNamedSlotValueDirectUnsafe(
      vmcast<JSObject>(*toPHV(target)), runtime, propIndex, shv);
}

extern "C" void _sh_prstore_direct_bool(
    SHRuntime *shr,
    SHLegacyValue *target,
    uint32_t propIndex,
    SHLegacyValue *value) {
  assert(_sh_ljs_is_bool(*value));
  Runtime &runtime = getRuntime(shr);
  auto shv = SmallHermesValue::encodeBoolValue(_sh_ljs_get_bool(*value));
  JSObject::setNamedSlotValueDirectUnsafe<std::false_type>(
      vmcast<JSObject>(*toPHV(target)), runtime, propIndex, shv);
}

extern "C" void _sh_prstore_direct_number(
    SHRuntime *shr,
    SHLegacyValue *target,
    uint32_t propIndex,
    SHLegacyValue *value) {
  assert(_sh_ljs_is_double(*value));
  Runtime &runtime = getRuntime(shr);
  auto shv =
      SmallHermesValue::encodeNumberValue(_sh_ljs_get_double(*value), runtime);
  JSObject::setNamedSlotValueDirectUnsafe(
      vmcast<JSObject>(*toPHV(target)), runtime, propIndex, shv);
}

extern "C" void _sh_prstore_direct_object(
    SHRuntime *shr,
    SHLegacyValue *target,
    uint32_t propIndex,
    SHLegacyValue *value) {
  assert(_sh_ljs_is_object(*value));
  Runtime &runtime = getRuntime(shr);
  auto shv = SmallHermesValue::encodeObjectValue(
      vmcast<JSObject>(*toPHV(value)), runtime);
  JSObject::setNamedSlotValueDirectUnsafe(
      vmcast<JSObject>(*toPHV(target)), runtime, propIndex, shv);
}

extern "C" void _sh_prstore_direct_string(
    SHRuntime *shr,
    SHLegacyValue *target,
    uint32_t propIndex,
    SHLegacyValue *value) {
  assert(_sh_ljs_is_string(*value));
  Runtime &runtime = getRuntime(shr);
  auto shv = SmallHermesValue::encodeStringValue(
      vmcast<StringPrimitive>(*toPHV(value)), runtime);
  JSObject::setNamedSlotValueDirectUnsafe(
      vmcast<JSObject>(*toPHV(target)), runtime, propIndex, shv);
}

extern "C" void _sh_prstore_indirect(
    SHRuntime *shr,
    SHLegacyValue *target,
    uint32_t propIndex,
    SHLegacyValue *value) {
  Runtime &runtime = getRuntime(shr);
  SmallHermesValue shv =
      SmallHermesValue::encodeHermesValue(*toPHV(value), runtime);
  JSObject::setNamedSlotValueIndirectUnsafe(
      vmcast<JSObject>(*toPHV(target)), runtime, propIndex, shv);
}

extern "C" void _sh_store_parent(
    SHRuntime *shr,
    const SHLegacyValue *storedValue,
    const SHLegacyValue *object) {
  Runtime &runtime = getRuntime(shr);
  Handle<JSObject> objectHandle = Handle<JSObject>::vmcast(toPHV(object));
  Handle<JSObject> parentHandle = Handle<JSObject>::vmcast(toPHV(storedValue));
  JSObject::unsafeSetParentInternal(
      objectHandle.get(), runtime, parentHandle.get());
}

/// Check that \p index is an unsigned integer smaller than \p length. If it is,
/// return it, otherwise, throw.
inline static uint32_t
fastarrayBoundsCheck(SHRuntime *shr, uint32_t length, double index) {
  uint32_t intIndex = hermes::unsafeTruncateDouble<uint32_t>(index);
  if (LLVM_LIKELY(intIndex < length && intIndex == index))
    return intIndex;

  _sh_throw_array_oob(shr);
}

extern "C" void _sh_throw_array_oob(SHRuntime *shr) {
  (void)getRuntime(shr).raiseRangeError("array load index out of range");
  _sh_throw_current(shr);
}

extern "C" SHLegacyValue
_sh_fastarray_load_impl(SHRuntime *shr, SHLegacyValue *array, double index) {
  Runtime &runtime = getRuntime(shr);
  auto arrayHandle = Handle<FastArray>::vmcast(toPHV(array));

  uint32_t intIndex =
      fastarrayBoundsCheck(shr, arrayHandle->getLength(runtime), index);
  return arrayHandle->at(runtime, intIndex)->unboxToHV(runtime);
}

extern "C" void _sh_fastarray_store(
    SHRuntime *shr,
    const SHLegacyValue *storedValue,
    SHLegacyValue *array,
    double index) {
  Runtime &runtime = getRuntime(shr);
  auto arrayHandle = Handle<FastArray>::vmcast(toPHV(array));

  uint32_t intIndex =
      fastarrayBoundsCheck(shr, arrayHandle->getLength(runtime), index);
  auto shv = SmallHermesValue::encodeHermesValue(*toPHV(storedValue), runtime);
  arrayHandle->unsafeSet(runtime, intIndex, shv);
}

extern "C" void _sh_fastarray_push(
    SHRuntime *shr,
    SHLegacyValue *pushedValue,
    SHLegacyValue *array) {
  Runtime &runtime = getRuntime(shr);
  auto arr = Handle<FastArray>::vmcast(toPHV(array));
  auto val = Handle<>(toPHV(pushedValue));
  auto res = FastArray::push(arr, runtime, val);
  if (LLVM_UNLIKELY(res == ExecutionStatus::EXCEPTION))
    _sh_throw_current(shr);
}

extern "C" void _sh_fastarray_append(
    SHRuntime *shr,
    SHLegacyValue *other,
    SHLegacyValue *array) {
  Runtime &runtime = getRuntime(shr);
  auto arr = Handle<FastArray>::vmcast(toPHV(array));
  auto source = Handle<FastArray>::vmcast(toPHV(other));
  auto res = FastArray::append(arr, runtime, source);

  if (LLVM_UNLIKELY(res == ExecutionStatus::EXCEPTION))
    _sh_throw_current(shr);
}

extern "C" int _sh_errno(void) {
  return errno;
}

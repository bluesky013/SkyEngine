//
// Created by blues on 2024/6/2.
//

#include <python/PythonBinding.h>

#include <framework/serialization/SerializationContext.h>
#include <core/logger/Logger.h>

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

static const char *TAG = "PythonBinding";

namespace sky::py {
namespace {

    struct PySkyObject {
        PyObject_HEAD
        Any value;
        const TypeNode *node;
    };

    struct PySkyFunction {
        PyObject_HEAD
        PyObject *owner;
        std::string *name;
    };

    struct PySkySequence {
        PyObject_HEAD
        PyObject *owner;
        const serialize::TypeMemberNode *member;
        Any container;
    };

    struct MemberClosure {
        const serialize::TypeMemberNode *member;
    };

    class PyRef {
    public:
        PyRef() = default;
        explicit PyRef(PyObject *obj) : ptr(obj) {}
        ~PyRef() { Py_XDECREF(ptr); }

        PyRef(const PyRef &) = delete;
        PyRef &operator=(const PyRef &) = delete;

        PyRef(PyRef &&other) noexcept : ptr(other.ptr) { other.ptr = nullptr; }
        PyRef &operator=(PyRef &&other) noexcept
        {
            if (this != &other) {
                Py_XDECREF(ptr);
                ptr = other.ptr;
                other.ptr = nullptr;
            }
            return *this;
        }

        PyObject *Get() const { return ptr; }
        PyObject *Release()
        {
            PyObject *tmp = ptr;
            ptr = nullptr;
            return tmp;
        }
        explicit operator bool() const { return ptr != nullptr; }

    private:
        PyObject *ptr = nullptr;
    };

    PyTypeObject PySkyFunctionType = { PyVarObject_HEAD_INIT(nullptr, 0) };
    PyTypeObject PySkySequenceType = { PyVarObject_HEAD_INIT(nullptr, 0) };

    std::unordered_map<Uuid, PyTypeObject *> g_typeCache;
    std::unordered_map<PyTypeObject *, const TypeNode *> g_typeNodes;
    std::vector<std::string *> g_specNames;
    std::vector<std::vector<PyGetSetDef> *> g_getsetArrays;
    std::vector<std::vector<MemberClosure *> *> g_closureArrays;

    PyObject *AnyToPython(const Any &value);
    bool PythonToAny(PyObject *obj, const TypeInfoRT *info, Any &out);
    PyTypeObject *GetOrCreateType(const TypeNode *node);
    const TypeInfoRT *FindTypeInfo(const Uuid &id);

    bool IsUnsignedId(const Uuid &id)
    {
        return id == TypeInfo<uint8_t>::RegisteredId() ||
               id == TypeInfo<uint16_t>::RegisteredId() ||
               id == TypeInfo<uint32_t>::RegisteredId() ||
               id == TypeInfo<uint64_t>::RegisteredId();
    }

    const TypeInfoRT *FindTypeInfo(const Uuid &id)
    {
        static const std::unordered_map<Uuid, const TypeInfoRT *> scalars = {
            {TypeInfo<bool>::RegisteredId(), TypeInfoObj<bool>::Get()->RtInfo()},
            {TypeInfo<int8_t>::RegisteredId(), TypeInfoObj<int8_t>::Get()->RtInfo()},
            {TypeInfo<uint8_t>::RegisteredId(), TypeInfoObj<uint8_t>::Get()->RtInfo()},
            {TypeInfo<int16_t>::RegisteredId(), TypeInfoObj<int16_t>::Get()->RtInfo()},
            {TypeInfo<uint16_t>::RegisteredId(), TypeInfoObj<uint16_t>::Get()->RtInfo()},
            {TypeInfo<int32_t>::RegisteredId(), TypeInfoObj<int32_t>::Get()->RtInfo()},
            {TypeInfo<uint32_t>::RegisteredId(), TypeInfoObj<uint32_t>::Get()->RtInfo()},
            {TypeInfo<int64_t>::RegisteredId(), TypeInfoObj<int64_t>::Get()->RtInfo()},
            {TypeInfo<uint64_t>::RegisteredId(), TypeInfoObj<uint64_t>::Get()->RtInfo()},
            {TypeInfo<char>::RegisteredId(), TypeInfoObj<char>::Get()->RtInfo()},
            {TypeInfo<float>::RegisteredId(), TypeInfoObj<float>::Get()->RtInfo()},
            {TypeInfo<double>::RegisteredId(), TypeInfoObj<double>::Get()->RtInfo()},
            {TypeInfo<std::string>::RegisteredId(), TypeInfoObj<std::string>::Get()->RtInfo()},
        };
        auto iter = scalars.find(id);
        if (iter != scalars.end()) {
            return iter->second;
        }
        const TypeNode *node = GetTypeNode(id);
        return node != nullptr ? node->info : nullptr;
    }

    unsigned long long ReadUnsigned(const void *data, size_t size)
    {
        unsigned long long value = 0;
        std::memcpy(&value, data, size > sizeof(value) ? sizeof(value) : size);
        return value;
    }

    long long ReadSigned(const void *data, size_t size)
    {
        long long value = 0;
        std::memcpy(&value, data, size > sizeof(value) ? sizeof(value) : size);
        return value;
    }

    PyObject *MakeObject(const TypeNode *node, const Any &value)
    {
        PyTypeObject *type = GetOrCreateType(node);
        if (type == nullptr) {
            return nullptr;
        }

        auto *obj = reinterpret_cast<PySkyObject *>(type->tp_alloc(type, 0));
        if (obj == nullptr) {
            return nullptr;
        }
        new (&obj->value) Any(value);
        obj->node = node;
        return reinterpret_cast<PyObject *>(obj);
    }

    PyObject *AnyToPython(const Any &value)
    {
        const TypeInfoRT *info = value.Info();
        if (info == nullptr || value.Data() == nullptr) {
            Py_RETURN_NONE;
        }

        if (info->registeredId == TypeInfo<std::string>::RegisteredId()) {
            return PyUnicode_FromString(static_cast<const std::string *>(value.Data())->c_str());
        }
        if (info->registeredId == TypeInfo<bool>::RegisteredId()) {
            return PyBool_FromLong(*static_cast<const bool *>(value.Data()) ? 1 : 0);
        }
        if (info->registeredId == TypeInfo<float>::RegisteredId()) {
            return PyFloat_FromDouble(*static_cast<const float *>(value.Data()));
        }
        if (info->registeredId == TypeInfo<double>::RegisteredId()) {
            return PyFloat_FromDouble(*static_cast<const double *>(value.Data()));
        }
        if (info->staticInfo->isEnum) {
            return PyLong_FromUnsignedLongLong(ReadUnsigned(value.Data(), info->staticInfo->size));
        }
        if (info->staticInfo->isInteger) {
            if (IsUnsignedId(info->registeredId)) {
                return PyLong_FromUnsignedLongLong(ReadUnsigned(value.Data(), info->staticInfo->size));
            }
            return PyLong_FromLongLong(ReadSigned(value.Data(), info->staticInfo->size));
        }

        const TypeNode *node = GetTypeNode(info);
        if (node != nullptr) {
            return MakeObject(node, value);
        }

        PyErr_Format(PyExc_TypeError, "cannot convert type '%s' to python", std::string(info->name).c_str());
        return nullptr;
    }

    bool PythonToAny(PyObject *obj, const TypeInfoRT *info, Any &out)
    {
        if (info == nullptr || info->staticInfo == nullptr) {
            PyErr_SetString(PyExc_TypeError, "unknown target type");
            return false;
        }

        if (info->registeredId == TypeInfo<bool>::RegisteredId()) {
            bool value = PyObject_IsTrue(obj) == 1;
            out = Any::Create(info, &value);
            return true;
        }
        if (info->registeredId == TypeInfo<std::string>::RegisteredId()) {
            if (!PyUnicode_Check(obj)) {
                PyErr_SetString(PyExc_TypeError, "expected str");
                return false;
            }
            const char *text = PyUnicode_AsUTF8(obj);
            if (text == nullptr) {
                return false;
            }
            std::string value(text);
            out = Any::Create(info, &value);
            return true;
        }
        if (info->registeredId == TypeInfo<float>::RegisteredId()) {
            double number = PyFloat_AsDouble(obj);
            if (number == -1.0 && PyErr_Occurred() != nullptr) {
                return false;
            }
            float value = static_cast<float>(number);
            out = Any::Create(info, &value);
            return true;
        }
        if (info->registeredId == TypeInfo<double>::RegisteredId()) {
            double value = PyFloat_AsDouble(obj);
            if (value == -1.0 && PyErr_Occurred() != nullptr) {
                return false;
            }
            out = Any::Create(info, &value);
            return true;
        }
        if (info->staticInfo->isEnum) {
            long long value = PyLong_AsLongLong(obj);
            if (value == -1 && PyErr_Occurred() != nullptr) {
                return false;
            }
            unsigned char buffer[sizeof(long long)] = {0};
            std::memcpy(buffer, &value, info->staticInfo->size > sizeof(buffer) ? sizeof(buffer) : info->staticInfo->size);
            out = Any::Create(info, buffer);
            return true;
        }
        if (info->staticInfo->isInteger) {
            if (IsUnsignedId(info->registeredId)) {
                unsigned long long value = PyLong_AsUnsignedLongLong(obj);
                if (value == static_cast<unsigned long long>(-1) && PyErr_Occurred() != nullptr) {
                    return false;
                }
                out = Any::Create(info, &value);
                return true;
            }
            long long value = PyLong_AsLongLong(obj);
            if (value == -1 && PyErr_Occurred() != nullptr) {
                return false;
            }
            out = Any::Create(info, &value);
            return true;
        }

        if (g_typeNodes.count(Py_TYPE(obj)) != 0) {
            auto *src = reinterpret_cast<PySkyObject *>(obj);
            if (src->value.Info() != nullptr && src->value.Data() != nullptr &&
                src->value.Info()->registeredId == info->registeredId) {
                out = Any::Create(info, src->value.Data());
                return true;
            }
        }

        PyErr_Format(PyExc_TypeError, "cannot convert python object to '%s'", std::string(info->name).c_str());
        return false;
    }

    void SequenceWriteBack(PySkySequence *self)
    {
        if (self->member->setterFn != nullptr) {
            auto *owner = reinterpret_cast<PySkyObject *>(self->owner);
            self->member->setterFn(owner->value.Data(), self->container.Data());
        }
    }

    Py_ssize_t SequenceLength(PyObject *selfObj)
    {
        auto *self = reinterpret_cast<PySkySequence *>(selfObj);
        auto *view = self->member->info->containerInfo->sequenceView;
        return static_cast<Py_ssize_t>(view->Count(self->container.Data()));
    }

    PyObject *SequenceGetItem(PyObject *selfObj, Py_ssize_t index)
    {
        auto *self = reinterpret_cast<PySkySequence *>(selfObj);
        auto *containerInfo = self->member->info->containerInfo;
        auto *view = containerInfo->sequenceView;
        const auto count = static_cast<Py_ssize_t>(view->Count(self->container.Data()));
        if (index < 0) {
            index += count;
        }
        if (index < 0 || index >= count) {
            PyErr_SetString(PyExc_IndexError, "sequence index out of range");
            return nullptr;
        }
        const TypeInfoRT *elementInfo = FindTypeInfo(containerInfo->valueType);
        if (elementInfo == nullptr) {
            PyErr_SetString(PyExc_TypeError, "unknown sequence element type");
            return nullptr;
        }
        void *element = view->GetByIndex(self->container.Data(), static_cast<size_t>(index));
        return AnyToPython(Any::Create(elementInfo, element));
    }

    PyObject *SequenceSubscript(PyObject *selfObj, PyObject *indexObj)
    {
        Py_ssize_t index = PyNumber_AsSsize_t(indexObj, PyExc_IndexError);
        if (index == -1 && PyErr_Occurred() != nullptr) {
            return nullptr;
        }
        return SequenceGetItem(selfObj, index);
    }

    PyObject *SequenceAppend(PyObject *selfObj, PyObject *args)
    {
        auto *self = reinterpret_cast<PySkySequence *>(selfObj);
        PyObject *item = nullptr;
        if (!PyArg_ParseTuple(args, "O", &item)) {
            return nullptr;
        }
        auto *containerInfo = self->member->info->containerInfo;
        const TypeInfoRT *elementInfo = FindTypeInfo(containerInfo->valueType);
        if (elementInfo == nullptr) {
            PyErr_SetString(PyExc_TypeError, "unknown sequence element type");
            return nullptr;
        }
        Any value;
        if (!PythonToAny(item, elementInfo, value)) {
            return nullptr;
        }
        void *slot = containerInfo->sequenceView->Emplace(self->container.Data());
        if (elementInfo->copy != nullptr) {
            elementInfo->copy(value.Data(), slot);
        }
        SequenceWriteBack(self);
        Py_RETURN_NONE;
    }

    PyObject *SequenceErase(PyObject *selfObj, PyObject *args)
    {
        auto *self = reinterpret_cast<PySkySequence *>(selfObj);
        Py_ssize_t index = 0;
        if (!PyArg_ParseTuple(args, "n", &index)) {
            return nullptr;
        }
        auto *containerInfo = self->member->info->containerInfo;
        auto *view = containerInfo->sequenceView;
        const auto count = static_cast<Py_ssize_t>(view->Count(self->container.Data()));
        if (index < 0) {
            index += count;
        }
        if (index < 0 || index >= count) {
            PyErr_SetString(PyExc_IndexError, "sequence index out of range");
            return nullptr;
        }
        view->EraseByIndex(self->container.Data(), static_cast<size_t>(index));
        SequenceWriteBack(self);
        Py_RETURN_NONE;
    }

    void SequenceDealloc(PyObject *selfObj)
    {
        auto *self = reinterpret_cast<PySkySequence *>(selfObj);
        self->container.~Any();
        Py_XDECREF(self->owner);
        Py_TYPE(selfObj)->tp_free(selfObj);
    }

    PyMethodDef g_sequenceMethods[] = {
        {"append", SequenceAppend, METH_VARARGS, "append an element"},
        {"erase", SequenceErase, METH_VARARGS, "erase an element by index"},
        {nullptr, nullptr, 0, nullptr},
    };

    PySequenceMethods g_sequenceAsSequence = {
        SequenceLength, nullptr, nullptr, SequenceGetItem, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
    };
    PyMappingMethods g_sequenceAsMapping = { SequenceLength, SequenceSubscript, nullptr };

    PyObject *MemberGet(PyObject *selfObj, void *closure)
    {
        auto *self = reinterpret_cast<PySkyObject *>(selfObj);
        auto *member = static_cast<MemberClosure *>(closure)->member;
        if (self->value.Data() == nullptr || member->getterConstFn == nullptr) {
            Py_RETURN_NONE;
        }

        Any value = member->getterConstFn(self->value.Data());
        auto *containerInfo = member->info != nullptr ? member->info->containerInfo : nullptr;
        if (containerInfo != nullptr && containerInfo->sequenceView != nullptr) {
            auto *sequence = PyObject_New(PySkySequence, &PySkySequenceType);
            if (sequence == nullptr) {
                return nullptr;
            }
            Py_INCREF(selfObj);
            sequence->owner = selfObj;
            sequence->member = member;
            new (&sequence->container) Any(std::move(value));
            return reinterpret_cast<PyObject *>(sequence);
        }
        return AnyToPython(value);
    }

    int MemberSet(PyObject *selfObj, PyObject *value, void *closure)
    {
        auto *self = reinterpret_cast<PySkyObject *>(selfObj);
        auto *member = static_cast<MemberClosure *>(closure)->member;
        if (member->setterFn == nullptr) {
            PyErr_SetString(PyExc_AttributeError, "member is read-only");
            return -1;
        }

        Any converted;
        if (!PythonToAny(value, member->info, converted)) {
            return -1;
        }
        if (!member->setterFn(self->value.Data(), converted.Data())) {
            PyErr_SetString(PyExc_TypeError, "failed to set member");
            return -1;
        }
        return 0;
    }

    PyObject *ObjectNew(PyTypeObject *type, PyObject *args, PyObject *kwds)
    {
        auto *self = reinterpret_cast<PySkyObject *>(type->tp_alloc(type, 0));
        if (self == nullptr) {
            return nullptr;
        }
        new (&self->value) Any();
        self->node = nullptr;
        return reinterpret_cast<PyObject *>(self);
    }

    int ObjectInit(PyObject *selfObj, PyObject *args, PyObject *kwds)
    {
        auto *self = reinterpret_cast<PySkyObject *>(selfObj);
        auto typeIt = g_typeNodes.find(Py_TYPE(selfObj));
        if (typeIt == g_typeNodes.end() || typeIt->second == nullptr) {
            PyErr_SetString(PyExc_TypeError, "unbound reflected type");
            return -1;
        }
        const TypeNode *node = typeIt->second;

        Py_ssize_t argc = PyTuple_Size(args);
        if (argc < 0) {
            return -1;
        }

        for (const auto &ctr : node->constructList) {
            if (static_cast<Py_ssize_t>(ctr.argsNum) != argc) {
                continue;
            }

            std::vector<Any> anyArgs;
            anyArgs.reserve(static_cast<size_t>(argc));
            bool ok = true;
            for (Py_ssize_t i = 0; i < argc; ++i) {
                const TypeInfoRT *argInfo = i < static_cast<Py_ssize_t>(ctr.argTypes.size()) ? ctr.argTypes[static_cast<size_t>(i)] : nullptr;
                Any converted;
                if (!PythonToAny(PyTuple_GetItem(args, i), argInfo, converted)) {
                    ok = false;
                    break;
                }
                anyArgs.push_back(std::move(converted));
            }
            if (!ok) {
                PyErr_Clear();
                continue;
            }
            if (ctr.checkFn != nullptr && !ctr.checkFn(anyArgs.data())) {
                continue;
            }

            Any result = ctr.constructFn(anyArgs.data());
            if (result.Info() != nullptr) {
                self->value = std::move(result);
                self->node = node;
                return 0;
            }
        }

        PyErr_Format(PyExc_TypeError, "no matching constructor for '%s'", std::string(node->info->name).c_str());
        return -1;
    }

    void ObjectDealloc(PyObject *selfObj)
    {
        auto *self = reinterpret_cast<PySkyObject *>(selfObj);
        self->value.~Any();
        Py_TYPE(selfObj)->tp_free(selfObj);
    }

    void FunctionDealloc(PyObject *selfObj);

    PyObject *FunctionCall(PyObject *selfObj, PyObject *args, PyObject *kwargs)
    {
        auto *self = reinterpret_cast<PySkyFunction *>(selfObj);
        auto *owner = reinterpret_cast<PySkyObject *>(self->owner);
        if (owner->node == nullptr) {
            PyErr_SetString(PyExc_RuntimeError, "unbound object");
            return nullptr;
        }

        auto iter = owner->node->functions.find(std::string_view(*self->name));
        if (iter == owner->node->functions.end()) {
            PyErr_SetString(PyExc_AttributeError, "function not found");
            return nullptr;
        }
        const auto &fn = iter->second;

        Py_ssize_t argc = PyTuple_Size(args);
        if (argc < 0) {
            return nullptr;
        }
        if (static_cast<uint32_t>(argc) != fn.argsNum) {
            PyErr_Format(PyExc_TypeError, "expected %u arguments", fn.argsNum);
            return nullptr;
        }

        std::vector<Any> anyArgs;
        anyArgs.reserve(static_cast<size_t>(argc));
        for (Py_ssize_t i = 0; i < argc; ++i) {
            const TypeInfoRT *argInfo = i < static_cast<Py_ssize_t>(fn.argTypes.size()) ? fn.argTypes[static_cast<size_t>(i)] : nullptr;
            Any converted;
            if (!PythonToAny(PyTuple_GetItem(args, i), argInfo, converted)) {
                return nullptr;
            }
            anyArgs.push_back(std::move(converted));
        }
        if (fn.checkFn != nullptr && !fn.checkFn(anyArgs.data())) {
            PyErr_SetString(PyExc_TypeError, "argument type mismatch");
            return nullptr;
        }

        Any result = fn.memberFun(owner->value.Data(), anyArgs.data());
        return AnyToPython(result);
    }

    void FunctionDealloc(PyObject *selfObj)
    {
        auto *self = reinterpret_cast<PySkyFunction *>(selfObj);
        Py_XDECREF(self->owner);
        delete self->name;
        Py_TYPE(selfObj)->tp_free(selfObj);
    }

    PyObject *ObjectGetAttro(PyObject *selfObj, PyObject *nameObj)
    {
        PyObject *result = PyObject_GenericGetAttr(selfObj, nameObj);
        if (result != nullptr) {
            return result;
        }
        if (!PyErr_ExceptionMatches(PyExc_AttributeError)) {
            return nullptr;
        }
        PyErr_Clear();

        auto *self = reinterpret_cast<PySkyObject *>(selfObj);
        const char *name = PyUnicode_AsUTF8(nameObj);
        if (name == nullptr) {
            return nullptr;
        }

        if (self->node != nullptr) {
            auto iter = self->node->functions.find(std::string_view(name));
            if (iter != self->node->functions.end()) {
                auto *fn = PyObject_New(PySkyFunction, &PySkyFunctionType);
                if (fn == nullptr) {
                    return nullptr;
                }
                Py_INCREF(selfObj);
                fn->owner = selfObj;
                fn->name = new std::string(name);
                return reinterpret_cast<PyObject *>(fn);
            }
        }

        PyErr_Format(PyExc_AttributeError, "'%s' object has no attribute '%s'", Py_TYPE(selfObj)->tp_name, name);
        return nullptr;
    }

    PyTypeObject *GetOrCreateType(const TypeNode *node)
    {
        if (node == nullptr || node->info == nullptr) {
            return nullptr;
        }

        auto cached = g_typeCache.find(node->info->registeredId);
        if (cached != g_typeCache.end()) {
            return cached->second;
        }

        auto *name = new std::string("sky." + std::string(node->info->name));
        auto *getset = new std::vector<PyGetSetDef>();
        auto *closures = new std::vector<MemberClosure *>();

        for (const auto &entry : node->members) {
            const auto &member = entry.second;
            if (member.getterConstFn == nullptr) {
                continue;
            }
            auto *closure = new MemberClosure{ &member };
            closures->push_back(closure);

            PyGetSetDef def{};
            def.name = const_cast<char *>(entry.first.data());
            def.get = MemberGet;
            def.set = member.setterFn != nullptr ? MemberSet : nullptr;
            def.closure = closure;
            getset->push_back(def);
        }
        getset->push_back(PyGetSetDef{nullptr, nullptr, nullptr, nullptr, nullptr});

        g_specNames.push_back(name);
        g_getsetArrays.push_back(getset);
        g_closureArrays.push_back(closures);

        PyType_Slot slots[6];
        slots[0] = {Py_tp_new, reinterpret_cast<void *>(ObjectNew)};
        slots[1] = {Py_tp_init, reinterpret_cast<void *>(ObjectInit)};
        slots[2] = {Py_tp_dealloc, reinterpret_cast<void *>(ObjectDealloc)};
        slots[3] = {Py_tp_getattro, reinterpret_cast<void *>(ObjectGetAttro)};
        slots[4] = {Py_tp_getset, getset->data()};
        slots[5] = {0, nullptr};

        PyType_Spec spec{};
        spec.name = name->c_str();
        spec.basicsize = sizeof(PySkyObject);
        spec.itemsize = 0;
        spec.flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
        spec.slots = slots;

        auto *type = reinterpret_cast<PyTypeObject *>(PyType_FromSpec(&spec));
        if (type == nullptr) {
            return nullptr;
        }

        g_typeCache[node->info->registeredId] = type;
        g_typeNodes[type] = node;

        for (const auto &entry : node->enums) {
            PyRef value(PyLong_FromUnsignedLongLong(entry.first));
            if (value) {
                PyObject_SetAttrString(reinterpret_cast<PyObject *>(type), entry.second.data(), value.Get());
            }
        }

        return type;
    }

    PyObject *SkyTypes(PyObject *self, PyObject *args)
    {
        const auto &all = SerializationContext::Get()->GetAllTypes();
        PyRef list(PyList_New(0));
        if (!list) {
            return nullptr;
        }
        for (const auto &entry : all) {
            PyRef name(PyUnicode_FromStringAndSize(entry.first.data(), static_cast<Py_ssize_t>(entry.first.size())));
            if (!name) {
                return nullptr;
            }
            if (PyList_Append(list.Get(), name.Get()) != 0) {
                return nullptr;
            }
        }
        return list.Release();
    }

    PyObject *SkyType(PyObject *self, PyObject *args)
    {
        const char *name = nullptr;
        if (!PyArg_ParseTuple(args, "s", &name)) {
            return nullptr;
        }
        const TypeNode *node = SerializationContext::Get()->FindType(name);
        if (node == nullptr) {
            PyErr_Format(PyExc_KeyError, "type '%s' is not registered", name);
            return nullptr;
        }
        PyTypeObject *type = GetOrCreateType(node);
        if (type == nullptr) {
            return nullptr;
        }
        Py_INCREF(type);
        return reinterpret_cast<PyObject *>(type);
    }

    PyObject *SkyMake(PyObject *self, PyObject *args)
    {
        Py_ssize_t argc = PyTuple_Size(args);
        if (argc < 1) {
            PyErr_SetString(PyExc_TypeError, "make(name, *args)");
            return nullptr;
        }
        const char *name = PyUnicode_AsUTF8(PyTuple_GetItem(args, 0));
        if (name == nullptr) {
            return nullptr;
        }
        const TypeNode *node = SerializationContext::Get()->FindType(name);
        if (node == nullptr) {
            PyErr_Format(PyExc_KeyError, "type '%s' is not registered", name);
            return nullptr;
        }
        PyTypeObject *type = GetOrCreateType(node);
        if (type == nullptr) {
            return nullptr;
        }

        PyRef ctorArgs(PyTuple_New(argc - 1));
        if (!ctorArgs) {
            return nullptr;
        }
        for (Py_ssize_t i = 1; i < argc; ++i) {
            PyObject *item = PyTuple_GetItem(args, i);
            Py_INCREF(item);
            PyTuple_SetItem(ctorArgs.Get(), i - 1, item);
        }
        return PyObject_CallObject(reinterpret_cast<PyObject *>(type), ctorArgs.Get());
    }

    PyMethodDef g_skyMethods[] = {
        {"types", SkyTypes, METH_NOARGS, "list registered type names"},
        {"type", SkyType, METH_VARARGS, "get a reflected type by name"},
        {"make", SkyMake, METH_VARARGS, "construct a reflected value by type name"},
        {nullptr, nullptr, 0, nullptr},
    };

    PyModuleDef g_skyModule = {
        PyModuleDef_HEAD_INIT,
        "sky",
        "SkyEngine reflection bindings",
        -1,
        g_skyMethods,
    };

} // namespace

    bool InitializeReflectionBindings()
    {
        SerializationContext::Get();
        SKY_ASSERT(TypeInfo<float>::RegisteredId() && TypeInfo<int32_t>::RegisteredId());

        PySkyFunctionType.tp_name = "sky.function";
        PySkyFunctionType.tp_basicsize = sizeof(PySkyFunction);
        PySkyFunctionType.tp_dealloc = FunctionDealloc;
        PySkyFunctionType.tp_call = FunctionCall;
        PySkyFunctionType.tp_flags = Py_TPFLAGS_DEFAULT;
        if (PyType_Ready(&PySkyFunctionType) < 0) {
            LOG_E(TAG, "failed to ready function type");
            return false;
        }

        PySkySequenceType.tp_name = "sky.sequence";
        PySkySequenceType.tp_basicsize = sizeof(PySkySequence);
        PySkySequenceType.tp_dealloc = SequenceDealloc;
        PySkySequenceType.tp_flags = Py_TPFLAGS_DEFAULT;
        PySkySequenceType.tp_methods = g_sequenceMethods;
        PySkySequenceType.tp_as_sequence = &g_sequenceAsSequence;
        PySkySequenceType.tp_as_mapping = &g_sequenceAsMapping;
        PySkySequenceType.tp_iter = PySeqIter_New;
        if (PyType_Ready(&PySkySequenceType) < 0) {
            LOG_E(TAG, "failed to ready sequence type");
            return false;
        }

        PyObject *module = PyModule_Create(&g_skyModule);
        if (module == nullptr) {
            LOG_E(TAG, "failed to create sky module");
            return false;
        }
        if (PyDict_SetItemString(PyImport_GetModuleDict(), "sky", module) != 0) {
            Py_DECREF(module);
            LOG_E(TAG, "failed to register sky module");
            return false;
        }
        Py_DECREF(module);
        return true;
    }

    void ShutdownReflectionBindings()
    {
        g_typeCache.clear();
        g_typeNodes.clear();

        for (auto *name : g_specNames) {
            delete name;
        }
        g_specNames.clear();

        for (auto *getset : g_getsetArrays) {
            delete getset;
        }
        g_getsetArrays.clear();

        for (auto *closures : g_closureArrays) {
            for (auto *closure : *closures) {
                delete closure;
            }
            delete closures;
        }
        g_closureArrays.clear();
    }

} // namespace sky::py

// Optional CPython stable-ABI 3.11+ exporter. This file contains ownership and
// buffer-protocol adaptation only; native GR owns layout checks and immutable data.
#define PY_SSIZE_T_CLEAN
#include "gr/c_api.h"
#include <Python.h>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

struct Exporter {
    PyObject_HEAD gr_buffer_lease *lease;
    gr_buffer_info info;
    Py_ssize_t *shape;
    Py_ssize_t *strides;
    char *format;
};
struct State {
    PyObject *type;
};
static int getbuffer(PyObject *object, Py_buffer *out, int flags) {
    out->obj = nullptr;
    auto *self = reinterpret_cast<Exporter *>(object);
    if (!self->lease) {
        PyErr_SetString(PyExc_BufferError, "Uninitialized native buffer exporter");
        return -1;
    }
    if ((flags & PyBUF_FORMAT) == PyBUF_FORMAT && (flags & PyBUF_ND) != PyBUF_ND) {
        PyErr_SetString(PyExc_BufferError, "Typed GR views require dimensional buffer metadata");
        return -1;
    }
    if ((flags & PyBUF_WRITABLE) == PyBUF_WRITABLE) {
        PyErr_SetString(PyExc_BufferError, "GR buffer storage is immutable");
        return -1;
    }
    Py_buffer candidate{};
    candidate.buf = const_cast<std::uint8_t *>(self->info.data);
    candidate.obj = object;
    candidate.len = static_cast<Py_ssize_t>(self->info.logical_bytes);
    candidate.readonly = 1;
    candidate.itemsize = static_cast<Py_ssize_t>(self->info.item_size);
    candidate.ndim = static_cast<int>(self->info.ndim);
    candidate.shape = self->shape;
    candidate.strides = self->strides;
    candidate.format = self->format;
    if (((flags & PyBUF_C_CONTIGUOUS) == PyBUF_C_CONTIGUOUS && !PyBuffer_IsContiguous(&candidate, 'C')) ||
        ((flags & PyBUF_F_CONTIGUOUS) == PyBUF_F_CONTIGUOUS && !PyBuffer_IsContiguous(&candidate, 'F')) ||
        ((flags & PyBUF_ANY_CONTIGUOUS) == PyBUF_ANY_CONTIGUOUS && !PyBuffer_IsContiguous(&candidate, 'A')) ||
        ((flags & PyBUF_STRIDES) != PyBUF_STRIDES && !PyBuffer_IsContiguous(&candidate, 'C'))) {
        PyErr_SetString(PyExc_BufferError, "Consumer requires a contiguous buffer");
        return -1;
    }
    *out = candidate;
    if ((flags & PyBUF_FORMAT) != PyBUF_FORMAT)
        out->format = nullptr;
    if ((flags & PyBUF_ND) != PyBUF_ND) {
        out->ndim = 1;
        out->shape = nullptr;
        out->itemsize = 1;
    }
    if ((flags & PyBUF_STRIDES) != PyBUF_STRIDES)
        out->strides = nullptr;
    Py_INCREF(object);
    return 0;
}
static void destroy(PyObject *object) {
    auto *self = reinterpret_cast<Exporter *>(object);
    gr_buffer_unpin(self->lease);
    delete[] self->shape;
    delete[] self->strides;
    delete[] self->format;
    auto *type = Py_TYPE(object);
    reinterpret_cast<freefunc>(PyType_GetSlot(type, Py_tp_free))(object);
    Py_DECREF(type);
}
static PyObject *export_view(PyObject *module, PyObject *args) {
    const char *handle;
    Py_ssize_t handle_size;
    if (!PyArg_ParseTuple(args, "s#", &handle, &handle_size))
        return nullptr;
    auto *state = static_cast<State *>(PyModule_GetState(module));
    auto *self =
        reinterpret_cast<Exporter *>(PyType_GenericAlloc(reinterpret_cast<PyTypeObject *>(state->type), 0));
    if (!self)
        return nullptr;
    try {
        char *response = nullptr;
        std::size_t size = 0;
        const int status = gr_buffer_pin(handle, static_cast<std::size_t>(handle_size), &self->lease,
                                         &self->info, &response, &size);
        if (status != 0) {
            PyErr_SetString(PyExc_BufferError, response ? response : "Native buffer pin failed");
            gr_free(response);
            Py_DECREF(reinterpret_cast<PyObject *>(self));
            return nullptr;
        }
        gr_free(response);
        const auto &info = self->info;
        if (info.logical_bytes > static_cast<std::size_t>(PY_SSIZE_T_MAX) || info.ndim > 64 ||
            info.item_size > static_cast<std::size_t>(PY_SSIZE_T_MAX))
            throw std::runtime_error("Buffer layout exceeds Python size range");
        if (info.ndim) {
            self->shape = new Py_ssize_t[info.ndim];
            self->strides = new Py_ssize_t[info.ndim];
        }
        for (std::size_t i = 0; i < info.ndim; ++i) {
            if (info.shape[i] > static_cast<std::uint64_t>(PY_SSIZE_T_MAX) ||
                info.strides[i] > PY_SSIZE_T_MAX || info.strides[i] < PY_SSIZE_T_MIN)
                throw std::runtime_error("Buffer dimension exceeds Python size range");
            self->shape[i] = static_cast<Py_ssize_t>(info.shape[i]);
            self->strides[i] = static_cast<Py_ssize_t>(info.strides[i]);
        }
        std::string format = std::to_string(info.item_size) + "s";
        if (info.item_size == 1)
            format = "B";
        if (info.known_format == 1) {
            const std::uint16_t check = 1;
            const bool little = *reinterpret_cast<const std::uint8_t *>(&check) == 1;
            format = info.byte_order == (little ? 1u : 2u) ? "q" : info.byte_order == 1 ? "<q" : ">q";
        }
        self->format = new char[format.size() + 1];
        std::memcpy(self->format, format.c_str(), format.size() + 1);
        PyObject *result = PyMemoryView_FromObject(reinterpret_cast<PyObject *>(self));
        Py_DECREF(reinterpret_cast<PyObject *>(self));
        return result;
    } catch (const std::exception &e) {
        PyErr_SetString(PyExc_BufferError, e.what());
        Py_DECREF(reinterpret_cast<PyObject *>(self));
        return nullptr;
    } catch (...) {
        PyErr_NoMemory();
        Py_DECREF(reinterpret_cast<PyObject *>(self));
        return nullptr;
    }
}
static PyObject *identity(PyObject *, PyObject *) {
    return PyLong_FromVoidPtr(reinterpret_cast<void *>(&gr_abi_version));
}
static PyMethodDef methods[] = {
    {"export", export_view, METH_VARARGS, "Return a readonly view owning its native lease."},
    {"library_identity", identity, METH_NOARGS, "Check exact linked native library identity."},
    {nullptr}};
static int traverse(PyObject *module, visitproc visit, void *arg) {
    auto *state = static_cast<State *>(PyModule_GetState(module));
    Py_VISIT(state->type);
    return 0;
}
static int clear(PyObject *module) {
    auto *state = static_cast<State *>(PyModule_GetState(module));
    Py_CLEAR(state->type);
    return 0;
}
static PyModuleDef definition = {PyModuleDef_HEAD_INIT,
                                 "_gr_buffer",
                                 "GR immutable native buffer export",
                                 sizeof(State),
                                 methods,
                                 nullptr,
                                 traverse,
                                 clear,
                                 nullptr};
PyMODINIT_FUNC PyInit__gr_buffer() {
    PyObject *module = PyModule_Create(&definition);
    if (!module)
        return nullptr;
    PyType_Slot slots[] = {{Py_tp_dealloc, reinterpret_cast<void *>(destroy)},
                           {Py_bf_getbuffer, reinterpret_cast<void *>(getbuffer)},
                           {0, nullptr}};
    PyType_Spec spec = {"gr_os._ImmutableBufferExporter", sizeof(Exporter), 0,
                        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION, slots};
    auto *state = static_cast<State *>(PyModule_GetState(module));
    state->type = PyType_FromSpec(&spec);
    if (!state->type) {
        Py_DECREF(module);
        return nullptr;
    }
    return module;
}

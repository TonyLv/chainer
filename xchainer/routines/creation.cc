#include "xchainer/routines/creation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

#include "xchainer/array.h"
#include "xchainer/device.h"
#include "xchainer/dtype.h"
#include "xchainer/scalar.h"
#include "xchainer/shape.h"
#include "xchainer/strides.h"

namespace xchainer {
namespace internal {

size_t GetRequiredBytes(const Shape& shape, const Strides& strides, size_t element_size) {
    assert(shape.ndim() == strides.ndim());

    if (shape.GetTotalSize() == 0) {
        return 0;
    }

    // Calculate the distance between the first and the last element, plus single element size.
    size_t total_bytes = element_size;
    for (int8_t i = 0; i < shape.ndim(); ++i) {
        total_bytes += (shape[i] - 1) * std::abs(strides[i]);
    }
    return total_bytes;
}

Array FromHostData(const Shape& shape, Dtype dtype, const std::shared_ptr<void>& data, const Strides& strides, Device& device) {
    auto bytesize = GetRequiredBytes(shape, strides, GetElementSize(dtype));
    std::shared_ptr<void> device_data = device.FromHostMemory(data, bytesize);
    return MakeArray(shape, strides, dtype, device, device_data);
}

Array Empty(const Shape& shape, Dtype dtype, const Strides& strides, Device& device) {
    auto bytesize = GetRequiredBytes(shape, strides, GetElementSize(dtype));
    std::shared_ptr<void> data = device.Allocate(bytesize);
    return MakeArray(shape, strides, dtype, device, data);
}

}  // namespace internal

Array Empty(const Shape& shape, Dtype dtype, Device& device) {
    auto bytesize = static_cast<size_t>(shape.GetTotalSize() * GetElementSize(dtype));
    std::shared_ptr<void> data = device.Allocate(bytesize);
    return internal::MakeArray(shape, Strides{shape, dtype}, dtype, device, data);
}

Array Full(const Shape& shape, Scalar fill_value, Dtype dtype, Device& device) {
    Array array = Empty(shape, dtype, device);
    array.Fill(fill_value);
    return array;
}

Array Full(const Shape& shape, Scalar fill_value, Device& device) { return Full(shape, fill_value, fill_value.dtype(), device); }

Array Zeros(const Shape& shape, Dtype dtype, Device& device) { return Full(shape, 0, dtype, device); }

Array Ones(const Shape& shape, Dtype dtype, Device& device) { return Full(shape, 1, dtype, device); }

Array Arange(Scalar start, Scalar stop, Scalar step, Dtype dtype, Device& device) {
    // TODO(hvy): Simplify comparison if Scalar::operator== supports dtype conversion.
    if (step == Scalar{0, step.dtype()}) {
        throw XchainerError("Cannot create an arange array with 0 step size.");
    }

    // Compute the size of the output.
    auto d_start = static_cast<double>(start);
    auto d_stop = static_cast<double>(stop);
    auto d_step = static_cast<double>(step);
    if (d_step < 0) {
        std::swap(d_start, d_stop);
        d_step *= -1;
    }
    auto size = std::max(int64_t{0}, static_cast<int64_t>(std::ceil((d_stop - d_start) / d_step)));
    if (size > 2 && dtype == Dtype::kBool) {
        throw DtypeError("Cannot create an arange array of booleans with size larger than 2.");
    }

    Array out = Empty({size}, dtype, device);
    device.Arange(start, step, out);
    return out;
}

Array Arange(Scalar start, Scalar stop, Scalar step, Device& device) {
    // step may be passed by the default value 1, in which case type promotion is allowed.
    // TODO(hvy): Revisit after supporting type promotion.
    bool start_stop_any_float = GetKind(start.dtype()) == DtypeKind::kFloat || GetKind(stop.dtype()) == DtypeKind::kFloat;
    return Arange(start, stop, step, start_stop_any_float ? Dtype::kFloat64 : step.dtype(), device);
}

Array Arange(Scalar start, Scalar stop, Dtype dtype, Device& device) { return Arange(start, stop, 1, dtype, device); }

Array Arange(Scalar stop, Dtype dtype, Device& device) { return Arange(0, stop, 1, dtype, device); }

Array Arange(Scalar stop, Device& device) { return Arange(0, stop, 1, stop.dtype(), device); }

Array EmptyLike(const Array& a, Device& device) { return Empty(a.shape(), a.dtype(), device); }

Array FullLike(const Array& a, Scalar fill_value, Device& device) { return Full(a.shape(), fill_value, a.dtype(), device); }

Array ZerosLike(const Array& a, Device& device) { return Zeros(a.shape(), a.dtype(), device); }

Array OnesLike(const Array& a, Device& device) { return Ones(a.shape(), a.dtype(), device); }

Array Copy(const Array& a) {
    // No graph will be disconnected.
    Array out = a.AsConstant({}, CopyKind::kCopy);
    assert(out.IsContiguous());
    return out;
}

}  // namespace xchainer

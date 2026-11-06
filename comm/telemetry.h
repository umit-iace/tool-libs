/** @file telemetry.h
 *
 * Copyright (c) 2026 IACE
 */
#pragma once
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <core/streams.h>
#include <core/timed.h>
#include "frameregistry.h"

/** periodic frames to pyWisp with a bandwidth summary at build time
 *
 * A frame is described once, by a struct with its id, a name and the fields
 * sent after the time stamp. `fields` hands all of them at once to the given
 * visitor, which packs them at runtime and sizes them at compile time:
 * ```
 * struct Model {
 *     struct MeasData {
 *         static constexpr uint8_t id = 15;
 *         static constexpr char name[] = "measurement";
 *         template<typename F>
 *         static auto fields(const Model &m, F &&f) {
 *             return f(m.state.x, m.meas.omega);
 *         }
 *     };
 *     using Telemetry = telemetry::Frames<Model, MeasData, ...>;
 *
 *     void init() {
 *         telemetry::every(e.during(e.RUN), 20, *this, out, Telemetry{});
 *     }
 * };
 * ```
 * Fields are arithmetic, fixed size arrays, fixed size Eigen matrices or
 * other trivially copyable structs. A frame with its own period defines
 * `static constexpr uint32_t period`, a frame sent elsewhere defines
 * `payload` instead of `fields`, e.g. \ref Experiment::StatusFrame.
 *
 * The summary of a link is created in a separate source file with
 * \ref TELEMETRY_SUMMARY, which also fails the build if the frames exceed the
 * link. The cmake function `tool_libs_telemetry_summary(target source)`
 * compiles that file with the settings of the target, without linking it, and
 * prints the summary after each build.
 */
namespace telemetry {

/// framing of a frame by Min::Out, 3 header bytes, id, length, crc32, eof,
/// stuff bytes are rare and not counted
inline constexpr std::size_t MIN_OVERHEAD = 3 + 1 + 1 + 4 + 1;

/** the link the frames are sent over */
struct Link {
    char name[16];
    uint32_t capacity; ///< [B/s]
    uint32_t overhead; ///< [B] per frame
    uint32_t maxLoad; ///< [‰] of the capacity the frames may use
};

namespace detail {
constexpr Link link(const char *name, uint32_t capacity, uint32_t overhead, uint32_t maxLoad) {
    Link l{{}, capacity, overhead, maxLoad};
    for (std::size_t i = 0; i + 1 < sizeof(l.name) && name[i]; ++i) l.name[i] = name[i];
    return l;
}
}

/// serial link 8N1, 10 bits per byte, the queue in front of it drops frames when it is full
constexpr Link uart(uint32_t baud, uint32_t maxLoad = 500) {
    return detail::link("UART 8N1", baud / 10, MIN_OVERHEAD, maxLoad);
}

/// network link with one datagram per frame (IPv4 & UDP header: 28 bytes)
constexpr Link udp(uint32_t bytesPerSecond, uint32_t maxLoad = 500) {
    return detail::link("UDP", bytesPerSecond, MIN_OVERHEAD + 28, maxLoad);
}

/// periodic frames of `Owner`
template<typename Owner, typename... F>
struct Frames {};

/// fixed size Eigen matrices, detected without depending on Eigen
template<typename T>
concept FixedMatrix = requires(const T &v) {
    typename T::Scalar;
    { T::SizeAtCompileTime } -> std::convertible_to<int>;
    v.data();
};

/// [B] of a field as packed by \ref pack
template<typename T>
constexpr std::size_t size() {
    if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>) {
        return sizeof(T);
    } else if constexpr (std::is_array_v<T>) {
        return std::extent_v<T> * size<std::remove_extent_t<T>>();
    } else if constexpr (FixedMatrix<T>) {
        static_assert(T::SizeAtCompileTime > 0, "only fixed size matrices can be sent");
        return T::SizeAtCompileTime * sizeof(typename T::Scalar);
    } else {
        static_assert(std::is_trivially_copyable_v<T>, "field can not be sent as it is");
        return sizeof(T);
    }
}

/// pack a field into the frame, elementwise for arrays & matrices
template<typename T>
void pack(Frame &f, const T &v) {
    if constexpr (std::is_array_v<T>) {
        for (const auto &x: v) pack(f, x);
    } else if constexpr (FixedMatrix<T>) {
        static_assert(T::SizeAtCompileTime > 0, "only fixed size matrices can be sent");
        for (auto *x = v.data(); x != v.data() + T::SizeAtCompileTime; ++x) f.pack(*x);
    } else {
        f.pack(v);
    }
}

/// visitor: payload with the time stamp
struct Payload {
    template<typename... T>
    constexpr auto operator()(const T &...) const {
        return std::integral_constant<std::size_t, sizeof(uint32_t) + (size<T>() + ... + 0)>{};
    }
};

/// visitor: packs the fields
struct Packer {
    Frame &f;
    template<typename... T>
    void operator()(const T &... v) const {
        (pack(f, v), ...);
    }
};

/// payload of a frame of `Owner` [B]
template<typename Owner, typename F>
constexpr std::size_t payload() {
    if constexpr (requires { F::payload; }) {
        return F::payload;
    } else {
        return decltype(F::fields(std::declval<const Owner &>(), Payload{}))::value;
    }
}

/// period of a frame [ms], `ms` if it has none of its own
template<typename F>
constexpr uint32_t period(uint32_t ms) {
    if constexpr (requires { F::period; }) {
        return F::period;
    } else {
        return ms;
    }
}

/// pack and send a frame of `owner` with the time stamp
template<typename Owner, typename F>
void send(const void *owner, Sink<Frame> &out, uint32_t time) {
    Frame f{F::id};
    f.pack(time);
    F::fields(*static_cast<const Owner *>(owner), Packer{f});
    out.push(std::move(f));
}

/** scheduled sender, one type for all frames keeps the code small */
struct Sender {
    const void *owner;
    Sink<Frame> &out;
    void (*fn)(const void *, Sink<Frame> &, uint32_t);
    void send(uint32_t time, uint32_t) {
        fn(owner, out, time);
    }
};

/** register the frames to be sent every `ms` (or their own period)
 *
 * the senders live as long as the program, like the registrations
 */
template<typename Owner, typename... F>
void every(Schedule::Recurring::Registry &reg, uint32_t ms, const Owner &owner,
           Sink<Frame> &out, Frames<Owner, F...>) {
    (reg.every(period<F>(ms), *new Sender{&owner, out, &send<Owner, F>}, &Sender::send), ...);
}

/** a frame in the summary, plain data read from the object file */
struct Entry {
    char name[12];
    uint32_t id;
    uint32_t period; ///< [ms]
    uint32_t bytes; ///< [B] including the framing of the link

    constexpr double rate() const {
        return bytes * 1000.0 / period;
    }
};

/** summary of the frames of a link, see \ref TELEMETRY_SUMMARY */
template<std::size_t N>
struct Summary {
    Link link;
    uint32_t count;
    Entry entries[N];

    /// [B/s] of all frames
    constexpr double total() const {
        double sum = 0;
        for (const auto &e: entries) sum += e.rate();
        return sum;
    }
    constexpr bool fits() const {
        return total() * 1000 <= double(link.capacity) * link.maxLoad;
    }
};

namespace detail {
template<typename Owner, typename F>
constexpr Entry entry(const Link &link, uint32_t ms) {
    Entry e{{}, F::id, period<F>(ms), static_cast<uint32_t>(payload<Owner, F>() + link.overhead)};
    for (std::size_t i = 0; i + 1 < sizeof(e.name) && F::name[i]; ++i) e.name[i] = F::name[i];
    return e;
}
}

/// summary of the frames, further frames (e.g. \ref Experiment::StatusFrame) have their own period
template<typename Owner, typename... F, typename... More>
constexpr auto summary(const Link &link, uint32_t ms, Frames<Owner, F...>, More...) {
    return Summary<sizeof...(F) + sizeof...(More)>{
        link, sizeof...(F) + sizeof...(More), {
            detail::entry<Owner, F>(link, ms)...,
            detail::entry<void, More>(link, ms)...,
        }
    };
}
}

/** define the summary of the frames sent over a link and check that they fit
 *
 * ```
 * // telemetry.cpp
 * #include "model.h"
 * TELEMETRY_SUMMARY(telemetry::uart(921600), 20, Model::Telemetry{}, Experiment::StatusFrame{})
 * ```
 * the symbol name is read by the cmake function `tool_libs_telemetry_summary`
 */
#define TELEMETRY_SUMMARY(link, ms, ...) \
    extern "C" constexpr auto telemetrySummary = telemetry::summary(link, ms, __VA_ARGS__); \
    static_assert(telemetrySummary.fits(), \
                  "telemetry exceeds the link, raise its capacity or the periods");

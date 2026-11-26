/**
 * @file IFE_Failure.hpp
 * @brief How a failure leaves IFE: thrown inside the module, returned as an
 *        `Iris::Result` at its boundary (development/style_guide.md, Errors).
 * @copyright Iris Developers, 2025-2026
 *
 * A read deep in the block graph throws a `Failure` at the point it fails,
 * carrying IFE's `FileError`, the offending value and the location of the
 * throw; RAII unwinds whatever it held. Every public function wraps its body
 * in `boundary()`, which catches by type and returns the `Result`, so no
 * exception crosses the API and a consumer reads the code with
 * `result.code_as<FileError>()`.
 *
 * Private to src/: never installed, never included by a public header.
 */
#ifndef IFE_Failure_hpp
#define IFE_Failure_hpp

#include <concepts>
#include <exception>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "IrisFileExtension.hpp"   // FileError, Iris::Result

namespace Iris::File {

/// Every failure this library reports: its own domain and code, the message,
/// and where it was made.
inline Result failure(FileError code, std::string message,
                      std::source_location where = std::source_location::current()) noexcept {
    return Result::failure(Domain::IRIS_FILE_EXTENSION, code, std::move(message), where);
}

/// A failure thrown inside the module. Derived from std::runtime_error so its
/// message is copied without allocating, as an exception's must be.
class Failure : public std::runtime_error {
public:
    Failure(FileError code, const std::string& message,
            std::source_location where = std::source_location::current())
        : std::runtime_error(message), _code(code), _where(where) {}
    /// A Result another boundary returned, rethrown with its code and place.
    explicit Failure(const Result& result)
        : Failure(result.code_as<FileError>(), result.message, result.where) {}

    [[nodiscard]] Result result() const noexcept { return failure(_code, what(), _where); }

private:
    FileError            _code;
    std::source_location _where;
};

/**
 * @brief Run @p body as the boundary of the public function @p name.
 *
 * Success when the body returns; otherwise the failure it threw, caught by
 * type: a `Failure` keeps its code and place; `std::bad_alloc` is
 * OUT_OF_MEMORY; any other exception is UNEXPECTED_EXCEPTION naming its
 * message and the function. The body writes its outputs only once it can no
 * longer fail, so a failed call leaves them as the caller had them.
 */
template <std::invocable Body>
Result boundary(std::string_view name, Body&& body,
                std::source_location where = std::source_location::current()) noexcept {
    try {
        std::forward<Body>(body)();
        return Result::success(where);
    } catch (const Failure& thrown) {
        return thrown.result();
    } catch (const std::bad_alloc&) {
        return failure(FileError::OUT_OF_MEMORY, std::string(name) + " ran out of memory", where);
    } catch (const std::exception& error) {
        return failure(FileError::UNEXPECTED_EXCEPTION,
                       std::string(name) + " caught an unexpected exception: " + error.what(), where);
    } catch (...) {
        // The last handler at a boundary (style guide, Errors): nothing in IFE
        // throws a type outside std::exception, so this names one that did.
        return failure(FileError::UNEXPECTED_EXCEPTION,
                       std::string(name) + " caught an exception of a type outside std::exception", where);
    }
}

}  // namespace Iris::File

#endif  // IFE_Failure_hpp

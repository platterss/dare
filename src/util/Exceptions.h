#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <stdexcept>

struct UnrecoverableException final : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct TaskCancelled final : std::exception {
    [[nodiscard]] const char* what() const noexcept override {
        return "The task was manually cancelled.";
    }
};

#endif // EXCEPTIONS_H
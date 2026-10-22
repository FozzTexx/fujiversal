#ifndef TYPES_H
#define TYPES_H

struct error_is_true {
    bool value;
    explicit error_is_true(bool v) : value(v) {}
    operator bool() const { return value; }
    bool is_success() const { return !value; }
    bool is_error()   const { return value; }
};
#define RETURN_ERROR_AS_TRUE()    return error_is_true(true)
#define RETURN_SUCCESS_AS_FALSE() return error_is_true(false)
#define RETURN_ERROR_IF(expr)     return error_is_true(expr)

#endif /* TYPES_H */

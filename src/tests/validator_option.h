#ifndef ARGGEN_VALIDATOR_OPTION_H
#define ARGGEN_VALIDATOR_OPTION_H

// WARNING: Automatically generated code by arggen.py. Do not edit.

#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>


class ArgError : public std::runtime_error {
public:
    ArgError(const std::string &msg) : std::runtime_error(msg) {}
};


struct ValidatorOption {
    // options: ('-c', '--comment'), arg_type: ArgType.BOOL
    bool comment = false;
    // options: ('files',), arg_type: ArgType.REST
    std::vector<std::string> files;
    // options: ('-i', '--interactive'), arg_type: ArgType.BOOL
    bool interactive = false;

    std::string to_string() const;
    bool operator==(const ValidatorOption &rhs) const;
    bool operator!=(const ValidatorOption &rhs) const;
    static ValidatorOption parse_args(const std::vector<std::string> &args);
    static ValidatorOption parse_argv(int argc, const char *const argv[]);
};

#endif // ARGGEN_VALIDATOR_OPTION_H

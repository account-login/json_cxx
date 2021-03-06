#include <sstream>

#include "helper.h"


using std::ostringstream;


vector<Token::Ptr> get_tokens(const ustring &str) {
    Scanner scanner;
    for (auto ch : str) {
        scanner.feed(ch);
    }
    scanner.feed('\0');

    vector<Token::Ptr> ans;
    Token::Ptr tok;
    while ((tok = scanner.pop())) {
        ans.emplace_back(move(tok));
    }
    return ans;
}


Node::Ptr parse_string(const string &input) {
    auto tokens = get_tokens(u8_decode(input.data()));
    Parser parser;
    for (const auto &tok : tokens) {
        parser.feed(*tok);
    }
    return parser.pop_result();
}


string format_node(const Node &node, const FormatOption &opt) {
    Formatter fmtter(opt);
    ostringstream os;
    fmtter.format(os, node);
    return os.str();
}

#include <cassert>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "exceptions.h"
#include "scanner.h"
#include "node.h"
#include "utils.hpp"


using std::move;
using std::numeric_limits;
using std::pow;
using std::string;
using std::to_string;


Token *Token::clone() const {
    Token *tok = new Token(this->type);
    tok->start = this->start;
    tok->end = this->end;
    return tok;
}


string Token::name() const {
    return "Token:" + string(1, static_cast<char>(this->type));
}


string Token::repr_value() const {
    return "";
}


string Token::repr_short() const {
    string name = this->name();
    string value = this->repr_value();
    if (!value.empty()) {
        return name + " " + value;
    } else {
        return name;
    }
}


string Token::repr_full() const {
    return "<" + this->repr_short()
        + " start=" + repr(this->start) + " end=" + repr(this->end) + ">";
}


bool Token::operator==(const Token &other) const {
    return this->type == other.type;
}


bool Token::operator!=(const Token &other) const {
    return !(*this == other);
}


template<>
string TokenBool::name() const {
    return "Bool";
}


template<>
string TokenBool::repr_value() const {
    return this->value ? "true" : "false";
}


template<>
string TokenInt::name() const {
    return "Int";
}


template<>
string TokenInt::repr_value() const {
    return to_string(this->value);
}


template<>
string TokenFloat::name() const {
    return "Float";
}


template<>
string TokenFloat::repr_value() const {
    return to_string(this->value);
}


template<>
string TokenString::name() const {
    return "Str";
}


template<>
string TokenString::repr_value() const {
    return NodeString(this->value).repr();
}


template<>
string TokenComment::name() const {
    return "Comment";
}


template<>
string TokenComment::repr_value() const {
    return u8_encode(this->value);  // TODO: ...
}


void Scanner::feed(CharConf::CharType ch) {
    // TODO: limit stack depth
    this->prev_pos = this->cur_pos;
    this->cur_pos.add_char(ch);
    this->refeed(ch);
}


void Scanner::refeed(CharConf::CharType ch) {
    switch (this->state) {
        case ScannerState::INIT:
            return this->st_init(ch);
        case ScannerState::ID:
            return this->st_id(ch);
        case ScannerState::NUMBER:
            return this->st_number(ch);
        case ScannerState::STRING:
            return this->st_string(ch);
        case ScannerState::COMMENT:
            return this->st_comment(ch);
        case ScannerState::ENDED:
            return this->exception("received char in ENDED state");
    }
    assert(!"Unreachable");
}


Token::Ptr Scanner::pop() {
    if (this->buffer.empty()) {
        return Token::Ptr();
    } else {
        Token::Ptr tok = move(this->buffer.front());
        this->buffer.pop_front();
        return tok;
    }
}


void Scanner::reset() {
    *this = Scanner();
}


void Scanner::st_init(CharConf::CharType ch) {
    this->start_pos = this->cur_pos;
    if (ch == '\0') {
        Token *tok = new Token(TokenType::END);
        tok->start = this->cur_pos;
        tok->end = this->cur_pos;
        this->buffer.emplace_back(tok);
        this->state = ScannerState::ENDED;
    } else if (USTRING(" \t\n\r").find(ch) != ustring::npos) {  // space
        // pass
    } else if (USTRING("[]{},:").find(ch) != ustring::npos) {   // single char token
        Token *tok = new Token(static_cast<TokenType>(ch));
        tok->start = this->cur_pos;
        tok->end = this->cur_pos;
        this->buffer.emplace_back(tok);
    } else if (ch == '"') {
        this->state = ScannerState::STRING;
        this->refeed(ch);
    } else if (is_digit(ch) || ch == '.' || ch == '+' || ch == '-') {
        this->state = ScannerState::NUMBER;
        this->refeed(ch);
    } else if (is_alpha(ch)) {
        this->state = ScannerState::ID;
        this->refeed(ch);
    } else if (ch == '/') {
        this->state = ScannerState::COMMENT;
    } else {
        this->unknown_char(ch);
    }
}


void Scanner::st_id(CharConf::CharType ch) {
    // TODO: limit length
    if (is_alpha(ch)) {
        this->id_state.value.push_back(ch);
    } else {
        Token *tok = nullptr;
        if (this->id_state.value == USTRING("null")) {
            tok = new Token(TokenType::NIL);
        } else if (this->id_state.value == USTRING("true")) {
            tok = new TokenBool(true);
        } else if (this->id_state.value == USTRING("false")) {
            tok = new TokenBool(false);
        } else {
            assert(!this->id_state.value.empty());
        }

        if (tok != nullptr) {
            tok->start = this->start_pos;
            tok->end = this->prev_pos;
            this->buffer.emplace_back(tok);
            // reset
            this->id_state = IdState();
            this->state = ScannerState::INIT;
            this->refeed(ch);
        } else {
            this->exception(
                "bad identifier: '" + u8_encode(this->id_state.value) + "', "
                "expect null|true|false",
                this->start_pos, this->prev_pos
            );
        }
    }
}


void Scanner::st_number(CharConf::CharType ch) {
    // TODO: limit length
    NumberState &ns = this->num_state;
    if (ns.state == NumberSubState::INIT) {
        ns.state = NumberSubState::SIGNED;
        if (ch == '-') {
            ns.num_sign = -1;
        } else {
            this->st_number(ch);
        }
    } else if (ns.state == NumberSubState::SIGNED) {
        if (ch == '0') {
            ns.state = NumberSubState::ZEROED;
        } else if (is_digit(ch)) {
            ns.int_digits.push_back(static_cast<char>(ch));
            ns.state = NumberSubState::INT_DIGIT;
        } else {
            this->unknown_char(ch, "expect digit");
        }
    } else if (ns.state == NumberSubState::ZEROED) {
        if (ch == '.') {
            ns.state = NumberSubState::DOTTED;
        } else if (ch == 'e' || ch == 'E') {
            ns.state = NumberSubState::EXP;
        } else {
            this->finish_num(ch);
        }
    } else if (ns.state == NumberSubState::INT_DIGIT) {
        if (is_digit(ch)) {
            ns.int_digits.push_back(static_cast<char>(ch));
        } else if (ch == '.') {
            ns.state = NumberSubState::DOTTED;
        } else if (ch == 'e' || ch == 'E') {
            ns.state = NumberSubState::EXP;
        } else {
            this->finish_num(ch);
        }
    } else if (ns.state == NumberSubState::DOTTED) {
        if (is_digit(ch)) {
            ns.dot_digits.push_back(static_cast<char>(ch));
            ns.state = NumberSubState::DOT_DIGIT;
        } else {
            this->unknown_char(ch, "expect digit");
        }
    } else if (ns.state == NumberSubState::DOT_DIGIT) {
        if (is_digit(ch)) {
            ns.dot_digits.push_back(static_cast<char>(ch));
        } else if (ch == 'e' || ch == 'E') {
            ns.state = NumberSubState::EXP;
        } else {
            this->finish_num(ch);
        }
    } else if (ns.state == NumberSubState::EXP) {
        if (ch == '+' || ch == '-') {
            ns.state = NumberSubState::EXP_SIGNED;
            if (ch == '-') {
                ns.exp_sign = -1;
            }
        } else if (is_digit(ch)) {
            ns.exp_digits.push_back(static_cast<char>(ch));
            ns.state = NumberSubState::EXP_DIGIT;
        } else {
            this->unknown_char(ch, "expect digit or sign");
        }
    } else if (ns.state == NumberSubState::EXP_SIGNED) {
        if (is_digit(ch)) {
            ns.exp_digits.push_back(static_cast<char>(ch));
            ns.state = NumberSubState::EXP_DIGIT;
        } else {
            this->unknown_char(ch, "expect digit");
        }
    } else if (ns.state == NumberSubState::EXP_DIGIT) {
        if (is_digit(ch)) {
            ns.exp_digits.push_back(static_cast<char>(ch));
        } else {
            this->finish_num(ch);
        }
    } else {
        assert(!"Unreachable");
    }
}


void Scanner::st_string(CharConf::CharType ch) {
    // TODO: limit length
    StringState &ss = this->string_state;
    if (ss.state == StringSubState::INIT) {
        if (ch != '"') {
            this->unknown_char(ch, "expect double quote");
        } else {
            ss.state = StringSubState::NORMAL;
        }
    } else if (ss.state == StringSubState::NORMAL) {
        if (ch == '"') {
            Token *tok = new TokenString(ss.value);
            tok->start = this->start_pos;
            tok->end = this->cur_pos;
            this->buffer.emplace_back(tok);
            // reset
            this->string_state = StringState();
            this->state = ScannerState::INIT;
        } else if (ch == '\\') {
            ss.state = StringSubState::ESCAPE;
        } else if (ch < 0x20) {
            this->unknown_char(ch, "unescaped control char");
        } else {
            ss.value.push_back(ch);
        }
    } else if (ss.state == StringSubState::ESCAPE) {
        auto it = Scanner::escape_map.find(ch);
        if (it != Scanner::escape_map.end()) {
            ss.value.push_back(it->second);
            ss.state = StringSubState::NORMAL;
        } else if (ch == 'u') {
            ss.state = StringSubState::HEX;
        } else {
            this->unknown_char(ch, "unknown escapes");
        }
    } else if (ss.state == StringSubState::HEX) {
        if (ss.hex.size() == 4) {
            StringSubState next_state = StringSubState::NORMAL;
            unichar uch = static_cast<CharConf::CharType>(strtol(ss.hex.data(), nullptr, 16));
            if (ss.last_surrogate) {
                if (is_surrogate_low(uch)) {
                    unichar hi = ss.value.back();
                    ss.value.pop_back();
                    uch = u16_assemble_surrogate(hi, uch);
                } else {
                    this->unknown_char(uch, "expect lower surrogate");
                }
                ss.last_surrogate = false;
            } else {
                if (is_surrogate_high(uch)) {
                    ss.last_surrogate = true;
                    next_state = StringSubState::SURROGATED;
                } else if (is_surrogate_low(uch)) {
                    this->unknown_char(uch, "unexpected lower surrogate");
                }
            }

            ss.value.push_back(uch);
            ss.hex.clear();
            ss.state = next_state;
            this->refeed(ch);
        } else if (is_xdigit(ch)) {
            ss.hex.push_back(static_cast<char>(to_lower(ch)));
        } else {
            this->unknown_char(ch, "expect hex digit");
        }
    } else if (ss.state == StringSubState::SURROGATED) {
        if (ch == '\\') {
            ss.state = StringSubState::SURROGATED_ESCAPE;
        } else {
            this->unknown_char(ch, "expect lower surrogate escape");
        }
    } else if (ss.state == StringSubState::SURROGATED_ESCAPE) {
        if (ch == 'u') {
            ss.state = StringSubState::HEX;
        } else {
            this->unknown_char(ch, "expect lower surrogate escape");
        }
    } else {
        assert(!"Unreachable");
    }
}


void Scanner::st_comment(CharConf::CharType ch) {
    CommentState &cs = this->comment_state;
    switch (cs.state) {
    case CommentSubState::SLASH:
        switch (ch) {
        case '/':
            cs.state = CommentSubState::SLASH_DOUBLE;
            break;
        case '*':
            cs.state = CommentSubState::STAR_BEGIN;
            break;
        default:
            this->unknown_char(ch, "expect '/' or '*'");
        }
        break;
    case CommentSubState::SLASH_DOUBLE:
        switch (ch) {
        case '\n':
            this->finish_comment();
            break;
        case '\0':
            this->finish_comment();
            this->refeed(ch);
            break;
        default:
            cs.value.push_back(ch);
        }
        break;
    case CommentSubState::STAR_BEGIN:
        switch (ch) {
        case '*':
            cs.state = CommentSubState::STAR_MAY_END;
            break;
        case '\0':
            this->unknown_char(ch, "expect '*/'");
        default:
            cs.value.push_back(ch);
        }
        break;
    case CommentSubState::STAR_MAY_END:
        switch (ch) {
        case '/':
            this->finish_comment();
            break;
        default:
            cs.value.push_back('*');
            cs.state = CommentSubState::STAR_BEGIN;
            this->refeed(ch);
        }
        break;
    default:
        assert(!"Unreachable");
    }
}


void Scanner::finish_comment() {
    Token *tok = new TokenComment(this->comment_state.value);
    tok->start = this->start_pos;
    tok->end = this->cur_pos;
    this->buffer.emplace_back(tok);
    // reset
    this->comment_state = CommentState();
    this->state = ScannerState::INIT;
}


const map<CharConf::CharType, CharConf::CharType> Scanner::escape_map {
    {'b', '\b'},
    {'f', '\f'},
    {'n', '\n'},
    {'r', '\r'},
    {'t', '\t'},
    {'"', '"'},
    {'\\', '\\'},
    {'/', '/'},
};


void Scanner::exception(const string &msg, SourcePos start, SourcePos end) {
    if (!start.is_valid()) {
        start = this->start_pos;
    }
    if (!end.is_valid()) {
        end = this->cur_pos;
    }
    throw TokenizerError(msg, start, end);
}


void Scanner::unknown_char(CharConf::CharType ch, const string &additional) {
    string msg = "Unknown char: " + NodeString({ch}).repr();
    if (!additional.empty()) {
        msg += ", " + additional;
    }
    this->exception(msg);
}


void Scanner::finish_num(CharConf::CharType ch) {
    Token *tok = this->num_state.to_token();
    tok->start = this->start_pos;
    tok->end = this->prev_pos;
    this->buffer.emplace_back(tok);
    // reset states
    this->num_state = NumberState();
    this->state = ScannerState::INIT;
    this->refeed(ch);
}


template<class T>
static T string_to_number(const string &digits) {
    T val = 0;
    for (auto ch : digits) {
        val *= 10;
        val += ch - '0';
    }
    return val;
}


Token *NumberState::to_token() const {
    int64_t iv = string_to_number<int64_t>(this->int_digits);
    double fv = string_to_number<double>(this->int_digits);

    fv += string_to_number<double>(this->dot_digits) \
        * pow(10, -static_cast<double>(this->dot_digits.size()));

    if (!this->exp_digits.empty()) {
        double exp = string_to_number<double>(this->exp_digits) * this->exp_sign;   // inf
        fv *= pow(10, exp);     // inf
        iv *= pow(10, exp);
    }

    iv *= this->num_sign;
    fv *= this->num_sign;

    if (this->dot_digits.empty() && this->exp_sign > 0
        && numeric_limits<int64_t>::min() < fv && fv < numeric_limits<int64_t>::max())
    {
        return new TokenInt(iv);
    } else {
        return new TokenFloat(fv);  // TODO: handle inf
    }
}

#ifndef JSON_CXX_PARSER_H
#define JSON_CXX_PARSER_H

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "scanner.h"
#include "unicode.h"


using std::move;
using std::string;
using std::unique_ptr;
using std::vector;


enum class NodeType {
    NIL,
    BOOL,
    INT,
    FLOAT,
    STRING,
    LIST,
    PAIR,
    OBJECT,
};


struct Node {
    typedef unique_ptr<Node> Ptr;

    explicit Node(NodeType type) : type(type) {}
    virtual ~Node() {}
    virtual Node *clone() const = 0;
    virtual bool operator==(const Node &other) const = 0;
    virtual bool operator!=(const Node &other) const;
    virtual string repr(unsigned int indent = 0) const = 0;

    NodeType type;
};


REPR(Node) {
    return value.repr();
}


template<class ValueType, NodeType node_type>
struct SimpleNode : Node {
    typedef  SimpleNode<ValueType, node_type> _SelfType;
    typedef unique_ptr<_SelfType> Ptr;

    explicit SimpleNode(ValueType value) : Node(node_type), value(value) {}

    virtual bool operator==(const Node &other) const {
        const _SelfType *node = dynamic_cast<const _SelfType *>(&other);
        return node != nullptr && this->value == node->value;
    }

    virtual string repr(unsigned int indent = 0) const {
        return string(indent * 4, ' ') + ::repr(this->value);
    }

    virtual _SelfType *clone() const {
        return new _SelfType(this->value);
    }

    ValueType value;
};


typedef SimpleNode<bool, NodeType::BOOL> NodeBool;
typedef SimpleNode<int64_t, NodeType::INT> NodeInt;
typedef SimpleNode<double, NodeType::FLOAT> NodeFloat;
typedef SimpleNode<ustring, NodeType::STRING> NodeString;


#define NODE_COMMON_DECL(node_type) \
    typedef unique_ptr<node_type> Ptr; \
    virtual bool operator==(const Node &other) const; \
    virtual string repr(unsigned int indent = 0) const; \
    virtual node_type *clone() const


struct NodeNull : Node {
    NodeNull() : Node(NodeType::NIL) {}
    NODE_COMMON_DECL(NodeNull);
};


struct NodeList : Node {
    NodeList() : Node(NodeType::LIST) {}
    NODE_COMMON_DECL(NodeList);

    vector<Node::Ptr> value;
};


struct NodePair : Node {
    NodePair(NodeString::Ptr &&key, Node::Ptr &&value)
        : Node(NodeType::PAIR) , key(move(key)), value(move(value))
    {}
    NODE_COMMON_DECL(NodePair);

    NodeString::Ptr key;
    Node::Ptr value;
};


struct NodeObject : Node {
    NodeObject() : Node(NodeType::OBJECT) {}
    NODE_COMMON_DECL(NodeObject);

    vector<NodePair::Ptr> pairs;
};


#undef NODE_COMMON_DECL


template<class NodeClass>
typename NodeClass::Ptr clone_node(const Node &node) {
    NodeClass *cloned = dynamic_cast<NodeClass *>(node.clone());
    return typename NodeClass::Ptr(cloned);
}


enum class ParserState {
    JSON,
    JSON_END,
    STRING,
    LIST,
    LIST_END,
    PAIR,
    PAIR_END,
    OBJECT,
    OBJECT_END,
};


class Parser {
public:
    Parser();
    void feed(const Token &tok);
    Node::Ptr pop_result();
    bool is_finished() const;

private:
    vector<ParserState> states;
    vector<Node::Ptr> nodes;

    void unexpected_token(const Token &tok, const vector<TokenType> &expected);
    void enter_json();
    void enter_list();
    void enter_list_item();
    void enter_object();
    void enter_object_item();
    void enter_pair();

    template<class Tokenclass, class NodeClass>
    void handle_simple_token(const Token &tok) {
        NodeClass *node = new NodeClass(reinterpret_cast<const Tokenclass &>(tok).value);
        this->nodes.emplace_back(node);
        this->states.pop_back();
    }
};


#endif //JSON_CXX_PARSER_H

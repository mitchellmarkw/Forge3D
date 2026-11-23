// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Variables.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <numbers>
#include <set>
#include <stdexcept>

namespace forge::variables {

namespace {

const std::set<std::string>& reserved()
{
    static const std::set<std::string> words{"min", "max", "abs", "sqrt", "round", "floor", "ceil", "pi"};
    return words;
}

bool nameStart(char c)
{
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool namePart(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

std::string lower(std::string s)
{
    for (char& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

class Parser {
public:
    Parser(const std::string& text, const std::function<std::optional<double>(const std::string&)>& lookup)
        : m_text(text), m_lookup(lookup)
    {
    }

    double run()
    {
        skip();
        if (m_pos < m_text.size() && m_text[m_pos] == '=') {
            ++m_pos;
            skip();
        }
        if (m_pos >= m_text.size())
            throw std::invalid_argument("The formula is empty.");
        const double v = sum();
        skip();
        if (m_pos < m_text.size())
            throw std::invalid_argument("I don't understand \"" + m_text.substr(m_pos) + "\" in the formula.");
        if (!std::isfinite(v))
            throw std::invalid_argument("The formula doesn't work out to a number (dividing by 0?).");
        return v;
    }

private:
    void skip()
    {
        while (m_pos < m_text.size() && std::isspace(static_cast<unsigned char>(m_text[m_pos])))
            ++m_pos;
    }
    bool eat(char c)
    {
        skip();
        if (m_pos < m_text.size() && m_text[m_pos] == c) {
            ++m_pos;
            return true;
        }
        return false;
    }
    double sum()
    {
        double v = product();
        for (;;) {
            if (eat('+'))
                v += product();
            else if (eat('-'))
                v -= product();
            else
                return v;
        }
    }
    double product()
    {
        double v = power();
        for (;;) {
            if (eat('*'))
                v *= power();
            else if (eat('/')) {
                const double d = power();
                if (d == 0.0)
                    throw std::invalid_argument("The formula divides by 0.");
                v /= d;
            } else
                return v;
        }
    }
    double power()
    {
        const double base = unary();
        if (eat('^'))
            return std::pow(base, power());  // right to left
        return base;
    }
    double unary()
    {
        if (eat('-'))
            return -unary();
        if (eat('+'))
            return unary();
        return atom();
    }
    double atom()
    {
        skip();
        if (m_pos >= m_text.size())
            throw std::invalid_argument("The formula stops part way.");
        const char c = m_text[m_pos];
        if (c == '(') {
            ++m_pos;
            const double v = sum();
            if (!eat(')'))
                throw std::invalid_argument("A bracket isn't closed.");
            return v;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            const char* start = m_text.c_str() + m_pos;
            char* end = nullptr;
            const double v = std::strtod(start, &end);
            if (end == start)
                throw std::invalid_argument("I don't understand \"" + m_text.substr(m_pos) + "\" in the formula.");
            m_pos += std::size_t(end - start);
            return v;
        }
        if (nameStart(c)) {
            const std::size_t from = m_pos;
            while (m_pos < m_text.size() && namePart(m_text[m_pos]))
                ++m_pos;
            const std::string name = m_text.substr(from, m_pos - from);
            const std::string word = lower(name);
            if (eat('(')) {
                std::vector<double> args;
                if (!eat(')')) {
                    do
                        args.push_back(sum());
                    while (eat(','));
                    if (!eat(')'))
                        throw std::invalid_argument("A bracket isn't closed after " + name + "(.");
                }
                return call(word, name, args);
            }
            if (word == "pi")
                return std::numbers::pi;
            if (reserved().count(word))
                throw std::invalid_argument(name + " needs brackets: " + name + "(...).");
            const std::optional<double> v = m_lookup(name);
            if (!v)
                throw std::invalid_argument("There's no variable called " + name + ".");
            return *v;
        }
        throw std::invalid_argument("I don't understand \"" + m_text.substr(m_pos) + "\" in the formula.");
    }
    static double call(const std::string& word, const std::string& name, const std::vector<double>& a)
    {
        const auto one = [&] {
            if (a.size() != 1)
                throw std::invalid_argument(name + "( ) takes one value.");
            return a[0];
        };
        if (word == "min" || word == "max") {
            if (a.empty())
                throw std::invalid_argument(name + "( ) needs at least one value.");
            return word == "min" ? *std::min_element(a.begin(), a.end()) : *std::max_element(a.begin(), a.end());
        }
        if (word == "abs")
            return std::abs(one());
        if (word == "sqrt") {
            const double v = one();
            if (v < 0.0)
                throw std::invalid_argument("sqrt( ) of a negative number.");
            return std::sqrt(v);
        }
        if (word == "round")
            return std::round(one());
        if (word == "floor")
            return std::floor(one());
        if (word == "ceil")
            return std::ceil(one());
        throw std::invalid_argument("There's no function called " + name + "( ).");
    }

    const std::string& m_text;
    const std::function<std::optional<double>(const std::string&)>& m_lookup;
    std::size_t m_pos = 0;
};

struct PlacementField {
    const char* key;
    const char* label;
    double Placement::*member;
};

const std::vector<PlacementField>& placementFields()
{
    static const std::vector<PlacementField> fields{
        {"at.x", "X position", &Placement::x},
        {"at.y", "Y position", &Placement::y},
        {"at.z", "Z position", &Placement::z},
        {"at.rotationX", "Turn about X", &Placement::rotationX},
        {"at.rotationY", "Turn about Y", &Placement::rotationY},
        {"at.rotationZ", "Turn about Z", &Placement::rotationZ},
    };
    return fields;
}

// A joint step's sizes (slice 81): Quick Socket, Quick Hinge and Hand Port steps.
struct StepField {
    const char* key;
    const char* label;
    double SocketFinish::*socket = nullptr;
    double HingeFinish::*hinge = nullptr;
    double PortFinish::*port = nullptr;
};

const std::vector<StepField>& stepFields()
{
    static const std::vector<StepField> fields{
        {"socket.size", "Socket size", &SocketFinish::size, nullptr, nullptr},
        {"socket.depth", "Socket depth", &SocketFinish::depth, nullptr, nullptr},
        {"socket.fit", "Socket fit", &SocketFinish::fit, nullptr, nullptr},
        {"socket.cover", "Cover over the magnet", &SocketFinish::cover, nullptr, nullptr},
        {"hinge.width", "Tab width", nullptr, &HingeFinish::width, nullptr},
        {"hinge.barrel", "Tab end", nullptr, &HingeFinish::barrel, nullptr},
        {"hinge.pinSize", "Pin diameter", nullptr, &HingeFinish::pinSize, nullptr},
        {"hinge.fit", "Hinge fit", nullptr, &HingeFinish::fit, nullptr},
        {"hinge.gap", "Gap between the parts", nullptr, &HingeFinish::gap, nullptr},
        {"hinge.lip", "Swivel lip", nullptr, &HingeFinish::lip, nullptr},
        {"port.size", "Port size", nullptr, nullptr, &PortFinish::size},
        {"port.length", "Peg length", nullptr, nullptr, &PortFinish::length},
        {"port.fit", "Port fit", nullptr, nullptr, &PortFinish::fit},
        {"port.ball", "Ball end", nullptr, nullptr, &PortFinish::ball},
    };
    return fields;
}

// The step field `key` names on this body, if it's that kind of step.
double* stepValue(Body& body, const std::string& key)
{
    if (!body.isBoolean())
        return nullptr;
    for (const auto& f : stepFields()) {
        if (key != f.key)
            continue;
        if (f.socket && body.boolean->socket)
            return &(*body.boolean->socket.*f.socket);
        if (f.hinge && body.boolean->hinge)
            return &(*body.boolean->hinge.*f.hinge);
        if (f.port && body.boolean->port)
            return &(*body.boolean->port.*f.port);
        return nullptr;
    }
    return nullptr;
}

std::optional<std::size_t> specIndex(const Body& body, const std::string& key)
{
    if (body.isBoolean())
        return std::nullopt;
    const auto& fields = specFields(body.spec);
    for (std::size_t i = 0; i < fields.size(); ++i)
        if (key == fields[i].key)
            return i;
    return std::nullopt;
}

}  // namespace

bool validName(const std::string& name)
{
    if (name.empty() || !std::isalpha(static_cast<unsigned char>(name[0])) || name.size() > 40)
        return false;
    for (char c : name)
        if (!namePart(c))
            return false;
    return !reserved().count(lower(name));
}

void checkName(const std::string& name)
{
    if (name.empty())
        throw std::invalid_argument("A variable needs a name.");
    if (!validName(name)) {
        if (reserved().count(lower(name)))
            throw std::invalid_argument("\"" + name + "\" is the name of a function; choose another.");
        throw std::invalid_argument("\"" + name +
                                    "\" can't be a name: use letters, digits and _, starting with a letter "
                                    "(PegDia, Wall_2).");
    }
}

double evaluate(const std::string& formula, const std::function<std::optional<double>(const std::string&)>& lookup)
{
    return Parser(formula, lookup).run();
}

std::vector<std::string> namesIn(const std::string& formula)
{
    std::vector<std::string> out;
    for (std::size_t i = 0; i < formula.size();) {
        if (std::isdigit(static_cast<unsigned char>(formula[i])) || formula[i] == '.') {
            // A number, with any exponent ("1e3" isn't a name).
            while (i < formula.size() && (namePart(formula[i]) || formula[i] == '.'))
                ++i;
            continue;
        }
        if (nameStart(formula[i])) {
            const std::size_t from = i;
            while (i < formula.size() && namePart(formula[i]))
                ++i;
            const std::string name = formula.substr(from, i - from);
            if (!reserved().count(lower(name)) && std::find(out.begin(), out.end(), name) == out.end())
                out.push_back(name);
            continue;
        }
        ++i;
    }
    return out;
}

std::string renamed(const std::string& formula, const std::string& from, const std::string& to)
{
    std::string out;
    for (std::size_t i = 0; i < formula.size();) {
        if (std::isdigit(static_cast<unsigned char>(formula[i])) || formula[i] == '.') {
            const std::size_t start = i;
            while (i < formula.size() && (namePart(formula[i]) || formula[i] == '.'))
                ++i;
            out += formula.substr(start, i - start);
            continue;
        }
        if (nameStart(formula[i])) {
            const std::size_t start = i;
            while (i < formula.size() && namePart(formula[i]))
                ++i;
            const std::string name = formula.substr(start, i - start);
            out += name == from ? to : name;
            continue;
        }
        out += formula[i++];
    }
    return out;
}

std::string cleaned(const std::string& formula)
{
    std::size_t a = 0, b = formula.size();
    while (a < b && std::isspace(static_cast<unsigned char>(formula[a])))
        ++a;
    if (a < b && formula[a] == '=')
        ++a;
    while (a < b && std::isspace(static_cast<unsigned char>(formula[a])))
        ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(formula[b - 1])))
        --b;
    return formula.substr(a, b - a);
}

void checkTable(const std::vector<Variable>& table)
{
    std::set<std::string> seen;
    for (const Variable& v : table) {
        checkName(v.name);
        if (!seen.insert(lower(v.name)).second)
            throw std::invalid_argument("There's more than one variable called " + v.name + ".");
    }
}

Values compute(const std::vector<Variable>& table)
{
    Values out;
    std::map<std::string, const Variable*> byName;
    for (const Variable& v : table)
        byName[v.name] = &v;
    std::set<std::string> working;
    const std::function<std::optional<double>(const std::string&)> get =
        [&](const std::string& name) -> std::optional<double> {
        if (const auto done = out.value.find(name); done != out.value.end())
            return done->second;
        if (out.problem.count(name))
            throw std::invalid_argument(name + " doesn't work out.");
        const auto found = byName.find(name);
        if (found == byName.end())
            return std::nullopt;
        if (working.count(name))
            throw std::invalid_argument(name + " depends on itself.");
        working.insert(name);
        try {
            const double v = evaluate(found->second->formula, get);
            working.erase(name);
            out.value[name] = v;
            return v;
        } catch (...) {
            working.erase(name);
            throw;
        }
    };
    for (const Variable& v : table) {
        if (out.value.count(v.name) || out.problem.count(v.name))
            continue;
        try {
            get(v.name);
        } catch (const std::exception& e) {
            out.problem[v.name] = e.what();
        }
    }
    return out;
}

bool hasField(const Body& body, const std::string& key)
{
    if (stepValue(const_cast<Body&>(body), key))
        return true;
    for (const auto& f : placementFields())
        if (key == f.key)
            return true;
    return specIndex(body, key).has_value();
}

double fieldValue(const Body& body, const std::string& key)
{
    if (const double* v = stepValue(const_cast<Body&>(body), key))
        return *v;
    for (const auto& f : placementFields())
        if (key == f.key)
            return body.placement.*f.member;
    if (const auto i = specIndex(body, key))
        return specValue(body.spec, *i);
    throw std::invalid_argument("This shape has no " + key + ".");
}

void setFieldValue(Body& body, const std::string& key, double value)
{
    if (double* v = stepValue(body, key)) {
        *v = value;
        return;
    }
    for (const auto& f : placementFields())
        if (key == f.key) {
            body.placement.*f.member = value;
            return;
        }
    if (const auto i = specIndex(body, key)) {
        setSpecValue(body.spec, *i, value);
        return;
    }
    throw std::invalid_argument("This shape has no " + key + ".");
}

std::string fieldLabel(const Body& body, const std::string& key)
{
    for (const auto& f : stepFields())
        if (key == f.key)
            return f.label;
    for (const auto& f : placementFields())
        if (key == f.key)
            return f.label;
    if (const auto i = specIndex(body, key))
        return specFields(body.spec)[*i].label;
    return key;
}

std::vector<std::string> dropChanged(const Body& before, Body& after)
{
    std::vector<std::string> dropped;
    for (auto it = after.formulas.begin(); it != after.formulas.end();) {
        const bool gone = !hasField(before, it->first) || !hasField(after, it->first);
        if (gone || std::abs(fieldValue(before, it->first) - fieldValue(after, it->first)) > 1e-9) {
            dropped.push_back(fieldLabel(before, it->first));
            it = after.formulas.erase(it);
        } else {
            ++it;
        }
    }
    return dropped;
}

Body applied(const Body& body, const Values& values, std::vector<std::string>* problems)
{
    Body out = body;
    const auto lookup = [&](const std::string& name) -> std::optional<double> {
        if (const auto v = values.value.find(name); v != values.value.end())
            return v->second;
        if (const auto p = values.problem.find(name); p != values.problem.end())
            throw std::invalid_argument(name + " doesn't work out: " + p->second);
        return std::nullopt;
    };
    for (const auto& [key, formula] : body.formulas) {
        try {
            if (!hasField(out, key))
                throw std::invalid_argument("it has no " + key);
            setFieldValue(out, key, evaluate(formula, lookup));
        } catch (const std::exception& e) {
            if (problems)
                problems->push_back(body.name + " (" + fieldLabel(body, key) + "): " + e.what());
        }
    }
    return out;
}

}  // namespace forge::variables

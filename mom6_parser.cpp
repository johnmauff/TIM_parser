#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/analyze.hpp>

#include <iostream>
#include <unordered_map>
#include <variant>
#include <string>

namespace pegtl = tao::pegtl;

// ============================================================
// Value type
// ============================================================

using Value = std::variant<bool, std::int64_t, double, std::string>;

struct Config {
    std::unordered_map<std::string, Value> data;
    std::string current_key;
};

// ============================================================
// Grammar
// ============================================================
// Whitespace: spaces or tabs (never consumes newline)
struct ws : pegtl::star< pegtl::sor< pegtl::one<' '>, pegtl::one<'\t'> > > {};

// Identifier
struct identifier :
    pegtl::seq<
        pegtl::sor< pegtl::alpha, pegtl::one<'_'> >,
        pegtl::star< pegtl::sor< pegtl::alnum, pegtl::one<'_'> > >
    > {};

// Match "true" case-insensitively
struct true_kw : pegtl::seq<
    pegtl::sor< pegtl::one<'T','t'> >,
    pegtl::sor< pegtl::one<'R','r'> >,
    pegtl::sor< pegtl::one<'U','u'> >,
    pegtl::sor< pegtl::one<'E','e'> >
> {};

// Match "false" case-insensitively
struct false_kw : pegtl::seq<
    pegtl::sor< pegtl::one<'F','f'> >,
    pegtl::sor< pegtl::one<'A','a'> >,
    pegtl::sor< pegtl::one<'L','l'> >,
    pegtl::sor< pegtl::one<'S','s'> >,
    pegtl::sor< pegtl::one<'E','e'> >
> {};

// Boolean
struct boolean : pegtl::sor<
    true_kw,
    false_kw,
    pegtl::sor< pegtl::one<'T','t'> >,
    pegtl::sor< pegtl::one<'F','f'> >,
    pegtl::seq< pegtl::one<'.'>, true_kw, pegtl::one<'.'> >,
    pegtl::seq< pegtl::one<'.'>, false_kw, pegtl::one<'.'> >
> {};

// Integer
//struct integer :
//    pegtl::seq< pegtl::opt< pegtl::one<'+','-'> >, pegtl::plus< pegtl::digit > > {};

// Real
//struct exponent :
//    pegtl::seq< pegtl::one<'e','E'>, pegtl::opt< pegtl::one<'+','-'> >, pegtl::plus< pegtl::digit > > {};

//struct real :
//    pegtl::seq<
//        pegtl::opt< pegtl::one<'+','-'> >,
//        pegtl::plus< pegtl::digit >,
//        pegtl::one<'.'>,
//        pegtl::plus< pegtl::digit >,
//        pegtl::opt< exponent >
//    > {};
struct sign : pegtl::one<'+','-'> {};

struct digits : pegtl::plus<pegtl::digit> {};

// decimal part like 0.001 or 123.456
struct decimal :
    pegtl::seq<
        pegtl::opt<digits>, // digits before dot optional (e.g., .001)
        pegtl::one<'.'>,
        digits              // at least one digit after dot
    > {};

// exponent part like e6 or e-06
struct exponent :
    pegtl::seq<
        pegtl::one<'e','E'>,
        pegtl::opt<sign>,
        digits
    > {};

// floating point number: integer or decimal, optionally with exponent
struct number :
    pegtl::seq<
        pegtl::sor<decimal, digits>,
        pegtl::opt<exponent>
    > {};


// String (double quoted)
struct quoted_string :
    pegtl::if_must<
        pegtl::one<'"'>,
        pegtl::until< pegtl::one<'"'> >
    > {};


// File paths
struct path_char :
    pegtl::sor<
        pegtl::alnum,
        pegtl::one<'/','_','-','.'>
    > {};

struct path :
    pegtl::plus<path_char> {};


// Value
struct value : pegtl::sor< quoted_string, number,  path, boolean > {};


// C-block comment Opening: /*
struct c_comment_open : pegtl::string<'/','*'> {};

// Closing: */
struct c_comment_close : pegtl::string<'*','/'> {};

// C-style comment: /* ... */  (spans multiple lines)
 struct c_comment : pegtl::seq<
    pegtl::string<'/','*'>,    // match /* literally
    pegtl::until< pegtl::string<'*','/'> >
> {};


// Comment start

struct comment_start : pegtl::sor<
   pegtl::one<'!','#'>,
   pegtl::string<'/','/'>
> {};
//struct comment_start : pegtl::one<'!','#'> {};


// Comment: must consume at least one character, stop at eolf or eof
struct comment :
    pegtl::sor<
        pegtl::seq< ws, comment_start, pegtl::star< pegtl::not_one< '\n', '\r' > > >,
        pegtl::seq< ws, pegtl::string<'/','*'>, pegtl::until< pegtl::string<'*','/'> > >
    > {};


// Assignment (with optional inline comment)
struct assignment :
    pegtl::seq<
        identifier, ws,
        pegtl::one<'='>,
        ws, value, ws,
        pegtl::opt<comment>
    > {};

// struct content line
struct content_line : 
    pegtl::seq< 
	pegtl::sor<assignment, comment, c_comment>, 
	pegtl::eol
    > {};

//struct blank_line :
struct blank_line :
    pegtl::eol{};

// struct normal_line
struct normal_line :
    pegtl::sor<
	content_line,
	blank_line
    >{};

// last line in the file
struct last_line :
    pegtl::sor<assignment,comment,c_comment> {};

// File
struct grammar :
    pegtl::must<
	pegtl::star<normal_line>, 
	pegtl::opt<last_line>,
	pegtl::eof 
     > {};

// ============================================================
// Actions
// ============================================================

template<typename Rule>
struct action : pegtl::nothing<Rule> {};

// Capture key
template<>
struct action<identifier> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg) {
        cfg.current_key = in.string();
    }
};

// Boolean
template<>
struct action<boolean> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg) {
        std::string s = in.string();
        bool val = (s == "True" || s == "true" || s == "T");
        cfg.data[cfg.current_key] = val;
    }
};

// Numbers
template<>
struct action<number> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg) {
        double val = std::stod(in.string());
        cfg.data[cfg.current_key] = val;
    }
};

/*

// Integer
template<>
struct action<integer> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg) {
        cfg.data[cfg.current_key] = std::stoll(in.string());
    }
};


// Real
template<>
struct action<real> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg) {
        cfg.data[cfg.current_key] = std::stod(in.string());
    }
};

*/

// String
template<>
struct action<quoted_string> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg) {
        std::string s = in.string();
        cfg.data[cfg.current_key] = s.substr(1, s.size() - 2);
    }
};

// New path action
template<>
struct action<path> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg) {
	std::string s = in.string();
	cfg.data[cfg.current_key] = s.substr(0,s.size());
    }
};

// ============================================================
// Main
// ============================================================

int main(int argc, char* argv[]) {

    if (argc != 2) {
        std::cerr << "Usage: mom6_parser <file>\n";
        return 1;
    }

    (void) tao::pegtl::analyze<grammar>();
    try {
        Config cfg;

        pegtl::file_input<> in(argv[1]);

        pegtl::parse<grammar, action>(in, cfg);

        for (const auto& [k,v] : cfg.data) {
            std::cout << k << " = ";
	    std::visit([](auto&& val){
    		using T = std::decay_t<decltype(val)>;
    		if constexpr (std::is_same_v<T,bool>) {
        	   std::cout << (val ? "True" : "False");
    		} else {
        	   std::cout << val;
    		}
	    }, v);
            std::cout << "\n";
        }

    } catch (const pegtl::parse_error& e) {
     	const auto& p = e.positions().front();
    	std::cerr << p.source << ":"
              << p.line << ":"
              << p.column << ": "
              << e.what() << "\n";
    	return 1;
    }


    return 0;
}


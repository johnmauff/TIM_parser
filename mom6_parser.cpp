#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/analyze.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <variant>

namespace pegtl = tao::pegtl;

// ============================================================
// Value type
// ============================================================

using Value = std::variant<bool, std::int64_t, double, std::string>;

// ============================================================
// Stores information read in from the MOM configuration file
// ============================================================
struct Config {
    // map: block_name -> key -> vector<string>
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::string>>> values;

    std::string current_block = "global";  // default when outside blocks
    std::string current_key;
};

// ============================================================
// Grammar
// ============================================================
// Whitespace: spaces or tabs (never consumes newline)
struct ws : 
    pegtl::star< pegtl::sor< pegtl::one<' '>, pegtl::one<'\t'> > > {};

// Identifier
struct identifier :
    pegtl::seq<
        pegtl::sor< pegtl::alpha, pegtl::one<'_'> >,
        pegtl::star< pegtl::sor< pegtl::alnum, pegtl::one<'_'> > >
    > {};

// delimiter for vector assignment
struct comma :
    pegtl::seq< ws, pegtl::one<','>, ws > {};

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

// Numbers:  The following define either floating point 
//           or integer numbers
// optional sign
struct sign : 
    pegtl::opt<pegtl::one<'+','-'>> {};

// digits
struct digits : 
    pegtl::plus<pegtl::digit> {};

// fractional part (dot with optional digits)
struct fraction : 
    pegtl::seq<pegtl::one<'.'>, pegtl::opt<digits>> {};

// exponent part (e or E, optional sign, digits)
struct exponent : 
    pegtl::seq<pegtl::one<'e','E'>, sign, digits> {};

// full floating-point number
struct number : 
    pegtl::seq<sign, digits, pegtl::opt<fraction>, pegtl::opt<exponent>> {};

// "double quoted"
struct double_quoted_string :
    pegtl::if_must< pegtl::one<'"'>, pegtl::until< pegtl::one<'"'> > > {};

// 'single quoted'
struct single_quoted_string :
    pegtl::if_must< pegtl::one<'\''>, pegtl::until< pegtl::one<'\''> > > {};

// A general quoted string
struct quoted_string :
    pegtl::sor< double_quoted_string, single_quoted_string > {};


// File paths do not need to be quoted
struct path_char :
    pegtl::sor< pegtl::alnum, pegtl::one<'/','_','-','.'> > {};

struct path : 
    pegtl::plus<path_char> {};

// Value
struct single_value : 
    pegtl::sor< quoted_string, number,  path, boolean > {};

struct value : 
    pegtl::list< single_value, comma > {};

struct value_list : 
    pegtl::list< value, comma > {};

// C-block comment Opening: /*
struct c_comment_open : 
    pegtl::string<'/','*'> {};

// Closing: */
struct c_comment_close : 
    pegtl::string<'*','/'> {};

// C-style comment: /* ... */  (spans multiple lines)
 struct c_comment :
    pegtl::seq<
    	pegtl::string<'/','*'>,    // match /* literally
    	pegtl::until< pegtl::string<'*','/'> >
    > {};


// Comment start  Supports #, !, or //
struct comment_start : 
   pegtl::sor< pegtl::one<'!','#'>, pegtl::string<'/','/'> > {};

// Comment: must consume at least one character, stop at eolf or eof
struct comment :
    pegtl::sor<
        pegtl::seq< ws, comment_start, pegtl::star< pegtl::not_one< '\n', '\r' > > >,
        pegtl::seq< ws, pegtl::string<'/','*'>, pegtl::until< pegtl::string<'*','/'> > >
    > {};

// configuration blocks
struct block_open :
    pegtl::seq<
        identifier,
        pegtl::one<'%'>
    > {};

struct block_close :
    pegtl::seq<
        pegtl::one<'%'>,
        identifier
    > {};

struct block_content :
    pegtl::until< block_close > {};

struct assignment_key : identifier {};

// Assignment (with optional inline comment)
struct assignment :
    pegtl::seq<
        assignment_key, ws,
        pegtl::one<'='>,
        ws, value, ws,
        pegtl::opt<comment>
    > {};

// struct content line
struct content_line : 
    pegtl::seq< pegtl::sor<assignment, comment, c_comment>, pegtl::eol > {};

//struct blank_line :
struct blank_line : 
    pegtl::eol{};

struct block :
    pegtl::seq<
        block_open,
	pegtl::eol,
	pegtl::star< 
	   pegtl::sor< content_line, blank_line>   // This is a normal line minus the block... So block is not recursive
	>,
        block_close,
        pegtl::opt< pegtl::eol>
    > {};

// struct normal_line
struct normal_line :
    pegtl::sor<block, content_line, blank_line >{};

// last line in the file
struct last_line :
    pegtl::sor<block, assignment, comment, c_comment> {};

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
struct action : 
    pegtl::nothing<Rule> {};

// Capture key
template<>
struct action<identifier> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg) {
        cfg.current_key = in.string();
    }
};

template<>
struct action<number> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg) {
        cfg.values[cfg.current_block][cfg.current_key].push_back(in.string());
        //cfg.values[cfg.current_block][cfg.current_key] = {in.string()};
    }
};

template<>
struct action<boolean> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg) {
        std::string val = (in.string() == "True" || in.string() == "T") ? "True" : "False";
        cfg.values[cfg.current_block][cfg.current_key].push_back(val);
    }
};

template<>
struct action<quoted_string> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg) {
        std::string s = in.string();
        if (!s.empty()) s = s.substr(1, s.size()-2); // remove quotes
        cfg.values[cfg.current_block][cfg.current_key].push_back(s);
    }
};

template<>
struct action<path> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg) {
        cfg.values[cfg.current_block][cfg.current_key].push_back(in.string());
    }
};

// blocks
template<>
struct action<block_open> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg) {
        std::string s = in.string();
        s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
        s.pop_back(); // remove %
        cfg.current_block = s;

        // Optional: initialize map for this block
        cfg.values[cfg.current_block]; 
    }
};

template<>
struct action<block_close> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg) {
        std::string s = in.string();
        s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
        s.erase(0,1); // remove leading %

        if (s != cfg.current_block) {
            throw std::runtime_error("Block mismatch: closing " + s +
                                     " but open block is " + cfg.current_block);
        }

        cfg.current_block = "global"; // reset when leaving block
    }
};

template<>
struct action<assignment_key> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg) {
        cfg.current_key = in.string();

        // Overwrite previous assignment
        cfg.values[cfg.current_block][cfg.current_key].clear();
    }
};

// ============================================================
// Main
// ============================================================
/*
*/
int main(int argc, char* argv[]) {

    if (argc < 2) {
        std::cerr << "Usage: mom6_parser <files>\n";
        return 1;
    }

    (void) tao::pegtl::analyze<grammar>();
    try {
        Config cfg;

	// Parse one or more files
	for(int i = 1; i < argc; ++i){ 
          pegtl::file_input<> in(argv[i]);
          pegtl::parse<grammar, action>(in, cfg);
        }

	// ------------------- Print all entries -------------------
	for (const auto& [block, block_map] : cfg.values) {
            std::cout << "[" << block << "]\n";
            for (const auto& [key, vec] : block_map) {
                std::cout << key << " = ";
                for (size_t i = 0; i < vec.size(); ++i) {
                    std::cout << vec[i];
                    if (i+1 < vec.size()) std::cout << ", ";
                }
                std::cout << "\n";
            }
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

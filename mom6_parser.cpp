#include <tao/pegtl.hpp>

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

namespace common {
   // ============================================================
   // Common grammar definitions
   // ============================================================
   //

   // Identifier
   struct identifier :
      pegtl::seq<
        pegtl::sor< pegtl::alpha, pegtl::one<'_'> >,
        pegtl::star< pegtl::sor< pegtl::alnum, pegtl::one<'_'> > >
      > {};

   // Different representation of boolean logicals
   struct kw_true  : pegtl::ascii::istring<'t','r','u','e'> {};
   struct kw_false : pegtl::ascii::istring<'f','a','l','s','e'> {};
   struct short_true : pegtl::sor< pegtl::ascii::one<'t'>, pegtl::ascii::one<'T'> > {};
   struct short_false : pegtl::sor< pegtl::ascii::one<'f'>, pegtl::ascii::one<'F'> > {};

   // core boolean
   struct boolean_core :
    pegtl::sor<
        kw_true,
        kw_false,
        short_true,
        short_false
    > {};
   
    // boolean
    struct boolean :
    pegtl::seq<
        pegtl::opt< pegtl::one<'.'> >,
        boolean_core,
        pegtl::opt< pegtl::one<'.'> >
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

   // Identifies the variable that is being assigned.
   //  The overwrite features of the MOM configuration files 
   //  requires special treatment of the assignment variable
   struct assignment_key : identifier {};

   // Horizontal white space.  THis does not consume 
   // an end-of-line characters
   struct hws : pegtl::star< pegtl::one<' ','\t'>> {};

   //struct blank_line :
   struct blank_line : pegtl::seq<hws,pegtl::eol> {};

}

namespace nml {

   using namespace common;

   // a single value
   struct single_value :
      pegtl::sor<quoted_string, number, boolean> {};

   // Fortran90 comment character
   struct comment :
      pegtl::seq<
         pegtl::one<'!'>,
         pegtl::until<pegtl::eol>
      > {};

   // White space that consumes both horizontal 
   // and end-of-line characters
   struct ws : 
      pegtl::star<
	pegtl::sor<
	     pegtl::one<' ', '\t'>,
	     comment,
	     pegtl::eol
	 >
       > {};

   struct block_key : identifier {};

   // Comma
   struct comma :
	   pegtl::seq<pegtl::one<','>,ws> {};

   // a list of values separated by commas
   struct value_list : 
	pegtl::seq<
           single_value,
	   pegtl::star<pegtl::seq<comma,single_value>>,
           pegtl::opt<comma>
	> {};

   // Assignment (with optional inline comment)
   struct assignment :
      pegtl::seq<
	  hws, assignment_key, hws,
          pegtl::one<'='>,
          ws, value_list, hws,
  	  pegtl::opt<comment>,
	  pegtl::opt<pegtl::eol>
      > {};

   // Namelist block opens
   struct block_open :
      pegtl::seq<
	ws,
	pegtl::one<'&'>,
        block_key,
	pegtl::opt<comment>,
	ws,
	pegtl::opt<pegtl::eol>
      > {};

   // Namelist block close
   struct block_close :
       pegtl::seq<
	 ws,
	 pegtl::one<'/'>,
	 ws,
	 pegtl::opt<comment>,
	 pegtl::opt<pegtl::eol>
       > {};

   // A namlist block contains assignments,
   // comments,  or blank lines
   struct block_content :
	pegtl::star< pegtl::sor<assignment, comment, blank_line> > {};

   // A Fortran Namelist blocks :
   struct block :
      pegtl::seq< block_open, block_content, block_close > {};

   // Namelist files can contain either a namelist block or 
   // a fortran style comment
   struct normal_line :
      pegtl::sor<block, comment>{};

   // Fortran namelist File
   struct grammar :
       pegtl::must< pegtl::star<normal_line>, pegtl::eof > {};
}

namespace MOMcfg {

   using  namespace common;

   // File paths do not need to be quoted
   struct path_char :
      pegtl::sor< pegtl::alnum, pegtl::one<'/','_','-','.'> > {};

   // Unquoted file paths are supported in the MOM confi files 
   struct path : pegtl::plus<path_char> {};

   // a single value
   struct single_value : 
      pegtl::sor< quoted_string, number,  path, boolean > {};

   // The comma acts as a delimiter for vector assignments
   struct comma :
      pegtl::seq< hws, pegtl::one<','>, hws > {};

   // a list of values separated by commas
   struct value_list : 
	pegtl::seq<
           single_value,
	   pegtl::star<pegtl::seq<comma,single_value>>
	> {};

   // C-block sytle comments
   //
   // comment opening: /*
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

   // Inline comments:
   //
   // Comment start  Supports #, !, or //
   struct comment_open : 
      pegtl::sor< pegtl::one<'!','#'>, pegtl::string<'/','/'> > {};

   // Comment: must consume at least one character, stop at eolf or eof
   struct comment :
      pegtl::sor<
        pegtl::seq< hws, comment_open, pegtl::star< pegtl::not_one< '\n', '\r' > > >,
        pegtl::seq< hws, c_comment_open, pegtl::until< c_comment_close>>
      > {};

   // Assignment (with optional inline comment)
   struct assignment :
      pegtl::seq<
        hws,assignment_key, hws,
        pegtl::one<'='>,
        hws, value_list, hws,
        pegtl::opt<comment>
      > {};

   // struct content line
   struct content_line : 
      pegtl::seq< pegtl::sor<assignment, c_comment, comment>, pegtl::eol > {};

   // configuration blocks
   struct block_open :
      pegtl::seq<
        identifier,
        pegtl::one<'%'>> {};

   struct block_close :
      pegtl::seq<
        pegtl::one<'%'>,
        identifier> {};

   struct block_content :
      pegtl::until< block_close > {};

   // Block of parameters
   struct block :
      pegtl::seq<
        block_open, pegtl::eol,
	pegtl::star<pegtl::sor< content_line, blank_line>>,  // block is not currently recursive
        block_close, pegtl::opt< pegtl::eol>
      > {};

   // struct normal_line
   struct normal_line :
      pegtl::sor<block, content_line, blank_line >{};

   // MOM_configuration File
   struct grammar :
       pegtl::must<
	  pegtl::star<normal_line>, 
	  pegtl::eof 
       > {};
}

// ============================================================
// Actions
// ============================================================

template<typename Rule>
struct action : 
    pegtl::nothing<Rule> {};

// Capture key
template<>
struct action<common::identifier> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg) {
        cfg.current_key = in.string();
    }
};

template<>
struct action<common::number> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg) {
        cfg.values[cfg.current_block][cfg.current_key].push_back(in.string());
        //cfg.values[cfg.current_block][cfg.current_key] = {in.string()};
    }
};

template<>
struct action<common::boolean> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg)
    {
        std::string s = in.string();

        // Remove optional dots
        if (!s.empty() && s.front() == '.')
            s.erase(0,1);
        if (!s.empty() && s.back() == '.')
            s.pop_back();

        // Lowercase once
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c){ return std::tolower(c); });

        bool value = (s == "t" || s == "true");

        cfg.values[cfg.current_block][cfg.current_key]
            .push_back(value ? "True" : "False");
    }
};

template<>
struct action<common::quoted_string> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg) {
        std::string s = in.string();
        if (!s.empty()) s = s.substr(1, s.size()-2); // remove quotes
        cfg.values[cfg.current_block][cfg.current_key].push_back(s);
    }
};

template<>
struct action<MOMcfg::path> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg) {
        cfg.values[cfg.current_block][cfg.current_key].push_back(in.string());
    }
};

// MOMcfg blocks
template<>
struct action<MOMcfg::block_open> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg) {
        std::string s = in.string();
        s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
        s.pop_back(); // remove trailing %
        cfg.current_block = s;

        // Optional: initialize map for this block
        cfg.values[cfg.current_block]; 
    }
};

template<>
struct action<MOMcfg::block_close> {
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

// NML  blocks
template<>
struct action<nml::block_key> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg) {
        std::string s = in.string();
        //s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
	//s.erase(0,1); // remove leading &
        cfg.current_block = s;

        // Optional: initialize map for this block
        cfg.values[cfg.current_block]; 
    }
};

template<>
struct action<nml::block_close> {
    template<typename Input>
    static void apply(const Input& in, Config& cfg) {
	(void) in;
        cfg.current_block = "global"; // reset when leaving block
    }
};




template<>
struct action<common::assignment_key> {
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


int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " [-p|--print] config_file1 [config_file2 ...]\n";
        return 1;
    }

    enum class ParseMode {None, NML, CFG};
    ParseMode mode = ParseMode::None;

    bool print_config = false;
    std::vector<std::string> files;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

	if (arg == "--nml") {
	   mode = ParseMode::NML;
	} else if (arg == "--cfg") {
	   mode = ParseMode::CFG;
	} else if (arg == "-p" || arg == "--print") {
            print_config = true;
        } else {
            files.push_back(arg);
        }
    }

    if (files.empty()) {
        std::cerr << "No configuration files provided.\n";
        return 1;
    }
    
    Config cfg;

    try {

	// Parse one or more files
	for (const auto& file : files) {
          pegtl::file_input<> in(file);
          if (mode == ParseMode::NML) {
             pegtl::parse<nml::grammar, action>(in, cfg);
	  } else {
             pegtl::parse<MOMcfg::grammar, action>(in, cfg);
	  }
        }

	if(print_config) {
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

#ifndef _4235e9ca_a704_450e_b2e1_0526cbd806ae
#define _4235e9ca_a704_450e_b2e1_0526cbd806ae

#include <upcxx/os_env.hpp>

#include <fstream>
#include <string>

namespace bench {
  // A row is a conjunction of name=value assignments. All names are const char*
  // strings, values are type T...
  template<typename ...T>
  struct row;
  
  template<>
  struct row<> {
    void emit_name_eq_val(std::ostream &o, bool comma_prefix=false) const {}
  };
  
  template<typename A, typename ...B>
  struct row<A,B...> {
    const char *name_;
    A val_;
    row<B...> tail_;

    template<typename T>
    static void emit_val(std::ostream &o, T const &val) {
      o << val;
    }
    static void emit_val(std::ostream &o, std::string const &val) {
      o << '"' << val << '"';
    }
    static void emit_val(std::ostream &o, const char *val) {
      o << '"' << val << '"';
    }
    
    void emit_name_eq_val(std::ostream &o) const {
      o << ", " << name_ << '=';
      emit_val(o, val_);
      tail_.emit_name_eq_val(o);
    }
  };
  
  // Build a row consisting of a single key=val pair. Combine multiple of these
  // with operator&.
  template<typename T>
  constexpr row<T> column(char const *name, T val) {
    return row<T>{name, val, row<>{}};
  }
  
  // operator& conjoins two rows into a result row.
  template<typename A0, typename ...A, typename ...B>
  constexpr row<A0,A...,B...> operator&(row<A0,A...> a, row<B...> b) {
    return {a.name_, a.val_, a.tail_ & b};
  }
  template<typename ...B>
  constexpr row<B...> operator&(row<> a, row<B...> b) {
    return b;
  }

  // Writes a report file consisting of emitted rows. This may not be entered
  // concurrently, so you will need to funnel your report data to a single rank
  // to write the report. Constructor reads env vars:
  //  "report_file": location to append data points (default="bench.out").
  //  "report_args": python formatted string of keyword argument assignments
  //    to be passed as additional independent variable assignments to the `emit`
  //    function.
  class report {
    std::string args, app, filename;
    std::ofstream f;
    
  public:
    report(const char *appstr/* = typically pass __FILE__*/) {
      app = std::string(appstr);
      
      std::size_t pos = app.rfind("/");
      pos = pos == std::string::npos ? 0 : pos+1;
      app = app.substr(pos);
      
      if(app.size() > 4 && app.substr(app.size()-4, 4) == ".cpp")
        app = app.substr(0, app.size()-4);
      
      args = upcxx::os_env<std::string>("report_args", "");
      filename = upcxx::os_env<std::string>("report_file", "report.out");
      
      f.open(filename, std::ofstream::app);
    }
    
    ~report() {
      std::cerr << "Report written to '" << filename << "'." << std::endl;
    }
    
    void blank() {
      f << std::endl;
    }
    
    // Given a list of dependent variable names and a row, emit a line data-point line.
    template<typename ...T>
    void emit(std::initializer_list<char const*> dependent_vars, row<T...> const &x) {
      f << "emit([";
      for(auto dep: dependent_vars)
        f << '"' << dep << "\",";
      f << ']';
      f << ", app=\"" << app << '"';
      x.emit_name_eq_val(f);
      f << ',' << args << ")" << std::endl;
    }
  };
}

#endif

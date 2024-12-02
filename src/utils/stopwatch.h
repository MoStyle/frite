// Original author is Gael Guennebaud

#ifndef __STOPWATCH_H__
#define __STOPWATCH_H__

#include <chrono>
#include <iostream>
#include <iomanip>
#include <string>
#include <stack>

#define FRITE_PROFILING 0

extern int g_stopwatch_level;
extern std::stack<double> g_stopwatch_stack;
#define SW_FUNC (std::string(__func__)+std::string("()")).c_str()
template <typename Unit=std::chrono::milliseconds>

class StopWatch {
public:
#if FRITE_PROFILING

  typedef std::chrono::system_clock clock;

  StopWatch(const char* msg = "", std::int64_t ops=0, const char* ops_unit = "Flop")
    : m_msg(msg), m_ops_unit(ops_unit), m_ops(ops), m_start(clock::now()), m_done(false)
  {
      g_stopwatch_level++;
      g_stopwatch_stack.push(0);
  }

  void stop()
  {
    double nested_sum = g_stopwatch_stack.top();
    g_stopwatch_stack.pop();
    g_stopwatch_level--;
    m_done = true;
    double d = std::chrono::duration<double>(clock::now() - m_start).count();
    if(!g_stopwatch_stack.empty())
        g_stopwatch_stack.top() += d;
    // maybe use printf? (faster)
    // std::cout.unsetf(std::ios_base::unitbuf);
    for(int k=0; k<g_stopwatch_level; ++k)
        std::cout << "  ";
    std::cout << m_msg << " " << second_to_unit(d,Unit{}) << unit_str(Unit{});
    if(nested_sum>0)
        std::cout << " (" << second_to_unit(d-nested_sum,Unit{}) << unit_str(Unit{}) << ")";
    if(m_ops)
        std::cout << std::setprecision(3) << "  ; " << m_ops/d*1e-9 << " G" << m_ops_unit << "/s";
    std::cout << "\n";
    std::cout << std::flush;
  }

  double stop_silent() 
  {
    double nested_sum = g_stopwatch_stack.top();
    g_stopwatch_stack.pop();
    g_stopwatch_level--;
    m_done = true;
    double d = std::chrono::duration<double>(clock::now() - m_start).count();
    if(!g_stopwatch_stack.empty())
        g_stopwatch_stack.top() += d;
    return second_to_unit(d,Unit{});
  }

  ~StopWatch()
  {
    if(!m_done)
      stop();
  }
protected:
  static const char* unit_str(std::chrono::microseconds) { return "µs"; }
  static const char* unit_str(std::chrono::milliseconds) { return "ms"; }
  static const char* unit_str(std::chrono::seconds)      { return "s "; }

  static double second_to_unit(double x, std::chrono::microseconds) { return x*1e6; }
  static double second_to_unit(double x, std::chrono::milliseconds) { return x*1e3; }
  static double second_to_unit(double x, std::chrono::seconds)      { return x; }

  std::string m_msg;
  std::string m_ops_unit;
  std::int64_t m_ops;
  clock::time_point m_start;
  bool m_done;

#else
    StopWatch(const char* msg = "", std::int64_t ops=0, const char* ops_unit = "Flop") {}
    void stop() {}
    double stop_silent() { return -1.0; }
#endif

};

#endif // __STOPWATCH_H__
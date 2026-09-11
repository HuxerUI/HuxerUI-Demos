#include "graph_math.h"
#include <muParser.h>
#include <kiss_fft.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

namespace graph {
namespace {
const std::set<std::string> functions{"sin", "cos", "tan", "asin", "acos", "atan", "sqrt", "abs", "exp",
    "ln", "log10", "floor", "ceil", "min", "max"};
double DegreesSin(double x) { return std::sin(x * Pi / 180); }
double DegreesCos(double x) { return std::cos(x * Pi / 180); }
double DegreesTan(double x) { return std::tan(x * Pi / 180); }
double DegreesAsin(double x) { return std::asin(x) * 180 / Pi; }
double DegreesAcos(double x) { return std::acos(x) * 180 / Pi; }
double DegreesAtan(double x) { return std::atan(x) * 180 / Pi; }
double Minimum(double a, double b) { return std::min(a, b); }
double Maximum(double a, double b) { return std::max(a, b); }
void CheckText(const std::string& text) {
  if (text.empty() || text.size() > 2048) throw std::runtime_error("Expression must contain 1–2048 characters.");
  int depth = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (c == '(' && ++depth > 48) throw std::runtime_error("Expression nesting is too deep.");
    if (c == ')') --depth;
    if (c == ',' && depth == 0) throw std::runtime_error("Enter one expression per curve.");
    if (static_cast<unsigned char>(c) >= 128 || c == '"' || c == ';' || c == '{' || c == '}' || c == '[')
      throw std::runtime_error("Use numbers, variables and supported mathematical operators.");
    if (c == '=' && (i == 0 || std::string("<>=!").find(text[i - 1]) == std::string::npos) &&
        (i + 1 == text.size() || text[i + 1] != '=')) throw std::runtime_error("Assignments are not supported; enter the right-hand side only.");
  }
}
bool Finite(double x) { return std::isfinite(x) && std::abs(x) < 1e100; }
}

struct Evaluator::Impl {
  mu::Parser parser;
  double x = 0;
  std::map<std::string, double> values;
};
Evaluator::Evaluator(const std::string& text, const std::vector<Parameter>& parameters, bool degrees)
    : impl_(std::make_unique<Impl>()) {
  CheckText(text);
  auto& p = impl_->parser;
  p.ClearFun(); p.ClearConst();
  p.DefineFun("sin", degrees ? DegreesSin : static_cast<double(*)(double)>(std::sin));
  p.DefineFun("cos", degrees ? DegreesCos : static_cast<double(*)(double)>(std::cos));
  p.DefineFun("tan", degrees ? DegreesTan : static_cast<double(*)(double)>(std::tan));
  p.DefineFun("asin", degrees ? DegreesAsin : static_cast<double(*)(double)>(std::asin));
  p.DefineFun("acos", degrees ? DegreesAcos : static_cast<double(*)(double)>(std::acos));
  p.DefineFun("atan", degrees ? DegreesAtan : static_cast<double(*)(double)>(std::atan));
  p.DefineFun("sqrt", static_cast<double(*)(double)>(std::sqrt));
  p.DefineFun("abs", static_cast<double(*)(double)>(std::fabs));
  p.DefineFun("exp", static_cast<double(*)(double)>(std::exp));
  p.DefineFun("ln", static_cast<double(*)(double)>(std::log));
  p.DefineFun("log10", static_cast<double(*)(double)>(std::log10));
  p.DefineFun("floor", static_cast<double(*)(double)>(std::floor));
  p.DefineFun("ceil", static_cast<double(*)(double)>(std::ceil));
  p.DefineFun("min", Minimum); p.DefineFun("max", Maximum);
  p.DefineConst("pi", Pi); p.DefineConst("e", std::exp(1.0));
  p.DefineVar("x", &impl_->x); p.DefineVar("t", &impl_->x); p.DefineVar("theta", &impl_->x);
  for (const auto& value : parameters) impl_->values[value.name] = value.value;
  for (auto& [name, value] : impl_->values) p.DefineVar(name, &value);
  try {
    p.SetExpr(text);
    (void)p.GetUsedVar();
    (void)p.Eval();
  } catch (const mu::Parser::exception_type& error) {
    throw std::runtime_error(error.GetMsg());
  }
}
Evaluator::~Evaluator() = default;
double Evaluator::At(double x) {
  impl_->x = x;
  try { return impl_->parser.Eval(); } catch (...) { return std::numeric_limits<double>::quiet_NaN(); }
}
std::vector<std::string> UnknownParameters(const std::string& text, const std::vector<Parameter>& known) {
  const std::regex tokens(R"((?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?|[A-Za-z_][A-Za-z_0-9]*)");
  std::set<std::string> names;
  for (auto it = std::sregex_iterator(text.begin(), text.end(), tokens); it != std::sregex_iterator(); ++it) {
    const auto name = it->str();
    if (!std::isalpha(static_cast<unsigned char>(name.front())) && name.front() != '_') continue;
    if (name == "x" || name == "t" || name == "theta" || name == "pi" || name == "e" || functions.contains(name)) continue;
    auto next = static_cast<std::size_t>(it->position() + it->length());
    while (next < text.size() && std::isspace(static_cast<unsigned char>(text[next]))) ++next;
    if (next < text.size() && text[next] == '(') continue;
    if (std::ranges::none_of(known, [&](const auto& p) { return p.name == name; })) names.insert(name);
  }
  return {names.begin(), names.end()};
}

PlotResult CalculatePlots(const Experiment& e, std::stop_token stop) {
  PlotResult result;
  for (const auto& expression : e.expressions) {
    Curve curve; curve.id = expression.id;
    if (!expression.visible) { result.curves.push_back(curve); continue; }
    try {
      Evaluator first(expression.first, e.parameters, e.degrees);
      std::unique_ptr<Evaluator> second;
      if (expression.model == Model::Parametric) second = std::make_unique<Evaluator>(expression.second, e.parameters, e.degrees);
      const auto point = [&](double u) {
        const double y = first.At(u);
        if (expression.model == Model::Explicit) return Point{u, y};
        if (expression.model == Model::Parametric) return Point{y, second->At(u)};
        const double angle = e.degrees ? u * Pi / 180 : u;
        return Point{y * std::cos(angle), y * std::sin(angle)};
      };
      const auto good = [](Point p) { return Finite(p.x) && Finite(p.y); };
      const auto distance = [&](Point a, Point b) {
        return std::hypot((a.x - b.x) / (e.view.xmax - e.view.xmin), (a.y - b.y) / (e.view.ymax - e.view.ymin));
      };
      const double from = expression.model == Model::Explicit ? e.view.xmin : expression.from;
      const double to = expression.model == Model::Explicit ? e.view.xmax : expression.to;
      curve.segments.emplace_back();
      int budget = 16000;
      const auto append = [&](Point p) {
        if (!good(p)) { if (!curve.segments.back().empty()) curve.segments.emplace_back(); }
        else curve.segments.back().push_back(p);
      };
      const auto refine = [&](auto&& self, double a, Point pa, double b, Point pb, int depth) -> void {
        if (--budget <= 0 || stop.stop_requested()) return;
        const double middle = (a + b) * .5;
        const auto pm = point(middle);
        const bool valid = good(pa) && good(pb) && good(pm);
        const double error = valid ? distance(pm, {(pa.x + pb.x) * .5, (pa.y + pb.y) * .5}) : 1;
        if ((!valid || error > .0008 || distance(pa, pb) > .1) && depth < 8) {
          self(self, a, pa, middle, pm, depth + 1);
          self(self, middle, pm, b, pb, depth + 1);
        } else if (!valid || error > .03 || distance(pa, pb) > 2) {
          if (!curve.segments.back().empty()) curve.segments.emplace_back();
          append(pb);
        } else append(pb);
      };
      auto previous = point(from); append(previous);
      for (int i = 1; i <= 320 && !stop.stop_requested(); ++i) {
        const double a = from + (to - from) * (i - 1) / 320;
        const double b = from + (to - from) * i / 320;
        auto next = point(b); refine(refine, a, previous, b, next, 0); previous = next;
      }
      if (std::ranges::all_of(curve.segments, [](const auto& segment) { return segment.empty(); }))
        curve.error = "The function has no finite samples in this interval.";
    } catch (const std::exception& error) { curve.error = error.what(); }
    result.curves.push_back(std::move(curve));
  }
  return result;
}

std::vector<std::complex<double>> Transform(const std::vector<std::complex<double>>& input, bool inverse) {
  const auto n = input.size();
  if (n < 2 || n > 65536 || (n & (n - 1))) throw std::runtime_error("FFT size must be a power of two, at most 65536.");
  using Plan = std::unique_ptr<kiss_fft_state, decltype(&std::free)>;
  thread_local Plan forward(nullptr, std::free), backward(nullptr, std::free);
  thread_local std::size_t size = 0;
  if (size != n) {
    forward.reset(kiss_fft_alloc(static_cast<int>(n), 0, nullptr, nullptr));
    backward.reset(kiss_fft_alloc(static_cast<int>(n), 1, nullptr, nullptr));
    size = n;
  }
  if (!forward || !backward) throw std::bad_alloc();
  std::vector<kiss_fft_cpx> in(n), out(n);
  for (std::size_t i = 0; i < n; ++i) in[i] = {input[i].real(), input[i].imag()};
  kiss_fft(inverse ? backward.get() : forward.get(), in.data(), out.data());
  std::vector<std::complex<double>> result(n);
  const double scale = inverse ? 1.0 / n : 1;
  for (std::size_t i = 0; i < n; ++i) result[i] = {out[i].r * scale, out[i].i * scale};
  return result;
}
double SourceValue(const Signal& s, double time) {
  if (s.source == Source::Series) {
    double value = 0;
    for (int i = 0; i < s.terms; ++i) { const int h = i * 2 + 1; value += 4 * s.amplitude / (Pi * h) * std::sin(2 * Pi * h * s.fundamental * time); }
    return value;
  }
  double value = 0;
  for (const auto& o : s.oscillators) if (o.enabled) {
    const double phase = o.frequency * time + o.phase / 360;
    const double cycle = phase - std::floor(phase);
    double wave = 0;
    switch (o.wave) {
      case Wave::Sine: wave = std::sin(2 * Pi * phase); break;
      case Wave::Square: wave = cycle < .5 ? 1 : -1; break;
      case Wave::Triangle: wave = 1 - 4 * std::abs(cycle - .5); break;
      case Wave::Saw: wave = 2 * cycle - 1; break;
      case Wave::Constant: wave = 1; break;
    }
    value += o.amplitude * wave;
  }
  return value;
}
SignalResult CalculateSignal(const Experiment& e, std::stop_token stop) {
  SignalResult r;
  try {
    const auto& s = e.signal;
    const auto n = s.reconstruct ? s.reconstruction_source.size() : static_cast<std::size_t>(s.count);
    if (n < 256 || n > 65536 || (n & (n - 1))) throw std::runtime_error("Choose 256–65536 samples, in powers of two.");
    r.rate = s.reconstruct ? s.reconstruction_rate : s.rate; r.start = s.start;
    r.raw.resize(n);
    std::unique_ptr<Evaluator> evaluator;
    if (!s.reconstruct && s.source == Source::Expression) evaluator = std::make_unique<Evaluator>(s.expression, e.parameters);
    for (std::size_t i = 0; i < n; ++i) {
      if (stop.stop_requested()) return {};
      r.raw[i] = s.reconstruct ? s.reconstruction_source[i] : s.source == Source::Samples ?
          (i < s.samples.size() ? s.samples[i] : throw std::runtime_error("The sample count exceeds the imported record.")) :
          evaluator ? evaluator->At(s.start + i / s.rate) : SourceValue(s, s.start + i / s.rate);
      if (!Finite(r.raw[i])) throw std::runtime_error("The signal contains an undefined or non-finite sample at index " + std::to_string(i) + ".");
    }
    double mean = 0, gain = 0;
    if (!s.reconstruct && s.remove_mean) { for (double x : r.raw) mean += x; mean /= n; }
    std::vector<std::complex<double>> values(n);
    for (std::size_t i = 0; i < n; ++i) {
      double w = 1;
      if (!s.reconstruct && s.window == Window::Hann) w = .5 - .5 * std::cos(2 * Pi * i / n);
      if (!s.reconstruct && s.window == Window::Hamming) w = .54 - .46 * std::cos(2 * Pi * i / n);
      gain += w; values[i] = (r.raw[i] - mean) * w;
    }
    r.coefficients = Transform(values);
    if (s.reconstruct) {
      for (const auto& [k, value] : s.edits) {
        if (k < 0 || k > static_cast<int>(n / 2)) throw std::runtime_error("The edited bin is outside this record.");
        r.coefficients[k] = (k == 0 || k == n / 2) ? std::complex<double>(value.real(), 0) : value;
        if (k > 0 && k < n / 2) r.coefficients[n - k] = std::conj(value);
      }
      const auto inverse = Transform(r.coefficients, true);
      r.reconstructed.resize(n);
      for (std::size_t i = 0; i < n; ++i) {
        r.reconstructed[i] = inverse[i].real(); r.rms += std::pow(r.reconstructed[i] - r.raw[i], 2);
      }
      r.rms = std::sqrt(r.rms / n);
    }
    for (std::size_t i = 0; i <= n / 2; ++i) {
      r.amplitude.push_back(std::abs(r.coefficients[i]) * ((i == 0 || i == n / 2) ? 1 : 2) / gain);
      r.phase.push_back(std::arg(r.coefficients[i]) * 180 / Pi);
    }
    if (!s.reconstruct && s.source == Source::Series) r.aliasing = s.fundamental * (2 * s.terms - 1) >= s.rate / 2;
    if (!s.reconstruct && s.source == Source::Mixer) r.aliasing = std::ranges::any_of(s.oscillators, [&](const auto& o) {
      return o.enabled && o.wave != Wave::Constant && (o.wave != Wave::Sine || o.frequency >= s.rate / 2);
    });
  } catch (const std::exception& error) { r.error = error.what(); }
  return r;
}
void BeginReconstruction(Experiment& e) {
  auto original = e; original.signal.reconstruct = false;
  const auto result = CalculateSignal(original);
  if (!result.error.empty()) throw std::runtime_error(result.error);
  e.signal.reconstruction_source = result.raw; e.signal.reconstruction_rate = e.signal.rate;
  e.signal.edits.clear(); e.signal.reconstruct = true;
}
void ApplyReconstruction(Experiment& e) {
  const auto result = CalculateSignal(e);
  if (!result.error.empty()) throw std::runtime_error(result.error);
  if (result.reconstructed.empty()) throw std::runtime_error("Start reconstruction first.");
  e.signal.samples = result.reconstructed; e.signal.source = Source::Samples;
  e.signal.rate = result.rate; e.signal.count = static_cast<int>(result.raw.size());
  e.signal.reconstruct = false; e.signal.edits.clear(); e.signal.reconstruction_source.clear();
}
void LowPass(Experiment& e, double cutoff) {
  if (!e.signal.reconstruct) BeginReconstruction(e);
  auto& s = e.signal; s.edits.clear();
  const auto n = s.reconstruction_source.size();
  for (int k = 0; k <= static_cast<int>(n / 2); ++k)
    if (k * s.reconstruction_rate / n > cutoff) s.edits[k] = {0, 0};
}
Experiment Example(int index) {
  if (index < 0 || index >= 8) throw std::invalid_argument("Invalid example index.");
  Experiment e; e.example_id = index;
  if (index == 1) { e.name = "Shape a function"; e.mode = 0; }
  if (index == 2) { e.name = "A curve with a break"; e.mode = 0; e.expressions = {{0, Model::Explicit, "1/x"}, {1, Model::Explicit, "tan(x)"}, {2, Model::Explicit, "sqrt(x)"}}; }
  if (index == 3) { e.name = "Lissajous motion"; e.mode = 0; e.equal_axes = true; e.view = {-3, 3, -3, 3}; e.expressions = {{0, Model::Parametric, "2*sin(3*t+a)", "2*sin(2*t)"}}; }
  if (index == 4) { e.name = "Polar petals"; e.mode = 0; e.equal_axes = true; e.view = {-3, 3, -3, 3}; e.expressions = {{0, Model::Polar, "2*cos(3*theta)"}}; }
  if (index == 5) { e.name = "Two close frequencies"; e.signal.source = Source::Mixer; e.signal.oscillators = {{0, Wave::Sine, 1, 8}, {1, Wave::Sine, 1, 8.5}}; e.time_view = {0, 4, -2.3, 2.3}; }
  if (index == 6) { e.name = "Off the frequency bin"; e.signal.source = Source::Mixer; e.signal.oscillators = {{0, Wave::Sine, 1, 5.3}}; e.signal.rate = 128; e.signal.count = 256; }
  if (index == 7) { e.name = "Sampling illusion"; e.signal.source = Source::Mixer; e.signal.oscillators = {{0, Wave::Sine, 1, 9}}; e.signal.rate = 12; e.signal.count = 256; e.time_view = {0, 1.2, -1.4, 1.4}; e.frequency_view = {0, 6, 0, 1.2}; }
  return e;
}
bool ExampleModified(const Experiment& e) {
  if (e.example_id < 0) return false;
  const auto original = Example(e.example_id);
  return e.expressions != original.expressions || e.parameters != original.parameters ||
      e.signal != original.signal || e.degrees != original.degrees;
}
void SampleFunction(Experiment& e, int id) {
  const auto found = std::ranges::find(e.expressions, id, &Expression::id);
  if (found == e.expressions.end() || found->model != Model::Explicit) throw std::runtime_error("Choose an explicit function.");
  if (e.signal.reconstruct) throw std::runtime_error("Apply or discard frequency edits first.");
  Evaluator evaluator(found->first, e.parameters, e.degrees);
  auto signal = e.signal;
  signal.source = Source::Samples; signal.start = e.view.xmin;
  signal.rate = signal.count / (e.view.xmax - e.view.xmin); signal.samples.clear();
  for (int i = 0; i < signal.count; ++i) {
    const double value = evaluator.At(signal.start + i / signal.rate);
    if (!Finite(value)) throw std::runtime_error("This interval contains an undefined sample. Choose another coordinate range.");
    signal.samples.push_back(value);
  }
  e.signal = std::move(signal); e.mode = 1;
  e.time_view = {e.view.xmin, e.view.xmax, e.view.ymin, e.view.ymax};
  e.frequency_view = {0, e.signal.rate / 2, 0, 1.6};
}
std::string Number(double value, int precision) {
  if (!std::isfinite(value)) return "undefined";
  if (std::abs(value) < 1e-11) value = 0;
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(precision) << value;
  return out.str();
}
} // namespace graph

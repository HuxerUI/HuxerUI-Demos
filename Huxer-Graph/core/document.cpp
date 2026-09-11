#include "graph_math.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

namespace graph {
using Json = nlohmann::json;
namespace {
Json Bounds(const Viewport& v) { return {v.xmin, v.xmax, v.ymin, v.ymax}; }
Viewport ReadBounds(const Json& j) { return {j.at(0), j.at(1), j.at(2), j.at(3)}; }
void Require(bool valid, const char* message) { if (!valid) throw std::runtime_error(message); }
bool Bounded(double x, double limit = 1e12) { return std::isfinite(x) && std::abs(x) <= limit; }
std::string Escape(std::string text) {
  std::string out;
  for (char c : text) {
    if (c == '&') out += "&amp;"; else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;"; else if (c == '"') out += "&quot;"; else out += c;
  }
  return out;
}
}
void Validate(const Experiment& e) {
  Require(e.example_id >= -1 && e.example_id < 8, "Invalid example identity.");
  Require(!e.name.empty() && e.name.size() <= 240, "Invalid experiment name.");
  Require(e.mode >= 0 && e.mode <= 1, "Invalid workspace.");
  for (const auto& v : {e.view, e.time_view, e.frequency_view}) {
    Require(Bounded(v.xmin) && Bounded(v.xmax) && Bounded(v.ymin) && Bounded(v.ymax) &&
        v.xmax - v.xmin >= 1e-8 && v.ymax - v.ymin >= 1e-8, "Invalid coordinate range.");
  }
  Require(e.expressions.size() <= 24 && e.parameters.size() <= 32 && e.signal.oscillators.size() <= 16, "The document exceeds the experiment limits.");
  std::set<int> ids;
  for (const auto& x : e.expressions) {
    Require(x.id >= 0 && x.id < 1000000 && ids.insert(x.id).second, "Expression identifiers must be unique.");
    Require(static_cast<int>(x.model) >= 0 && static_cast<int>(x.model) <= 2 && x.first.size() <= 2048 && x.second.size() <= 2048,
        "Invalid expression model.");
    Require(Bounded(x.from) && Bounded(x.to) && x.to > x.from, "Invalid parameter interval.");
  }
  std::set<std::string> names;
  for (const auto& p : e.parameters) {
    Require(std::regex_match(p.name, std::regex("[A-Za-z][A-Za-z_0-9]{0,23}")) && names.insert(p.name).second,
        "Parameter names must be unique identifiers.");
    Require(!UnknownParameters(p.name, {}).empty(), "This parameter name is reserved.");
    Require(Bounded(p.minimum) && Bounded(p.maximum) && Bounded(p.value) && Bounded(p.step) &&
        p.maximum > p.minimum && p.step > 0 && p.step <= p.maximum - p.minimum && p.value >= p.minimum && p.value <= p.maximum,
        "Invalid parameter range or value.");
  }
  const auto& s = e.signal;
  Require(static_cast<int>(s.source) >= 0 && static_cast<int>(s.source) <= 3 &&
      static_cast<int>(s.window) >= 0 && static_cast<int>(s.window) <= 2, "Invalid signal source or window.");
  Require(s.terms >= 1 && s.terms <= 64 && Bounded(s.fundamental, 1e6) && s.fundamental > 0 && Bounded(s.amplitude, 1e6), "Invalid harmonic series.");
  Require(Bounded(s.rate, 1e6) && s.rate > 0 && Bounded(s.start) && s.count >= 256 && s.count <= 65536 && !(s.count & (s.count - 1)), "Invalid sampling settings.");
  Require(s.expression.size() <= 2048, "Signal expression is too long.");
  ids.clear();
  for (const auto& o : s.oscillators) {
    Require(o.id >= 0 && o.id < 1000000 && ids.insert(o.id).second && static_cast<int>(o.wave) >= 0 && static_cast<int>(o.wave) <= 4,
        "Invalid oscillator.");
    Require(Bounded(o.amplitude, 1e6) && Bounded(o.frequency, 1e6) && o.frequency >= 0 && Bounded(o.phase, 1e6), "Invalid oscillator value.");
  }
  for (const auto* samples : {&s.samples, &s.reconstruction_source}) {
    Require(samples->size() <= 65536, "The sample record is too large.");
    for (double x : *samples) Require(Bounded(x, 1e100), "Samples must be finite.");
  }
  if (s.source == Source::Samples) Require(s.samples.size() >= s.count, "The imported record is shorter than the sample count.");
  if (s.reconstruct) {
    const auto n = s.reconstruction_source.size();
    Require(n >= 256 && n <= 65536 && !(n & (n - 1)) && Bounded(s.reconstruction_rate, 1e6) && s.reconstruction_rate > 0,
        "Invalid reconstruction snapshot.");
    for (const auto& [k, value] : s.edits) Require(k >= 0 && k <= n / 2 && Bounded(value.real(), 1e100) && Bounded(value.imag(), 1e100) &&
        ((k != 0 && k != n / 2) || value.imag() == 0), "Invalid complex frequency edit.");
  }
}
std::string Serialize(const Experiment& e) {
  Validate(e);
  Json j{{"schema", 1}, {"example_id", e.example_id}, {"name", e.name}, {"mode", e.mode}, {"degrees", e.degrees}, {"equal_axes", e.equal_axes}, {"dark", e.dark},
         {"view", Bounds(e.view)}, {"time_view", Bounds(e.time_view)}, {"frequency_view", Bounds(e.frequency_view)}};
  j["expressions"] = Json::array(); j["parameters"] = Json::array();
  for (const auto& x : e.expressions) j["expressions"].push_back({{"id", x.id}, {"model", static_cast<int>(x.model)}, {"first", x.first},
      {"second", x.second}, {"from", x.from}, {"to", x.to}, {"visible", x.visible}});
  for (const auto& p : e.parameters) j["parameters"].push_back({p.name, p.value, p.minimum, p.maximum, p.step});
  const auto& s = e.signal;
  j["signal"] = {{"source", static_cast<int>(s.source)}, {"terms", s.terms}, {"fundamental", s.fundamental}, {"amplitude", s.amplitude},
      {"rate", s.rate}, {"start", s.start}, {"count", s.count}, {"window", static_cast<int>(s.window)}, {"remove_mean", s.remove_mean},
      {"decibels", s.decibels}, {"expression", s.expression}, {"samples", s.samples}, {"reconstruct", s.reconstruct},
      {"reconstruction_source", s.reconstruction_source}, {"reconstruction_rate", s.reconstruction_rate}, {"oscillators", Json::array()}, {"edits", Json::array()}};
  for (const auto& o : s.oscillators) j["signal"]["oscillators"].push_back({o.id, static_cast<int>(o.wave), o.amplitude, o.frequency, o.phase, o.enabled});
  for (const auto& [k, v] : s.edits) j["signal"]["edits"].push_back({k, v.real(), v.imag()});
  return j.dump(2);
}
Experiment Deserialize(const std::string& text) {
  Require(text.size() <= 10 * 1024 * 1024, "The document exceeds 10 MiB.");
  const auto j = Json::parse(text, [](int depth, Json::parse_event_t, Json&) {
    if (depth > 16) throw std::runtime_error("The document nesting is too deep."); return true;
  });
  Require(j.at("schema").get<int>() == 1, "Unsupported experiment schema.");
  Experiment e; e.name = j.at("name"); e.mode = j.at("mode"); e.degrees = j.at("degrees");
  e.example_id = j.value("example_id", -1);
  e.equal_axes = j.at("equal_axes"); e.dark = j.at("dark"); e.view = ReadBounds(j.at("view"));
  e.time_view = ReadBounds(j.at("time_view")); e.frequency_view = ReadBounds(j.at("frequency_view"));
  e.expressions.clear(); e.parameters.clear();
  for (const auto& x : j.at("expressions")) e.expressions.push_back({x.at("id"), static_cast<Model>(x.at("model").get<int>()),
      x.at("first"), x.at("second"), x.at("from"), x.at("to"), x.at("visible")});
  for (const auto& p : j.at("parameters")) e.parameters.push_back({p.at(0), p.at(1), p.at(2), p.at(3), p.at(4)});
  const auto& s = j.at("signal"); auto& o = e.signal;
  o.source = static_cast<Source>(s.at("source").get<int>()); o.window = static_cast<Window>(s.at("window").get<int>());
  o.terms = s.at("terms"); o.fundamental = s.at("fundamental"); o.amplitude = s.at("amplitude");
  o.rate = s.at("rate"); o.start = s.at("start"); o.count = s.at("count"); o.remove_mean = s.at("remove_mean"); o.decibels = s.at("decibels");
  o.expression = s.at("expression"); o.samples = s.at("samples").get<std::vector<double>>();
  o.reconstruct = s.at("reconstruct"); o.reconstruction_source = s.at("reconstruction_source").get<std::vector<double>>();
  o.reconstruction_rate = s.at("reconstruction_rate"); o.oscillators.clear();
  for (const auto& w : s.at("oscillators")) o.oscillators.push_back({w.at(0), static_cast<Wave>(w.at(1).get<int>()), w.at(2), w.at(3), w.at(4), w.at(5)});
  for (const auto& edit : s.at("edits")) o.edits[edit.at(0)] = {edit.at(1), edit.at(2)};
  Validate(e); return e;
}
std::vector<std::vector<double>> ParseCsv(const std::string& text) {
  Require(text.size() <= 10 * 1024 * 1024, "CSV exceeds 10 MiB.");
  std::vector<std::vector<double>> result;
  std::istringstream stream(text); stream.imbue(std::locale::classic());
  std::string line; std::size_t columns = 0; bool header_allowed = true;
  while (std::getline(stream, line)) {
    if (line.empty() || line == "\r" || line.front() == '#') continue;
    std::replace(line.begin(), line.end(), '\t', ',');
    std::vector<double> row; std::istringstream fields(line); std::string token; bool numeric = true;
    while (std::getline(fields, token, ',')) {
      if (row.size() >= 16) throw std::runtime_error("CSV supports at most 16 columns.");
      std::istringstream value(token); value.imbue(std::locale::classic()); double x = 0;
      value >> x; const bool parsed = !value.fail(); value >> std::ws;
      if (!parsed || !value.eof() || !Bounded(x, 1e100)) { numeric = false; break; }
      row.push_back(x);
    }
      if (!numeric && row.empty() && header_allowed) { header_allowed = false; continue; }
    header_allowed = false;
    Require(numeric && !row.empty() && line.back() != ',', "CSV contains an invalid or missing numeric value.");
    if (!columns) columns = row.size();
    Require(row.size() == columns, "CSV rows must have the same number of columns.");
    result.push_back(row);
    Require(result.size() <= 65536, "CSV exceeds 65536 samples.");
  }
  Require(result.size() >= 256, "CSV needs at least 256 numeric samples.");
  return result;
}
void ImportCsv(Experiment& e, const std::vector<std::vector<double>>& rows, int time_column, int value_column) {
  Require(rows.size() >= 256 && rows.size() <= 65536, "Invalid sample count.");
  Require(value_column >= 0 && value_column < rows.front().size() && time_column >= -1 &&
      time_column < static_cast<int>(rows.front().size()) && time_column != value_column, "Choose distinct time and amplitude columns.");
  Signal s = e.signal; s.reconstruct = false; s.edits.clear(); s.reconstruction_source.clear(); s.samples.clear();
  if (time_column >= 0) {
    const double delta = rows[1][time_column] - rows[0][time_column];
    Require(std::isfinite(delta) && delta > 0, "Timestamps must increase uniformly.");
    for (std::size_t i = 1; i < rows.size(); ++i)
      Require(std::abs(rows[i][time_column] - rows[i - 1][time_column] - delta) <= std::max(1e-9, delta * 1e-5),
          "Nonuniform timestamps need resampling before import.");
    s.rate = 1 / delta; s.start = rows.front()[time_column];
  }
  for (const auto& row : rows) s.samples.push_back(row.at(value_column));
  s.count = 256; while (s.count * 2 <= rows.size()) s.count *= 2;
  s.source = Source::Samples; e.signal = std::move(s); e.mode = 1;
  e.time_view.xmin = e.signal.start; e.time_view.xmax = e.signal.start + e.signal.count / e.signal.rate;
  e.frequency_view.xmin = 0; e.frequency_view.xmax = e.signal.rate / 2;
  Validate(e);
}
std::string ExportCsv(const Experiment& e, bool spectrum) {
  const auto r = CalculateSignal(e);
  if (!r.error.empty()) throw std::runtime_error(r.error);
  std::ostringstream out; out.imbue(std::locale::classic()); out << std::setprecision(17);
  out << "# Huxer Graph; rate=" << r.rate << "; N=" << r.raw.size() << "; window="
      << (e.signal.reconstruct ? 0 : static_cast<int>(e.signal.window)) << "; remove_mean="
      << (!e.signal.reconstruct && e.signal.remove_mean) << "; forward=unscaled; inverse=1/N; amplitude=one-sided/window-sum\n";
  if (spectrum) {
    out << "frequency_hz,real,imaginary,amplitude,phase_degrees\n";
    const double threshold = *std::max_element(r.amplitude.begin(), r.amplitude.end()) * 1e-8;
    for (std::size_t k = 0; k < r.amplitude.size(); ++k) {
      out << k * r.rate / r.raw.size() << ',' << r.coefficients[k].real() << ',' << r.coefficients[k].imag() << ',' << r.amplitude[k] << ',';
      if (r.amplitude[k] > threshold) out << r.phase[k];
      out << '\n';
    }
  } else {
    out << "time_s,original" << (r.reconstructed.empty() ? "\n" : ",reconstructed\n");
    for (std::size_t i = 0; i < r.raw.size(); ++i) {
      out << r.start + i / r.rate << ',' << r.raw[i]; if (!r.reconstructed.empty()) out << ',' << r.reconstructed[i]; out << '\n';
    }
  }
  return out.str();
}
std::string ExportSvg(const Experiment& e, int plot) {
  auto v = plot == 0 ? e.view : plot == 1 ? e.time_view : e.frequency_view;
  if (plot == 0 && e.equal_axes) {
    const double center = (v.ymin + v.ymax) / 2;
    const double half_height = (v.xmax - v.xmin) * 610 / 1090 / 2;
    v.ymin = center - half_height; v.ymax = center + half_height;
  }
  const auto x = [&](double n) { return 64 + (n - v.xmin) / (v.xmax - v.xmin) * 1090; };
  const auto y = [&](double n) { return 700 - (n - v.ymin) / (v.ymax - v.ymin) * 610; };
  std::ostringstream out; out.imbue(std::locale::classic());
  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1200\" height=\"800\" viewBox=\"0 0 1200 800\"><rect width=\"1200\" height=\"800\" fill=\"white\"/>";
  out << "<text x=\"64\" y=\"38\" font-family=\"sans-serif\" font-size=\"22\">" << Escape(e.name) << "</text>";
  out << "<defs><clipPath id=\"plot\"><rect x=\"64\" y=\"90\" width=\"1090\" height=\"610\"/></clipPath></defs>";
  for (int i = 0; i <= 8; ++i) {
    double a = v.xmin + (v.xmax - v.xmin) * i / 8, b = v.ymin + (v.ymax - v.ymin) * i / 8;
    out << "<path d=\"M" << x(a) << " 90V700 M64 " << y(b) << "H1154\" stroke=\"#e3e8f1\"/>";
    out << "<g font-family=\"sans-serif\" font-size=\"12\" fill=\"#65718a\"><text x=\"" << x(a) << "\" y=\"724\" text-anchor=\"middle\">" << Number(a) << "</text><text x=\"54\" y=\"" << y(b) << "\" text-anchor=\"end\">" << Number(b) << "</text></g>";
  }
  const char* colors[]{"#355ff0", "#cd4b91", "#df8b20", "#16838f"};
  const auto path = [&](const std::vector<Point>& points, const char* color) {
    out << "<path clip-path=\"url(#plot)\" fill=\"none\" stroke=\"" << color << "\" stroke-width=\"2\" d=\"";
    bool start = true;
    for (const auto& p : points) {
      const auto px = x(p.x), py = y(p.y);
      if (!Bounded(px, 1e7) || !Bounded(py, 1e7)) { start = true; continue; }
      out << (start ? 'M' : 'L') << px << ' ' << py << ' '; start = false;
    }
    out << "\"/>";
  };
  if (plot == 0) {
    const auto r = CalculatePlots(e);
    for (const auto& c : r.curves) { if (!c.error.empty()) throw std::runtime_error(c.error); for (const auto& segment : c.segments) path(segment, colors[c.id % 4]); }
    out << "<text x=\"64\" y=\"766\" font-family=\"sans-serif\" font-size=\"12\">x / y · " << (e.degrees ? "degrees" : "radians") << "</text>";
  } else {
    const auto r = CalculateSignal(e); if (!r.error.empty()) throw std::runtime_error(r.error);
    if (plot == 1) {
      std::vector<Point> points;
      for (std::size_t i = 0; i < r.raw.size(); ++i) points.push_back({r.start + i / r.rate, r.raw[i]}); path(points, colors[0]);
      if (!r.reconstructed.empty()) { for (std::size_t i = 0; i < points.size(); ++i) points[i].y = r.reconstructed[i]; path(points, colors[1]); }
    } else {
      for (std::size_t k = 0; k < r.amplitude.size(); ++k) {
        const double f = k * r.rate / r.raw.size();
        const double a = e.signal.decibels ? 20 * std::log10(std::max(1e-6, r.amplitude[k])) : r.amplitude[k];
        path({{f, e.signal.decibels ? -120.0 : 0.0}, {f, a}}, colors[0]);
      }
    }
    out << "<text x=\"64\" y=\"766\" font-family=\"sans-serif\" font-size=\"12\">" << (plot == 1 ? "Time (s) / amplitude" : e.signal.decibels ? "Frequency (Hz) / dB re 1 amplitude unit" : "Frequency (Hz) / amplitude")
        << " · N=" << r.raw.size() << " · sample rate=" << r.rate << " Hz · " << (e.signal.reconstruct ? "raw reconstruction" : e.signal.window == Window::Hann ? "Hann" : e.signal.window == Window::Hamming ? "Hamming" : "Rectangular") << "</text>";
  }
  out << "</svg>"; return out.str();
}
} // namespace graph

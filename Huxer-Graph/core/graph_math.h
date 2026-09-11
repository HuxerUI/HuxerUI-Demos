#pragma once

#include <complex>
#include <map>
#include <memory>
#include <stop_token>
#include <string>
#include <vector>

namespace graph {

constexpr double Pi = 3.14159265358979323846;
enum class Model { Explicit, Parametric, Polar };
enum class Wave { Sine, Square, Triangle, Saw, Constant };
enum class Source { Series, Mixer, Expression, Samples };
enum class Window { Rectangular, Hann, Hamming };
struct Parameter {
  std::string name = "a";
  double value = 1.5, minimum = -3, maximum = 3, step = .05;
  bool operator==(const Parameter&) const = default;
};
struct Expression {
  int id = 0;
  Model model = Model::Explicit;
  std::string first = "a*sin(b*x+c)", second = "sin(t)";
  double from = 0, to = 2 * Pi;
  bool visible = true;
  bool operator==(const Expression&) const = default;
};
struct Oscillator {
  int id = 0;
  Wave wave = Wave::Sine;
  double amplitude = 1, frequency = 2, phase = 0;
  bool enabled = true;
  bool operator==(const Oscillator&) const = default;
};
struct Viewport {
  double xmin = -8, xmax = 8, ymin = -4, ymax = 4;
};
struct Signal {
  Source source = Source::Series;
  int terms = 3;
  double fundamental = 2, amplitude = 1, rate = 256, start = 0;
  int count = 1024;
  Window window = Window::Rectangular;
  bool remove_mean = false, decibels = false;
  std::string expression = "sin(2*pi*2*t)";
  std::vector<Oscillator> oscillators{{}};
  std::vector<double> samples;
  std::vector<double> reconstruction_source;
  double reconstruction_rate = 256;
  std::map<int, std::complex<double>> edits;
  bool reconstruct = false;
  bool operator==(const Signal&) const = default;
};
struct Experiment {
  int example_id = -1;
  std::string name = "Build a square wave";
  int mode = 1;
  bool degrees = false, equal_axes = false, dark = false;
  std::vector<Expression> expressions{{0}, {1, Model::Explicit, "0.15*x^2-1"},
                                       {2, Model::Explicit, "cos(x)*exp(-0.08*x^2)"}};
  std::vector<Parameter> parameters{{"a", 1.5}, {"b", 1, .1, 3, .05}, {"c", 0, -Pi, Pi, .05}};
  Viewport view;
  Viewport time_view{0, 1.5, -1.8, 1.8};
  Viewport frequency_view{0, 16, 0, 1.6};
  Signal signal;
};
struct Point { double x = 0, y = 0; };
struct Curve {
  int id = 0;
  std::vector<std::vector<Point>> segments;
  std::string error;
};
struct PlotResult { std::vector<Curve> curves; };
struct SignalResult {
  std::vector<double> raw, amplitude, phase, reconstructed;
  std::vector<std::complex<double>> coefficients;
  double rms = 0, rate = 256, start = 0;
  bool aliasing = false;
  std::string error;
};
class Evaluator {
public:
  Evaluator(const std::string& text, const std::vector<Parameter>& parameters, bool degrees = false);
  ~Evaluator();
  double At(double x);
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
std::vector<std::string> UnknownParameters(const std::string& text, const std::vector<Parameter>& known);
PlotResult CalculatePlots(const Experiment& experiment, std::stop_token stop = {});
SignalResult CalculateSignal(const Experiment& experiment, std::stop_token stop = {});
std::vector<std::complex<double>> Transform(const std::vector<std::complex<double>>& input, bool inverse = false);
double SourceValue(const Signal& signal, double time);
Experiment Example(int index);
bool ExampleModified(const Experiment& experiment);
void BeginReconstruction(Experiment& experiment);
void ApplyReconstruction(Experiment& experiment);
void LowPass(Experiment& experiment, double cutoff);
void SampleFunction(Experiment& experiment, int id);
void Validate(const Experiment& experiment);
std::string Serialize(const Experiment& experiment);
Experiment Deserialize(const std::string& text);
std::vector<std::vector<double>> ParseCsv(const std::string& text);
void ImportCsv(Experiment& experiment, const std::vector<std::vector<double>>& rows, int time_column, int value_column);
std::string ExportCsv(const Experiment& experiment, bool spectrum);
std::string ExportSvg(const Experiment& experiment, int plot);
std::string Number(double value, int precision = 4);

} // namespace graph

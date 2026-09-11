#include "graph_math.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace graph;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void Near(double a, double b, const char* message) { Check(std::abs(a - b) < 1e-10, message); }
template<class F> void Reject(F fn) { bool caught = false; try { fn(); } catch (...) { caught = true; } Check(caught, "Expected rejection"); }
int main() {
  try {
    auto preset = Example(3);
    Check(!ExampleModified(preset), "Unmodified example identity");
    preset.dark = true; preset.mode = 1; preset.view.xmin -= 1; preset.name = "Renamed experiment";
    Check(!ExampleModified(preset), "Presentation changes preserve example status");
    preset.parameters[0].value += .25;
    Check(ExampleModified(preset), "Parameter changes mark an example modified");
    const auto restored = Deserialize(Serialize(preset));
    Check(restored.example_id == 3 && ExampleModified(restored), "Example identity survives saving and reopening");
    preset.parameters[0].value -= .25;
    Check(!ExampleModified(preset), "Restoring parameters clears modified status");
    preset.example_id = 8; Reject([&] { Validate(preset); });
    Near(Evaluator("-2^2", {}).At(0), -4, "Unary precedence");
    Near(Evaluator("2^3^2", {}).At(0), 512, "Power associativity");
    Near(Evaluator("2^-2", {}).At(0), .25, "Negative exponent");
    Near(Evaluator("sin(90)", {}, true).At(0), 1, "Degree conversion");
    Near(Evaluator("x<0 ? 0 : sqrt(x)", {}).At(-1), 0, "Lazy conditional");
    Reject([] { Evaluator("x=2", {}); }); Reject([] { Evaluator("foo(x)", {}); });
    Check(UnknownParameters("2e3*sin(x)+gain", {}).size() == 1, "Parameter discovery");
    auto e = Example(2); auto p = CalculatePlots(e);
    Check(p.curves[0].segments.size() > 1, "Reciprocal gap");
    for (const auto& segment : p.curves[0].segments)
      for (std::size_t i = 1; i < segment.size(); ++i) Check(segment[i-1].x * segment[i].x >= 0, "No bridge over pole");
    e = Example(0); const auto r = CalculateSignal(e);
    Check(r.error.empty(), "Signal generation");
    for (int i = 0; i < 3; ++i) Near(r.amplitude[(2*i+1)*8], 4/(Pi*(2*i+1)), "Harmonic amplitude");
    Near(r.phase[8], -90, "Sine phase reference");
    e.signal.window = Window::Hann; Near(CalculateSignal(e).amplitude[8], 4/Pi, "Hann gain");
    BeginReconstruction(e); LowPass(e, 8); Near(CalculateSignal(e).rms, 4/(5*Pi*std::sqrt(2)), "Low-pass reconstruction");
    LowPass(e, 128); Near(CalculateSignal(e).rms, 0, "Unmodified reconstruction");
    const auto document = Serialize(e); Check(Serialize(Deserialize(document)) == document, "Document round trip");
    e.signal.edits[8] = {100, 50}; const auto edited = CalculateSignal(e); Check(edited.coefficients[1016] == std::conj(edited.coefficients[8]), "Hermitian symmetry");
    ApplyReconstruction(e); Check(e.signal.source == Source::Samples && !e.signal.reconstruct, "Commit reconstruction");
    std::vector<std::complex<double>> input(32);
    for (int i = 0; i < 32; ++i) input[i] = std::sin(i*.7)+.3*std::cos(i*1.2);
    const auto bins = Transform(input), inverse = Transform(bins, true);
    for (int k = 0; k < 32; ++k) {
      std::complex<double> expected{};
      for (int n = 0; n < 32; ++n) expected += input[n]*std::polar(1.0,-2*Pi*k*n/32);
      Check(std::abs(bins[k]-expected)<1e-10, "Direct DFT comparison");
      Check(std::abs(inverse[k]-input[k])<1e-10, "FFT round trip");
    }
    e = Example(0); const auto csv = ExportCsv(e, false); auto rows = ParseCsv(csv); ImportCsv(e, rows, 0, 1);
    Near(CalculateSignal(e).amplitude[8], 4/Pi, "CSV round trip");
    rows[3][0] += .1; Reject([&] { ImportCsv(e, rows, 0, 1); });
    Reject([&] { ParseCsv("0,NaN\n" + csv); });
    Check(ExportSvg(Example(1), 0).find("<svg") == 0, "SVG export");
    e = Example(0); e.signal.source = Source::Expression; e.signal.expression = "sqrt(-1)";
    Check(!CalculateSignal(e).error.empty(), "Undefined sample");
    e = Example(0); e.signal.source = Source::Mixer; e.signal.oscillators = {{0, Wave::Constant, 2}};
    Near(CalculateSignal(e).amplitude[0], 2, "DC scaling");
    e.signal.oscillators = {{0, Wave::Sine, 1, 128, 90}};
    Near(CalculateSignal(e).amplitude[512], 1, "Nyquist scaling");
    e = Example(1); e.expressions = {{0, Model::Explicit, "sin(x)"}}; e.degrees = true; e.view = {0, 360, -1, 1};
    SampleFunction(e, 0); Near(e.signal.samples[256], 1, "Sampling preserves degree mode and coordinate interval");
    std::cout << "Expression, sampling, FFT/IFFT, document, CSV and SVG checks passed.\n";
    return 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}

#include "graph_ui.h"
#include <algorithm>

namespace graph::ui {
namespace {
Expression* Find(Experiment& e, int id) {
  const auto found = std::ranges::find(e.expressions, id, &Expression::id);
  return found == e.expressions.end() ? nullptr : &*found;
}
Oscillator* FindWave(Experiment& e, int id) {
  const auto found = std::ranges::find(e.signal.oscillators, id, &Oscillator::id);
  return found == e.signal.oscillators.end() ? nullptr : &*found;
}
}
[[huxerui::composable]]
static View ExpressionRow(Expression expression) {
  const auto c = UseEnvironment<Context>();
  const auto& t = UseTheme(); const int id = expression.id;
  const auto mutate = [=](auto update) { Change(c, [=](auto& e) { if (auto* value = Find(e, id)) update(*value); }, true, false); };
  std::string error;
  if (c.plots.Get()) for (const auto& curve : c.plots.Get()->curves) if (curve.id == id) error = curve.error;
  const auto missing = UnknownParameters(expression.first + " " + expression.second, c.experiment.Get().parameters);
  Views rows;
  rows.Add(Row {
    Checkbox(expression.visible).OnChanged([=](bool visible) { mutate([=](auto& x) { x.visible = visible; }); })
        .With(Semantics{.label = "f" + std::to_string(id + 1)}),
    Text("f" + std::to_string(id + 1)).Style({Font::System(14), CurveColor(id, c.experiment.Get().dark)}),
    Select(std::vector<StringVariant>{app::strings::explicit_model, app::strings::parametric, app::strings::polar}, static_cast<int>(expression.model),
        [](const auto& name) { return Text(name); }).Label(app::strings::model)
        .OnChanged([=](std::size_t model) { mutate([=](auto& x) { x.model = static_cast<Model>(model); }); }).With(Grow()),
    expression.model == Model::Explicit ? Icon(app::images::sample, app::strings::sample_function, [=] {
      if (c.experiment.Get().signal.reconstruct) { FinishReconstruction(c); return; }
      Change(c, [=](auto& e) { SampleFunction(e, id); });
    }) : View(),
    Icon(app::images::close, app::strings::remove, [=] { Change(c, [=](auto& e) { std::erase_if(e.expressions, [=](const auto& x) { return x.id == id; }); }, true, false); }),
  }.With(Spacing(4), CrossAlign(CrossAxisAlignment::Center)));
  rows.Add(FormulaField(expression.model == Model::Explicit ? StringVariant("y(x)") : expression.model == Model::Polar ? StringVariant("r(theta)") : StringVariant("x(t)"), expression.first,
      [=](std::string text) { mutate([=](auto& x) { x.first = text; }); }));
  if (expression.model == Model::Parametric) rows.Add(FormulaField("y(t)", expression.second, [=](std::string text) { mutate([=](auto& x) { x.second = text; }); }));
  if (expression.model != Model::Explicit) {
    rows.Add(NumberControl(app::strings::from, expression.from, -100, expression.to - .001, .01,
        [=](double value) { mutate([=](auto& x) { x.from = value; }); }));
    rows.Add(NumberControl(app::strings::to, expression.to, expression.from + .001, 100, .01,
        [=](double value) { mutate([=](auto& x) { x.to = value; }); }));
  }
  if (!error.empty()) rows.Add(Label(error, 12).With(Semantics{.live_region = SemanticLiveRegion::Polite}));
  if (!missing.empty()) rows.Add(Button(app::strings::add_missing).OnClick([=] {
    Change(c, [=](auto& e) { for (const auto& name : missing) if (e.parameters.size() < 32 &&
        std::ranges::none_of(e.parameters, [&](const auto& p) { return p.name == name; })) e.parameters.push_back({name, 1}); });
  }));
  return Column {std::move(rows)}.With(Padding(UseViewportClass() == ViewportClass::Compact ? 12 : 8), Spacing(6), CrossAlign(CrossAxisAlignment::Stretch),
      Background(t.colors.surface), Border{t.colors.outline, 1}, CornerRadius(10));
}
[[huxerui::composable]]
static View FunctionEditor() {
  const auto c = UseEnvironment<Context>();
  const auto& e = c.experiment.Get();
  Views rows;
  if (UseViewportClass() != ViewportClass::Compact) {
    rows.Add(Label(app::strings::expressions, 16, false, true));
  }
  for (const auto& x : e.expressions) rows.Add(ExpressionRow(x).Key(x.id));
  rows.Add(Button(app::strings::add_expression).With(Enabled(e.expressions.size() < 24)).OnClick([=] {
    Change(c, [](auto& next) {
      int id = 0; for (const auto& x : next.expressions) id = std::max(id, x.id + 1);
      next.expressions.push_back({id, Model::Explicit, "sin(x)"});
    }, true, false);
  }));
  rows.Add(Divider());
  rows.Add(Row {Label(app::strings::parameters, 16, false, true).With(Grow()),
    Icon(app::images::plus, app::strings::add_parameter, [=] { ShowParameter(c); })}.With(CrossAlign(CrossAxisAlignment::Center)));
  for (const auto& p : e.parameters) rows.Add(Column {
    NumberControl(p.name, p.value, p.minimum, p.maximum, p.step, [=](double value) {
      Change(c, [=](auto& next) { for (auto& parameter : next.parameters) if (parameter.name == p.name) parameter.value = value; });
    }),
    Button(app::strings::parameter_settings).OnClick([=] { ShowParameter(c, p.name); }),
  }.With(Spacing(4), CrossAlign(CrossAxisAlignment::Stretch)).Key(p.name));
  return Column {std::move(rows)}.With(Spacing(UseViewportClass() == ViewportClass::Compact ? 14 : 8), CrossAlign(CrossAxisAlignment::Stretch));
}
[[huxerui::composable]]
static View WaveRow(Oscillator wave) {
  const auto c = UseEnvironment<Context>(); const int id = wave.id; const auto& t = UseTheme();
  const auto mutate = [=](auto update) { Change(c, [=](auto& e) { if (auto* value = FindWave(e, id)) update(*value); }); };
  return Column {
    Row {Checkbox(wave.enabled).OnChanged([=](bool value) { mutate([=](auto& w) { w.enabled = value; }); }),
      Label("#" + std::to_string(id + 1), 15, false, true), Spacer(),
      Icon(app::images::plus, app::strings::duplicate, [=] { Change(c, [=](auto& e) {
        if (e.signal.oscillators.size() >= 16) return;
        auto copy = wave; for (const auto& o : e.signal.oscillators) copy.id = std::max(copy.id, o.id + 1); e.signal.oscillators.push_back(copy);
      }); }),
      Icon(app::images::close, app::strings::remove, [=] { Change(c, [=](auto& e) { std::erase_if(e.signal.oscillators, [=](const auto& w) { return w.id == id; }); }); }),
    }.With(CrossAlign(CrossAxisAlignment::Center)),
    Choice(app::strings::wave, {app::strings::sine, app::strings::square, app::strings::triangle, app::strings::saw, app::strings::constant}, static_cast<int>(wave.wave),
        [=](std::size_t value) { mutate([=](auto& w) { w.wave = static_cast<Wave>(value); }); }),
    NumberControl(app::strings::amplitude, wave.amplitude, -4, 4, .05, [=](double value) { mutate([=](auto& w) { w.amplitude = value; }); }),
    wave.wave == Wave::Constant ? View() : NumberControl(app::strings::frequency, wave.frequency, .1, 128, .1, [=](double value) { mutate([=](auto& w) { w.frequency = value; }); }),
    wave.wave == Wave::Constant ? View() : NumberControl(app::strings::phase, wave.phase, -180, 180, 1, [=](double value) { mutate([=](auto& w) { w.phase = value; }); }),
  }.With(Padding(12), Spacing(10), CrossAlign(CrossAxisAlignment::Stretch), Background(t.colors.surface), Border{t.colors.outline, 1}, CornerRadius(10));
}
[[huxerui::composable]]
static View SignalEditor() {
  const auto c = UseEnvironment<Context>(); const auto& s = c.experiment.Get().signal;
  const auto cutoff = UseState(8.0);
  Views rows;
  if (UseViewportClass() != ViewportClass::Compact) rows.Add(Label(app::strings::signal_controls, 16, false, true));
  if (s.reconstruct) {
    rows.Add(Label(app::strings::reconstruction_hint, 14, true));
    rows.Add(Button(app::strings::edit_bin).OnClick([=] { ShowBin(c); }));
    rows.Add(NumberControl(app::strings::cutoff, cutoff.Get(), 0, s.reconstruction_rate / 2, .25,
        [=](double value) { cutoff = value; Change(c, [=](auto& e) { LowPass(e, value); }); }));
    rows.Add(PrimaryButton(app::strings::apply, [=] { Change(c, [](auto& e) { ApplyReconstruction(e); }); }));
    rows.Add(Button(app::strings::discard).OnClick([=] { Change(c, [](auto& e) { e.signal.reconstruct = false; e.signal.edits.clear(); e.signal.reconstruction_source.clear(); }); }));
  } else {
    rows.Add(Choice(app::strings::source, {app::strings::series, app::strings::mixer, app::strings::expression, app::strings::samples}, static_cast<int>(s.source),
        [=](std::size_t value) {
          if (value == 3 && c.experiment.Get().signal.samples.empty()) { Import(c); return; }
          Change(c, [=](auto& e) { e.signal.source = static_cast<Source>(value); });
        }));
    if (s.source == Source::Series) {
      rows.Add(Text("Σ 4/(πh) · sin(2πhf₀t)").Style({Font::System(18), UseTheme().colors.primary})
          .Align(TextAlign::Center).With(Padding(UseViewportClass() == ViewportClass::Compact ? 14 : 10), Background(UseTheme().colors.primary_container), CornerRadius(10)));
      rows.Add(NumberControl(app::strings::terms, s.terms, 1, 32, 1, [=](double value) { Change(c, [=](auto& e) { e.signal.terms = static_cast<int>(value); }); }));
      rows.Add(NumberControl(app::strings::fundamental, s.fundamental, .1, 32, .1, [=](double value) { Change(c, [=](auto& e) { e.signal.fundamental = value; }); }));
      rows.Add(NumberControl(app::strings::amplitude, s.amplitude, .1, 4, .05, [=](double value) { Change(c, [=](auto& e) { e.signal.amplitude = value; }); }));
      rows.Add(Label(app::strings::components, 15, false, true));
      for (int i = 0; i < std::min(s.terms, 4); ++i) {
        const int h = 2 * i + 1;
        rows.Add(Row {Text("h" + std::to_string(h)).Style({Font::System(14), CurveColor(i, c.experiment.Get().dark)}), Spacer(),
            Label(Number(s.fundamental * h) + " Hz · A " + Number(4 * s.amplitude / (Pi * h)), 13, true)}
            .With(Padding(UseViewportClass() == ViewportClass::Compact ? 12 : 6), Background(UseTheme().colors.surface), Border{UseTheme().colors.outline, 1}, CornerRadius(8)));
      }
    } else if (s.source == Source::Mixer) {
      for (const auto& wave : s.oscillators) rows.Add(WaveRow(wave).Key(wave.id));
      rows.Add(Button(app::strings::add_wave).With(Enabled(s.oscillators.size() < 16)).OnClick([=] { Change(c, [](auto& e) {
        int id = 0; for (const auto& o : e.signal.oscillators) id = std::max(id, o.id + 1); e.signal.oscillators.push_back({id});
      }); }));
    } else if (s.source == Source::Expression) rows.Add(FormulaField("x(t)", s.expression, [=](std::string value) { Change(c, [=](auto& e) { e.signal.expression = value; }); }));
    else rows.Add(Label(std::to_string(s.samples.size()) + " samples", 13, true));
    rows.Add(Divider()); rows.Add(Label(app::strings::sampling, 15, false, true));
    rows.Add(NumberControl(app::strings::sample_rate, s.rate, 1, 4096, 1, [=](double value) { Change(c, [=](auto& e) { e.signal.rate = value; }); }));
    std::vector<StringVariant> sizes; std::vector<int> counts;
    for (int n = 256; n <= 65536; n *= 2) if (s.source != Source::Samples || n <= s.samples.size()) { counts.push_back(n); sizes.emplace_back(std::to_string(n)); }
    rows.Add(Choice(app::strings::sample_count, sizes, std::ranges::find(counts, s.count) - counts.begin(), [=](std::size_t value) { Change(c, [=](auto& e) { e.signal.count = counts[value]; }); }));
    rows.Add(NumberControl(app::strings::start_time, s.start, -10, 10, .01, [=](double value) { Change(c, [=](auto& e) { e.signal.start = value; }); }));
    rows.Add(Choice(app::strings::window, {app::strings::rectangular, app::strings::hann, app::strings::hamming}, static_cast<int>(s.window),
        [=](std::size_t value) { Change(c, [=](auto& e) { e.signal.window = static_cast<Window>(value); }); }));
    rows.Add(Checkbox(app::strings::remove_mean, s.remove_mean).OnChanged([=](bool value) { Change(c, [=](auto& e) { e.signal.remove_mean = value; }); }));
  }
  rows.Add(Checkbox(app::strings::decibels, s.decibels).OnChanged([=](bool value) { Change(c, [=](auto& e) {
    e.signal.decibels = value; e.frequency_view.ymin = value ? -120 : 0; e.frequency_view.ymax = value ? 10 : 1.6;
  }); }));
  return Column {std::move(rows)}.With(Spacing(UseViewportClass() == ViewportClass::Compact ? 14 : 8), CrossAlign(CrossAxisAlignment::Stretch));
}
[[huxerui::composable]]
View Editor(int mode) {
  return ScrollView {mode == 0 ? FunctionEditor() : SignalEditor()}
      .With(Padding(UseViewportClass() == ViewportClass::Compact ? 20 : 16), Background(UseTheme().colors.surface_container_low), ScrollBar());
}
[[huxerui::composable]]
View Settings() {
  const auto c = UseEnvironment<Context>(); const auto& e = c.experiment.Get();
  return Column {
    Choice(app::strings::angle_unit, {app::strings::radians, app::strings::degrees}, e.degrees ? 1 : 0,
        [=](std::size_t value) { Change(c, [=](auto& next) { next.degrees = value != 0; }, true, false); }),
    Switch(app::strings::equal_axes, e.equal_axes).OnChanged([=](bool value) { Change(c, [=](auto& next) { next.equal_axes = value; }, true, false); }),
    Button(app::strings::numeric_view).OnClick([=] { ShowRanges(c, e.mode == 0 ? 0 : c.selected_plot.Get() == 2 ? 2 : 1); }),
    Button(app::strings::retry).OnClick([=] { SaveLater(c); }),
    Divider(), Label(app::strings::about, 17, false, true), Label(app::strings::about_text, 14, true),
  }.With(Spacing(16), CrossAlign(CrossAxisAlignment::Stretch));
}
} // namespace graph::ui

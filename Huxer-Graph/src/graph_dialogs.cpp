#include "graph_ui.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace graph::ui {
namespace {
using Fields = std::vector<std::pair<StringVariant, std::string>>;
double Parse(const std::string& text) {
  std::istringstream stream(text); stream.imbue(std::locale::classic()); double value = 0;
  stream >> value; const bool parsed = !stream.fail(); stream >> std::ws;
  if (!parsed || !stream.eof() || !std::isfinite(value)) throw std::runtime_error("Enter a finite number.");
  return value;
}
}
[[huxerui::composable]]
static View Form(DialogContext dialog, StringVariant title, Fields fields, std::function<void(std::vector<std::string>)> submit,
                 StringVariant hint = {}, std::function<void()> remove = {}) {
  std::vector<TextEditingValue> initial; for (const auto& field : fields) initial.push_back(TextEditingValue::FromText(field.second));
  const auto values = UseState(initial); const auto error = UseState(std::string{});
  const auto apply = [=] {
    try { std::vector<std::string> text; for (const auto& value : values.Get()) text.push_back(value.text); submit(text); dialog.Dismiss(); }
    catch (const std::exception& failure) { error = failure.what(); }
  };
  Views rows; rows.Add(Label(title, 21, false, true));
  for (std::size_t i = 0; i < fields.size(); ++i) rows.Add(TextField(values.Get()[i]).Label(fields[i].first)
      .OnChanged([=](const TextEditingValue& value) { values.Update([=](auto& v) { v[i] = value; }); }).OnSubmitted(apply).Key(i));
  rows.Add(Label(hint, 13, true));
  if (!error.Get().empty()) rows.Add(Label(error.Get(), 13));
  if (remove) rows.Add(Button(app::strings::remove).OnClick([=] { remove(); dialog.Dismiss(); }));
  rows.Add(Row {Button(app::strings::cancel).OnClick([=] { dialog.Dismiss(); }), Spacer(), PrimaryButton(app::strings::apply, apply)}.With(Spacing(12)));
  return ScrollView {Column {std::move(rows)}.With(Padding(24), Spacing(16), CrossAlign(CrossAxisAlignment::Stretch))}
      .With(Frame{.max_width = 440, .max_height = 620}, Background(UseTheme().colors.surface), CornerRadius(16), SafeAreaPadding());
}
void ShowMessage(const Context& c, StringVariant title, StringVariant content) { c.dialogs->Show(title, content, app::strings::done); }
void ShowEditor(const Context& c, int mode) {
  c.sheets->Show([=](BottomSheetContext sheet) {
    return ProvideEnvironment(c, Column {
      Row {Label(mode == 0 ? app::strings::expressions : app::strings::signal_controls, 18, false, true).With(Grow()),
        Icon(app::images::close, app::strings::close, [=] { EndEdit(c); sheet.Dismiss(); })}
          .With(Padding(EdgeInsets::Symmetric(20, 8)), CrossAlign(CrossAxisAlignment::Center)),
      Editor(mode).With(Grow()),
    }.With(Frame{.height = 540, .max_width = 640}, CrossAlign(CrossAxisAlignment::Stretch), SafeAreaPadding()));
  });
}
void ShowParameter(const Context& c, std::string name) {
  Parameter p;
  if (!name.empty()) {
    for (const auto& current : c.experiment.Get().parameters) if (current.name == name) p = current;
  } else p.name = "d";
  const auto commit = [=](const std::vector<std::string>& values) {
    Parameter next{values[0], Parse(values[1]), Parse(values[2]), Parse(values[3]), Parse(values[4])};
    auto model = c.experiment.Get();
    if (name.empty()) model.parameters.push_back(next);
    else for (auto& value : model.parameters) if (value.name == name) value = next;
    Validate(model); Change(c, [=](auto& e) { e.parameters = model.parameters; });
  };
  c.dialogs->Show([=](DialogContext dialog) {
    return ProvideEnvironment(c, Form(dialog, app::strings::parameter_settings,
      {{app::strings::name, p.name}, {app::strings::value, Number(p.value, 15)}, {app::strings::minimum, Number(p.minimum, 15)},
       {app::strings::maximum, Number(p.maximum, 15)}, {app::strings::step, Number(p.step, 15)}}, commit, {},
      name.empty() ? std::function<void()>{} : std::function<void()>([=] { Change(c, [=](auto& e) { std::erase_if(e.parameters, [&](const auto& p) { return p.name == name; }); }); })));
  });
}
void ShowRanges(const Context& c, int plot) {
  const auto& e = c.experiment.Get(); const auto v = plot == 0 ? e.view : plot == 1 ? e.time_view : e.frequency_view;
  c.dialogs->Show([=](DialogContext dialog) {
    return ProvideEnvironment(c, Form(dialog, app::strings::numeric_view,
      {{"x min", Number(v.xmin, 15)}, {"x max", Number(v.xmax, 15)}, {"y min", Number(v.ymin, 15)}, {"y max", Number(v.ymax, 15)}},
      [=](const auto& values) { const Viewport next{Parse(values[0]), Parse(values[1]), Parse(values[2]), Parse(values[3])};
        auto model = c.experiment.Get(); (plot == 0 ? model.view : plot == 1 ? model.time_view : model.frequency_view) = next;
        Validate(model); SetViewport(c, plot, next);
      }));
  });
}
void ShowRename(const Context& c) {
  c.dialogs->Show([=](DialogContext dialog) {
    return ProvideEnvironment(c, Form(dialog, app::strings::rename, {{app::strings::name, c.experiment.Get().name}}, [=](const auto& values) {
      auto next = c.experiment.Get(); next.name = values[0]; Validate(next); Change(c, [=](auto& e) { e.name = values[0]; });
    }));
  });
}
void ConfirmReplace(const Context& c, Experiment next) {
  c.dialogs->Show(app::strings::replace_confirm, app::strings::replace_hint, app::strings::apply, app::strings::cancel,
      [=] { EndEdit(c); Change(c, [=](auto& e) { e = next; }); });
}
[[huxerui::composable]]
static View ReconstructionChoices(const Context& c, DialogContext dialog) {
  return Column {
    Label(app::strings::reconstruction_changes, 21, false, true), Label(app::strings::reconstruction_choice, 14, true),
    Button(app::strings::apply).OnClick([=] { Change(c, [](auto& e) { ApplyReconstruction(e); }); dialog.Dismiss(); }),
    Button(app::strings::discard).OnClick([=] { Change(c, [](auto& e) { e.signal.reconstruct = false; e.signal.edits.clear(); e.signal.reconstruction_source.clear(); }); dialog.Dismiss(); }),
    Button(app::strings::cancel).OnClick([=] { dialog.Dismiss(); }),
  }.With(Padding(24), Spacing(16), Frame{.max_width = 420}, CrossAlign(CrossAxisAlignment::Stretch),
      Background(UseTheme().colors.surface), CornerRadius(16));
}
void FinishReconstruction(const Context& c) {
  c.dialogs->Show([=](DialogContext dialog) { return ProvideEnvironment(c, ReconstructionChoices(c, dialog)); });
}
void ShowBin(const Context& c) {
  if (!c.experiment.Get().signal.reconstruct) return;
  const auto result = CalculateSignal(c.experiment.Get());
  if (!result.error.empty()) { c.error = result.error; return; }
  const int bin = std::clamp(c.selected_bin.Get(), 0, static_cast<int>(result.amplitude.size() - 1));
  const bool real = bin == 0 || bin == result.raw.size() / 2;
  c.dialogs->Show([=](DialogContext dialog) {
    return ProvideEnvironment(c, Form(dialog, app::strings::edit_bin,
      {{app::strings::bin, std::to_string(bin)}, {real ? app::strings::signed_real : app::strings::amplitude,
        Number(real ? result.coefficients[bin].real() : result.amplitude[bin], 15)},
       {app::strings::phase, Number(real ? 0 : result.phase[bin], 15)}},
      [=](const auto& values) {
        const double index = Parse(values[0]); const int k = static_cast<int>(index);
        const double amplitude = Parse(values[1]), phase = Parse(values[2]);
        if (index != k || k < 0 || k >= result.amplitude.size()) throw std::runtime_error("Choose a valid integer bin index.");
        const bool target_real = k == 0 || k == result.raw.size() / 2;
        if (target_real != real) throw std::runtime_error("Select DC or Nyquist on the plot to use its signed real editor.");
        if (target_real && phase != 0) throw std::runtime_error("DC and Nyquist coefficients must be real; keep phase at zero.");
        if (!target_real && amplitude < 0) throw std::runtime_error("Amplitude must be nonnegative.");
        const auto value = target_real ? std::complex<double>(amplitude, 0) : std::polar(amplitude * result.raw.size() / 2, phase * Pi / 180);
        Change(c, [=](auto& e) { e.signal.edits[k] = value; }); c.selected_bin = k;
      }, app::strings::reconstruction_hint));
  });
}
static Task<std::string> ReadBounded(FileReference reference) {
  auto opened = co_await reference.OpenReadAsync();
  if (!opened.Succeeded()) throw std::runtime_error("Could not open the selected file.");
  auto stream = std::move(opened).Value(); std::string text;
  while (true) {
    auto bytes = co_await stream.ReadAsync(65536);
    if (!bytes.Succeeded()) throw std::runtime_error("Could not read the selected file.");
    if (bytes.Value().empty()) break;
    if (text.size() + bytes.Value().size() > 10 * 1024 * 1024) throw std::runtime_error("Files must not exceed 10 MiB.");
    text.append(reinterpret_cast<const char*>(bytes.Value().data()), bytes.Value().size());
  }
  co_return text;
}
void OpenProject(const Context& c) {
  c.tasks.Launch([c]() -> Task<void> {
    try {
      auto file = co_await c.picker->OpenFileAsync({.name = "Huxer Graph", .extensions = {"hgraph"}});
      if (!file) co_return;
      auto text = co_await ReadBounded(*file); auto e = Deserialize(text); ConfirmReplace(c, std::move(e));
    } catch (const std::exception& error) { c.error = error.what(); }
  });
}
void Export(const Context& c, int kind) {
  const auto snapshot = c.experiment.Get(); const int plot = snapshot.mode == 0 ? 0 : c.selected_plot.Get() == 2 ? 2 : 1;
  c.tasks.Launch([c, snapshot, kind, plot]() -> Task<void> {
    try {
      std::string text = kind == 0 ? Serialize(snapshot) : kind == 1 ? ExportSvg(snapshot, plot) : ExportCsv(snapshot, kind == 3);
      const std::string extension = kind == 0 ? "hgraph" : kind == 1 ? "svg" : "csv";
      const std::string name = "experiment." + extension;
      const auto file = c.directories.temporary_directory.Child(name);
      if (!co_await file.WriteStringAsync(text)) throw std::runtime_error("Could not prepare the export.");
      if (!co_await c.picker->SaveFileAsync(file, {.suggested_name = name, .filter = {.name = "Huxer Graph", .extensions = {extension}}}))
        ShowMessage(c, app::strings::save_copy, app::strings::export_incomplete);
    } catch (const std::exception& error) { c.error = error.what(); }
  });
}
void Import(const Context& c) {
  if (c.experiment.Get().signal.reconstruct) { FinishReconstruction(c); return; }
  c.tasks.Launch([c]() -> Task<void> {
    try {
      auto file = co_await c.picker->OpenFileAsync({.name = "CSV", .extensions = {"csv", "txt"}});
      if (!file) co_return;
      auto text = co_await ReadBounded(*file);
      const auto rows = std::make_shared<const std::vector<std::vector<double>>>(ParseCsv(text));
      c.dialogs->Show([=](DialogContext dialog) {
        return ProvideEnvironment(c, Form(dialog, app::strings::csv_mapping,
          {{app::strings::time_column, rows->front().size() > 1 ? "1" : "0"}, {app::strings::value_column, rows->front().size() > 1 ? "2" : "1"}},
          [=](const auto& values) {
            const double t = Parse(values[0]), a = Parse(values[1]);
            if (std::floor(t) != t || std::floor(a) != a || t < 0 || a < 1 || t > 16 || a > 16) throw std::runtime_error("Use 1-based columns; zero means no time column.");
            auto next = c.experiment.Get(); ImportCsv(next, *rows, static_cast<int>(t) - 1, static_cast<int>(a) - 1);
            Change(c, [=](auto& e) { e = next; }); ResetView(c, 1, true); ResetView(c, 2, true);
          }, app::strings::csv_hint));
      });
    } catch (const std::exception& error) { c.error = error.what(); }
  });
}
} // namespace graph::ui

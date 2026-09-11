#pragma once
#include "graph_math.h"
#include <huxerui/huxerui.h>
#include <app_resources.h>
#include <functional>
#include <optional>

namespace graph::ui {
using namespace huxerui;
struct History {
  std::vector<Experiment> undo, redo;
  std::optional<Experiment> transaction;
};
struct Jobs {
  TaskHandle plots, signal, save;
  std::stop_source plot_stop, signal_stop;
  int save_generation = 0;
  bool saving = false;
  bool save_failed = false;
};
struct Context {
  State<Experiment> experiment;
  State<std::shared_ptr<const PlotResult>> plots;
  State<std::shared_ptr<const SignalResult>> signal;
  State<int> revision, plot_generation, signal_generation;
  State<bool> ready, computing, trace, formula_focused;
  State<std::string> error;
  State<StringVariant> status;
  State<int> selected_plot, selected_curve, selected_bin;
  std::shared_ptr<History> history;
  std::shared_ptr<Jobs> jobs;
  TaskScope tasks;
  AppDirectories directories;
  std::shared_ptr<FilePicker> picker;
  std::shared_ptr<DialogHandle> dialogs;
  std::shared_ptr<BottomSheetHandle> sheets;
  static Context Default() { throw std::logic_error("Graph context is not installed."); }
  bool operator==(const Context& other) const noexcept { return history == other.history; }
};
View App();
bool Desktop();
View Label(StringVariant text, float size = 14, bool muted = false, bool bold = false);
View Icon(ImageResource icon, StringVariant label, std::function<void()> action);
View PrimaryButton(StringVariant label, std::function<void()> action);
View HistoryActions();
View Choice(StringVariant label, std::vector<StringVariant> items, std::size_t selected, std::function<void(std::size_t)> change);
View NumberControl(StringVariant label, double value, double min, double max, double step, std::function<void(double)> change);
View FormulaField(StringVariant label, std::string value, std::function<void(std::string)> change);
View Editor(int mode);
View Workspace(int mode);
View Plot(int kind);
View Settings();
void Change(const Context& c, std::function<void(Experiment&)> update, bool history = true, bool recompute_signal = true);
void Refresh(const Context& c, bool recompute_signal = true);
void SaveLater(const Context& c);
void Undo(const Context& c, bool redo = false);
void BeginEdit(const Context& c);
void EndEdit(const Context& c, bool cancel = false);
void OpenProject(const Context& c);
void Export(const Context& c, int kind);
void Import(const Context& c);
void ShowEditor(const Context& c, int mode);
void ShowParameter(const Context& c, std::string name = "");
void ShowBin(const Context& c);
void ShowRanges(const Context& c, int plot);
void ShowRename(const Context& c);
void ConfirmReplace(const Context& c, Experiment experiment);
void FinishReconstruction(const Context& c);
void SetViewport(const Context& c, int kind, Viewport value);
void ResetView(const Context& c, int kind, bool fit = false);
void ShowMessage(const Context& c, StringVariant title, StringVariant content);
Color CurveColor(int id, bool dark = false);
} // namespace graph::ui

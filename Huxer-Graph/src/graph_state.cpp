#include "graph_ui.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace graph::ui {
using namespace std::chrono_literals;
void SaveLater(const Context& c) {
  ++c.jobs->save_generation;
  c.status = app::strings::unsaved;
  if (c.jobs->saving) return;
  c.jobs->saving = true;
  c.jobs->save_failed = false;
  c.jobs->save = c.tasks.Launch([c]() -> Task<void> {
    try {
      while (true) {
        const auto generation = c.jobs->save_generation;
        co_await Delay(700ms);
        if (generation != c.jobs->save_generation) continue;
        const auto snapshot = c.experiment.Get();
        const auto text = Serialize(snapshot);
        c.status = app::strings::saving;
        const auto current = c.directories.data_directory.Child("experiment.hgraph");
        const auto previous = c.directories.data_directory.Child("previous.hgraph");
        const auto pending = c.directories.data_directory.Child("pending.hgraph");
        if (!co_await pending.WriteStringAsync(text)) throw std::runtime_error("Could not write the experiment.");
        if ((co_await current.StatAsync()).Succeeded()) {
          if (!co_await current.CopyToAsync(previous, true)) throw std::runtime_error("Could not retain the previous save.");
        }
        if (!co_await pending.MoveToAsync(current, true)) throw std::runtime_error("Could not finish saving the experiment.");
        c.status = app::strings::saved;
        if (generation == c.jobs->save_generation) break;
      }
    } catch (const std::exception& error) { c.jobs->save_failed = true; c.status = app::strings::save_failed; c.error = error.what(); }
    c.jobs->saving = false;
  });
}
void Refresh(const Context& c, bool recompute_signal) {
  const auto snapshot = c.experiment.Get();
  c.jobs->plots.Cancel();
  c.jobs->plot_stop.request_stop(); c.jobs->plot_stop = std::stop_source{};
  const auto plot_stop = c.jobs->plot_stop.get_token();
  const int generation = ++c.plot_generation;
  c.computing = true;
  c.jobs->plots = c.tasks.Launch([c, snapshot, generation, plot_stop]() -> Task<void> {
    co_await Delay(35ms);
    try {
#if defined(__EMSCRIPTEN__)
      auto result = CalculatePlots(snapshot);
#else
      auto result = co_await RunWorker([snapshot, plot_stop] { return CalculatePlots(snapshot, plot_stop); });
#endif
      if (generation == c.plot_generation.Get()) { c.plots = std::make_shared<const PlotResult>(std::move(result)); c.computing = false; }
    } catch (const std::exception& error) { c.error = error.what(); c.computing = false; }
  });
  if (!recompute_signal) return;
  c.jobs->signal.Cancel();
  c.jobs->signal_stop.request_stop(); c.jobs->signal_stop = std::stop_source{};
  const auto signal_stop = c.jobs->signal_stop.get_token();
  const int signal_generation = ++c.signal_generation;
  c.jobs->signal = c.tasks.Launch([c, snapshot, signal_generation, signal_stop]() -> Task<void> {
    co_await Delay(35ms);
#if defined(__EMSCRIPTEN__)
    auto result = CalculateSignal(snapshot);
#else
    auto result = co_await RunWorker([snapshot, signal_stop] { return CalculateSignal(snapshot, signal_stop); });
#endif
    if (signal_generation == c.signal_generation.Get()) c.signal = std::make_shared<const SignalResult>(std::move(result));
  });
}
void Change(const Context& c, std::function<void(Experiment&)> update, bool history, bool recompute_signal) {
  try {
    auto next = c.experiment.Get(); update(next); Validate(next);
    if (history && !c.history->transaction) {
      c.history->undo.push_back(c.experiment.Get());
      if (c.history->undo.size() > 60) c.history->undo.erase(c.history->undo.begin());
      c.history->redo.clear();
    }
    c.experiment = std::move(next); ++c.revision;
    c.error = std::string{}; Refresh(c, recompute_signal); SaveLater(c);
  } catch (const std::exception& error) { c.error = error.what(); }
}
void BeginEdit(const Context& c) { if (!c.history->transaction) c.history->transaction = c.experiment.Get(); }
void EndEdit(const Context& c, bool cancel) {
  if (!c.history->transaction) return;
  auto before = std::move(*c.history->transaction); c.history->transaction.reset();
  if (cancel) { c.experiment = std::move(before); Refresh(c); SaveLater(c); }
  else if (Serialize(before) != Serialize(c.experiment.Get())) {
    c.history->undo.push_back(std::move(before));
    if (c.history->undo.size() > 60) c.history->undo.erase(c.history->undo.begin());
    c.history->redo.clear();
  }
  ++c.revision;
}
void Undo(const Context& c, bool redo) {
  EndEdit(c);
  auto& source = redo ? c.history->redo : c.history->undo;
  auto& destination = redo ? c.history->undo : c.history->redo;
  if (source.empty()) return;
  destination.push_back(c.experiment.Get()); c.experiment = source.back(); source.pop_back();
  ++c.revision; Refresh(c); SaveLater(c);
}
void SetViewport(const Context& c, int kind, Viewport value) {
  Change(c, [=](auto& e) { (kind == 0 ? e.view : kind == 1 ? e.time_view : e.frequency_view) = value; }, false, false);
}
void ResetView(const Context& c, int kind, bool fit) {
  Viewport v = kind == 0 ? Viewport{} : kind == 1 ? Viewport{0, 1.5, -1.8, 1.8} : Viewport{0, 16, 0, 1.6};
  const auto& e = c.experiment.Get();
  if (kind && c.signal.Get() && !c.signal.Get()->raw.empty()) {
    const auto& r = *c.signal.Get();
    if (kind == 1) {
      v.xmin = r.start; v.xmax = r.start + (fit ? r.raw.size() / r.rate : std::min(1.5, r.raw.size() / r.rate));
      double peak = .5; for (double x : r.raw) peak = std::max(peak, std::abs(x)); v.ymin = -peak * 1.2; v.ymax = peak * 1.2;
    } else {
      v.xmax = fit ? r.rate / 2 : std::min(16.0, r.rate / 2); v.ymax = 1.2 * std::max(.1, *std::max_element(r.amplitude.begin(), r.amplitude.end()));
      if (e.signal.decibels) { v.ymin = -120; v.ymax = std::max(10.0, 20 * std::log10(v.ymax)); }
    }
  }
  if (!kind && fit && c.plots.Get()) {
    bool found = false;
    for (const auto& curve : c.plots.Get()->curves) for (const auto& segment : curve.segments) for (auto point : segment) {
      if (std::abs(point.x) > 1e6 || std::abs(point.y) > 1e6) continue;
      if (!found) { v = {point.x, point.x, point.y, point.y}; found = true; }
      else { v.xmin = std::min(v.xmin, point.x); v.xmax = std::max(v.xmax, point.x); v.ymin = std::min(v.ymin, point.y); v.ymax = std::max(v.ymax, point.y); }
    }
    const double dx = std::max(.5, (v.xmax - v.xmin) * .08), dy = std::max(.5, (v.ymax - v.ymin) * .12);
    v.xmin -= dx; v.xmax += dx; v.ymin -= dy; v.ymax += dy;
  }
  SetViewport(c, kind, v);
}
} // namespace graph::ui

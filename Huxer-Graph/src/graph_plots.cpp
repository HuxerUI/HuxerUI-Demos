#include "graph_ui.h"
#include <algorithm>
#include <cmath>

namespace graph::ui {
namespace {
struct Geometry {
  Size size{600, 300};
  Viewport view;
  huxerui::Point pointer{300, 150};
  float left = 50, right = 16, top = 10, bottom = 28;
  float Width() const { return std::max(1.0F, size.width - left - right); }
  float Height() const { return std::max(1.0F, size.height - top - bottom); }
  huxerui::Point Pixel(graph::Point p) const {
    return {static_cast<float>(left + (p.x - view.xmin) / (view.xmax - view.xmin) * Width()),
            static_cast<float>(size.height - bottom - (p.y - view.ymin) / (view.ymax - view.ymin) * Height())};
  }
  graph::Point Value(huxerui::Point p) const {
    return {view.xmin + (p.x - left) / Width() * (view.xmax - view.xmin),
            view.ymax - (p.y - top) / Height() * (view.ymax - view.ymin)};
  }
};
std::vector<double> Ticks(double min, double max, int count) {
  double rough = (max - min) / count, base = std::pow(10, std::floor(std::log10(rough)));
  double scaled = rough / base, step = (scaled > 5 ? 10 : scaled > 2 ? 5 : scaled > 1 ? 2 : 1) * base;
  std::vector<double> values;
  for (double x = std::ceil(min / step) * step; x <= max + step * 1e-8 && values.size() < 40; x += step) values.push_back(x);
  return values;
}
void Zoom(const Context& c, int kind, const Geometry& g, double factor, huxerui::Point anchor, huxerui::Point pan = {}) {
  auto v = g.view; auto at = g.Value(anchor - pan);
  double dx = pan.x / g.Width() * (v.xmax - v.xmin) * factor;
  double dy = pan.y / g.Height() * (v.ymax - v.ymin) * factor;
  Viewport next{at.x + (v.xmin - at.x) * factor - dx, at.x + (v.xmax - at.x) * factor - dx,
                at.y + (v.ymin - at.y) * factor + dy, at.y + (v.ymax - at.y) * factor + dy};
  if (next.xmax - next.xmin < 1e-7 || next.ymax - next.ymin < 1e-7 || next.xmax - next.xmin > 1e10 || next.ymax - next.ymin > 1e10) return;
  SetViewport(c, kind, next);
}
}
[[huxerui::composable]]
View Plot(int kind) {
  const auto c = UseEnvironment<Context>();
  const auto snapshot = c.experiment.Get();
  const auto curves = c.plots.Get(); const auto signal = c.signal.Get();
  const auto geometry = UseState(std::make_shared<Geometry>()).Get();
  auto readout = UseState(std::string{}); auto cursor = UseState(std::optional<graph::Point>{});
  const auto theme = UseTheme();
  const bool compact = UseViewportClass() == ViewportClass::Compact;
  const TextStyle tick{compact ? Font::Monospace(11) : Font::System(12), theme.colors.on_surface_variant};
  const TextShapingOptions tick_shaping{.locale = std::string(UseEnvironment<Locale>().LanguageTag())};
  const float tick_cell = compact ? UseTextMeasurer().MeasureRun("0", tick, tick_shaping).advance : 0;
  const auto view = kind == 0 ? snapshot.view : kind == 1 ? snapshot.time_view : snapshot.frequency_view;
  const auto phase_undefined = UseString(app::strings::phase_undefined);
  const auto inspect = [=](huxerui::Point position) {
    geometry->pointer = position; c.selected_plot = kind;
    auto point = geometry->Value(position); std::string text;
    if (kind == 0 && curves) {
      double nearest = 1e30; graph::Point chosen{}; int selected = -1;
      for (const auto& curve : curves->curves) for (const auto& segment : curve.segments) for (const auto& p : segment) {
        const auto pixel = geometry->Pixel(p); const double distance = std::hypot(pixel.x - position.x, pixel.y - position.y);
        if (distance < nearest) { nearest = distance; chosen = p; selected = curve.id; }
      }
      if (selected >= 0) { c.selected_curve = selected; text = "f" + std::to_string(selected + 1) + " · x " + Number(chosen.x, 6) + " · y " + Number(chosen.y, 6); cursor = chosen; }
    } else if (kind && signal && signal->error.empty() && !signal->raw.empty()) {
      if (kind == 1) {
        const auto n = std::clamp(static_cast<int>(std::round((point.x - signal->start) * signal->rate)), 0, static_cast<int>(signal->raw.size() - 1));
        text = "n " + std::to_string(n) + " · t " + Number(signal->start + n / signal->rate, 6) + " s · A " + Number(signal->raw[n], 6);
        cursor = graph::Point{signal->start + n / signal->rate, signal->raw[n]};
      } else {
        const auto k = std::clamp(static_cast<int>(std::round(point.x * signal->raw.size() / signal->rate)), 0, static_cast<int>(signal->amplitude.size() - 1));
        c.selected_bin = k;
        const double peak = *std::max_element(signal->amplitude.begin(), signal->amplitude.end());
        text = "k " + std::to_string(k) + " · " + Number(k * signal->rate / signal->raw.size(), 6) + " Hz · A " + Number(signal->amplitude[k], 5)
            + " · " + (signal->amplitude[k] > peak * 1e-8 ? Number(signal->phase[k], 5) + "°" : phase_undefined);
        const double a = snapshot.signal.decibels ? 20 * std::log10(std::max(1e-6, signal->amplitude[k])) : signal->amplitude[k];
        cursor = graph::Point{k * signal->rate / signal->raw.size(), a};
      }
    }
    readout = text;
  };
  View canvas = Canvas([=](PaintContext& paint, Size size) {
    geometry->size = size; geometry->view = view;
    geometry->left = compact ? 8 : 50; geometry->right = compact ? 6 : 16;
    for (;;) {
      if (kind == 0 && snapshot.equal_axes) {
        const double extent = (view.xmax - view.xmin) / geometry->Width() * geometry->Height();
        const double center = (view.ymax + view.ymin) / 2;
        geometry->view.ymin = center - extent / 2; geometry->view.ymax = center + extent / 2;
      }
      if (!compact) break;
      float required = 8;
      for (double y : Ticks(geometry->view.ymin, geometry->view.ymax, 4)) {
        required = std::max(required, std::ceil(static_cast<float>(Number(y).size()) * tick_cell) + 8);
      }
      if (required <= geometry->left) break;
      geometry->left = required;
    }
    const auto& g = *geometry; const auto& v = g.view;
    const auto grid = theme.colors.outline;
    for (double x : Ticks(v.xmin, v.xmax, compact ? 4 : 8)) {
      const float px = g.Pixel({x, 0}).x;
      const auto label = Number(x);
      const float width = compact ? std::ceil(static_cast<float>(label.size()) * tick_cell) + 2 : 64;
      const float label_x = compact ? std::clamp(px - width / 2, 2.0F, std::max(2.0F, size.width - width - 2)) : px - 32;
      paint.DrawLine({px, g.top}, {px, size.height - g.bottom}, grid, StrokeStyle{.width = std::abs(x) < 1e-10 ? 1.5F : .6F});
      paint.DrawText({label_x, size.height - 23, width, 20}, label, tick,
          {.shaping = tick_shaping, .align = TextAlign::Center, .wrap = TextWrap::NoWrap});
    }
    for (double y : Ticks(v.ymin, v.ymax, 4)) {
      const float py = g.Pixel({0, y}).y;
      paint.DrawLine({g.left, py}, {size.width - g.right, py}, grid, StrokeStyle{.width = std::abs(y) < 1e-10 ? 1.5F : .6F});
      paint.DrawText({compact ? 4.0F : 0.0F, py - 10, g.left - 8, 20}, Number(y), tick,
          {.shaping = tick_shaping, .align = TextAlign::Trailing, .wrap = TextWrap::NoWrap});
    }
    paint.PushClip({g.left, g.top, g.Width(), g.Height()});
    const auto draw = [&](const std::vector<graph::Point>& points, Color color, bool dashed = false) {
      Path path; bool start = true;
      for (const auto& p : points) {
        const auto pixel = g.Pixel(p);
        if (!std::isfinite(pixel.x) || !std::isfinite(pixel.y) || std::abs(pixel.x) > 1e7 || std::abs(pixel.y) > 1e7) { start = true; continue; }
        if (start) path.MoveTo(pixel); else path.LineTo(pixel); start = false;
      }
      paint.StrokePath(path, color, StrokeStyle{.width = dashed ? 1.3F : 2.2F, .cap = StrokeCap::Round, .join = StrokeJoin::Round, .dash_pattern = dashed ? std::vector<float>{5, 5} : std::vector<float>{}});
    };
    if (kind == 0 && curves) {
      for (const auto& curve : curves->curves) for (const auto& segment : curve.segments) draw(segment, CurveColor(curve.id, snapshot.dark));
    } else if (signal && signal->error.empty() && !signal->raw.empty()) {
      if (kind == 1) {
        if (!snapshot.signal.reconstruct && snapshot.signal.source == Source::Series) {
          std::vector<graph::Point> target;
          for (int i = 0; i <= 1200; ++i) {
            const double x = v.xmin + (v.xmax - v.xmin) * i / 1200;
            target.push_back({x, std::sin(2 * Pi * snapshot.signal.fundamental * x) >= 0 ? snapshot.signal.amplitude : -snapshot.signal.amplitude});
          }
          draw(target, theme.colors.on_surface_variant, true);
        }
        std::vector<graph::Point> points, reconstructed;
        const auto first = std::clamp(static_cast<int>(std::floor((v.xmin - signal->start) * signal->rate)), 0, static_cast<int>(signal->raw.size() - 1));
        const auto last = std::clamp(static_cast<int>(std::ceil((v.xmax - signal->start) * signal->rate)), 0, static_cast<int>(signal->raw.size() - 1));
        for (int i = first; i <= last; ++i) {
          const double x = signal->start + i / signal->rate;
          points.push_back({x, signal->raw[i]});
          if (!signal->reconstructed.empty()) reconstructed.push_back({x, signal->reconstructed[i]});
        }
        draw(points, CurveColor(0, snapshot.dark));
        if (last - first < 60) for (const auto& point : points) paint.DrawCircle(g.Pixel(point), 3, CurveColor(0, snapshot.dark));
        if (!reconstructed.empty()) draw(reconstructed, CurveColor(1, snapshot.dark));
      } else {
        for (std::size_t k = 0; k < signal->amplitude.size(); ++k) {
          const double frequency = k * signal->rate / signal->raw.size();
          if (frequency < v.xmin || frequency > v.xmax) continue;
          const double value = snapshot.signal.decibels ? 20 * std::log10(std::max(1e-6, signal->amplitude[k])) : signal->amplitude[k];
          paint.DrawLine(g.Pixel({frequency, snapshot.signal.decibels ? -120.0 : 0.0}), g.Pixel({frequency, value}), CurveColor(0, snapshot.dark), StrokeStyle{.width = 2});
          if (value > v.ymin + (v.ymax - v.ymin) * .15) paint.DrawCircle(g.Pixel({frequency, value}), 3, CurveColor(0, snapshot.dark));
        }
      }
    }
    if (cursor.Get()) {
      const auto p = g.Pixel(*cursor.Get());
      paint.DrawLine({p.x, g.top}, {p.x, size.height - g.bottom}, theme.colors.on_surface_variant, StrokeStyle{.width = 1, .dash_pattern = {3, 4}});
      paint.DrawCircle(p, 5, theme.colors.primary);
    }
    paint.PopClip();
  }).With(Grow(), Frame{.min_height = 100}, Focusable(), PointerCursor(PointerCursorKind::Crosshair), DragGesture{}, TransformGesture{},
      Semantics{.label = kind == 0 ? app::strings::coordinates : kind == 1 ? app::strings::time_domain : app::strings::spectrum})
      .On<ViewEvents::Hover>([=](const HoverEvent& event) { if (event.type != HoverEventType::Leave) inspect(event.position); })
      .On<ViewEvents::ScrollInput>([=](const ScrollInputEvent& event) {
        Zoom(c, kind, *geometry, std::exp(std::clamp(event.delta_y * .002, -.3, .3)), geometry->pointer); return true;
      }).On<DragEvents::Started>([=](const DragEvent&) { BeginEdit(c); c.selected_plot = kind; })
      .On<DragEvents::Changed>([=](const DragEvent& event) {
        if (c.trace.Get()) inspect(event.position); else Zoom(c, kind, *geometry, 1, event.position, event.delta);
      }).On<DragEvents::Ended>([=](const DragEvent&) { EndEdit(c); })
      .On<DragEvents::Canceled>([=](const DragEvent&) { EndEdit(c); })
      .On<TransformEvents::Started>([=](const TransformEvent&) { BeginEdit(c); })
      .On<TransformEvents::Changed>([=](const TransformEvent& event) {
        Zoom(c, kind, *geometry, 1 / std::max(.01F, event.scale), event.centroid, event.pan);
      }).On<TransformEvents::Ended>([=](const TransformEvent&) { EndEdit(c); })
      .On<TransformEvents::Canceled>([=](const TransformEvent&) { EndEdit(c); });
  const auto zoom = [=](double scale) { Zoom(c, kind, *geometry, scale,
      {geometry->left + geometry->Width() / 2, geometry->top + geometry->Height() / 2}); };
  View title = Label(kind == 0 ? app::strings::coordinates : kind == 1 ? app::strings::time_domain : app::strings::spectrum,
      compact ? 14 : 16, false, true);
  View trace = kind == 0 ? View(Checkbox(app::strings::trace, c.trace.Get())
      .OnChanged([=](bool value) { c.trace = value; })) : View();
  View tools = Row {
    Icon(app::images::minus, app::strings::zoom_out, [=] { zoom(1.35); }),
    Icon(app::images::plus, app::strings::zoom_in, [=] { zoom(1 / 1.35); }),
    Icon(app::images::fit, app::strings::fit, [=] { c.selected_plot = kind; ResetView(c, kind, true); }),
  }.With(Spacing(3), CrossAlign(CrossAxisAlignment::Center));
  View heading = compact && kind == 0 ? View(Column {
    title,
    Row {trace, Spacer(), tools}.With(CrossAlign(CrossAxisAlignment::Center)),
  }.With(Spacing(4), CrossAlign(CrossAxisAlignment::Stretch))) : View(Row {
    View(title).With(Grow()), trace, tools,
  }.With(Spacing(8), CrossAlign(CrossAxisAlignment::Center)));
  return Column {
    View(heading).With(Padding(EdgeInsets::Symmetric(compact ? 14 : 0, 0))),
    canvas,
    Label(readout.Get().empty() ? StringVariant(kind == 0 ? app::strings::inspect_hint : kind == 1 ? app::strings::time_hint : app::strings::frequency_hint) : StringVariant(readout.Get()), compact ? 11 : 12, true)
        .With(Frame{.min_height = compact ? 30.0F : 24.0F}, Padding(EdgeInsets::Symmetric(compact ? 14 : 0, 0)),
            Semantics{.live_region = SemanticLiveRegion::Polite}),
  }.With(Grow(), Spacing(4), CrossAlign(CrossAxisAlignment::Stretch));
}
[[huxerui::composable]]
View Workspace(int mode) {
  const auto c = UseEnvironment<Context>(); const auto& e = c.experiment.Get();
  const bool compact = UseViewportClass() == ViewportClass::Compact;
  const auto scrolling = UseScrollController();
  const auto inset = [=](View child) { return View(child).With(Padding(EdgeInsets::Symmetric(compact ? 14 : 0, 0))); };
  Views content;
  if (mode == 0) {
    if (e.expressions.empty() || std::ranges::none_of(e.expressions, [](const auto& x) { return x.visible; })) {
      content.Add(inset(Column {Label(app::strings::empty, 22, false, true), Label(app::strings::empty_hint, 14, true)}
          .With(Grow(), Spacing(12), MainAlign(MainAxisAlignment::Center), CrossAlign(CrossAxisAlignment::Center))));
    } else content.Add(Plot(0));
  } else {
    auto result = c.signal.Get();
    View legend = Row {
      Text(app::strings::original).Style({Font::System(12), CurveColor(0, e.dark)}),
      e.signal.reconstruct ? View(Text(app::strings::reconstructed).Style({Font::System(12), CurveColor(1, e.dark)})) :
          e.signal.source == Source::Series ? Label(app::strings::target, 12, true) : View(),
    }.With(Spacing(16), CrossAlign(CrossAxisAlignment::Center));
    View trace = Checkbox(app::strings::trace, c.trace.Get()).OnChanged([=](bool value) { c.trace = value; });
    View reconstruct = PrimaryButton(app::strings::reconstruct, [=] {
      if (c.experiment.Get().signal.reconstruct) FinishReconstruction(c);
      else Change(c, [](auto& next) { BeginReconstruction(next); });
    });
    if (UseViewportClass() == ViewportClass::Expanded) {
      content.Add(Row {legend, Spacer(), trace, reconstruct}.With(Spacing(16), CrossAlign(CrossAxisAlignment::Center)));
    } else {
      content.Add(inset(Row {trace, Spacer(), reconstruct}.With(Spacing(8), CrossAlign(CrossAxisAlignment::Center))));
      content.Add(inset(legend));
    }
    if (result && !result->error.empty()) content.Add(inset(Column {Label(app::strings::failed, 20, false, true), Label(result->error, 14, true)}
        .With(Grow(), Spacing(12), MainAlign(MainAxisAlignment::Center), CrossAlign(CrossAxisAlignment::Center))));
    else {
      content.Add(Plot(1)); content.Add(Divider()); content.Add(Plot(2));
      if (result && !result->raw.empty()) {
        content.Add(inset(Label("N " + std::to_string(result->raw.size()) + " · Δf " + Number(result->rate / result->raw.size()) + " Hz · T "
            + Number(result->raw.size() / result->rate) + " s · Nyquist " + Number(result->rate / 2) + " Hz", 12, true)));
        if (e.signal.reconstruct) content.Add(inset(Row {Label("RMS " + Number(result->rms, 6), 12, true).With(Grow()),
            Button(app::strings::edit_bin).OnClick([=] { ShowBin(c); })}.With(CrossAlign(CrossAxisAlignment::Center))));
        if (result->aliasing) content.Add(inset(Label(app::strings::alias_warning, 12, true)));
      }
    }
  }
  View plots = Column {std::move(content)}.With(Grow(), Padding(EdgeInsets::Symmetric(compact ? 0 : 24, compact ? 14 : 24)),
      Spacing(compact ? 8 : 16), CrossAlign(CrossAxisAlignment::Stretch));
  const float minimum_height = mode == 0 ? 420.0F : e.signal.reconstruct ? 680.0F : 590.0F;
  plots = ScrollView {View(plots).With(Frame{.height = std::max(minimum_height, scrolling.ViewportExtent())})}
      .Controller(scrolling).With(Grow(), ScrollBar());
  if (!compact) return Row {Editor(mode).With(Frame{.width = 330}), plots}.With(Grow(), CrossAlign(CrossAxisAlignment::Stretch));
  return plots;
}
} // namespace graph::ui

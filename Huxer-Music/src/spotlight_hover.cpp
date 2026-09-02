#include "spotlight_hover.h"

#include <algorithm>

namespace huxer_music {

using namespace huxerui;

class SpotlightHover::Extension final : public NodeExtension {
public:
  Extension(MountedNode& node, const SpotlightHover& value) {
    Update(node, value);
  }

  void Update(MountedNode&, const SpotlightHover& value) {
    value_ = value;
    InvalidatePaint(PaintInvalidation::Content);
  }

  [[nodiscard]] bool HoverHitTest(MountedNode& node, Point position) const override {
    const Size size = node.LayoutSize();
    return Rect{0.0F, 0.0F, size.width, size.height}.Contains(position);
  }

  void OnHover(MountedNode&, const HoverEvent& event) override {
    if (event.type == HoverEventType::Leave) {
      intensity_.AnimateTo(0.0F, TweenSpec{0.18, Easing::EaseOut});
    } else {
      center_ = event.position;
      if (intensity_.Target() != 1.0F) {
        intensity_.AnimateTo(1.0F, TweenSpec{0.4, Easing::EaseOut});
      }
    }
    InvalidatePaint(PaintInvalidation::Content);
  }

  FrameResult OnFrame(MountedNode&, const FrameInfo& frame) override {
    const MotionAdvanceResult result = intensity_.Advance(frame);
    InvalidatePaint(PaintInvalidation::Content);
    return {result.needs_frame, result.wake_after};
  }

  void PaintBehindContent(const MountedNode&, PaintContext& context) const override {
    const Rect bounds = context.Bounds();
    const float intensity = std::clamp(intensity_.Value(), 0.0F, 1.0F);
    if (intensity <= 0.001F || bounds.width <= 0.0F || bounds.height <= 0.0F) {
      return;
    }

    context.PushClip(bounds, CornerRadii{value_.corner_radius});
    context.DrawRect(
        bounds,
        Color::Rgb(39, 52, 73, intensity),
        CornerRadii{value_.corner_radius}
    );
    context.DrawRect(
        bounds,
        RadialGradient{
            .center = {
                std::clamp((center_.x - bounds.x) / bounds.width, 0.0F, 1.0F),
                std::clamp((center_.y - bounds.y) / bounds.height, 0.0F, 1.0F),
            },
            .radius = {value_.radius / bounds.width, value_.radius / bounds.height},
            .stops = {
                {0.0F, Color::Rgb(255, 255, 255, 0.36F * intensity)},
                {0.48F, Color::Rgb(255, 255, 255, 0.16F * intensity)},
                {1.0F, Color::Transparent()},
            },
        },
        CornerRadii{value_.corner_radius}
    );
    context.PopClip();
  }

private:
  SpotlightHover value_;
  Point center_;
  MotionController intensity_{0.0F};
};

const detail::ModifierDescriptor& SpotlightHover::Descriptor() {
  return detail::ModifierDescriptorFor<SpotlightHover, Extension>();
}

} // namespace huxer_music

#include "music_motion.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace huxer_music {

using namespace huxerui;

namespace {

constexpr float kTau = std::numbers::pi_v<float> * 2.0F;

float WrapPhase(float value) {
  return std::fmod(value, kTau);
}

} // namespace

class AlbumMotion::Extension final : public NodeExtension {
public:
  Extension(MountedNode& node, const AlbumMotion& value) {
    Update(node, value);
  }

  void Update(MountedNode&, const AlbumMotion& value) {
    value_ = value;
    InvalidatePaint(PaintInvalidation::Content);
  }

  FrameResult OnFrame(MountedNode&, const FrameInfo& frame) override {
    if (!value_.playing) {
      return {};
    }

    disc_phase_ = WrapPhase(disc_phase_ + static_cast<float>(frame.delta_time) * kTau / 9.0F);
    light_phase_ = WrapPhase(light_phase_ + static_cast<float>(frame.delta_time) * kTau / 5.6F);
    InvalidatePaint(PaintInvalidation::Content);
    return {.needs_frame = true};
  }

  void PaintBehindContent(const MountedNode&, PaintContext& context) const override {
    const Rect bounds = context.Bounds();
    if (bounds.width <= 0.0F || bounds.height <= 0.0F) {
      return;
    }

    const Point center{bounds.x + bounds.width * 0.66F, bounds.y + bounds.height * 0.42F};
    const float radius = bounds.width * 0.28F;
    const float light_x = 0.48F + 0.28F * std::sin(light_phase_);
    const float light_y = 0.42F + 0.2F * std::cos(light_phase_);
    const float record_light_x =
        (center.x - bounds.x + std::cos(disc_phase_) * radius * 0.58F) / bounds.width;
    const float record_light_y =
        (center.y - bounds.y + std::sin(disc_phase_) * radius * 0.58F) / bounds.height;

    context.PushClip(bounds, CornerRadii{value_.corner_radius});
    context.DrawRect(
        bounds,
        RadialGradient{
            .center = {light_x, light_y},
            .radius = {0.38F, 0.46F},
            .stops = {
                {0.0F, Color::Rgb(255, 255, 255, 0.25F)},
                {0.24F, Color::Rgb(255, 255, 255, 0.13F)},
                {0.62F, Color::Rgb(255, 255, 255, 0.035F)},
                {1.0F, Color::Transparent()},
            },
        },
        CornerRadii{value_.corner_radius}
    );

    context.DrawCircle(center, radius, Color::Rgb(8, 11, 18, 0.58F));
    context.DrawCircle(center, radius * 0.72F, Color::Rgb(255, 255, 255, 0.035F));
    context.DrawCircle(center, radius * 0.41F, Color::Rgb(255, 255, 255, 0.12F));
    context.DrawCircle(center, radius * 0.125F, Color::Rgb(249, 246, 251, 0.92F));
    context.DrawArc(
        center,
        radius * 0.73F,
        disc_phase_,
        4.55F,
        Color::Rgb(255, 255, 255, 0.28F),
        StrokeStyle{.width = 1.4F, .cap = StrokeCap::Round}
    );
    context.DrawArc(
        center,
        radius * 0.87F,
        disc_phase_ + 2.1F,
        1.5F,
        Color::Rgb(255, 255, 255, 0.11F),
        StrokeStyle{.width = 1.0F, .cap = StrokeCap::Round}
    );
    context.DrawArc(
        center,
        radius * 0.94F,
        disc_phase_ - 0.28F,
        0.86F,
        Color::Rgb(255, 255, 255, 0.3F),
        StrokeStyle{.width = 3.0F, .cap = StrokeCap::Round}
    );

    const Point marker{
        center.x + std::cos(disc_phase_) * radius * 0.72F,
        center.y + std::sin(disc_phase_) * radius * 0.72F,
    };
    context.DrawCircle(marker, 3.3F, value_.secondary);
    context.DrawCircle(marker, 1.35F, Color::Rgb(255, 255, 255, 0.9F));
    context.DrawRect(
        bounds,
        RadialGradient{
            .center = {record_light_x, record_light_y},
            .radius = {0.2F, 0.2F},
            .stops = {
                {0.0F, Color::Rgb(255, 255, 255, 0.2F)},
                {0.3F, Color::Rgb(255, 255, 255, 0.08F)},
                {1.0F, Color::Transparent()},
            },
        },
        CornerRadii{value_.corner_radius}
    );
    context.PopClip();
  }

private:
  AlbumMotion value_;
  float disc_phase_ = 0.35F;
  float light_phase_ = 0.0F;
};

const detail::ModifierDescriptor& AlbumMotion::Descriptor() {
  return detail::ModifierDescriptorFor<AlbumMotion, Extension>();
}

class AmbientMotion::Extension final : public NodeExtension {
public:
  Extension(MountedNode& node, const AmbientMotion& value) {
    Update(node, value);
  }

  void Update(MountedNode&, const AmbientMotion& value) {
    value_ = value;
    InvalidatePaint(PaintInvalidation::Content);
  }

  FrameResult OnFrame(MountedNode&, const FrameInfo& frame) override {
    if (!value_.playing) {
      return {};
    }

    phase_ = WrapPhase(phase_ + static_cast<float>(frame.delta_time) * kTau / 12.0F);
    InvalidatePaint(PaintInvalidation::Content);
    return {.needs_frame = true};
  }

  void PaintBehindContent(const MountedNode&, PaintContext& context) const override {
    const Rect bounds = context.Bounds();
    if (bounds.width <= 0.0F || bounds.height <= 0.0F) {
      return;
    }

    const float first_x = 0.16F + 0.11F * std::sin(phase_);
    const float first_y = 0.24F + 0.08F * std::cos(phase_ * 0.8F);
    const float second_x = 0.82F + 0.08F * std::cos(phase_ * 0.65F);
    const float second_y = 0.78F + 0.1F * std::sin(phase_ * 0.72F);

    context.DrawRect(
        bounds,
        RadialGradient{
            .center = {first_x, first_y},
            .radius = {0.64F, 0.8F},
            .stops = {
                {0.0F, Color{value_.accent.red, value_.accent.green, value_.accent.blue, 0.095F}},
                {1.0F, Color::Transparent()},
            },
        }
    );
    context.DrawRect(
        bounds,
        RadialGradient{
            .center = {second_x, second_y},
            .radius = {0.48F, 0.62F},
            .stops = {
                {0.0F, Color{value_.secondary.red, value_.secondary.green, value_.secondary.blue, 0.07F}},
                {1.0F, Color::Transparent()},
            },
        }
    );
  }

private:
  AmbientMotion value_;
  float phase_ = 0.0F;
};

const detail::ModifierDescriptor& AmbientMotion::Descriptor() {
  return detail::ModifierDescriptorFor<AmbientMotion, Extension>();
}

class EqualizerMotion::Extension final : public NodeExtension {
public:
  Extension(MountedNode& node, const EqualizerMotion& value) {
    Update(node, value);
  }

  void Update(MountedNode&, const EqualizerMotion& value) {
    value_ = value;
    InvalidatePaint(PaintInvalidation::Content);
  }

  FrameResult OnFrame(MountedNode&, const FrameInfo& frame) override {
    const float delta = static_cast<float>(frame.delta_time);
    phase_ = WrapPhase(phase_ + delta * kTau * 1.65F);

    const std::array<float, 3> targets = value_.playing
                                             ? std::array<float, 3>{
                                                   0.38F + 0.58F * std::abs(std::sin(phase_)),
                                                   0.3F + 0.65F * std::abs(std::sin(phase_ * 0.71F + 1.9F)),
                                                   0.34F + 0.61F * std::abs(std::sin(phase_ * 1.18F + 3.8F)),
                                               }
                                             : std::array<float, 3>{0.22F, 0.22F, 0.22F};
    const float response = 1.0F - std::exp(-delta * (value_.playing ? 16.0F : 9.0F));
    bool settled = true;
    for (std::size_t index = 0; index < levels_.size(); ++index) {
      levels_[index] += (targets[index] - levels_[index]) * response;
      settled = settled && std::abs(levels_[index] - targets[index]) < 0.01F;
    }

    InvalidatePaint(PaintInvalidation::Content);
    return {.needs_frame = value_.playing || !settled};
  }

  void PaintBehindContent(const MountedNode&, PaintContext& context) const override {
    const Rect bounds = context.Bounds();
    if (bounds.width <= 0.0F || bounds.height <= 0.0F) {
      return;
    }

    constexpr float bar_width = 2.5F;
    constexpr float gap = 2.0F;
    const float group_width = bar_width * 3.0F + gap * 2.0F;
    const float start_x = bounds.x + (bounds.width - group_width) * 0.5F;
    for (std::size_t index = 0; index < levels_.size(); ++index) {
      const float height = std::max(3.0F, bounds.height * levels_[index]);
      const Rect bar{
          start_x + static_cast<float>(index) * (bar_width + gap),
          bounds.y + bounds.height - height,
          bar_width,
          height,
      };
      context.DrawRect(bar, index == 1 ? value_.secondary : value_.accent, CornerRadii{1.25F});
    }
  }

private:
  EqualizerMotion value_;
  std::array<float, 3> levels_{0.42F, 0.76F, 0.56F};
  float phase_ = 0.0F;
};

const detail::ModifierDescriptor& EqualizerMotion::Descriptor() {
  return detail::ModifierDescriptorFor<EqualizerMotion, Extension>();
}

} // namespace huxer_music

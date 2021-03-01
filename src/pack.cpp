
#include "pack.h"
#include "rbp/MaxRectsBinPack.h"
#include "common.h"
#include <optional>

namespace {
  rbp::MaxRectsBinPack::FreeRectChoiceHeuristic to_rbp_method(PackMethod method) {
    const auto first = static_cast<int>(PackMethod::MaxRects_BestShortSideFit);
    const auto last = static_cast<int>(PackMethod::MaxRects_ContactPointRule);
    const auto index = std::clamp(static_cast<int>(method), first, last) - first;
    return static_cast<rbp::MaxRectsBinPack::FreeRectChoiceHeuristic>(index);
  }

  bool advance(PackMethod& method, PackMethod first_method) {
    if (method == PackMethod::MaxRects_ContactPointRule)
      method = PackMethod::MaxRects_BestShortSideFit;
    else
      method = static_cast<PackMethod>(static_cast<int>(method) + 1);
    return (method != first_method);
  }

  bool can_fit(const PackSettings& settings, int width, int height) {
    return ((width <= settings.max_width &&
             height <= settings.max_height) ||
             (settings.allow_rotate &&
              width <= settings.max_height &&
              height <= settings.max_width));
  }

  void apply_padding(const PackSettings& settings, int& width, int& height, bool indent) {
    const auto dir = (indent ? 1 : -1);
    width -= dir * settings.border_padding * 2;
    height -= dir * settings.border_padding * 2;
    width += dir * settings.over_allocate;
    height += dir * settings.over_allocate;
  }

  bool correct_settings(PackSettings& settings, std::vector<PackSize>& sizes) {
    // clamp min and max (not to numeric_limits<int>::max() to prevent overflow)
    const auto size_limit = 1000000;
    if (settings.max_width <= 0 || settings.max_width > size_limit)
      settings.max_width = size_limit;
    if (settings.max_height <= 0 || settings.max_height > size_limit)
      settings.max_height = size_limit;
    std::clamp(settings.min_width, 0, settings.max_width);
    std::clamp(settings.min_height, 0, settings.max_height);

    // immediately apply padding and over allocation, only relevant for power-of-two and alignment constraint
    apply_padding(settings, settings.min_width, settings.min_height, true);
    apply_padding(settings, settings.max_width, settings.max_height, true);

    auto max_rect_width = 0;
    auto max_rect_height = 0;
    for (auto it = begin(sizes); it != end(sizes); )
      if (!can_fit(settings, it->width, it->height)) {
        it = sizes.erase(it);
      }
      else {
        max_rect_width = std::max(max_rect_width, it->width);
        max_rect_height = std::max(max_rect_height, it->height);
        ++it;
      }

    if (settings.allow_rotate)
      max_rect_width = max_rect_height = std::min(max_rect_width, max_rect_height);
    settings.min_width = std::max(settings.min_width, max_rect_width);
    settings.min_height = std::max(settings.min_height, max_rect_height);
    return true;
  }

  struct RunSettings {
    int width;
    int height;
    PackMethod method;
  };

  struct Run : RunSettings {
    std::vector<PackSheet> sheets;
    int total_area;
  };

  void correct_size(const PackSettings& settings, int& width, int& height) {
    width = std::max(width, settings.min_width);
    height = std::max(height, settings.min_height);
    apply_padding(settings, width, height, false);

    if (settings.power_of_two) {
      width = ceil_to_pot(width);
      height = ceil_to_pot(height);
    }

    if (settings.align_width)
      width = ceil(width, settings.align_width);

    if (settings.square)
      width = height = std::max(width, height);

    apply_padding(settings, width, height, true);
    width = std::min(width, settings.max_width);
    height = std::min(height, settings.max_height);
    apply_padding(settings, width, height, false);

    if (settings.power_of_two) {
      width = floor_to_pot(width);
      height = floor_to_pot(height);
    }

    if (settings.align_width)
      width = floor(width, settings.align_width);

    if (settings.square)
      width = height = std::min(width, height);

    apply_padding(settings, width, height, true);
  }

  bool is_better_than(const Run& a, const Run& b) {
    if (a.sheets.size() < b.sheets.size())
      return true;
    if (b.sheets.size() < a.sheets.size())
      return false;
    return (a.total_area < b.total_area);
  }

  int get_perfect_area(const std::vector<PackSize>& sizes) {
    auto area = 0;
    for (const auto& size : sizes)
      area += size.width * size.height;
    return area;
  }

  std::pair<int, int> get_run_size(const PackSettings& settings, int area) {
    auto width = sqrt(area);
    auto height = div_ceil(area, width);
    if (width < settings.min_width || width > settings.max_width) {
      width = std::clamp(width, settings.min_width, settings.max_width);
      height = div_ceil(area, width);
    }
    else if (height < settings.min_height || height > settings.max_height) {
      height = std::clamp(height, settings.min_height, settings.max_height);
      width = div_ceil(area, height);
    }
    correct_size(settings, width, height);
    return { width, height };
  }

  RunSettings get_initial_run_settings(const PackSettings& settings, int perfect_area) {
    const auto method = (settings.method == PackMethod::undefined ?
      PackMethod::MaxRects_BestLongSideFit : settings.method);

    const auto [width, height] = get_run_size(settings, perfect_area * 5 / 4);
    return { width, height, method };
  }

  enum class OptimizationStage {
    first_run,
    minimize_sheet_count,
    shrink_square,
    shrink_width_fast,
    shrink_height_fast,
    shrink_width_slow,
    shrink_height_slow,
    end
  };

  struct OptimizationState {
    const int perfect_area;
    RunSettings settings;
    OptimizationStage stage;
    PackMethod first_method;
    int iteration;
  };

  bool advance(OptimizationStage& stage) {
    if (stage == OptimizationStage::end)
      return false;
    stage = static_cast<OptimizationStage>(static_cast<int>(stage) + 1);
    return true;
  }

  // returns true when stage should be kept, false to advance
  bool optimize_stage(OptimizationState& state,
      const PackSettings& pack_settings, const Run& best_run) {

    auto& run = state.settings;
    switch (state.stage) {
      case OptimizationStage::first_run:
      case OptimizationStage::end:
        return false;

      case OptimizationStage::minimize_sheet_count: {
        if (best_run.sheets.size() <= 1 ||
            state.iteration > 5)
          return false;

        const auto& last_sheet = best_run.sheets.back();
        auto area = last_sheet.width * last_sheet.height;
        for (auto i = 0; area > 0; ++i) {
          if (run.width == pack_settings.max_width &&
              run.height == pack_settings.max_height)
            break;
          if (run.height == pack_settings.max_height ||
              (run.width < pack_settings.max_width && i % 2)) {
            ++run.width;
            area -= run.height;
          }
          else {
            ++run.height;
            area -= run.width;
          }
        }
        return true;
      }

      case OptimizationStage::shrink_square: {
        if (run.width != best_run.width ||
            run.height != best_run.height ||
            state.iteration > 5)
          return false;

        const auto [width, height] = get_run_size(pack_settings, state.perfect_area);
        run.width = (run.width + width) / 2;
        run.height = (run.height + height) / 2;
        return true;
      }

      case OptimizationStage::shrink_width_fast:
      case OptimizationStage::shrink_height_fast:
      case OptimizationStage::shrink_width_slow:
      case OptimizationStage::shrink_height_slow: {
        if (run.width != best_run.width ||
            run.height != best_run.height ||
            state.iteration > 5) {

          // when no method is set, retry with each method
          if (pack_settings.method != PackMethod::undefined ||
              !advance(run.method, state.first_method))
            return false;

          // do not try costy contact point rule
          if (run.method == PackMethod::MaxRects_ContactPointRule &&
              !advance(run.method, state.first_method))
            return false;

          run.width = best_run.width;
          run.height = best_run.height;
        }

        const auto [width, height] = get_run_size(pack_settings, state.perfect_area);
        switch (state.stage) {
          default:
          case OptimizationStage::shrink_width_fast:
            if (run.width > width + 4)
              run.width = (run.width + width) / 2;
            break;
          case OptimizationStage::shrink_height_fast:
            if (run.height > height + 4)
              run.height = (run.height + height) / 2;
            break;
          case OptimizationStage::shrink_width_slow:
            if (run.width > width)
              --run.width;
            break;
          case OptimizationStage::shrink_height_slow:
            if (run.height > height)
              --run.height;
            break;
        }
        return true;
      }
    }
    return false;
  }

  bool optimize_run_settings(OptimizationState& state,
      const PackSettings& pack_settings, const Run& best_run) {

    const auto previous_state = state;
    for (;;) {
      if (!optimize_stage(state, pack_settings, best_run))
        if (advance(state.stage)) {
          state.settings.width = best_run.width;
          state.settings.height = best_run.height;
          state.settings.method = best_run.method;
          state.first_method = best_run.method;
          state.iteration = 0;
          continue;
        }

      if (state.stage == OptimizationStage::end)
        return false;

      ++state.iteration;

      auto width = state.settings.width;
      auto height = state.settings.height;
      correct_size(pack_settings, width, height);
      if (width != previous_state.settings.width ||
          height != previous_state.settings.height ||
          state.settings.method != previous_state.settings.method) {
        state.settings.width = width;
        state.settings.height = height;
        return true;
      }
    }
  }
} // namespace

std::vector<PackSheet> pack(PackSettings settings, std::vector<PackSize> sizes) {
  if (!correct_settings(settings, sizes))
    return { };

  if (sizes.empty())
    return { };

  auto best_run = std::optional<Run>{ };
  auto max_rects = rbp::MaxRectsBinPack();
  auto rbp_rects = std::vector<rbp::Rect>();
  rbp_rects.reserve(sizes.size());
  const auto perfect_area = get_perfect_area(sizes);
  auto optimization_state = OptimizationState{
    .perfect_area = perfect_area,
    .settings = get_initial_run_settings(settings, perfect_area),
    .stage = OptimizationStage::first_run,
    .iteration = 0,
  };

  auto rbp_sizes = std::vector<rbp::RectSize>();
  rbp_sizes.reserve(sizes.size());
  for (const auto& size : sizes)
    rbp_sizes.push_back({ size.width, size.height, static_cast<int>(rbp_sizes.size()) });

  for (;;) {
    auto run_rbp_sizes = rbp_sizes;
    auto cancelled = false;
    auto run = Run{ optimization_state.settings, { }, 0 };

    while (!cancelled && !run_rbp_sizes.empty()) {
      rbp_rects.clear();
      max_rects.Init(run.width, run.height, settings.allow_rotate);
      max_rects.Insert(run_rbp_sizes, rbp_rects, to_rbp_method(run.method));

      auto [width, height] = max_rects.BottomRight();
      correct_size(settings, width, height);
      apply_padding(settings, width, height, false);

      auto& sheet = run.sheets.emplace_back(PackSheet{ width, height, { } });
      run.total_area += width * height;

      if (best_run && !is_better_than(run, *best_run)) {
        cancelled = true;
        continue;
      }

      sheet.rects.reserve(rbp_rects.size());
      for (auto& rbp_rect : rbp_rects) {
        const auto& size = sizes[static_cast<size_t>(rbp_rect.id)];
        sheet.rects.push_back({
          size.id,
          rbp_rect.x + settings.border_padding,
          rbp_rect.y + settings.border_padding,
          (rbp_rect.width != size.width)
        });
      }
    }

    if (!cancelled && (!best_run || is_better_than(run, *best_run)))
      best_run = std::move(run);

    if (!optimize_run_settings(optimization_state, settings, *best_run))
      break;
  }

  if (settings.max_sheets && std::cmp_less(settings.max_sheets, best_run->sheets.size()))
    best_run->sheets.resize(static_cast<size_t>(settings.max_sheets));

  return std::move(best_run->sheets);
}

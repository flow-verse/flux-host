// SPDX-FileCopyrightText: © 2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "dsp/tempo_map.h"
#include "dsp/tempo_map_qml_adapter.h"
#include "dsp/tick_types.h"
#include "structure/arrangement/arranger_object_all.h"
#include "utils/object_registry.h"

#include <gtest/gtest.h>

namespace zrythm::structure::arrangement
{
class AutomationClipTest : public ::testing::Test
{
protected:
  void SetUp () override
  {
    tempo_map = std::make_unique<dsp::TempoMap> (units::sample_rate (44100.0));
    tempo_map_wrapper = std::make_unique<dsp::TempoMapWrapper> (*tempo_map);
    clip =
      std::make_unique<AutomationClip> (*tempo_map_wrapper, registry, nullptr);

    // Set up clip properties
    clip->position ()->setTicks (100);
    clip->length ()->setTicks (200);
  }

  auto create_automation_point (double value)
  {
    // Create AutomationPoint using registry
    auto point_ref = utils::create_object<AutomationPoint> (
      registry, *tempo_map_wrapper, clip.get ());
    point_ref.get_object_as<AutomationPoint> ()->setValue (
      static_cast<float> (value));
    return point_ref;
  }

  void add_automation_point (double value, double position_ticks)
  {
    auto point_ref = create_automation_point (value);
    point_ref.get_object_as<AutomationPoint> ()->position ()->setTicks (
      position_ticks);
    clip->add_object (point_ref);
  }

  std::unique_ptr<dsp::TempoMap>        tempo_map;
  std::unique_ptr<dsp::TempoMapWrapper> tempo_map_wrapper;
  utils::ObjectRegistry                 registry;
  std::unique_ptr<AutomationClip>       clip;
};

TEST_F (AutomationClipTest, InitialState)
{
  EXPECT_EQ (clip->type (), ArrangerObject::Type::AutomationClip);
  EXPECT_EQ (clip->position ()->ticks (), 100);
  EXPECT_NE (clip->length (), nullptr);
  EXPECT_NE (clip->loopStartPosition (), nullptr);
  EXPECT_NE (clip->name (), nullptr);
  EXPECT_NE (clip->color (), nullptr);
  EXPECT_NE (clip->mute (), nullptr);
  EXPECT_EQ (clip->get_children_vector ().size (), 0);
}

TEST_F (AutomationClipTest, AddAutomationPoint)
{
  // Add an automation point
  add_automation_point (0.5, 100);

  // Verify point was added
  EXPECT_EQ (clip->get_children_vector ().size (), 1);
  auto point = clip->get_children_view ()[0];
  EXPECT_FLOAT_EQ (point->value (), 0.5f);
  EXPECT_EQ (point->position ()->ticks (), 100);
}

TEST_F (AutomationClipTest, RemoveAutomationPoint)
{
  // Add an automation point
  auto point_ref = create_automation_point (0.8);
  clip->add_object (point_ref);

  // Remove point
  auto removed_ref = clip->remove_object (point_ref.id ());

  // Verify point was removed
  EXPECT_EQ (clip->get_children_vector ().size (), 0);
  EXPECT_EQ (removed_ref.id (), point_ref.id ());
}

TEST_F (AutomationClipTest, ValueChange)
{
  // Add an automation point
  add_automation_point (0.3, 150);

  // Change value
  clip->get_children_view ()[0]->setValue (0.7f);

  // Verify change
  EXPECT_FLOAT_EQ (clip->get_children_view ()[0]->value (), 0.7f);
}

TEST_F (AutomationClipTest, GetPrevNextPoint)
{
  // Add points
  add_automation_point (0.2, 100);
  add_automation_point (0.4, 200);
  add_automation_point (0.6, 300);

  // Get next point
  auto point1 = clip->get_children_view ()[0];
  auto next_point = clip->get_next_ap (*point1, true);
  EXPECT_EQ (next_point->position ()->ticks (), 200);

  // Get previous point (should be null for first point)
  auto prev_point = clip->get_prev_ap (*point1);
  EXPECT_EQ (prev_point, nullptr);

  // Get previous point for middle point
  auto point2 = clip->get_children_view ()[1];
  prev_point = clip->get_prev_ap (*point2);
  EXPECT_EQ (prev_point->position ()->ticks (), 100);
}

TEST_F (AutomationClipTest, CurveCalculationLinearUp)
{
  // Add points going from lower to higher
  add_automation_point (0.0, 100);
  add_automation_point (1.0, 200);

  // Get curve value at midpoint
  auto   point1 = clip->get_children_view ()[0];
  double value = clip->get_normalized_value_in_curve (*point1, 0.5);

  // Default curve is linear, so midpoint should be 0.5
  EXPECT_DOUBLE_EQ (value, 0.5);

  // Test at other points
  EXPECT_DOUBLE_EQ (clip->get_normalized_value_in_curve (*point1, 0.0), 0.0);
  EXPECT_DOUBLE_EQ (clip->get_normalized_value_in_curve (*point1, 1.0), 1.0);
}

TEST_F (AutomationClipTest, CurveCalculationLinearDown)
{
  // Add points going from higher to lower
  add_automation_point (1.0, 100);
  add_automation_point (0.0, 200);

  // Get curve value at midpoint
  auto   point1 = clip->get_children_view ()[0];
  double value = clip->get_normalized_value_in_curve (*point1, 0.5);

  // Default curve is linear, so midpoint should be 0.5
  EXPECT_DOUBLE_EQ (value, 0.5);

  // Test at other points
  EXPECT_DOUBLE_EQ (clip->get_normalized_value_in_curve (*point1, 0.0), 1.0);
  EXPECT_DOUBLE_EQ (clip->get_normalized_value_in_curve (*point1, 1.0), 0.0);
}

TEST_F (AutomationClipTest, GetValueAtVirtTickEmptyClip)
{
  auto [val, ap] =
    clip->get_value_at_virt_tick (dsp::ContentTick{ units::ticks (50.0) });
  EXPECT_EQ (ap, nullptr);
  EXPECT_FLOAT_EQ (val, 0.0f);
}

TEST_F (AutomationClipTest, GetValueAtVirtTickSinglePoint)
{
  add_automation_point (0.5, 100);

  // Before the point — held flat at the point's value
  auto [val_before, ap_before] =
    clip->get_value_at_virt_tick (dsp::ContentTick{ units::ticks (50.0) });
  EXPECT_FLOAT_EQ (val_before, 0.5f);
  EXPECT_EQ (ap_before, clip->get_children_view ()[0]);

  // At/after the point — also held flat
  auto [val_after, ap_after] =
    clip->get_value_at_virt_tick (dsp::ContentTick{ units::ticks (150.0) });
  EXPECT_FLOAT_EQ (val_after, 0.5f);
  EXPECT_EQ (ap_after, clip->get_children_view ()[0]);
}

TEST_F (AutomationClipTest, GetValueAtVirtTickTwoPointsLinear)
{
  add_automation_point (0.0, 100);
  add_automation_point (1.0, 200);

  // Before first — held flat at first point's value
  auto [val_before, ap_before] =
    clip->get_value_at_virt_tick (dsp::ContentTick{ units::ticks (50.0) });
  EXPECT_FLOAT_EQ (val_before, 0.0f);

  // Midpoint — linear interpolation gives 0.5
  auto [val_mid, ap_mid] =
    clip->get_value_at_virt_tick (dsp::ContentTick{ units::ticks (150.0) });
  EXPECT_NEAR (val_mid, 0.5f, 1e-5f);
  // Governed by the first point (segment start)
  EXPECT_EQ (ap_mid, clip->get_children_view ()[0]);

  // After last — held flat at last point's value
  auto [val_after, ap_after] =
    clip->get_value_at_virt_tick (dsp::ContentTick{ units::ticks (250.0) });
  EXPECT_FLOAT_EQ (val_after, 1.0f);
}

TEST_F (AutomationClipTest, GetValueAtVirtTickDescending)
{
  add_automation_point (1.0, 100);
  add_automation_point (0.0, 200);

  // Midpoint — linear gives 0.5
  auto [val_mid, ap_mid] =
    clip->get_value_at_virt_tick (dsp::ContentTick{ units::ticks (150.0) });
  EXPECT_NEAR (val_mid, 0.5f, 1e-5f);
}

TEST_F (AutomationClipTest, GetValueAtVirtTickAtPointBoundaries)
{
  add_automation_point (0.3, 100);
  add_automation_point (0.7, 200);

  // At first point exactly (<= triggers early return)
  auto [val_at_first, ap_first] =
    clip->get_value_at_virt_tick (dsp::ContentTick{ units::ticks (100.0) });
  EXPECT_FLOAT_EQ (val_at_first, 0.3f);

  // At last point exactly (>= triggers early return)
  auto [val_at_last, ap_last] =
    clip->get_value_at_virt_tick (dsp::ContentTick{ units::ticks (200.0) });
  EXPECT_FLOAT_EQ (val_at_last, 0.7f);
}

TEST_F (AutomationClipTest, Serialization)
{
  // Add an automation point
  add_automation_point (0.75, 150);
  const auto point_id = clip->get_children_view ()[0]->get_uuid ();

  // Serialize
  nlohmann::json j;
  to_json (j, *clip);

  // Create new clip
  auto new_clip =
    std::make_unique<AutomationClip> (*tempo_map_wrapper, registry, nullptr);
  from_json (j, *new_clip);

  // Verify deserialization
  EXPECT_EQ (new_clip->get_children_vector ().size (), 1);
  auto point = new_clip->get_children_view ()[0];
  EXPECT_FLOAT_EQ (point->value (), 0.75f);
  EXPECT_EQ (point->position ()->ticks (), 150);
  EXPECT_EQ (point->get_uuid (), point_id);
}

TEST_F (AutomationClipTest, ObjectCloning)
{
  // Add an automation point
  add_automation_point (0.25, 250);
  const auto original_id = clip->get_children_view ()[0]->get_uuid ();

  // Clone clip with new identities
  auto cloned_clip =
    std::make_unique<AutomationClip> (*tempo_map_wrapper, registry, nullptr);
  init_from (*cloned_clip, *clip, utils::ObjectCloneType::NewIdentity);

  // Verify cloning
  EXPECT_EQ (cloned_clip->get_children_vector ().size (), 1);
  auto cloned_point = cloned_clip->get_children_view ()[0];
  EXPECT_NE (cloned_point->get_uuid (), original_id);
  EXPECT_FLOAT_EQ (cloned_point->value (), 0.25f);
  EXPECT_EQ (cloned_point->position ()->ticks (), 250);
}

} // namespace zrythm::structure::arrangement

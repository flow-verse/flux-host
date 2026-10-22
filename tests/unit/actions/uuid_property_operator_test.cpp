// SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "actions/uuid_property_operator.h"
#include "utils/object_registry.h"

#include "helpers/mock_qobject.h"
#include "helpers/mock_uuid_object.h"

#include "unit/actions/mock_undo_stack.h"
#include <gtest/gtest.h>

namespace zrythm::actions
{

class UuidPropertyOperatorTest : public ::testing::Test
{
protected:
  void SetUp () override
  {
    obj_ = new MockUuidObject ();
    registry_.register_object (*obj_);

    undo_stack_ = create_mock_undo_stack ();
    operator_ = std::make_unique<UuidPropertyOperator> (*undo_stack_, registry_);
  }

  void TearDown () override
  {
    operator_.reset ();
    undo_stack_.reset ();
    // If a command holding a UuidReference is on the stack, undo_stack_
    // destruction releases the last reference, which drives the registry
    // refcount to 0 and deletes obj_ during undo_stack_ teardown (before
    // registry_ is destroyed). Nothing accesses obj_ afterward, so the
    // dangling raw pointer is safe. registry_ then cleans up any remaining
    // objects (e.g. children not referenced by commands).
  }

  // Destruction order: operator_ and undo_stack_ destroyed first (releases
  // any command UUID references), then registry_ last.
  utils::ObjectRegistry                 registry_;
  MockUuidObject *                      obj_ = nullptr;
  std::unique_ptr<undo::UndoStack>      undo_stack_;
  std::unique_ptr<UuidPropertyOperator> operator_;
};

TEST_F (UuidPropertyOperatorTest, SetValue)
{
  EXPECT_EQ (obj_->property ("intValue").toInt (), 42);
  EXPECT_EQ (undo_stack_->count (), 0);

  operator_->setValue (obj_, QStringLiteral ("intValue"), 100);

  EXPECT_EQ (obj_->property ("intValue").toInt (), 100);
  EXPECT_EQ (undo_stack_->count (), 1);

  undo_stack_->undo ();
  EXPECT_EQ (obj_->property ("intValue").toInt (), 42);

  undo_stack_->redo ();
  EXPECT_EQ (obj_->property ("intValue").toInt (), 100);
}

TEST_F (UuidPropertyOperatorTest, SetValueOnSubObject)
{
  auto * child = new MockQObject (obj_);

  EXPECT_EQ (child->intValue (), 42);
  EXPECT_EQ (undo_stack_->count (), 0);

  operator_->setValueOnSubObject (obj_, child, QStringLiteral ("intValue"), 200);

  EXPECT_EQ (child->intValue (), 200);
  EXPECT_EQ (undo_stack_->count (), 1);

  undo_stack_->undo ();
  EXPECT_EQ (child->intValue (), 42);

  undo_stack_->redo ();
  EXPECT_EQ (child->intValue (), 200);
}

TEST_F (UuidPropertyOperatorTest, NullUuidObjectIsNoOp)
{
  operator_->setValue (nullptr, QStringLiteral ("intValue"), 100);
  EXPECT_EQ (undo_stack_->count (), 0);
  EXPECT_EQ (obj_->property ("intValue").toInt (), 42);
}

TEST_F (UuidPropertyOperatorTest, NullTargetIsNoOp)
{
  operator_->setValueOnSubObject (
    obj_, nullptr, QStringLiteral ("intValue"), 100);
  EXPECT_EQ (undo_stack_->count (), 0);
}

TEST_F (UuidPropertyOperatorTest, NullLifecycleObjectIsNoOp)
{
  auto * child = new MockQObject (obj_);
  operator_->setValueOnSubObject (
    nullptr, child, QStringLiteral ("intValue"), 100);
  EXPECT_EQ (undo_stack_->count (), 0);
  EXPECT_EQ (child->intValue (), 42);
}

TEST_F (UuidPropertyOperatorTest, NonUuidObjectIsNoOp)
{
  MockQObject plain_obj;
  operator_->setValue (&plain_obj, QStringLiteral ("intValue"), 100);
  EXPECT_EQ (undo_stack_->count (), 0);
  EXPECT_EQ (plain_obj.intValue (), 42);
}

TEST_F (UuidPropertyOperatorTest, UndoRedoCycle)
{
  operator_->setValue (obj_, QStringLiteral ("intValue"), 10);
  operator_->setValue (obj_, QStringLiteral ("intValue"), 20);
  operator_->setValue (obj_, QStringLiteral ("intValue"), 30);

  EXPECT_EQ (obj_->property ("intValue").toInt (), 30);
  // Commands should merge (same object + property, rapid succession)
  EXPECT_EQ (undo_stack_->count (), 1);

  undo_stack_->undo ();
  EXPECT_EQ (obj_->property ("intValue").toInt (), 42);

  undo_stack_->redo ();
  EXPECT_EQ (obj_->property ("intValue").toInt (), 30);
}

} // namespace zrythm::actions

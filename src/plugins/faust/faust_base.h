// SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

/**
 * @file
 *
 * @brief Base classes that Faust-generated code compiles against.
 *
 * These follow the interface described in the Faust manual
 * (https://faustdoc.grame.fr/manual/architectures): a minimal architecture
 * providing `FAUSTFLOAT`, `Meta`, `Soundfile`, `UI` and `dsp`, so that code
 * generated with `faust -lang cpp` is fully self-contained and does not
 * depend on any Faust headers.
 *
 * @note These classes must keep these exact names and signatures - the
 * generated code references them unqualified (the architecture file brings
 * them into scope with using-declarations).
 */

#pragma once

#ifndef FAUSTFLOAT
#  define FAUSTFLOAT float
#endif

namespace zrythm::plugins::faust
{

/** Global metadata receiver (name, author, version, etc.). */
struct Meta
{
  virtual ~Meta () = default;
  virtual void declare (const char * key, const char * value) = 0;
};

/** Placeholder for the soundfile primitive (unused by our DSPs). */
struct Soundfile
{
};

/** UI builder interface the generated code calls from buildUserInterface(). */
class UI
{
public:
  virtual ~UI () = default;

  // -- widget layouts --
  virtual void openTabBox (const char * label) { }
  virtual void openHorizontalBox (const char * label) { }
  virtual void openVerticalBox (const char * label) { }
  virtual void closeBox () { }

  // -- active widgets --
  virtual void addButton (const char * label, FAUSTFLOAT * zone) { }
  virtual void addCheckButton (const char * label, FAUSTFLOAT * zone) { }
  virtual void addVerticalSlider (
    const char * label,
    FAUSTFLOAT * zone,
    FAUSTFLOAT   init,
    FAUSTFLOAT   min,
    FAUSTFLOAT   max,
    FAUSTFLOAT   step)
  {
  }
  virtual void addHorizontalSlider (
    const char * label,
    FAUSTFLOAT * zone,
    FAUSTFLOAT   init,
    FAUSTFLOAT   min,
    FAUSTFLOAT   max,
    FAUSTFLOAT   step)
  {
  }
  virtual void addNumEntry (
    const char * label,
    FAUSTFLOAT * zone,
    FAUSTFLOAT   init,
    FAUSTFLOAT   min,
    FAUSTFLOAT   max,
    FAUSTFLOAT   step)
  {
  }

  // -- passive widgets --
  virtual void addHorizontalBargraph (
    const char * label,
    FAUSTFLOAT * zone,
    FAUSTFLOAT   min,
    FAUSTFLOAT   max)
  {
  }
  virtual void addVerticalBargraph (
    const char * label,
    FAUSTFLOAT * zone,
    FAUSTFLOAT   min,
    FAUSTFLOAT   max)
  {
  }

  // -- soundfiles --
  virtual void
  addSoundfile (const char * label, const char * filename, Soundfile ** sf_zone)
  {
  }

  // -- per-widget metadata declarations --
  virtual void declare (FAUSTFLOAT * zone, const char * key, const char * val)
  {
  }
};

/** Base class of the generated DSP class. */
class dsp
{
public:
  dsp () = default;
  virtual ~dsp () = default;

  /** Returns the number of audio inputs. */
  virtual int getNumInputs () = 0;

  /** Returns the number of audio outputs. */
  virtual int getNumOutputs () = 0;

  /** Triggers UI building calls on @p ui_interface. */
  virtual void buildUserInterface (UI * ui_interface) = 0;

  /** Returns the sample rate currently used by the instance. */
  virtual int getSampleRate () = 0;

  /** Global init: calls classInit() then instanceInit(). */
  virtual void init (int sample_rate) = 0;

  /** Initializes instance state. */
  virtual void instanceInit (int sample_rate) = 0;

  /** Initializes instance constant state. */
  virtual void instanceConstants (int sample_rate) = 0;

  /** Initializes default control parameter values. */
  virtual void instanceResetUserInterface () = 0;

  /** Initializes instance state (delay lines, etc.) but keeps control values. */
  virtual void instanceClear () = 0;

  /** Returns a copy of the instance. */
  virtual dsp * clone () = 0;

  /** Triggers global metadata declarations on @p m. */
  virtual void metadata (Meta * m) = 0;

  /**
   * @brief Processes one block of non-interleaved audio.
   *
   * @param count Number of frames.
   * @param inputs Input buffers (one per getNumInputs()).
   * @param outputs Output buffers (one per getNumOutputs()).
   */
  virtual void
  compute (int count, FAUSTFLOAT ** inputs, FAUSTFLOAT ** outputs) = 0;

  /** Timestamped compute variant (unused; forwards to the 3-arg compute). */
  virtual void
  compute (double date_usec, int count, FAUSTFLOAT ** inputs, FAUSTFLOAT ** outputs)
  {
    compute (count, inputs, outputs);
  }
};

} // namespace zrythm::plugins::faust

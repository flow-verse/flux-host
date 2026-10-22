/* ------------------------------------------------------------
author: "Zrythm"
copyright: "© 2022 Alexandros Theodotou"
license: "AGPL-3.0-or-later"
name: "Flanger"
version: "1.0"
Code generated with Faust 2.85.9 (https://faust.grame.fr)
Compilation options: -a data/plugins/zrythm-arch.cpp -lang cpp -fpga-mem-th 4 -ct 1 -cn flanger -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 0
------------------------------------------------------------ */

#ifndef  __flanger_H__
#define  __flanger_H__

/************************************************************************
 IMPORTANT NOTE : this file contains two clearly delimited sections :
 the ARCHITECTURE section (in two parts) and the USER section.
 The ARCHITECTURE section is licensed under the Zrythm license
 (LicenseRef-ZrythmLicense). The USER section is replaced by
 Faust-generated code, whose license follows the corresponding .dsp file.
 *************************************************************************/

/******************* BEGIN ARCHITECTURE SECTION (part 1/2) ****************/

// SPDX-FileCopyrightText: © 2026 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense


#include "plugins/faust/faust_base.h"

// The generated code references the base classes unqualified
using zrythm::plugins::faust::Meta;
using zrythm::plugins::faust::dsp;
using zrythm::plugins::faust::Soundfile;
using zrythm::plugins::faust::UI;

/******************** END ARCHITECTURE SECTION (part 1/2) *****************/

/************************** BEGIN USER SECTION ****************************/

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>

#ifndef FAUSTCLASS 
#define FAUSTCLASS flanger
#endif

#ifdef __APPLE__ 
#define exp10f __exp10f
#define exp10 __exp10
#endif

#if defined(_WIN32)
#define RESTRICT __restrict
#else
#define RESTRICT __restrict__
#endif


class flanger : public dsp {
	
 private:
	
	FAUSTFLOAT fCheckbox0;
	FAUSTFLOAT fHslider0;
	int iVec0[2];
	FAUSTFLOAT fHslider1;
	int fSampleRate;
	float fConst0;
	float fConst1;
	float fRec1[2];
	float fRec2[2];
	FAUSTFLOAT fHbargraph0;
	FAUSTFLOAT fHslider2;
	FAUSTFLOAT fHslider3;
	int IOTA0;
	float fVec1[4096];
	FAUSTFLOAT fHslider4;
	float fConst2;
	float fRec3[2];
	FAUSTFLOAT fHslider5;
	float fRec4[2];
	float fRec0[2];
	float fVec2[4096];
	float fRec5[2];
	
 public:
	flanger() {
	}
	
	flanger(const flanger&) = default;
	
	virtual ~flanger() = default;
	
	flanger& operator=(const flanger&) = default;
	
	void metadata(Meta* m) { 
		m->declare("author", "Zrythm");
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.22.0");
		m->declare("compile_options", "-a data/plugins/zrythm-arch.cpp -lang cpp -fpga-mem-th 4 -ct 1 -cn flanger -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 0");
		m->declare("copyright", "© 2022 Alexandros Theodotou");
		m->declare("delays.lib/name", "Faust Delay Library");
		m->declare("delays.lib/version", "1.2.0");
		m->declare("description", "Flanger effect");
		m->declare("filename", "flanger.dsp");
		m->declare("filters.lib/lowpass0_highpass1", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/name", "Faust Filters Library");
		m->declare("filters.lib/nlf2:author", "Julius O. Smith III");
		m->declare("filters.lib/nlf2:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/nlf2:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/version", "1.7.1");
		m->declare("license", "AGPL-3.0-or-later");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LGPL with exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.0");
		m->declare("name", "Flanger");
		m->declare("oscillators.lib/name", "Faust Oscillator Library");
		m->declare("oscillators.lib/version", "1.7.0");
		m->declare("phaflangers.lib/name", "Faust Phaser and Flanger Library");
		m->declare("phaflangers.lib/version", "1.1.0");
		m->declare("platform.lib/name", "Generic Platform Library");
		m->declare("platform.lib/version", "1.3.0");
		m->declare("signals.lib/name", "Faust Routing Library");
		m->declare("signals.lib/version", "1.6.0");
		m->declare("version", "1.0");
		m->declare("zrythm-utils.lib/copyright", "© 2022 Alexandros Theodotou");
		m->declare("zrythm-utils.lib/license", "AGPL-3.0-or-later");
		m->declare("zrythm-utils.lib/name", "Zrythm utils");
		m->declare("zrythm-utils.lib/version", "1.0");
	}

	virtual int getNumInputs() {
		return 2;
	}
	virtual int getNumOutputs() {
		return 2;
	}
	
	static void classInit(int sample_rate) {
	}
	
	virtual void instanceConstants(int sample_rate) {
		fSampleRate = sample_rate;
		fConst0 = std::min<float>(1.92e+05f, std::max<float>(1.0f, static_cast<float>(fSampleRate)));
		fConst1 = 6.2831855f / fConst0;
		fConst2 = 1e-06f * fConst0;
	}
	
	virtual void instanceResetUserInterface() {
		fCheckbox0 = static_cast<FAUSTFLOAT>(0.0f);
		fHslider0 = static_cast<FAUSTFLOAT>(1.0f);
		fHslider1 = static_cast<FAUSTFLOAT>(0.5f);
		fHslider2 = static_cast<FAUSTFLOAT>(0.0f);
		fHslider3 = static_cast<FAUSTFLOAT>(0.0f);
		fHslider4 = static_cast<FAUSTFLOAT>(1e+01f);
		fHslider5 = static_cast<FAUSTFLOAT>(1.0f);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
			iVec0[l0] = 0;
		}
		for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
			fRec1[l1] = 0.0f;
		}
		for (int l2 = 0; l2 < 2; l2 = l2 + 1) {
			fRec2[l2] = 0.0f;
		}
		IOTA0 = 0;
		for (int l3 = 0; l3 < 4096; l3 = l3 + 1) {
			fVec1[l3] = 0.0f;
		}
		for (int l4 = 0; l4 < 2; l4 = l4 + 1) {
			fRec3[l4] = 0.0f;
		}
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			fRec4[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			fRec0[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 4096; l7 = l7 + 1) {
			fVec2[l7] = 0.0f;
		}
		for (int l8 = 0; l8 < 2; l8 = l8 + 1) {
			fRec5[l8] = 0.0f;
		}
	}
	
	virtual void init(int sample_rate) {
		classInit(sample_rate);
		instanceInit(sample_rate);
	}
	
	virtual void instanceInit(int sample_rate) {
		instanceConstants(sample_rate);
		instanceResetUserInterface();
		instanceClear();
	}
	
	virtual flanger* clone() {
		return new flanger(*this);
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->declare(0, "tooltip", "Reference: https://ccrma.stanford.edu/~jos/pasp/Flanging.html");
		ui_interface->openVerticalBox("FLANGER");
		ui_interface->declare(0, "0", "");
		ui_interface->openHorizontalBox("0x00");
		ui_interface->declare(&fCheckbox0, "1", "");
		ui_interface->addCheckButton("Invert Flange Sum", &fCheckbox0);
		ui_interface->declare(&fHbargraph0, "2", "");
		ui_interface->declare(&fHbargraph0, "style", "led");
		ui_interface->declare(&fHbargraph0, "tooltip", "Display sum of flange delays");
		ui_interface->addHorizontalBargraph("Flange LFO", &fHbargraph0, FAUSTFLOAT(-1.5f), FAUSTFLOAT(1.5f));
		ui_interface->closeBox();
		ui_interface->declare(0, "1", "");
		ui_interface->openHorizontalBox("0x00");
		ui_interface->declare(&fHslider1, "1", "");
		ui_interface->declare(&fHslider1, "style", "knob");
		ui_interface->declare(&fHslider1, "unit", "Hz");
		ui_interface->addHorizontalSlider("Speed", &fHslider1, FAUSTFLOAT(0.5f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+01f), FAUSTFLOAT(0.01f));
		ui_interface->declare(&fHslider0, "2", "");
		ui_interface->declare(&fHslider0, "style", "knob");
		ui_interface->addHorizontalSlider("Depth", &fHslider0, FAUSTFLOAT(1.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(0.001f));
		ui_interface->declare(&fHslider3, "3", "");
		ui_interface->declare(&fHslider3, "style", "knob");
		ui_interface->addHorizontalSlider("Feedback", &fHslider3, FAUSTFLOAT(0.0f), FAUSTFLOAT(-0.999f), FAUSTFLOAT(0.999f), FAUSTFLOAT(0.001f));
		ui_interface->closeBox();
		ui_interface->declare(0, "2", "");
		ui_interface->openHorizontalBox("Delay Controls");
		ui_interface->declare(&fHslider4, "1", "");
		ui_interface->declare(&fHslider4, "style", "knob");
		ui_interface->declare(&fHslider4, "unit", "ms");
		ui_interface->addHorizontalSlider("Flange Delay", &fHslider4, FAUSTFLOAT(1e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(2e+01f), FAUSTFLOAT(0.001f));
		ui_interface->declare(&fHslider5, "2", "");
		ui_interface->declare(&fHslider5, "style", "knob");
		ui_interface->declare(&fHslider5, "unit", "ms");
		ui_interface->addHorizontalSlider("Delay Offset", &fHslider5, FAUSTFLOAT(1.0f), FAUSTFLOAT(0.0f), FAUSTFLOAT(2e+01f), FAUSTFLOAT(0.001f));
		ui_interface->closeBox();
		ui_interface->declare(0, "3", "");
		ui_interface->openHorizontalBox("0x00");
		ui_interface->declare(&fHslider2, "unit", "dB");
		ui_interface->addHorizontalSlider("Flanger Output Level", &fHslider2, FAUSTFLOAT(0.0f), FAUSTFLOAT(-6e+01f), FAUSTFLOAT(1e+01f), FAUSTFLOAT(0.1f));
		ui_interface->closeBox();
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* input1 = inputs[1];
		FAUSTFLOAT* output0 = outputs[0];
		FAUSTFLOAT* output1 = outputs[1];
		float fSlow0 = static_cast<float>(fHslider0);
		float fSlow1 = ((static_cast<int>(static_cast<float>(fCheckbox0))) ? -fSlow0 : fSlow0);
		float fSlow2 = fConst1 * static_cast<float>(fHslider1);
		float fSlow3 = std::cos(fSlow2);
		float fSlow4 = std::sin(fSlow2);
		float fSlow5 = std::pow(1e+01f, 0.05f * static_cast<float>(fHslider2));
		float fSlow6 = static_cast<float>(fHslider3);
		float fSlow7 = fConst2 * static_cast<float>(fHslider4);
		float fSlow8 = fConst2 * static_cast<float>(fHslider5);
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			iVec0[0] = 1;
			fRec1[0] = fSlow4 * fRec2[1] + fSlow3 * fRec1[1];
			fRec2[0] = static_cast<float>(1 - iVec0[1]) + fSlow3 * fRec2[1] - fSlow4 * fRec1[1];
			fHbargraph0 = static_cast<FAUSTFLOAT>(fRec2[0] + fRec1[0]);
			float fTemp0 = fSlow5 * static_cast<float>(input0[i0]);
			float fTemp1 = fSlow6 * fRec0[1] - fTemp0;
			fVec1[IOTA0 & 4095] = fTemp1;
			fRec3[0] = fSlow7 + 0.999f * fRec3[1];
			fRec4[0] = fSlow8 + 0.999f * fRec4[1];
			float fTemp2 = fRec4[0] + 0.5f * fRec3[0] * (fRec1[0] + 1.0f);
			int iTemp3 = static_cast<int>(fTemp2);
			float fTemp4 = std::floor(fTemp2);
			fRec0[0] = fVec1[(IOTA0 - std::min<int>(2049, std::max<int>(0, iTemp3))) & 4095] * (fTemp4 + (1.0f - fTemp2)) + (fTemp2 - fTemp4) * fVec1[(IOTA0 - std::min<int>(2049, std::max<int>(0, iTemp3 + 1))) & 4095];
			output0[i0] = static_cast<FAUSTFLOAT>(0.5f * (fTemp0 + fRec0[0] * fSlow1));
			float fTemp5 = fSlow5 * static_cast<float>(input1[i0]);
			float fTemp6 = fSlow6 * fRec5[1] - fTemp5;
			fVec2[IOTA0 & 4095] = fTemp6;
			float fTemp7 = fRec4[0] + 0.5f * fRec3[0] * (fRec2[0] + 1.0f);
			int iTemp8 = static_cast<int>(fTemp7);
			float fTemp9 = std::floor(fTemp7);
			fRec5[0] = fVec2[(IOTA0 - std::min<int>(2049, std::max<int>(0, iTemp8))) & 4095] * (fTemp9 + (1.0f - fTemp7)) + (fTemp7 - fTemp9) * fVec2[(IOTA0 - std::min<int>(2049, std::max<int>(0, iTemp8 + 1))) & 4095];
			output1[i0] = static_cast<FAUSTFLOAT>(0.5f * (fTemp5 + fRec5[0] * fSlow1));
			iVec0[1] = iVec0[0];
			fRec1[1] = fRec1[0];
			fRec2[1] = fRec2[0];
			IOTA0 = IOTA0 + 1;
			fRec3[1] = fRec3[0];
			fRec4[1] = fRec4[0];
			fRec0[1] = fRec0[0];
			fRec5[1] = fRec5[0];
		}
	}

};

/*************************** END USER SECTION *****************************/

/******************* BEGIN ARCHITECTURE SECTION (part 2/2) ***************/

#include <memory>
#include <mutex>

namespace zrythm_faust
{

  /** Creates a new instance of the generated DSP class. */
  std::unique_ptr<zrythm::plugins::faust::dsp> create_flanger ()
  {
    return std::make_unique<flanger> ();
  }

  /**
   * Calls the static classInit() for the given sample rate.
   *
   * classInit() fills static tables shared by all instances of the class, so it
   * is only called when needed: on first use, or when the sample rate changes
   * (processing is stopped during preparation, so this cannot race with
   * compute()).
   */
  void class_init_flanger (int sample_rate)
  {
    static std::mutex mutex;
    static int        initialized_sample_rate = 0;

    const std::lock_guard lock (mutex);
    if (initialized_sample_rate != sample_rate)
      {
        flanger::classInit (sample_rate);
        initialized_sample_rate = sample_rate;
      }
  }

} // namespace zrythm_faust

/******************** END ARCHITECTURE SECTION (part 2/2) ****************/

#endif

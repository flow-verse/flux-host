/* ------------------------------------------------------------
author: "Zrythm"
copyright: "© 2022 Alexandros Theodotou"
license: "AGPL-3.0-or-later"
name: "Zita Rev1"
version: "1.0"
Code generated with Faust 2.85.9 (https://faust.grame.fr)
Compilation options: -a data/plugins/zrythm-arch.cpp -lang cpp -fpga-mem-th 4 -ct 1 -cn zita_rev1 -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 0
------------------------------------------------------------ */

#ifndef  __zita_rev1_H__
#define  __zita_rev1_H__

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
#define FAUSTCLASS zita_rev1
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

static float zita_rev1_faustpower2_f(float value) {
	return value * value;
}

class zita_rev1 : public dsp {
	
 private:
	
	int IOTA0;
	float fVec0[16384];
	FAUSTFLOAT fHslider0;
	FAUSTFLOAT fHslider1;
	int fSampleRate;
	float fConst0;
	float fConst1;
	float fConst2;
	FAUSTFLOAT fHslider2;
	float fConst3;
	FAUSTFLOAT fHslider3;
	float fConst4;
	float fRec11[2];
	FAUSTFLOAT fHslider4;
	float fRec10[2];
	float fVec1[65536];
	float fConst5;
	int iConst6;
	FAUSTFLOAT fHslider5;
	float fConst7;
	float fVec2[8192];
	int iConst8;
	float fRec8[2];
	float fConst9;
	float fConst10;
	float fRec15[2];
	float fRec14[2];
	float fVec3[32768];
	float fConst11;
	int iConst12;
	float fVec4[4096];
	int iConst13;
	float fRec12[2];
	float fConst14;
	float fConst15;
	float fRec19[2];
	float fRec18[2];
	float fVec5[32768];
	float fConst16;
	int iConst17;
	float fVec6[8192];
	int iConst18;
	float fRec16[2];
	float fConst19;
	float fConst20;
	float fRec23[2];
	float fRec22[2];
	float fVec7[32768];
	float fConst21;
	int iConst22;
	float fVec8[4096];
	int iConst23;
	float fRec20[2];
	float fConst24;
	float fConst25;
	float fRec27[2];
	float fRec26[2];
	float fVec9[65536];
	float fConst26;
	int iConst27;
	float fVec10[16384];
	float fVec11[8192];
	int iConst28;
	float fRec24[2];
	float fConst29;
	float fConst30;
	float fRec31[2];
	float fRec30[2];
	float fVec12[65536];
	float fConst31;
	int iConst32;
	float fVec13[8192];
	int iConst33;
	float fRec28[2];
	float fConst34;
	float fConst35;
	float fRec35[2];
	float fRec34[2];
	float fVec14[65536];
	float fConst36;
	int iConst37;
	float fVec15[8192];
	int iConst38;
	float fRec32[2];
	float fConst39;
	float fConst40;
	float fRec39[2];
	float fRec38[2];
	float fVec16[65536];
	float fConst41;
	int iConst42;
	float fVec17[4096];
	int iConst43;
	float fRec36[2];
	float fRec0[3];
	float fRec1[3];
	float fRec2[3];
	float fRec3[3];
	float fRec4[3];
	float fRec5[3];
	float fRec6[3];
	float fRec7[3];
	
 public:
	zita_rev1() {
	}
	
	zita_rev1(const zita_rev1&) = default;
	
	virtual ~zita_rev1() = default;
	
	zita_rev1& operator=(const zita_rev1&) = default;
	
	void metadata(Meta* m) { 
		m->declare("author", "Zrythm");
		m->declare("basics.lib/name", "Faust Basic Element Library");
		m->declare("basics.lib/version", "1.22.0");
		m->declare("compile_options", "-a data/plugins/zrythm-arch.cpp -lang cpp -fpga-mem-th 4 -ct 1 -cn zita_rev1 -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 0");
		m->declare("copyright", "© 2022 Alexandros Theodotou");
		m->declare("delays.lib/name", "Faust Delay Library");
		m->declare("delays.lib/version", "1.2.0");
		m->declare("description", "Zita reverb algorithm");
		m->declare("filename", "zita_rev1.dsp");
		m->declare("filters.lib/allpass_comb:author", "Julius O. Smith III");
		m->declare("filters.lib/allpass_comb:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/allpass_comb:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/lowpass0_highpass1", "MIT-style STK-4.3 license");
		m->declare("filters.lib/lowpass0_highpass1:author", "Julius O. Smith III");
		m->declare("filters.lib/lowpass:author", "Julius O. Smith III");
		m->declare("filters.lib/lowpass:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/lowpass:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/name", "Faust Filters Library");
		m->declare("filters.lib/tf1:author", "Julius O. Smith III");
		m->declare("filters.lib/tf1:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/tf1:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/tf1s:author", "Julius O. Smith III");
		m->declare("filters.lib/tf1s:copyright", "Copyright (C) 2003-2019 by Julius O. Smith III <jos@ccrma.stanford.edu>");
		m->declare("filters.lib/tf1s:license", "MIT-style STK-4.3 license");
		m->declare("filters.lib/version", "1.7.1");
		m->declare("license", "AGPL-3.0-or-later");
		m->declare("maths.lib/author", "GRAME");
		m->declare("maths.lib/copyright", "GRAME");
		m->declare("maths.lib/license", "LGPL with exception");
		m->declare("maths.lib/name", "Faust Math Library");
		m->declare("maths.lib/version", "2.9.0");
		m->declare("name", "Zita Rev1");
		m->declare("platform.lib/name", "Generic Platform Library");
		m->declare("platform.lib/version", "1.3.0");
		m->declare("reverbs.lib/name", "Faust Reverb Library");
		m->declare("reverbs.lib/version", "1.5.1");
		m->declare("routes.lib/hadamard:author", "Remy Muller, revised by Romain Michon");
		m->declare("routes.lib/name", "Faust Signal Routing Library");
		m->declare("routes.lib/version", "1.3.0");
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
		fConst1 = std::floor(0.174713f * fConst0 + 0.5f);
		fConst2 = 6.9077554f * (fConst1 / fConst0);
		fConst3 = 6.2831855f / fConst0;
		fConst4 = 3.1415927f / fConst0;
		fConst5 = std::floor(0.022904f * fConst0 + 0.5f);
		iConst6 = static_cast<int>(std::min<float>(32768.0f, std::max<float>(0.0f, fConst1 - fConst5)));
		fConst7 = 0.001f * fConst0;
		iConst8 = static_cast<int>(std::min<float>(8192.0f, std::max<float>(0.0f, fConst5 + -1.0f)));
		fConst9 = std::floor(0.153129f * fConst0 + 0.5f);
		fConst10 = 6.9077554f * (fConst9 / fConst0);
		fConst11 = std::floor(0.020346f * fConst0 + 0.5f);
		iConst12 = static_cast<int>(std::min<float>(32768.0f, std::max<float>(0.0f, fConst9 - fConst11)));
		iConst13 = static_cast<int>(std::min<float>(4096.0f, std::max<float>(0.0f, fConst11 + -1.0f)));
		fConst14 = std::floor(0.127837f * fConst0 + 0.5f);
		fConst15 = 6.9077554f * (fConst14 / fConst0);
		fConst16 = std::floor(0.031604f * fConst0 + 0.5f);
		iConst17 = static_cast<int>(std::min<float>(32768.0f, std::max<float>(0.0f, fConst14 - fConst16)));
		iConst18 = static_cast<int>(std::min<float>(8192.0f, std::max<float>(0.0f, fConst16 + -1.0f)));
		fConst19 = std::floor(0.125f * fConst0 + 0.5f);
		fConst20 = 6.9077554f * (fConst19 / fConst0);
		fConst21 = std::floor(0.013458f * fConst0 + 0.5f);
		iConst22 = static_cast<int>(std::min<float>(32768.0f, std::max<float>(0.0f, fConst19 - fConst21)));
		iConst23 = static_cast<int>(std::min<float>(4096.0f, std::max<float>(0.0f, fConst21 + -1.0f)));
		fConst24 = std::floor(0.210389f * fConst0 + 0.5f);
		fConst25 = 6.9077554f * (fConst24 / fConst0);
		fConst26 = std::floor(0.024421f * fConst0 + 0.5f);
		iConst27 = static_cast<int>(std::min<float>(65536.0f, std::max<float>(0.0f, fConst24 - fConst26)));
		iConst28 = static_cast<int>(std::min<float>(8192.0f, std::max<float>(0.0f, fConst26 + -1.0f)));
		fConst29 = std::floor(0.192303f * fConst0 + 0.5f);
		fConst30 = 6.9077554f * (fConst29 / fConst0);
		fConst31 = std::floor(0.029291f * fConst0 + 0.5f);
		iConst32 = static_cast<int>(std::min<float>(32768.0f, std::max<float>(0.0f, fConst29 - fConst31)));
		iConst33 = static_cast<int>(std::min<float>(8192.0f, std::max<float>(0.0f, fConst31 + -1.0f)));
		fConst34 = std::floor(0.256891f * fConst0 + 0.5f);
		fConst35 = 6.9077554f * (fConst34 / fConst0);
		fConst36 = std::floor(0.027333f * fConst0 + 0.5f);
		iConst37 = static_cast<int>(std::min<float>(65536.0f, std::max<float>(0.0f, fConst34 - fConst36)));
		iConst38 = static_cast<int>(std::min<float>(8192.0f, std::max<float>(0.0f, fConst36 + -1.0f)));
		fConst39 = std::floor(0.219991f * fConst0 + 0.5f);
		fConst40 = 6.9077554f * (fConst39 / fConst0);
		fConst41 = std::floor(0.019123f * fConst0 + 0.5f);
		iConst42 = static_cast<int>(std::min<float>(65536.0f, std::max<float>(0.0f, fConst39 - fConst41)));
		iConst43 = static_cast<int>(std::min<float>(4096.0f, std::max<float>(0.0f, fConst41 + -1.0f)));
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = static_cast<FAUSTFLOAT>(5e+01f);
		fHslider1 = static_cast<FAUSTFLOAT>(2.0f);
		fHslider2 = static_cast<FAUSTFLOAT>(6e+03f);
		fHslider3 = static_cast<FAUSTFLOAT>(2e+02f);
		fHslider4 = static_cast<FAUSTFLOAT>(3.0f);
		fHslider5 = static_cast<FAUSTFLOAT>(2e+01f);
	}
	
	virtual void instanceClear() {
		IOTA0 = 0;
		for (int l0 = 0; l0 < 16384; l0 = l0 + 1) {
			fVec0[l0] = 0.0f;
		}
		for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
			fRec11[l1] = 0.0f;
		}
		for (int l2 = 0; l2 < 2; l2 = l2 + 1) {
			fRec10[l2] = 0.0f;
		}
		for (int l3 = 0; l3 < 65536; l3 = l3 + 1) {
			fVec1[l3] = 0.0f;
		}
		for (int l4 = 0; l4 < 8192; l4 = l4 + 1) {
			fVec2[l4] = 0.0f;
		}
		for (int l5 = 0; l5 < 2; l5 = l5 + 1) {
			fRec8[l5] = 0.0f;
		}
		for (int l6 = 0; l6 < 2; l6 = l6 + 1) {
			fRec15[l6] = 0.0f;
		}
		for (int l7 = 0; l7 < 2; l7 = l7 + 1) {
			fRec14[l7] = 0.0f;
		}
		for (int l8 = 0; l8 < 32768; l8 = l8 + 1) {
			fVec3[l8] = 0.0f;
		}
		for (int l9 = 0; l9 < 4096; l9 = l9 + 1) {
			fVec4[l9] = 0.0f;
		}
		for (int l10 = 0; l10 < 2; l10 = l10 + 1) {
			fRec12[l10] = 0.0f;
		}
		for (int l11 = 0; l11 < 2; l11 = l11 + 1) {
			fRec19[l11] = 0.0f;
		}
		for (int l12 = 0; l12 < 2; l12 = l12 + 1) {
			fRec18[l12] = 0.0f;
		}
		for (int l13 = 0; l13 < 32768; l13 = l13 + 1) {
			fVec5[l13] = 0.0f;
		}
		for (int l14 = 0; l14 < 8192; l14 = l14 + 1) {
			fVec6[l14] = 0.0f;
		}
		for (int l15 = 0; l15 < 2; l15 = l15 + 1) {
			fRec16[l15] = 0.0f;
		}
		for (int l16 = 0; l16 < 2; l16 = l16 + 1) {
			fRec23[l16] = 0.0f;
		}
		for (int l17 = 0; l17 < 2; l17 = l17 + 1) {
			fRec22[l17] = 0.0f;
		}
		for (int l18 = 0; l18 < 32768; l18 = l18 + 1) {
			fVec7[l18] = 0.0f;
		}
		for (int l19 = 0; l19 < 4096; l19 = l19 + 1) {
			fVec8[l19] = 0.0f;
		}
		for (int l20 = 0; l20 < 2; l20 = l20 + 1) {
			fRec20[l20] = 0.0f;
		}
		for (int l21 = 0; l21 < 2; l21 = l21 + 1) {
			fRec27[l21] = 0.0f;
		}
		for (int l22 = 0; l22 < 2; l22 = l22 + 1) {
			fRec26[l22] = 0.0f;
		}
		for (int l23 = 0; l23 < 65536; l23 = l23 + 1) {
			fVec9[l23] = 0.0f;
		}
		for (int l24 = 0; l24 < 16384; l24 = l24 + 1) {
			fVec10[l24] = 0.0f;
		}
		for (int l25 = 0; l25 < 8192; l25 = l25 + 1) {
			fVec11[l25] = 0.0f;
		}
		for (int l26 = 0; l26 < 2; l26 = l26 + 1) {
			fRec24[l26] = 0.0f;
		}
		for (int l27 = 0; l27 < 2; l27 = l27 + 1) {
			fRec31[l27] = 0.0f;
		}
		for (int l28 = 0; l28 < 2; l28 = l28 + 1) {
			fRec30[l28] = 0.0f;
		}
		for (int l29 = 0; l29 < 65536; l29 = l29 + 1) {
			fVec12[l29] = 0.0f;
		}
		for (int l30 = 0; l30 < 8192; l30 = l30 + 1) {
			fVec13[l30] = 0.0f;
		}
		for (int l31 = 0; l31 < 2; l31 = l31 + 1) {
			fRec28[l31] = 0.0f;
		}
		for (int l32 = 0; l32 < 2; l32 = l32 + 1) {
			fRec35[l32] = 0.0f;
		}
		for (int l33 = 0; l33 < 2; l33 = l33 + 1) {
			fRec34[l33] = 0.0f;
		}
		for (int l34 = 0; l34 < 65536; l34 = l34 + 1) {
			fVec14[l34] = 0.0f;
		}
		for (int l35 = 0; l35 < 8192; l35 = l35 + 1) {
			fVec15[l35] = 0.0f;
		}
		for (int l36 = 0; l36 < 2; l36 = l36 + 1) {
			fRec32[l36] = 0.0f;
		}
		for (int l37 = 0; l37 < 2; l37 = l37 + 1) {
			fRec39[l37] = 0.0f;
		}
		for (int l38 = 0; l38 < 2; l38 = l38 + 1) {
			fRec38[l38] = 0.0f;
		}
		for (int l39 = 0; l39 < 65536; l39 = l39 + 1) {
			fVec16[l39] = 0.0f;
		}
		for (int l40 = 0; l40 < 4096; l40 = l40 + 1) {
			fVec17[l40] = 0.0f;
		}
		for (int l41 = 0; l41 < 2; l41 = l41 + 1) {
			fRec36[l41] = 0.0f;
		}
		for (int l42 = 0; l42 < 3; l42 = l42 + 1) {
			fRec0[l42] = 0.0f;
		}
		for (int l43 = 0; l43 < 3; l43 = l43 + 1) {
			fRec1[l43] = 0.0f;
		}
		for (int l44 = 0; l44 < 3; l44 = l44 + 1) {
			fRec2[l44] = 0.0f;
		}
		for (int l45 = 0; l45 < 3; l45 = l45 + 1) {
			fRec3[l45] = 0.0f;
		}
		for (int l46 = 0; l46 < 3; l46 = l46 + 1) {
			fRec4[l46] = 0.0f;
		}
		for (int l47 = 0; l47 < 3; l47 = l47 + 1) {
			fRec5[l47] = 0.0f;
		}
		for (int l48 = 0; l48 < 3; l48 = l48 + 1) {
			fRec6[l48] = 0.0f;
		}
		for (int l49 = 0; l49 < 3; l49 = l49 + 1) {
			fRec7[l49] = 0.0f;
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
	
	virtual zita_rev1* clone() {
		return new zita_rev1(*this);
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("Zita Rev1");
		ui_interface->declare(&fHslider5, "1", "");
		ui_interface->declare(&fHslider5, "unit", "ms");
		ui_interface->addHorizontalSlider("Pre-Delay", &fHslider5, FAUSTFLOAT(2e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(1.0f));
		ui_interface->declare(&fHslider3, "2", "");
		ui_interface->declare(&fHslider3, "unit", "Hz");
		ui_interface->addHorizontalSlider("F1", &fHslider3, FAUSTFLOAT(2e+02f), FAUSTFLOAT(5e+01f), FAUSTFLOAT(1e+03f), FAUSTFLOAT(1.0f));
		ui_interface->declare(&fHslider2, "3", "");
		ui_interface->declare(&fHslider2, "unit", "Hz");
		ui_interface->addHorizontalSlider("F2", &fHslider2, FAUSTFLOAT(6e+03f), FAUSTFLOAT(1.5e+03f), FAUSTFLOAT(9.408e+04f), FAUSTFLOAT(1.0f));
		ui_interface->declare(&fHslider4, "4", "");
		ui_interface->declare(&fHslider4, "tooltip", "T60 = time (in seconds) to decay 60dB in low-frequency band");
		ui_interface->declare(&fHslider4, "unit", "s");
		ui_interface->addHorizontalSlider("Low RT60", &fHslider4, FAUSTFLOAT(3.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(8.0f), FAUSTFLOAT(0.1f));
		ui_interface->declare(&fHslider1, "5", "");
		ui_interface->declare(&fHslider1, "tooltip", "T60 = time (in seconds) to decay 60dB in middle band");
		ui_interface->declare(&fHslider1, "unit", "s");
		ui_interface->addHorizontalSlider("Mid RT60", &fHslider1, FAUSTFLOAT(2.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(8.0f), FAUSTFLOAT(0.1f));
		ui_interface->declare(&fHslider0, "6", "");
		ui_interface->declare(&fHslider0, "tooltip", "Mix amount");
		ui_interface->declare(&fHslider0, "unit", "percentage");
		ui_interface->addHorizontalSlider("Mix", &fHslider0, FAUSTFLOAT(5e+01f), FAUSTFLOAT(0.0f), FAUSTFLOAT(1e+02f), FAUSTFLOAT(0.1f));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* input1 = inputs[1];
		FAUSTFLOAT* output0 = outputs[0];
		FAUSTFLOAT* output1 = outputs[1];
		float fSlow0 = static_cast<float>(fHslider0);
		float fSlow1 = 1.0f - 0.01f * fSlow0;
		float fSlow2 = static_cast<float>(fHslider1);
		float fSlow3 = std::exp(-(fConst2 / fSlow2));
		float fSlow4 = zita_rev1_faustpower2_f(fSlow3);
		float fSlow5 = 1.0f - fSlow4;
		float fSlow6 = std::cos(fConst3 * static_cast<float>(fHslider2));
		float fSlow7 = 1.0f - fSlow6 * fSlow4;
		float fSlow8 = std::sqrt(std::max<float>(0.0f, zita_rev1_faustpower2_f(fSlow7) / zita_rev1_faustpower2_f(fSlow5) + -1.0f));
		float fSlow9 = fSlow7 / fSlow5;
		float fSlow10 = fSlow9 - fSlow8;
		float fSlow11 = 1.0f / std::tan(fConst4 * static_cast<float>(fHslider3));
		float fSlow12 = 1.0f - fSlow11;
		float fSlow13 = 1.0f / (fSlow11 + 1.0f);
		float fSlow14 = static_cast<float>(fHslider4);
		float fSlow15 = std::exp(-(fConst2 / fSlow14)) / fSlow3 + -1.0f;
		float fSlow16 = fSlow3 * (fSlow8 + (1.0f - fSlow9));
		int iSlow17 = static_cast<int>(std::min<float>(8192.0f, std::max<float>(0.0f, fConst7 * static_cast<float>(fHslider5))));
		float fSlow18 = std::exp(-(fConst10 / fSlow2));
		float fSlow19 = zita_rev1_faustpower2_f(fSlow18);
		float fSlow20 = 1.0f - fSlow19;
		float fSlow21 = 1.0f - fSlow19 * fSlow6;
		float fSlow22 = std::sqrt(std::max<float>(0.0f, zita_rev1_faustpower2_f(fSlow21) / zita_rev1_faustpower2_f(fSlow20) + -1.0f));
		float fSlow23 = fSlow21 / fSlow20;
		float fSlow24 = fSlow23 - fSlow22;
		float fSlow25 = std::exp(-(fConst10 / fSlow14)) / fSlow18 + -1.0f;
		float fSlow26 = fSlow18 * (fSlow22 + (1.0f - fSlow23));
		float fSlow27 = std::exp(-(fConst15 / fSlow2));
		float fSlow28 = zita_rev1_faustpower2_f(fSlow27);
		float fSlow29 = 1.0f - fSlow28;
		float fSlow30 = 1.0f - fSlow6 * fSlow28;
		float fSlow31 = std::sqrt(std::max<float>(0.0f, zita_rev1_faustpower2_f(fSlow30) / zita_rev1_faustpower2_f(fSlow29) + -1.0f));
		float fSlow32 = fSlow30 / fSlow29;
		float fSlow33 = fSlow32 - fSlow31;
		float fSlow34 = std::exp(-(fConst15 / fSlow14)) / fSlow27 + -1.0f;
		float fSlow35 = fSlow27 * (fSlow31 + (1.0f - fSlow32));
		float fSlow36 = std::exp(-(fConst20 / fSlow2));
		float fSlow37 = zita_rev1_faustpower2_f(fSlow36);
		float fSlow38 = 1.0f - fSlow37;
		float fSlow39 = 1.0f - fSlow6 * fSlow37;
		float fSlow40 = std::sqrt(std::max<float>(0.0f, zita_rev1_faustpower2_f(fSlow39) / zita_rev1_faustpower2_f(fSlow38) + -1.0f));
		float fSlow41 = fSlow39 / fSlow38;
		float fSlow42 = fSlow41 - fSlow40;
		float fSlow43 = std::exp(-(fConst20 / fSlow14)) / fSlow36 + -1.0f;
		float fSlow44 = fSlow36 * (fSlow40 + (1.0f - fSlow41));
		float fSlow45 = std::exp(-(fConst25 / fSlow2));
		float fSlow46 = zita_rev1_faustpower2_f(fSlow45);
		float fSlow47 = 1.0f - fSlow46;
		float fSlow48 = 1.0f - fSlow6 * fSlow46;
		float fSlow49 = std::sqrt(std::max<float>(0.0f, zita_rev1_faustpower2_f(fSlow48) / zita_rev1_faustpower2_f(fSlow47) + -1.0f));
		float fSlow50 = fSlow48 / fSlow47;
		float fSlow51 = fSlow50 - fSlow49;
		float fSlow52 = std::exp(-(fConst25 / fSlow14)) / fSlow45 + -1.0f;
		float fSlow53 = fSlow45 * (fSlow49 + (1.0f - fSlow50));
		float fSlow54 = std::exp(-(fConst30 / fSlow2));
		float fSlow55 = zita_rev1_faustpower2_f(fSlow54);
		float fSlow56 = 1.0f - fSlow55;
		float fSlow57 = 1.0f - fSlow6 * fSlow55;
		float fSlow58 = std::sqrt(std::max<float>(0.0f, zita_rev1_faustpower2_f(fSlow57) / zita_rev1_faustpower2_f(fSlow56) + -1.0f));
		float fSlow59 = fSlow57 / fSlow56;
		float fSlow60 = fSlow59 - fSlow58;
		float fSlow61 = std::exp(-(fConst30 / fSlow14)) / fSlow54 + -1.0f;
		float fSlow62 = fSlow54 * (fSlow58 + (1.0f - fSlow59));
		float fSlow63 = std::exp(-(fConst35 / fSlow2));
		float fSlow64 = zita_rev1_faustpower2_f(fSlow63);
		float fSlow65 = 1.0f - fSlow64;
		float fSlow66 = 1.0f - fSlow6 * fSlow64;
		float fSlow67 = std::sqrt(std::max<float>(0.0f, zita_rev1_faustpower2_f(fSlow66) / zita_rev1_faustpower2_f(fSlow65) + -1.0f));
		float fSlow68 = fSlow66 / fSlow65;
		float fSlow69 = fSlow68 - fSlow67;
		float fSlow70 = std::exp(-(fConst35 / fSlow14)) / fSlow63 + -1.0f;
		float fSlow71 = fSlow63 * (fSlow67 + (1.0f - fSlow68));
		float fSlow72 = std::exp(-(fConst40 / fSlow2));
		float fSlow73 = zita_rev1_faustpower2_f(fSlow72);
		float fSlow74 = 1.0f - fSlow73;
		float fSlow75 = 1.0f - fSlow6 * fSlow73;
		float fSlow76 = std::sqrt(std::max<float>(0.0f, zita_rev1_faustpower2_f(fSlow75) / zita_rev1_faustpower2_f(fSlow74) + -1.0f));
		float fSlow77 = fSlow75 / fSlow74;
		float fSlow78 = fSlow77 - fSlow76;
		float fSlow79 = std::exp(-(fConst40 / fSlow14)) / fSlow72 + -1.0f;
		float fSlow80 = fSlow72 * (fSlow76 + (1.0f - fSlow77));
		float fSlow81 = 0.0037f * fSlow0;
		for (int i0 = 0; i0 < count; i0 = i0 + 1) {
			float fTemp0 = static_cast<float>(input0[i0]);
			fVec0[IOTA0 & 16383] = fTemp0;
			fRec11[0] = -(fSlow13 * (fSlow12 * fRec11[1] - (fRec4[1] + fRec4[2])));
			fRec10[0] = fSlow16 * (fRec4[1] + fSlow15 * fRec11[0]) + fSlow10 * fRec10[1];
			fVec1[IOTA0 & 65535] = 0.35355338f * fRec10[0] + 1e-20f;
			float fTemp1 = 0.3f * fVec0[(IOTA0 - iSlow17) & 16383];
			float fTemp2 = fTemp1 + fVec1[(IOTA0 - iConst6) & 65535] - 0.6f * fRec8[1];
			fVec2[IOTA0 & 8191] = fTemp2;
			fRec8[0] = fVec2[(IOTA0 - iConst8) & 8191];
			float fRec9 = 0.6f * fTemp2;
			fRec15[0] = -(fSlow13 * (fSlow12 * fRec15[1] - (fRec0[1] + fRec0[2])));
			fRec14[0] = fSlow26 * (fRec0[1] + fSlow25 * fRec15[0]) + fSlow24 * fRec14[1];
			fVec3[IOTA0 & 32767] = 0.35355338f * fRec14[0] + 1e-20f;
			float fTemp3 = fVec3[(IOTA0 - iConst12) & 32767] + fTemp1 - 0.6f * fRec12[1];
			fVec4[IOTA0 & 4095] = fTemp3;
			fRec12[0] = fVec4[(IOTA0 - iConst13) & 4095];
			float fRec13 = 0.6f * fTemp3;
			float fTemp4 = fRec13 + fRec9;
			fRec19[0] = -(fSlow13 * (fSlow12 * fRec19[1] - (fRec2[1] + fRec2[2])));
			fRec18[0] = fSlow35 * (fRec2[1] + fSlow34 * fRec19[0]) + fSlow33 * fRec18[1];
			fVec5[IOTA0 & 32767] = 0.35355338f * fRec18[0] + 1e-20f;
			float fTemp5 = fVec5[(IOTA0 - iConst17) & 32767] - (fTemp1 + 0.6f * fRec16[1]);
			fVec6[IOTA0 & 8191] = fTemp5;
			fRec16[0] = fVec6[(IOTA0 - iConst18) & 8191];
			float fRec17 = 0.6f * fTemp5;
			fRec23[0] = -(fSlow13 * (fSlow12 * fRec23[1] - (fRec6[1] + fRec6[2])));
			fRec22[0] = fSlow44 * (fRec6[1] + fSlow43 * fRec23[0]) + fSlow42 * fRec22[1];
			fVec7[IOTA0 & 32767] = 0.35355338f * fRec22[0] + 1e-20f;
			float fTemp6 = fVec7[(IOTA0 - iConst22) & 32767] - (fTemp1 + 0.6f * fRec20[1]);
			fVec8[IOTA0 & 4095] = fTemp6;
			fRec20[0] = fVec8[(IOTA0 - iConst23) & 4095];
			float fRec21 = 0.6f * fTemp6;
			float fTemp7 = fRec21 + fRec17 + fTemp4;
			fRec27[0] = -(fSlow13 * (fSlow12 * fRec27[1] - (fRec1[1] + fRec1[2])));
			fRec26[0] = fSlow53 * (fRec1[1] + fSlow52 * fRec27[0]) + fSlow51 * fRec26[1];
			fVec9[IOTA0 & 65535] = 0.35355338f * fRec26[0] + 1e-20f;
			float fTemp8 = static_cast<float>(input1[i0]);
			fVec10[IOTA0 & 16383] = fTemp8;
			float fTemp9 = 0.3f * fVec10[(IOTA0 - iSlow17) & 16383];
			float fTemp10 = fTemp9 + 0.6f * fRec24[1] + fVec9[(IOTA0 - iConst27) & 65535];
			fVec11[IOTA0 & 8191] = fTemp10;
			fRec24[0] = fVec11[(IOTA0 - iConst28) & 8191];
			float fRec25 = -(0.6f * fTemp10);
			fRec31[0] = -(fSlow13 * (fSlow12 * fRec31[1] - (fRec5[1] + fRec5[2])));
			fRec30[0] = fSlow62 * (fRec5[1] + fSlow61 * fRec31[0]) + fSlow60 * fRec30[1];
			fVec12[IOTA0 & 65535] = 0.35355338f * fRec30[0] + 1e-20f;
			float fTemp11 = fVec12[(IOTA0 - iConst32) & 65535] + fTemp9 + 0.6f * fRec28[1];
			fVec13[IOTA0 & 8191] = fTemp11;
			fRec28[0] = fVec13[(IOTA0 - iConst33) & 8191];
			float fRec29 = -(0.6f * fTemp11);
			fRec35[0] = -(fSlow13 * (fSlow12 * fRec35[1] - (fRec3[1] + fRec3[2])));
			fRec34[0] = fSlow71 * (fRec3[1] + fSlow70 * fRec35[0]) + fSlow69 * fRec34[1];
			fVec14[IOTA0 & 65535] = 0.35355338f * fRec34[0] + 1e-20f;
			float fTemp12 = 0.6f * fRec32[1] + fVec14[(IOTA0 - iConst37) & 65535];
			fVec15[IOTA0 & 8191] = fTemp12 - fTemp9;
			fRec32[0] = fVec15[(IOTA0 - iConst38) & 8191];
			float fRec33 = 0.6f * (fTemp9 - fTemp12);
			fRec39[0] = -(fSlow13 * (fSlow12 * fRec39[1] - (fRec7[1] + fRec7[2])));
			fRec38[0] = fSlow80 * (fRec7[1] + fSlow79 * fRec39[0]) + fSlow78 * fRec38[1];
			fVec16[IOTA0 & 65535] = 0.35355338f * fRec38[0] + 1e-20f;
			float fTemp13 = 0.6f * fRec36[1] + fVec16[(IOTA0 - iConst42) & 65535];
			fVec17[IOTA0 & 4095] = fTemp13 - fTemp9;
			fRec36[0] = fVec17[(IOTA0 - iConst43) & 4095];
			float fRec37 = 0.6f * (fTemp9 - fTemp13);
			fRec0[0] = fRec36[1] + fRec32[1] + fRec28[1] + fRec24[1] + fRec20[1] + fRec16[1] + fRec8[1] + fRec12[1] + fRec37 + fRec33 + fRec29 + fRec25 + fTemp7;
			fRec1[0] = fRec20[1] + fRec16[1] + fRec8[1] + fRec12[1] + fTemp7 - (fRec36[1] + fRec32[1] + fRec28[1] + fRec24[1] + fRec37 + fRec33 + fRec25 + fRec29);
			float fTemp14 = fRec17 + fRec21;
			fRec2[0] = fRec28[1] + fRec24[1] + fRec8[1] + fRec12[1] + fRec29 + fRec25 + fTemp4 - (fRec36[1] + fRec32[1] + fRec20[1] + fRec16[1] + fRec37 + fRec33 + fTemp14);
			fRec3[0] = fRec36[1] + fRec32[1] + fRec8[1] + fRec12[1] + fRec37 + fRec33 + fTemp4 - (fRec28[1] + fRec24[1] + fRec20[1] + fRec16[1] + fRec29 + fRec25 + fTemp14);
			float fTemp15 = fRec9 + fRec21;
			float fTemp16 = fRec13 + fRec17;
			fRec4[0] = fRec32[1] + fRec24[1] + fRec16[1] + fRec12[1] + fRec33 + fRec25 + fTemp16 - (fRec36[1] + fRec28[1] + fRec20[1] + fRec8[1] + fRec37 + fRec29 + fTemp15);
			fRec5[0] = fRec36[1] + fRec28[1] + fRec16[1] + fRec12[1] + fRec37 + fRec29 + fTemp16 - (fRec32[1] + fRec24[1] + fRec20[1] + fRec8[1] + fRec33 + fRec25 + fTemp15);
			float fTemp17 = fRec9 + fRec17;
			float fTemp18 = fRec13 + fRec21;
			fRec6[0] = fRec36[1] + fRec24[1] + fRec20[1] + fRec12[1] + fRec37 + fRec25 + fTemp18 - (fRec32[1] + fRec28[1] + fRec16[1] + fRec8[1] + fRec33 + fRec29 + fTemp17);
			fRec7[0] = fRec32[1] + fRec28[1] + fRec20[1] + fRec12[1] + fRec33 + fRec29 + fTemp18 - (fRec36[1] + fRec24[1] + fRec16[1] + fRec8[1] + fRec37 + fRec25 + fTemp17);
			output0[i0] = static_cast<FAUSTFLOAT>(fSlow81 * (fRec1[0] + fRec2[0]) + fSlow1 * fTemp0);
			output1[i0] = static_cast<FAUSTFLOAT>(fSlow81 * (fRec1[0] - fRec2[0]) + fSlow1 * fTemp8);
			IOTA0 = IOTA0 + 1;
			fRec11[1] = fRec11[0];
			fRec10[1] = fRec10[0];
			fRec8[1] = fRec8[0];
			fRec15[1] = fRec15[0];
			fRec14[1] = fRec14[0];
			fRec12[1] = fRec12[0];
			fRec19[1] = fRec19[0];
			fRec18[1] = fRec18[0];
			fRec16[1] = fRec16[0];
			fRec23[1] = fRec23[0];
			fRec22[1] = fRec22[0];
			fRec20[1] = fRec20[0];
			fRec27[1] = fRec27[0];
			fRec26[1] = fRec26[0];
			fRec24[1] = fRec24[0];
			fRec31[1] = fRec31[0];
			fRec30[1] = fRec30[0];
			fRec28[1] = fRec28[0];
			fRec35[1] = fRec35[0];
			fRec34[1] = fRec34[0];
			fRec32[1] = fRec32[0];
			fRec39[1] = fRec39[0];
			fRec38[1] = fRec38[0];
			fRec36[1] = fRec36[0];
			fRec0[2] = fRec0[1];
			fRec0[1] = fRec0[0];
			fRec1[2] = fRec1[1];
			fRec1[1] = fRec1[0];
			fRec2[2] = fRec2[1];
			fRec2[1] = fRec2[0];
			fRec3[2] = fRec3[1];
			fRec3[1] = fRec3[0];
			fRec4[2] = fRec4[1];
			fRec4[1] = fRec4[0];
			fRec5[2] = fRec5[1];
			fRec5[1] = fRec5[0];
			fRec6[2] = fRec6[1];
			fRec6[1] = fRec6[0];
			fRec7[2] = fRec7[1];
			fRec7[1] = fRec7[0];
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
  std::unique_ptr<zrythm::plugins::faust::dsp> create_zita_rev1 ()
  {
    return std::make_unique<zita_rev1> ();
  }

  /**
   * Calls the static classInit() for the given sample rate.
   *
   * classInit() fills static tables shared by all instances of the class, so it
   * is only called when needed: on first use, or when the sample rate changes
   * (processing is stopped during preparation, so this cannot race with
   * compute()).
   */
  void class_init_zita_rev1 (int sample_rate)
  {
    static std::mutex mutex;
    static int        initialized_sample_rate = 0;

    const std::lock_guard lock (mutex);
    if (initialized_sample_rate != sample_rate)
      {
        zita_rev1::classInit (sample_rate);
        initialized_sample_rate = sample_rate;
      }
  }

} // namespace zrythm_faust

/******************** END ARCHITECTURE SECTION (part 2/2) ****************/

#endif

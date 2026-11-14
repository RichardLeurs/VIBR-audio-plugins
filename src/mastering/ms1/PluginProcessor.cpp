// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
// the factory presets of the MS-1: name, a note, and the values (plain host values) that are set after every parameter went back to its default
std::vector<vibr::PresetManager::Factory> ms1FactoryPresets()
{
    return {
        { "Init (everything off)", "All stages off, limiter at its defaults", {} },
        { "Gentle Glue", "2:1 bus compression with auto release, about 1 to 2 dB of reduction",
          { { "busOn", 1 }, { "busThresh", -14 }, { "busRatio", 1 }, { "busAttack", 4 }, { "busRelease", 4 }, { "busMakeup", 1.0f }, { "limGain", 2 } } },
        { "Warm Master", "Tape colour, soft glue and a solid mono low end",
          { { "colourOn", 1 }, { "colourChar", 0 }, { "colourDrive", 30 }, { "colourTilt", -0.5f }, { "busOn", 1 }, { "busThresh", -12 }, { "busRatio", 0 }, { "busAttack", 4 }, { "busRelease", 2 },
            { "imgOn", 1 }, { "monoBelow", 100 }, { "limGain", 3 } } },
        { "Wide and Tight", "Wider mids and highs, mono bass, auto safe width",
          { { "imgOn", 1 }, { "width1", 80 }, { "width2", 115 }, { "width3", 135 }, { "monoBelow", 120 }, { "imgSafe", 1 }, { "limGain", 2 } } },
        { "Streaming Loud", "Light multiband control, glue and the limiter steered to -14 LUFS",
          { { "mbOn", 1 }, { "mb1Thresh", -22 }, { "mb2Thresh", -24 }, { "mb3Thresh", -24 }, { "mb4Thresh", -26 }, { "mbKnee", 8 }, { "busOn", 1 }, { "busThresh", -12 }, { "busRatio", 1 }, { "busRelease", 4 },
            { "target", 0 }, { "limTarget", -14 }, { "limCeiling", -1 }, { "limChar", 0 }, { "limAuto", 1 } } },
        { "Club Loud", "Tube warmth, firmer multiband and a loud limiter at -8 LUFS",
          { { "colourOn", 1 }, { "colourChar", 1 }, { "colourDrive", 25 }, { "mbOn", 1 }, { "mb1Thresh", -18 }, { "mb2Thresh", -20 }, { "mb3Thresh", -20 }, { "mb4Thresh", -22 }, { "mb1Ratio", 3 },
            { "busOn", 1 }, { "busThresh", -10 }, { "busRatio", 2 }, { "busAttack", 3 }, { "busRelease", 1 }, { "target", 6 }, { "limTarget", -8 }, { "limCeiling", -0.3f }, { "limChar", 2 }, { "limAuto", 1 } } },
        { "Vinyl Pre-master", "Mono below 150 Hz, tame highs, -16 LUFS and a -3 dBTP ceiling",
          { { "imgOn", 1 }, { "monoBelow", 150 }, { "width3", 90 }, { "colourOn", 1 }, { "colourChar", 0 }, { "colourDrive", 15 }, { "colourTilt", -1.0f },
            { "target", 4 }, { "limTarget", -16 }, { "limCeiling", -3 }, { "limAuto", 1 } } },
        { "Broadcast R128", "Gentle control and the limiter steered to -23 LUFS",
          { { "busOn", 1 }, { "busThresh", -16 }, { "busRatio", 0 }, { "busRelease", 4 }, { "target", 5 }, { "limTarget", -23 }, { "limCeiling", -1 }, { "limAuto", 1 } } },
        { "De-ess and Smooth", "A dynamic EQ cut at 7 kHz that only works when the highs get harsh",
          { { "eqOn", 1 }, { "eq7On", 1 }, { "eq7Freq", 7000 }, { "eq7Gain", -6 }, { "eq7Q", 2.0f }, { "eq7Dyn", 1 }, { "eq7DynThr", -34 }, { "eq7DynRatio", 3 }, { "eq7DynAtk", 3 }, { "eq7DynRel", 80 },
            { "eq1On", 1 }, { "eq1Freq", 30 }, { "eq1Gain", -3 }, { "eq1Q", 0.71f } } },
    };
}
} // namespace

namespace
{
// host label and readable text for a float parameter (Hz above 1000 reads as kHz)
juce::AudioParameterFloatAttributes unitAttrs (const juce::String& unit, int decimals)
{
    return juce::AudioParameterFloatAttributes()
        .withLabel (unit)
        .withStringFromValueFunction ([decimals] (float v, int)
        {
            return juce::String (v, decimals);                       // the number only: the host adds the label (unit) itself
        })
        .withValueFromStringFunction ([] (const juce::String& s)
        {
            return s.retainCharacters ("0123456789.-+").getFloatValue();
        });
}

// unit of a parameter id: eq1Freq, mb2Attack, xover1 ...
juce::String unitFor (const juce::String& id)
{
    static const char* const dB[] = { "inGain", "limGain", "limCeiling", "outGain", "busThresh", "busMakeup", "mbKnee", "imgBalance", "colourTilt", "colourOut" };
    for (auto* k : dB) if (id == k) return "dB";
    if (id.endsWith ("Gain") || id.endsWith ("DynThr") || id.endsWith ("Thresh") || id.endsWith ("Make")) return "dB";
    if (id.endsWith ("Freq") || id.startsWith ("xover") || id == "imgX1" || id == "imgX2" || id == "monoBelow" || id == "mbScHz") return "Hz";
    if (id == "limRelease" || id == "limLook" || id.endsWith ("DynAtk") || id.endsWith ("DynRel") || id.endsWith ("Attack") || id.endsWith ("Release")) return "ms";
    if (id.endsWith ("DynRatio") || id.endsWith ("Ratio")) return ":1";
    if (id == "limTarget") return "LUFS";
    if (id == "limLink" || id == "busMix" || id == "mbMix" || id == "mbRms" || id.startsWith ("width") || id == "colourDrive" || id == "colourMix") return "%";
    return {};
}
int decimalsFor (const juce::String& id, const juce::String& unit)
{
    if (unit == "Hz" || unit == "%") return 0;
    if (unit == "ms") return id.endsWith ("Attack") || id.endsWith ("Atk") || id == "limLook" ? 1 : 0;
    return unit.isEmpty() ? 2 : 1;
}
juce::AudioParameterFloatAttributes attrsFor (const juce::String& id) { const auto u = unitFor (id); return unitAttrs (u, decimalsFor (id, u)); }
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout MS1AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto flt = [&layout] (const char* id, const char* name, float lo, float hi, float step, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, step), def, attrsFor (id)));
    };
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "power", 1 }, "Power", true));
    layout.add (vibr::makeBypassParam());
    flt ("inGain", "Input Gain", -24.0f, 24.0f, 0.1f, 0.0f);
    flt ("limGain", "Limiter Gain", 0.0f, 24.0f, 0.1f, 0.0f);
    flt ("limCeiling", "Limiter Ceiling", -3.0f, 0.0f, 0.1f, -1.0f);
    {
        NormalisableRange<float> r (10.0f, 1000.0f, 1.0f);
        r.setSkewForCentre (100.0f);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "limRelease", 1 }, "Limiter Release", r, 100.0f, attrsFor ("limRelease")));
    }
    flt ("limLook", "Limiter Lookahead", 0.5f, 5.0f, 0.1f, 2.0f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "limChar", 1 }, "Limiter Character", StringArray { "Transparent", "Punchy", "Loud" }, 0));
    flt ("limLink", "Limiter Stereo Link", 0.0f, 100.0f, 1.0f, 100.0f);
    flt ("limTarget", "Loudness Target (LUFS)", -24.0f, -6.0f, 0.5f, -14.0f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "limAuto", 1 }, "Auto Gain to Target", false));
    {
        StringArray names;
        for (int i = 0; i < ms1::kNumTargets; ++i) names.add (ms1::target (i).name);
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "target", 1 }, "Target Preset", names, 0));
    }
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "ditherBits", 1 }, "Dither Bit Depth", StringArray { "Off", "16 bit", "20 bit", "24 bit" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "ditherShape", 1 }, "Dither Shape", StringArray { "Flat", "Gentle", "Aggressive" }, 0));
    flt ("outGain", "Output Gain", -12.0f, 6.0f, 0.1f, 0.0f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "delta", 1 }, "Delta Listen", false));

    // corrective EQ: band 1 low shelf, 2 .. 7 bells, 8 high shelf
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "eqOn", 1 }, "EQ In", true));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "eqPhase", 1 }, "EQ Phase", StringArray { "Minimum", "Linear" }, 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "trimPolarity", 1 }, "Input Polarity Inverted", false));
    flt ("trimHpf", "Input High-pass (rumble, 0 = off)", 0.0f, 60.0f, 1.0f, 0.0f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "monoCheck", 1 }, "Mono Compatibility Check", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "limRelAuto", 1 }, "Limiter Auto Release", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "limFreeze", 1 }, "Limiter Freeze Auto Gain", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mbPhase", 1 }, "Multiband Crossover Phase", StringArray { "Minimum", "Linear" }, 0));
    static const float defFreq[ms1::kEqBands] = { 80.0f, 150.0f, 400.0f, 1000.0f, 2500.0f, 5000.0f, 9000.0f, 12000.0f };
    for (int n = 1; n <= ms1::kEqBands; ++n)
    {
        const String id = "eq" + String (n), nm = "EQ " + String (n) + " ";
        const bool shelf = n == 1 || n == ms1::kEqBands;
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id + "On", 1 }, nm + "On", true));
        NormalisableRange<float> fr (20.0f, 20000.0f, 1.0f); fr.setSkewForCentre (1000.0f);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id + "Freq", 1 }, nm + "Frequency", fr, defFreq[n - 1], attrsFor (id + "Freq")));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id + "Gain", 1 }, nm + "Gain", NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f, attrsFor (id + "Gain")));
        NormalisableRange<float> qr (0.2f, 10.0f, 0.01f); qr.setSkewForCentre (1.0f);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id + "Q", 1 }, nm + "Q", qr, shelf ? 0.71f : 1.0f, attrsFor (id + "Q")));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id + "Mode", 1 }, nm + "Mode", StringArray { "Stereo", "Left", "Right", "Mid", "Side" }, 0));
        // dynamic EQ: the band's gain follows the level in the band
        NormalisableRange<float> ar (1.0f, 100.0f, 0.1f), rr (10.0f, 1000.0f, 1.0f), dr (1.0f, 10.0f, 0.1f);
        ar.setSkewForCentre (10.0f); rr.setSkewForCentre (100.0f); dr.setSkewForCentre (3.0f);
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id + "Dyn", 1 }, nm + "Dynamic", false));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id + "DynThr", 1 }, nm + "Dynamic Threshold", NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -30.0f, attrsFor (id + "DynThr")));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id + "DynRatio", 1 }, nm + "Dynamic Ratio", dr, 2.0f, attrsFor (id + "DynRatio")));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id + "DynAtk", 1 }, nm + "Dynamic Attack", ar, 10.0f, attrsFor (id + "DynAtk")));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id + "DynRel", 1 }, nm + "Dynamic Release", rr, 100.0f, attrsFor (id + "DynRel")));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id + "DynBelow", 1 }, nm + "Dynamic Below Threshold", false));
    }

    // bus compressor (glue): off by default
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "busOn", 1 }, "Bus Compressor On", false));
    flt ("busThresh", "Bus Threshold", -30.0f, 0.0f, 0.1f, -12.0f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "busRatio", 1 }, "Bus Ratio", StringArray { "1.5:1", "2:1", "4:1", "10:1" }, 1));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "busAttack", 1 }, "Bus Attack", StringArray { "0.1 ms", "0.3 ms", "1 ms", "3 ms", "10 ms", "30 ms" }, 3));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "busRelease", 1 }, "Bus Release", StringArray { "0.1 s", "0.3 s", "0.6 s", "1.2 s", "Auto" }, 2));
    flt ("busMakeup", "Bus Make-up Gain", 0.0f, 12.0f, 0.1f, 0.0f);
    flt ("busMix", "Bus Mix", 0.0f, 100.0f, 1.0f, 100.0f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "busSc", 1 }, "Bus Sidechain High-pass", StringArray { "Off", "60 Hz", "90 Hz", "150 Hz" }, 0));

    // multiband compressor: 4 bands, 3 crossovers (Linkwitz-Riley 24 dB per octave)
    const auto skewed = [] (float lo, float hi, float step, float centre) { NormalisableRange<float> r (lo, hi, step); r.setSkewForCentre (centre); return r; };
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "mbOn", 1 }, "Multiband On", false));
    flt ("mbMix", "Multiband Mix", 0.0f, 100.0f, 1.0f, 100.0f);
    flt ("mbKnee", "Multiband Knee", 0.0f, 12.0f, 0.1f, 6.0f);
    flt ("mbRms", "Multiband Detector (0 peak .. 100 RMS)", 0.0f, 100.0f, 1.0f, 0.0f);
    flt ("mbScHz", "Multiband Sidechain High-pass (20 = off)", 20.0f, 300.0f, 1.0f, 20.0f);
    {
        static const float dx[3] = { 120.0f, 1200.0f, 6000.0f };
        for (int k = 1; k <= 3; ++k)
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "xover" + String (k), 1 }, "Crossover " + String (k), skewed (40.0f, 12000.0f, 1.0f, 800.0f), dx[k - 1], attrsFor ("xover" + String (k))));
        for (int n = 1; n <= 4; ++n)
        {
            const String id = "mb" + String (n), nm = "Band " + String (n) + " ";
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { id + "On", 1 }, nm + "On", true));
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { id + "Solo", 1 }, nm + "Solo", false));
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { id + "Auto", 1 }, nm + "Auto Release", false));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id + "Thresh", 1 }, nm + "Threshold", NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -18.0f, attrsFor (id + "Thresh")));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id + "Ratio", 1 }, nm + "Ratio", skewed (1.0f, 20.0f, 0.1f, 4.0f), 2.0f, attrsFor (id + "Ratio")));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id + "Attack", 1 }, nm + "Attack", skewed (0.1f, 200.0f, 0.1f, 10.0f), 10.0f, attrsFor (id + "Attack")));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id + "Release", 1 }, nm + "Release", skewed (10.0f, 2000.0f, 1.0f, 200.0f), 200.0f, attrsFor (id + "Release")));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id + "Make", 1 }, nm + "Make-up", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, attrsFor (id + "Make")));
        }
    }

    // stereo imager: 3 bands
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "imgOn", 1 }, "Imager On", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "imgX1", 1 }, "Imager Crossover 1", skewed (40.0f, 1000.0f, 1.0f, 200.0f), 200.0f, attrsFor ("imgX1")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "imgX2", 1 }, "Imager Crossover 2", skewed (1000.0f, 12000.0f, 1.0f, 4000.0f), 4000.0f, attrsFor ("imgX2")));
    for (int k = 1; k <= 3; ++k) flt (("width" + String (k)).toRawUTF8(), ("Imager Width " + String (k)).toRawUTF8(), 0.0f, 200.0f, 1.0f, 100.0f);
    flt ("monoBelow", "Mono Below (0 = off)", 0.0f, 250.0f, 1.0f, 0.0f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "imgSafe", 1 }, "Imager Auto Safe Width", false));
    flt ("imgSafeThr", "Imager Safe Width Correlation", -0.9f, 0.5f, 0.05f, -0.2f);
    flt ("imgBalance", "Mid / Side Balance", -6.0f, 6.0f, 0.1f, 0.0f);

    // colour
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "colourOn", 1 }, "Colour On", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "colourChar", 1 }, "Colour Character", StringArray { "Tape", "Tube", "Transformer" }, 0));
    flt ("colourDrive", "Colour Drive", 0.0f, 100.0f, 1.0f, 0.0f);
    flt ("colourTilt", "Colour Tone Tilt", -6.0f, 6.0f, 0.1f, 0.0f);
    flt ("colourMix", "Colour Mix", 0.0f, 100.0f, 1.0f, 100.0f);
    flt ("colourOut", "Colour Output", -12.0f, 12.0f, 0.1f, 0.0f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "colourMode", 1 }, "Colour Mode", StringArray { "Stereo", "Mid", "Side" }, 0));

    // reference A/B
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "refAB", 1 }, "Reference (B) Monitor", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "refMatch", 1 }, "Reference Level Match", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "refPlay", 1 }, "Reference Play", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "refFollow", 1 }, "Reference Follows Host", false));
    return layout;
}

MS1AudioProcessor::MS1AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "MS1", createLayout()),
      presets (*this, apvts, undoManager, "MS-1 Presets", ms1FactoryPresets(), { "power", "bypass", "delta", "refAB", "refPlay", "refMatch", "refFollow" }, { "refPath" })
{
    const auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pPower = get ("power"); pInGain = get ("inGain"); pLimGain = get ("limGain"); pCeiling = get ("limCeiling"); pRelease = get ("limRelease");
    pLook = get ("limLook"); pChar = get ("limChar"); pLink = get ("limLink"); pTarget = get ("limTarget"); pAuto = get ("limAuto");
    pBits = get ("ditherBits"); pShape = get ("ditherShape"); pOutGain = get ("outGain"); pDelta = get ("delta");
    pEqOn = get ("eqOn"); pEqPhase = get ("eqPhase"); pMbPhase = get ("mbPhase"); pPolarity = get ("trimPolarity"); pHpf = get ("trimHpf"); pMonoCheck = get ("monoCheck");
    pLimRelAuto = get ("limRelAuto"); pLimFreeze = get ("limFreeze");
    for (auto& a : mbPending) a.store (nullptr);
    for (auto& a : mbTrash) a.store (nullptr);
    irTimer.startTimerHz (20);                                                      // (the message thread: the ctor of a plugin runs there)
    pBusOn = get ("busOn"); pBusThresh = get ("busThresh"); pBusRatio = get ("busRatio"); pBusAttack = get ("busAttack"); pBusRelease = get ("busRelease");
    pBusMakeup = get ("busMakeup"); pBusMix = get ("busMix"); pBusSc = get ("busSc");
    pMbOn = get ("mbOn"); pMbMix = get ("mbMix");
    for (int k = 1; k <= 3; ++k) pXover[k - 1] = get (("xover" + juce::String (k)).toRawUTF8());
    for (int n = 1; n <= 4; ++n)
    {
        const juce::String id = "mb" + juce::String (n);
        auto& m = pMb[(size_t) (n - 1)];
        m.on = get ((id + "On").toRawUTF8()); m.solo = get ((id + "Solo").toRawUTF8()); m.autoRel = get ((id + "Auto").toRawUTF8()); m.thresh = get ((id + "Thresh").toRawUTF8());
        m.ratio = get ((id + "Ratio").toRawUTF8()); m.attack = get ((id + "Attack").toRawUTF8()); m.release = get ((id + "Release").toRawUTF8()); m.make = get ((id + "Make").toRawUTF8());
    }
    pImgOn = get ("imgOn"); pImgX[0] = get ("imgX1"); pImgX[1] = get ("imgX2");
    for (int k = 1; k <= 3; ++k) pWidth[k - 1] = get (("width" + juce::String (k)).toRawUTF8());
    pMonoBelow = get ("monoBelow"); pImgBalance = get ("imgBalance");
    pColOn = get ("colourOn"); pColChar = get ("colourChar"); pColDrive = get ("colourDrive"); pColTilt = get ("colourTilt"); pColMix = get ("colourMix");
    pColOut = get ("colourOut"); pColMode = get ("colourMode");
    pRefAB = get ("refAB"); pRefMatch = get ("refMatch"); pRefPlay = get ("refPlay"); pRefFollow = get ("refFollow");
    for (int n = 1; n <= ms1::kEqBands; ++n)
    {
        const juce::String id = "eq" + juce::String (n);
        auto& e = pEq[(size_t) (n - 1)];
        e.on = get ((id + "On").toRawUTF8()); e.freq = get ((id + "Freq").toRawUTF8()); e.gain = get ((id + "Gain").toRawUTF8());
        e.q = get ((id + "Q").toRawUTF8()); e.mode = get ((id + "Mode").toRawUTF8());
        auto& d = pEqDyn[(size_t) (n - 1)];
        d.dyn = get ((id + "Dyn").toRawUTF8()); d.thr = get ((id + "DynThr").toRawUTF8()); d.ratio = get ((id + "DynRatio").toRawUTF8());
        d.atk = get ((id + "DynAtk").toRawUTF8()); d.rel = get ((id + "DynRel").toRawUTF8()); d.below = get ((id + "DynBelow").toRawUTF8());
    }
    pMbKnee = get ("mbKnee"); pMbRms = get ("mbRms"); pMbSc = get ("mbScHz"); pImgSafe = get ("imgSafe"); pImgSafeThr = get ("imgSafeThr");
}

MS1AudioProcessor::~MS1AudioProcessor()
{
    irTimer.stopTimer();
    delete irPending.exchange (nullptr);
    delete irTrash.exchange (nullptr);
    for (auto& a : mbPending) delete a.exchange (nullptr);
    for (auto& a : mbTrash) delete a.exchange (nullptr);
}

bool MS1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void MS1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateNow = (float) sampleRate;
    limiter.prepare (sampleRateNow);
    eq.prepare (sampleRateNow);
    {   // the linear-phase EQ: a convolution that starts as a pure delay of N / 2 samples
        const int chunk = juce::jmax (samplesPerBlock, 4096), n = ms1::linearIrLength (sampleRateNow);
        linConv.prepare ({ sampleRate, (juce::uint32) chunk, 2 });
        juce::AudioBuffer<float> delta (2, n);
        delta.clear();
        delta.setSample (0, n / 2, 1.0f); delta.setSample (1, n / 2, 1.0f);
        linConv.loadImpulseResponse (std::move (delta), sampleRate, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
        irKey.clear(); wasLinear = false; linMs.store (false);
        const float freq[3] = { 120.0f, 1200.0f, 6000.0f };                                  // the multiband's linear-phase crossovers start at the default frequencies
        const auto bands = ms1::designLinearBands (freq, sampleRateNow, n);
        for (size_t k = 0; k < 4; ++k)
        {
            mbConv[k].prepare ({ sampleRate, (juce::uint32) chunk, 2 });
            juce::AudioBuffer<float> ir (1, n);
            std::copy (bands[k].begin(), bands[k].end(), ir.getWritePointer (0));
            mbConv[k].loadImpulseResponse (std::move (ir), sampleRate, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
            for (auto& ch : mbBand[k]) ch.assign ((size_t) chunk, 0.0f);
        }
        mbKey.clear(); wasMbLinear = false;
    }
    bus.prepare (sampleRateNow);
    mband.prepare (sampleRateNow);
    imager.prepare (sampleRateNow);
    colour.prepare (sampleRateNow);
    reference.prepare (sampleRate);
    for (auto& b : refBuf) b.assign ((size_t) juce::jmax (samplesPerBlock, 4096), 0.0f);
    refAct = 0.0f;
    loudness.prepare (sampleRateNow);
    specPre.prepare (sampleRateNow); specPost.prepare (sampleRateNow);
    levels.prepare (sampleRateNow); stereoMeter.prepare (sampleRateNow);
    dither.reset();
    for (auto& t : meterTp) t.reset();
    for (auto& d : dryDelay) d.assign (16384, 0.0f);
    for (auto& d : dryOut) d.assign ((size_t) juce::jmax (samplesPerBlock, 4096), 0.0f);
    dryPos = 0;
    powerSmooth.reset (sampleRate, 0.02); powerSmooth.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    inSmooth.reset (sampleRate, 0.02);    inSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pInGain->load()));
    outSmooth.reset (sampleRate, 0.02);   outSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutGain->load()));
    autoGain = pLimGain->load(); lastAuto = false;
    lastLatency = -1;
    tpMaxDb.store (-120.0f);
}

void MS1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples(), chunk = (int) dryOut[0].size();
    if (total == 0 || buffer.getNumChannels() < 2) return;
    for (int c = 0; c < 2; ++c)                                                     // NaN, inf and absurd values from the host become silence / a safe level: the chain never sees them
    {
        auto* d = buffer.getWritePointer (c);
        for (int i = 0; i < total; ++i) if (! (std::abs (d[i]) < 1.0e4f)) d[i] = 0.0f;          // false for NaN and inf too
    }
    for (int pos = 0; pos < total; pos += chunk)                    // a host block larger than the scratch buffers is processed in parts
        processChunk (buffer.getWritePointer (0) + pos, buffer.getWritePointer (1) + pos, std::min (chunk, total - pos));
}

void MS1AudioProcessor::processChunk (float* l, float* r, int n)
{
    if (resetFlag.exchange (false)) { loudness.resetIntegrated(); levels.reset(); stereoMeter.reset(); tpMaxDb.store (-120.0f); }
    const int specSrc = specSourceOpt.load (std::memory_order_relaxed), specMd = specModeOpt.load (std::memory_order_relaxed);
    specPre.setMode (specMd); specPost.setMode (specMd);
    const auto pick = [specSrc] (float a, float b) { return specSrc == 1 ? 0.5f * (a - b) : specSrc == 2 ? a : specSrc == 3 ? b : 0.5f * (a + b); };          // the spectrum source: mid, side, left, right

    // the lookahead sets the latency, which the host must know
    ms1::LimiterParams lp;
    lp.gainDb = pLimGain->load();  lp.ceilingDb = pCeiling->load();  lp.releaseMs = pRelease->load();  lp.lookaheadMs = pLook->load();
    lp.link = pLink->load() * 0.01f;  lp.character = juce::jlimit (0, 2, (int) std::lround (pChar->load()));
    lp.autoRelease = pLimRelAuto->load() > 0.5f;
    limiter.setLookahead (lp.lookaheadMs);
    const bool linearEq = pEqPhase->load() > 0.5f;
    const bool linearMb = pMbPhase->load() > 0.5f;
    const int latency = limiter.latency() + ms1::Colour::kLatency + (linearEq ? ms1::linearIrLength (sampleRateNow) / 2 : 0) + (linearMb ? ms1::linearIrLength (sampleRateNow) / 2 : 0);          // the lookahead + the true-peak filter + the oversampling filters of the colour stage
    if (latency != lastLatency) { lastLatency = latency; setLatencySamples (latency); }

    // the loudness target: a slow, bounded controller moves the limiter gain so the short-term loudness of the output reaches the target
    const bool autoOn = pAuto->load() > 0.5f;
    if (autoOn && ! lastAuto) autoGain = lp.gainDb;
    lastAuto = autoOn;
    if (autoOn)
    {
        const float st = loudness.shortTerm.load (std::memory_order_relaxed);
        if (st > -60.0f && pLimFreeze->load() < 0.5f)                                  // FREEZE: the auto gain keeps its value
        {
            const float err = pTarget->load() - st;                                     // dB
            const float dt = (float) n / sampleRateNow;
            autoGain = juce::jlimit (0.0f, 24.0f, autoGain + juce::jlimit (-1.5f, 1.5f, err * 0.25f) * dt);
        }
        lp.gainDb = autoGain;
    }
    else autoGain = lp.gainDb;
    autoGainDb.store (autoGain, std::memory_order_relaxed);

    // dry copy (delayed by the latency) for the power switch and the delta listen; the spectrum before
    std::array<float*, 2> io { l, r };
    for (int i = 0; i < n; ++i)
    {
        for (int c = 0; c < 2; ++c)
        {
            dryDelay[(size_t) c][(size_t) (dryPos & 16383)] = io[(size_t) c][i];
            dryOut[(size_t) c][(size_t) i] = dryDelay[(size_t) c][(size_t) ((dryPos - latency) & 16383)];
        }
        specPre.push (pick (l[i], r[i]));
        ++dryPos;
    }

    // input trim
    inSmooth.setTargetValue (juce::Decibels::decibelsToGain (pInGain->load()) * (pPolarity->load() > 0.5f ? -1.0f : 1.0f));          // the polarity switch goes through zero over 20 ms
    for (int i = 0; i < n; ++i) { const float g = inSmooth.getNextValue(); l[i] *= g; r[i] *= g; }
    {   // the rumble filter: 12 dB per octave, off = bit-exact
        const float hz = pHpf->load() < 1.0f ? 0.0f : std::max (10.0f, pHpf->load());
        if (hz != trimHpHz) { trimHpHz = hz; if (hz > 0.0f) for (auto& f : trimHp) f.set (hz, sampleRateNow, true); }
        if (hz > 0.0f) for (int i = 0; i < n; ++i) { l[i] = trimHp[0].tick (l[i]); r[i] = trimHp[1].tick (r[i]); }
        else for (auto& f : trimHp) f.clear();
    }

    // corrective EQ: minimum phase (no latency) or linear phase (a convolution, N / 2 samples of latency)
    {
        std::array<ms1::EqBand, ms1::kEqBands> bands;
        const bool eqIn = pEqOn->load() > 0.5f;
        for (size_t k = 0; k < bands.size(); ++k)
        {
            const auto& e = pEq[k];
            bands[k] = { e.freq->load(), e.gain->load(), e.q->load(), juce::jlimit (0, 4, (int) std::lround (e.mode->load())), e.on->load() > 0.5f && eqIn };
            const auto& d = pEqDyn[k];
            bands[k].dyn = d.dyn->load() > 0.5f; bands[k].dynBelow = d.below->load() > 0.5f;
            bands[k].dynThresholdDb = d.thr->load(); bands[k].dynRatio = d.ratio->load(); bands[k].dynAttackMs = d.atk->load(); bands[k].dynReleaseMs = d.rel->load();
        }
        if (linearEq)
        {
            if (! wasLinear) { linConv.reset(); wasLinear = true; }
            eq.analyse (l, r, n, bands.data());                                       // the detectors of the dynamic bands; the message thread designs the FIR
            if (auto* p = irPending.exchange (nullptr))                               // a new impulse response from the message thread
            {
                linConv.loadImpulseResponse (std::move (*p), sampleRateNow, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
                if (auto* old = irTrash.exchange (p)) delete old;                     // the emptied buffer object is freed by the message thread
            }
            const bool ms = linMs.load (std::memory_order_relaxed);
            if (ms) for (int i = 0; i < n; ++i) { const float m = 0.5f * (l[i] + r[i]), sd = 0.5f * (l[i] - r[i]); l[i] = m; r[i] = sd; }
            float* chans[2] = { l, r };
            juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
            linConv.process (juce::dsp::ProcessContextReplacing<float> (block));
            if (ms) for (int i = 0; i < n; ++i) { const float m = l[i], sd = r[i]; l[i] = m + sd; r[i] = m - sd; }
        }
        else
        {
            wasLinear = false;
            if (eqIn) eq.process (l, r, n, bands.data());
            else
            {
                std::array<ms1::EqBand, ms1::kEqBands> off;                           // every band flat: the bands fade out and the state is cleared
                for (auto& b : off) b.on = false;
                eq.process (l, r, n, off.data());
            }
        }
    }

    // multiband compressor and stereo imager (no latency)
    {
        ms1::MbParams mp;
        mp.on = pMbOn->load() > 0.5f;  mp.mix = pMbMix->load() * 0.01f;
        mp.kneeDb = pMbKnee->load(); mp.rmsBlend = pMbRms->load() * 0.01f; mp.sidechainHz = pMbSc->load();
        for (int k = 0; k < 3; ++k) mp.freq[k] = pXover[k]->load();
        mp.freq[1] = std::max (mp.freq[1], mp.freq[0] * 1.2f); mp.freq[2] = std::max (mp.freq[2], mp.freq[1] * 1.2f);           // the crossovers keep their order
        for (size_t k = 0; k < 4; ++k)
        {
            const auto& m = pMb[k];
            mp.band[k] = { m.on->load() > 0.5f, m.solo->load() > 0.5f, m.autoRel->load() > 0.5f, m.thresh->load(), m.ratio->load(), m.attack->load(), m.release->load(), m.make->load() };
        }
        if (linearMb)
        {
            if (! wasMbLinear) { for (auto& c : mbConv) c.reset(); wasMbLinear = true; }
            const float* bpl[4]; const float* bpr[4];
            for (size_t k = 0; k < 4; ++k)
            {
                if (auto* p = mbPending[k].exchange (nullptr))                               // a new band FIR from the message thread
                {
                    mbConv[k].loadImpulseResponse (std::move (*p), sampleRateNow, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
                    if (auto* old = mbTrash[k].exchange (p)) delete old;
                }
                std::copy (l, l + n, mbBand[k][0].begin()); std::copy (r, r + n, mbBand[k][1].begin());
                float* chans[2] = { mbBand[k][0].data(), mbBand[k][1].data() };
                juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
                mbConv[k].process (juce::dsp::ProcessContextReplacing<float> (block));
                bpl[k] = mbBand[k][0].data(); bpr[k] = mbBand[k][1].data();
            }
            mband.processBands (l, r, n, mp, bpl, bpr);
        }
        else { wasMbLinear = false; mband.process (l, r, n, mp); }
        for (int k = 0; k < 4; ++k) mbGr[(size_t) k].store (mband.gr[(size_t) k].load (std::memory_order_relaxed), std::memory_order_relaxed);

        ms1::ImagerParams ip;
        ip.on = pImgOn->load() > 0.5f;
        ip.freq[0] = pImgX[0]->load(); ip.freq[1] = std::max (pImgX[1]->load(), ip.freq[0] * 1.2f);
        for (int k = 0; k < 3; ++k) ip.width[k] = pWidth[k]->load() * 0.01f;
        const float mb = pMonoBelow->load(); ip.monoBelowHz = mb < 1.0f ? 0.0f : std::max (20.0f, mb);
        ip.balanceDb = pImgBalance->load();
        ip.safe = pImgSafe->load() > 0.5f; ip.safeThreshold = pImgSafeThr->load();
        imager.process (l, r, n, ip);
    }

    // colour (always delays by its latency)
    {
        ms1::ColourParams cp;
        cp.on = pColOn->load() > 0.5f; cp.character = juce::jlimit (0, 2, (int) std::lround (pColChar->load())); cp.mode = juce::jlimit (0, 2, (int) std::lround (pColMode->load()));
        cp.drive = pColDrive->load() * 0.01f; cp.tiltDb = pColTilt->load(); cp.mix = pColMix->load() * 0.01f; cp.outDb = pColOut->load();
        colour.process (l, r, n, cp);
    }

    // bus compressor (no latency)
    {
        ms1::BusParams bp;
        bp.on = pBusOn->load() > 0.5f;  bp.thresholdDb = pBusThresh->load();  bp.makeupDb = pBusMakeup->load();  bp.mix = pBusMix->load() * 0.01f;
        bp.ratio = (int) std::lround (pBusRatio->load());  bp.attack = (int) std::lround (pBusAttack->load());
        bp.release = (int) std::lround (pBusRelease->load());  bp.sidechain = (int) std::lround (pBusSc->load());
        bus.process (l, r, n, bp);
    }

    // limiter
    limiter.process (l, r, n, lp);
    grDb.store (limiter.grDb.load (std::memory_order_relaxed), std::memory_order_relaxed);

    // output: gain, then delta / power, then dither
    outSmooth.setTargetValue (juce::Decibels::decibelsToGain (pOutGain->load()));
    powerSmooth.setTargetValue (pPower->load() > 0.5f ? 1.0f : 0.0f);
    const bool delta = pDelta->load() > 0.5f;
    static const int kBitsOf[4] = { 0, 16, 20, 24 };
    const int bits = kBitsOf[juce::jlimit (0, 3, (int) std::lround (pBits->load()))];
    const int shape = juce::jlimit (0, 2, (int) std::lround (pShape->load()));
    for (int i = 0; i < n; ++i)
    {
        const float og = outSmooth.getNextValue(), pw = powerSmooth.getNextValue();
        for (int c = 0; c < 2; ++c)
        {
            const float proc = io[(size_t) c][i] * og, dry = dryOut[(size_t) c][(size_t) i];
            float y = delta ? (proc - dry) : proc;
            y = dry + (y - dry) * pw;                                                      // power off: the untouched sound, delayed by the latency
            io[(size_t) c][i] = (pw > 0.999f || pw < 0.001f) && bits == 0 ? y : dither.process (c, y, pw < 0.5f ? 0 : bits, shape);
        }
    }

    // meters of the output
    loudness.process (l, r, n);
    float peakL = 0.0f, peakR = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        peakL = std::max (peakL, meterTp[0].push (l[i]));
        peakR = std::max (peakR, meterTp[1].push (r[i]));
        specPost.push (pick (l[i], r[i]));
    }
    const float dL = juce::Decibels::gainToDecibels (peakL, -120.0f), dR = juce::Decibels::gainToDecibels (peakR, -120.0f);
    levels.process (l, r, n, std::max (peakL, peakR));                              // the chain's output (before the reference replaces it)
    stereoMeter.process (l, r, n);
    // reference: the B side of A/B, level matched, after the meters (they always measure the chain)
    {
        const bool wantRef = pRefAB->load() > 0.5f && reference.hasData();
        const bool running = pRefPlay->load() > 0.5f;
        juce::int64 hostPos = -1;
        if (pRefFollow->load() > 0.5f)
            if (auto* ph = getPlayHead())
                if (auto pos = ph->getPosition())
                    if (pos->getIsPlaying()) if (auto t = pos->getTimeInSamples()) hostPos = *t;
        if (wantRef || refAct > 0.0f || running)
        {
            reference.process (refBuf[0].data(), refBuf[1].data(), n, running, hostPos);
            // level match: the reference is brought to the loudness of the chain (integrated, else short-term, else the target)
            float program = loudness.integrated.load (std::memory_order_relaxed);
            if (program < -60.0f) program = loudness.shortTerm.load (std::memory_order_relaxed);
            if (program < -60.0f) program = pTarget->load();
            const float wanted = pRefMatch->load() > 0.5f && reference.integratedLufs() > -100.0f ? juce::jlimit (-24.0f, 24.0f, program - reference.integratedLufs()) : 0.0f;
            refGainDb += (wanted - refGainDb) * (1.0f - std::exp (-(float) n / (1.0f * sampleRateNow)));
            refMatchDb.store (refGainDb, std::memory_order_relaxed);
            const float g = juce::Decibels::decibelsToGain (refGainDb), inc = 1.0f / (0.02f * sampleRateNow);
            for (int i = 0; i < n; ++i)
            {
                refAct = wantRef ? std::min (1.0f, refAct + inc) : std::max (0.0f, refAct - inc);
                io[0][i] += (std::clamp (refBuf[0][(size_t) i] * g, -2.0f, 2.0f) - io[0][i]) * refAct;
                io[1][i] += (std::clamp (refBuf[1][(size_t) i] * g, -2.0f, 2.0f) - io[1][i]) * refAct;
            }
        }
    }
    if (pMonoCheck->load() > 0.5f) for (int i = 0; i < n; ++i) { const float m = 0.5f * (io[0][i] + io[1][i]); io[0][i] = io[1][i] = m; }        // a listening aid: the output summed to mono (the meters above measured the stereo signal)
    tpNowL.store (dL, std::memory_order_relaxed); tpNowR.store (dR, std::memory_order_relaxed);
    if (std::max (dL, dR) > tpMaxDb.load (std::memory_order_relaxed)) tpMaxDb.store (std::max (dL, dR), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* MS1AudioProcessor::createEditor() { return new MS1AudioProcessorEditor (*this); }

//==============================================================================
// the message thread, 20 times a second: the impulse responses of the linear-phase EQ and of the multiband crossovers (when they changed), the long-term
// spectrum of the input
void MS1AudioProcessor::updateMessageThread()
{
    delete irTrash.exchange (nullptr);

    if (learnReset.exchange (false)) { learnPower.fill (0.0); learnFrames.store (0); }
    {   // learn: the average power per band of the input while sound is playing
        bool sound = false;
        for (int b = 0; b < ms1::kBands; ++b) if (specPre.bands[b].load (std::memory_order_relaxed) > -90.0f) { sound = true; break; }
        if (sound)
        {
            for (int b = 0; b < ms1::kBands; ++b) learnPower[(size_t) b] += std::pow (10.0, (double) specPre.bands[b].load (std::memory_order_relaxed) / 10.0);
            learnFrames.fetch_add (1);
        }
    }

    if (pMbPhase != nullptr && pMbPhase->load() > 0.5f)                                  // the linear-phase crossovers of the multiband
    {
        float freq[3] = { pXover[0]->load(), pXover[1]->load(), pXover[2]->load() };
        freq[1] = std::max (freq[1], freq[0] * 1.2f); freq[2] = std::max (freq[2], freq[1] * 1.2f);
        std::vector<float> key { sampleRateNow, freq[0], freq[1], freq[2] };
        bool same = key.size() == mbKey.size();
        for (size_t i = 0; same && i < key.size(); ++i) same = std::abs (key[i] - mbKey[i]) < 0.5f;
        if (! same)
        {
            mbKey = key;
            const int n = ms1::linearIrLength (sampleRateNow);
            const auto bands = ms1::designLinearBands (freq, sampleRateNow, n);
            for (size_t k = 0; k < 4; ++k)
            {
                auto* buf = new juce::AudioBuffer<float> (1, n);
                std::copy (bands[k].begin(), bands[k].end(), buf->getWritePointer (0));
                delete mbPending[k].exchange (buf);
            }
        }
    }
    else mbKey.clear();
    for (auto& t : mbTrash) delete t.exchange (nullptr);

    if (pEqPhase == nullptr || pEqPhase->load() <= 0.5f) { irKey.clear(); return; }
    const bool eqIn = pEqOn->load() > 0.5f;
    std::array<ms1::EqBand, ms1::kEqBands> bands;
    std::vector<float> key { sampleRateNow, eqIn ? 1.0f : 0.0f };
    for (size_t k = 0; k < bands.size(); ++k)
    {
        const auto& e = pEq[k];
        bands[k] = { e.freq->load(), e.gain->load(), e.q->load(), juce::jlimit (0, 4, (int) std::lround (e.mode->load())), e.on->load() > 0.5f && eqIn };
        if (pEqDyn[k].dyn->load() > 0.5f) bands[k].gainDb *= eq.dynAmount[k].load (std::memory_order_relaxed);              // a dynamic band: the gain that applies now
        for (float v : { bands[k].freq, bands[k].gainDb, bands[k].q, (float) bands[k].mode, bands[k].on ? 1.0f : 0.0f }) key.push_back (v);
    }
    bool same = key.size() == irKey.size();
    for (size_t i = 0; same && i < key.size(); ++i) same = std::abs (key[i] - irKey[i]) < 0.02f + 1.0e-4f * std::abs (key[i]);
    if (same) return;
    irKey = key;

    const int n = ms1::linearIrLength (sampleRateNow);
    const auto ir = ms1::designLinearIr (bands, sampleRateNow, n);
    auto* buf = new juce::AudioBuffer<float> (2, n);
    std::copy (ir.a.begin(), ir.a.end(), buf->getWritePointer (0));
    std::copy (ir.b.begin(), ir.b.end(), buf->getWritePointer (1));
    linMs.store (ir.midSide);
    delete irPending.exchange (buf);                                                    // an older one that was not taken yet is replaced
}

bool MS1AudioProcessor::matchEq()
{
    const float* ref = reference.spectrum();
    const int frames = learnFrames.load();
    if (ref == nullptr || frames < 60) return false;                                    // needs a reference and at least 3 seconds of input
    std::array<float, ms1::kBands> in {};
    for (size_t b = 0; b < in.size(); ++b) in[b] = (float) (10.0 * std::log10 (std::max (1.0e-12, learnPower[b] / (double) frames)));
    const auto fit = ms1::matchEqFit (ref, in.data());
    undoManager.beginNewTransaction ("Match EQ");
    const auto set = [this] (const juce::String& id, float v) { if (auto* par = apvts.getParameter (id)) { par->beginChangeGesture(); par->setValueNotifyingHost (par->convertTo0to1 (v)); par->endChangeGesture(); } };
    set ("eqOn", 1.0f);
    for (int i = 0; i < ms1::kEqBands; ++i)
    {
        const juce::String id = "eq" + juce::String (i + 1);
        set (id + "On", 1.0f); set (id + "Freq", fit[(size_t) i].freq); set (id + "Gain", fit[(size_t) i].gainDb); set (id + "Q", fit[(size_t) i].q);
        set (id + "Mode", 0.0f); set (id + "Dyn", 0.0f);
    }
    return true;
}

void MS1AudioProcessor::setSpecOption (SpecOption o, int v)
{
    static const char* names[3] = { "specMode", "specSource", "specSlope" };
    (o == SpecMode ? specModeOpt : o == SpecSource ? specSourceOpt : specSlopeOpt).store (v);
    apvts.state.setProperty (names[o], v, nullptr);                                  // saved with the session
}

void MS1AudioProcessor::loadReference (const juce::File& f)
{
    apvts.state.setProperty ("refPath", f.getFullPathName(), nullptr);              // part of the saved state
    reference.loadAsync (f);
}

void MS1AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void MS1AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            specModeOpt.store ((int) apvts.state.getProperty ("specMode", 0)); specSourceOpt.store ((int) apvts.state.getProperty ("specSource", 0)); specSlopeOpt.store ((int) apvts.state.getProperty ("specSlope", 2));
            const juce::File f (apvts.state.getProperty ("refPath").toString());
            if (f.existsAsFile() && f != reference.file()) reference.loadAsync (f);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MS1AudioProcessor(); }

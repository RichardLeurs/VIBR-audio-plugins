// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once
#include <JuceHeader.h>

// Base of the vintage pedal editors: the rendered background image (loaded from the plugin's binary data), the resizable window at the image's aspect ratio
// (limits: minWidth up to the image's own size) and the helpers the pedals repeated (draw the background and switch to image pixels, click on the power slide
// switch = the bypass parameter). The pedal keeps its knobs, its look-and-feel member, its painting on top of the background and its timer.
namespace vibr::ui
{
class PedalEditor : public juce::AudioProcessorEditor
{
public:
    PedalEditor (juce::AudioProcessor& p, const void* backgroundPng, size_t backgroundSize, float imageW, float imageH, int baseWidth, int minWidth)
        : AudioProcessorEditor (&p), imgW (imageW), imgH (imageH),
          background (juce::ImageFileFormat::loadFrom (backgroundPng, backgroundSize))
    {
        constrainer.setFixedAspectRatio ((double) imgW / (double) imgH);
        constrainer.setSizeLimits (minWidth, (int) ((float) minWidth * imgH / imgW), (int) imgW, (int) imgH);
        setConstrainer (&constrainer);
        setResizable (true, true);
        setSize (baseWidth, (int) std::round ((float) baseWidth * imgH / imgW));
    }

    float imageScale() const { return (float) getWidth() / imgW; }

    // the background over the whole window, then the graphics are in image pixels
    void paintBackground (juce::Graphics& g) const
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (background, 0, 0, getWidth(), getHeight(), 0, 0, background.getWidth(), background.getHeight());
        g.addTransform (juce::AffineTransform::scale (imageScale()));
    }

    // a click (in window pixels) on the power slide switch at `led` (image pixels) toggles the bypass parameter; true if it hit
    bool toggleBypassAt (juce::Point<float> position, juce::Point<float> led) const
    {
        const float S = imageScale();
        if (! juce::Rectangle<float> (110.0f * S, 70.0f * S).withCentre (led * S).contains (position)) return false;
        auto* prm = getAudioProcessor()->getBypassParameter();
        if (prm == nullptr) return false;
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
        prm->endChangeGesture();
        return true;
    }

protected:
    const float imgW, imgH;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image background;
};
}

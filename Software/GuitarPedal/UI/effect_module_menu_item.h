#pragma once
#ifndef EFFECT_MODULE_MENU_ITEM_H
#define EFFECT_MODULE_MENU_ITEM_H

#include "daisy_seed.h"
#include "../Effect-Modules/base_effect_module.h"

using namespace daisy;

namespace bkshepherd
{

class EffectModuleMenuItem : public daisy::AbstractMenu::CustomItem
{
  public:
    EffectModuleMenuItem();
    virtual ~EffectModuleMenuItem();

    /** Sets the Active Effect Settings UI Page
     * @param parentUI  A pointer to the parent UI
     * @param page  A pointer to the UI Page
     */
    void SetActiveEffectSettingsPage(UI *parentUI, UiPage *page);

    /** Sets the Active Effect Module
     * @param effectModule  A pointer to the effect module
     */
    void SetActiveEffectModule(BaseEffectModule *effectModule);

    /** Sets the flag to indicate the module is saving data
     * @param isSavingData  a boolean value to indicate saving data
     */
    void SetIsSavingData(bool isSavingData);

    /** Returns true, if this item can be entered for direct editing of the value. */
    bool CanBeEnteredForEditing() const override;

    /** Called when the encoder of the buttons are used to modify the value. */
    void ModifyValue(int16_t increments, uint16_t stepsPerRevolution, bool isFunctionButtonPressed) override;

    /** Called when the value slider is used to modify the value. */
    void ModifyValue(float valueSliderPosition0To1,  bool isFunctionButtonPressed) override;

    /** Called when the okay button is pressed (and CanBeEnteredForEditing() returns false). */
    void OnOkayButton() override;

    /** Handles updating the custom UI for this Effect.
     * @param elapsedTime a float value of how much time (in seconds) has elapsed since the last update
     */
    void UpdateUI(float elapsedTime);

    /** Draws the item to a OneBitGraphicsDisplay.
     * @param display           The display to draw to
     * @param currentIndex      The index in the menu
     * @param numItemsTotal     The total number of items in the menu
     * @param boundsToDrawIn    The Rectangle to draw the item into
     * @param isEditing         True if the enter button was pressed and the value is being edited directly.
     */
    void Draw(OneBitGraphicsDisplay& display,
                          int                    currentIndex,
                          int                    numItemsTotal,
                          Rectangle              boundsToDrawIn,
                          bool                   isEditing) override;

  private:
    UI* m_parentUI;
    UiPage* m_effectSettingsPage;
    BaseEffectModule *m_effectModule;
    bool m_isSavingData;
};
} // namespace bkshepherd
#endif

#pragma once
#ifndef GUITAR_PEDAL_UI_H
#define GUITAR_PEDAL_UI_H

#include "daisy_seed.h"
#include "effect_module_menu_item.h"
#include "CustomMappedValues.h"
using namespace daisy;

const int kNumMainMenuItems = 3;
const int kNumGlobalSettingsMenuItems = 7;
const int kNumPresetSettingsItems = 3;

namespace bkshepherd
{

class GuitarPedalUI
{
  public:
    GuitarPedalUI();
    virtual ~GuitarPedalUI();

    void Init();

    /** Handle updating the Active Effect Module based on the ID */
    void UpdateActiveEffect(int effectID);

    /** Handle updating a Parameter Value for the Active Effect Module based on the ID */
    void UpdateActiveEffectParameterValue(int paramID, bool showChangeOnDisplay = false);

    /** Handle updating all Parameter Values for the Active Effect Module */
    void UpdateActiveEffectParameterValues();

    /** Handle Showing the Saving Settings Screen */
    void ShowSavingSettingsScreen();

    /** Query the UI to see if the Saving Settings Screen is showing
    \return True if the Savings Setting Screen is showing, False otherwise.
    */
    bool IsShowingSavingSettingsScreen();

    /** Gets the ID of the Active Effect from the Settings Menu
    \return the ID of the Active Effect
    */
    int GetActiveEffectIDFromSettingsMenu();

    /** Generates the Appropriate UI Events */
    void GenerateUIEvents();

    /** Handles updating the custom UI for this Effect.
     * @param elapsedTime a float value of how much time (in seconds) has elapsed since the last update
     */
    void UpdateUI(float elapsedTime);

  private:
    void InitUi();
    void InitEffectUiPages();
    void InitGlobalSettingsUIPages();

    UI m_ui;
    FullScreenItemMenu m_mainMenu;
    FullScreenItemMenu m_activeEffectSettingsMenu;
    FullScreenItemMenu m_globalSettingsMenu;
    FullScreenItemMenu m_presetsMenu;
    UiEventQueue       m_eventQueue;

    bool m_needToCloseActiveEffectSettingsMenu;
    float m_secondsTilReturnFromParamChange;
    int m_paramIdToReturnTo;

    AbstractMenu::ItemConfig m_mainMenuItems[kNumMainMenuItems];
    AbstractMenu::ItemConfig m_globalSettingsMenuItems[kNumGlobalSettingsMenuItems];
    AbstractMenu::ItemConfig m_presetsMenuItems[kNumPresetSettingsItems];
    int m_numActiveEffectSettingsItems;
    uint32_t m_activePresetSelected;
    AbstractMenu::ItemConfig *m_activeEffectSettingsMenuItems;
    EffectModuleMenuItem m_effectModuleMenuItem;

    const char** m_availableEffectNames;
    MappedStringListValue *m_availableEffectListMappedValues;
    MappedIntValue **m_activeEffectSettingIntValues;
    MappedIntValue m_activePresetSettingIntValue;
    MappedStringListValue **m_activeEffectSettingStringValues;
    MyMappedFloatValue **m_activeEffectSettingFloatValues;
    bool *m_activeEffectSettingBoolValues;
    MappedIntValue m_midiChannelSettingValue;

    bool m_displayingSaveSettingsNotification;
    float m_secondsSinceLastActiveEffectSettingsSave;
};
} // namespace bkshepherd
#endif

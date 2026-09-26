#pragma once
#ifndef GUITAR_PEDAL_UI_H
#define GUITAR_PEDAL_UI_H

#include "CustomMappedValues.h"
#include "daisy_seed.h"
#include "effect_module_menu_item.h"
using namespace daisy;

const int kNumMainMenuItems = 4;
const int kNumGlobalSettingsMenuItems = 8;
const int kNumPresetSettingsItems = 3;

namespace bkshepherd {

class GuitarPedalUI {
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

    /** Gets the confirmed effect ID selected in the Effect menu
    \return the ID of the Active Effect
    */
    int GetSelectedEffectID();

    /** Generates the Appropriate UI Events */
    void GenerateUIEvents();

    /** Handles updating the custom UI for this Effect.
     * @param elapsedTime a float value of how much time (in seconds) has elapsed since the last update
     */
    void UpdateUI(float elapsedTime);

    /** Resets every Parameter of the Active Effect to its factory default value. Used as the
     * "Defaults" callbackFunctionItem in the Active Effect settings menu.
     */
    void ResetActiveEffectParametersToDefaults();
    void OpenEffectSelectionMenu();
    void SelectEffect();
    void BackFromEffectSelectionMenu();

  private:
    void InitUi();
    void InitEffectUiPages();
    void InitGlobalSettingsUIPages();

    UI m_ui;
    FullScreenItemMenu m_mainMenu;
    FullScreenItemMenu m_activeEffectSettingsMenu;
    FullScreenItemMenu m_globalSettingsMenu;
    FullScreenItemMenu m_presetsMenu;
    FullScreenItemMenu m_effectSelectionMenu;
    UiEventQueue m_eventQueue;

    bool m_needToCloseActiveEffectSettingsMenu;
    bool m_effectSelectionConfirmed = false;
    int m_selectedEffectID = 0;
    float m_secondsTilReturnFromParamChange = 0.0f;
    int m_paramIdToReturnTo;

    AbstractMenu::ItemConfig m_mainMenuItems[kNumMainMenuItems];
    AbstractMenu::ItemConfig m_globalSettingsMenuItems[kNumGlobalSettingsMenuItems];
    AbstractMenu::ItemConfig m_presetsMenuItems[kNumPresetSettingsItems];
    AbstractMenu::ItemConfig *m_effectSelectionMenuItems = nullptr;
    int m_numActiveEffectSettingsItems;
    uint32_t m_activePresetSelected;
    // These are tested against nullptr and deleted before reallocation, so
    // they must start out null even for non-static instances
    AbstractMenu::ItemConfig *m_activeEffectSettingsMenuItems = nullptr;
    EffectModuleMenuItem m_effectModuleMenuItem;

    MappedIntValue **m_activeEffectSettingIntValues = nullptr;
    MappedIntValue m_activePresetSettingIntValue;
    MappedStringListValue **m_activeEffectSettingStringValues = nullptr;
    MyMappedFloatValue **m_activeEffectSettingFloatValues = nullptr;
    bool *m_activeEffectSettingBoolValues = nullptr;
    MappedIntValue m_midiChannelSettingValue;

    bool m_displayingSaveSettingsNotification;
    float m_secondsSinceLastActiveEffectSettingsSave;
};
} // namespace bkshepherd

/** Trampoline for wiring GuitarPedalUI::ResetActiveEffectParametersToDefaults() up as a
 * callbackFunctionItem, matching how FactoryReset()/RebootToBootloader() are wired.
 */
void HandleResetActiveEffectParametersToDefaults(void *context);
void HandleOpenEffectSelectionMenu(void *context);
void HandleSelectEffect(void *context);
void HandleBackFromEffectSelectionMenu(void *context);

#endif

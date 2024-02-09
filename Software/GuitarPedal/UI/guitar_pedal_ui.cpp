#include "guitar_pedal_ui.h"
#include "../Hardware-Modules/base_hardware_module.h"
#include "../guitar_pedal_storage.h"

using namespace bkshepherd;

// Globals
extern BaseHardwareModule hardware;
extern PersistentStorage<Settings> storage;
extern int availableEffectsCount;
extern BaseEffectModule **availableEffects;
extern int activeEffectID;
extern BaseEffectModule *activeEffect;

// These will be called from the UI system. @see InitUi() in UiSystemDemo.cpp
void FlushCanvas(const UiCanvasDescriptor& canvasDescriptor)
{
    if(canvasDescriptor.id_ == 0)
    {
        OneBitGraphicsDisplay& display = *((OneBitGraphicsDisplay*)(canvasDescriptor.handle_));
        display.Update();
    }
}

void ClearCanvas(const daisy::UiCanvasDescriptor& canvasDescriptor)
{
    if(canvasDescriptor.id_ == 0)
    {
        OneBitGraphicsDisplay& display = *((OneBitGraphicsDisplay*)(canvasDescriptor.handle_));
        display.Fill(false);
    }
}

// Default Constructor
GuitarPedalUI::GuitarPedalUI() : m_needToCloseActiveEffectSettingsMenu(false),
                                    m_paramIdToReturnTo(-1),
                                    m_numActiveEffectSettingsItems(0),
                                    m_activePresetSelected(0),
                                    m_activePresetSettingIntValue(0,255,0,1,1),
                                    m_midiChannelSettingValue(1,16,1,1,5),
                                    m_displayingSaveSettingsNotification(false),
                                    m_secondsSinceLastActiveEffectSettingsSave(0.0f)

{

}

// Destructor
GuitarPedalUI::~GuitarPedalUI()
{

}

void GuitarPedalUI::Init()
{
    InitUi();
    InitEffectUiPages();
    InitGlobalSettingsUIPages();
    m_ui.OpenPage(m_mainMenu);
}
 
void GuitarPedalUI::UpdateActiveEffect(int effectID)
{   
    if (hardware.SupportsDisplay())
    {
        // Update the Menu item for the active effect (important for active effect changes not coming from the menu)
        m_availableEffectListMappedValues->SetIndex(effectID);

        // Re-init the UI Pages for the Main Menu and Effect Parameters
        InitEffectUiPages();
    }
}

void GuitarPedalUI::UpdateActiveEffectParameterValue(int paramID, bool showChangeOnDisplay)
{
    if (hardware.SupportsDisplay())
    {
        int parameterType = activeEffect->GetParameterType(paramID);

        // Update the UI based on the parameter type
        if (parameterType == -1 || parameterType == 0)
        {
            // Unknown, Raw value or Float Magnitude Types
            m_activeEffectSettingIntValues[paramID]->Set(activeEffect->GetParameterRaw(paramID));
        }
        else if (parameterType == 1)
        {
            // Unknown, Raw value or Float Magnitude Types
            m_activeEffectSettingFloatValues[paramID]->Set(activeEffect->GetParameterAsFloat(paramID));
        }
        else if (parameterType == 2)
        {
            // Bool Type
            m_activeEffectSettingBoolValues[paramID] = activeEffect->GetParameterAsBool(paramID);
        }
        else if (parameterType == 3)
        {
            // Binned Value Type
            if (activeEffect->GetParameterBinNames(paramID) == NULL)
            {
                // Handle Case where Bin Values don't have names
                m_activeEffectSettingIntValues[paramID]->Set(activeEffect->GetParameterAsBinnedValue(paramID));
            }
            else
            {
                // Handle Case where Bin Values do have names
                m_activeEffectSettingStringValues[paramID]->SetIndex(activeEffect->GetParameterAsBinnedValue(paramID) - 1);
            }
        }

        if (showChangeOnDisplay)
        {
            m_secondsTilReturnFromParamChange = 0.5f;

            // Change the main menu to be the name of the value the Knob is changing
            if (!m_activeEffectSettingsMenu.IsActive())
            {
                // Open the page to the settings menu and proper parameter
                m_ui.OpenPage(m_activeEffectSettingsMenu);
                m_activeEffectSettingsMenu.SelectItem(paramID);
                m_needToCloseActiveEffectSettingsMenu = true;
            }
            else
            {
                // If we were already on the param menu and we didn't open it, make sure we store the param index so we can return to it
                if (m_paramIdToReturnTo == -1 && !m_needToCloseActiveEffectSettingsMenu)
                {
                    m_paramIdToReturnTo = m_activeEffectSettingsMenu.GetSelectedItemIdx();
                }

                // Change the menu to the proper parameter we are changing
                m_activeEffectSettingsMenu.SelectItem(paramID);
            }
        }
    }
}
    
void GuitarPedalUI::UpdateActiveEffectParameterValues()
{
    if (hardware.SupportsDisplay())
    {
        for (int paramID = 0; paramID < activeEffect->GetParameterCount(); paramID++)
        {
            UpdateActiveEffectParameterValue(paramID, false);
        }
    }
}

void GuitarPedalUI::ShowSavingSettingsScreen()
{
    m_displayingSaveSettingsNotification = true;
    m_secondsSinceLastActiveEffectSettingsSave = 0.0f;
}

bool GuitarPedalUI::IsShowingSavingSettingsScreen()
{
    return m_displayingSaveSettingsNotification;
}

int GuitarPedalUI::GetActiveEffectIDFromSettingsMenu()
{
    return m_availableEffectListMappedValues->GetIndex();
}

void GuitarPedalUI::InitUi()
{
    UI::SpecialControlIds specialControlIds;
    specialControlIds.okBttnId = 0; // Encoder button is our okay button
    specialControlIds.menuEncoderId = 0; // Encoder is used as the main menu navigation encoder

    // This is the canvas for the OLED display.
    UiCanvasDescriptor oledDisplayDescriptor;
    oledDisplayDescriptor.id_     = 0; // the unique ID
    oledDisplayDescriptor.handle_ = &hardware.display;   // a pointer to the display
    oledDisplayDescriptor.updateRateMs_      = 50; // 50ms == 20Hz
    oledDisplayDescriptor.screenSaverTimeOut = 0;  // display always on
    oledDisplayDescriptor.clearFunction_     = &ClearCanvas;
    oledDisplayDescriptor.flushFunction_     = &FlushCanvas;

    m_ui.Init(m_eventQueue,
            specialControlIds,
            {oledDisplayDescriptor},
            0);
}

void GuitarPedalUI::InitEffectUiPages()
{
    // ====================================================================
    // The Main Menu
    // ====================================================================

    // Link the effect to the Main Menu Item
    m_effectModuleMenuItem.SetActiveEffectSettingsPage(&m_ui, &m_activeEffectSettingsMenu);
    m_effectModuleMenuItem.SetActiveEffectModule(activeEffect);

    m_mainMenuItems[0].type = daisy::AbstractMenu::ItemType::customItem;
    m_mainMenuItems[0].asCustomItem.itemObject = &m_effectModuleMenuItem;

    m_mainMenuItems[1].type = daisy::AbstractMenu::ItemType::openUiPageItem;
    m_mainMenuItems[1].text = "Settings";
    m_mainMenuItems[1].asOpenUiPageItem.pageToOpen = &m_globalSettingsMenu;
    
    m_mainMenuItems[2].type = daisy::AbstractMenu::ItemType::openUiPageItem;
    m_mainMenuItems[2].text = "Preset";
    m_mainMenuItems[2].asOpenUiPageItem.pageToOpen = &m_presetsMenu;
    m_mainMenu.Init(m_mainMenuItems, kNumMainMenuItems);

    // ====================================================================
    // The "Settings" menu for the Active Effect (depends on what effect is active)
    // ====================================================================

    // Clean up any dynamically allocated memory
    if (m_activeEffectSettingIntValues != NULL)
    {
        for(int i = 0; i < m_numActiveEffectSettingsItems; ++i)
        {
            if (m_activeEffectSettingIntValues[i] != NULL)
            {
                delete m_activeEffectSettingIntValues[i];
            }
        }

        delete [] m_activeEffectSettingIntValues;
        m_activeEffectSettingIntValues = NULL;
    }

    // Clean up any dynamically allocated memory
    if (m_activeEffectSettingFloatValues != NULL)
    {
        delete [] m_activeEffectSettingFloatValues;
        m_activeEffectSettingFloatValues = NULL;
    }
    
    if (m_activeEffectSettingStringValues != NULL)
    {
        for(int i = 0; i < m_numActiveEffectSettingsItems; ++i)
        {
            if (m_activeEffectSettingStringValues[i] != NULL)
            {
                delete m_activeEffectSettingStringValues[i];
            }
        }
            

        delete [] m_activeEffectSettingStringValues;
        m_activeEffectSettingStringValues = NULL;
    }

    m_numActiveEffectSettingsItems = 0;

    if (m_activeEffectSettingBoolValues != NULL)
    {
        delete [] m_activeEffectSettingBoolValues;
    }

    if (m_activeEffectSettingsMenuItems != NULL)
    {
        delete [] m_activeEffectSettingsMenuItems;
    }

    m_numActiveEffectSettingsItems = activeEffect->GetParameterCount();
    m_activeEffectSettingIntValues = new MappedIntValue*[m_numActiveEffectSettingsItems];
    m_activeEffectSettingFloatValues = new MyMappedFloatValue*[m_numActiveEffectSettingsItems];
    m_activeEffectSettingStringValues = new MappedStringListValue*[m_numActiveEffectSettingsItems];
    m_activeEffectSettingBoolValues = new bool[m_numActiveEffectSettingsItems];

    // Initialize the param value stores to a known state.
    for(int i = 0; i < m_numActiveEffectSettingsItems; ++i)
    {
        m_activeEffectSettingIntValues[i] = NULL;
        m_activeEffectSettingStringValues[i] = NULL;
        m_activeEffectSettingBoolValues[i] = false;
    }

    m_activeEffectSettingsMenuItems = new AbstractMenu::ItemConfig[m_numActiveEffectSettingsItems + 1];
    
    for (int i = 0; i < m_numActiveEffectSettingsItems; i++)
    {
        m_activeEffectSettingsMenuItems[i].text = activeEffect->GetParameterName(i);

        int parameterType = activeEffect->GetParameterType(i);        

        if (parameterType == -1 || parameterType == ParameterValueType::Raw)
        {
            int minValue = activeEffect->GetParameterMin(i);
            int maxValue = activeEffect->GetParameterMax(i);
            // Unknown or Raw value Types
            m_activeEffectSettingsMenuItems[i].type = AbstractMenu::ItemType::valueItem;
            m_activeEffectSettingIntValues[i] = new MappedIntValue(minValue, maxValue, activeEffect->GetParameterRaw(i), 1, 5);
            m_activeEffectSettingsMenuItems[i].asMappedValueItem.valueToModify = m_activeEffectSettingIntValues[i];
        }
        else if (parameterType == ParameterValueType::FloatMagnitude)
        {
            float minValue = (float)activeEffect->GetParameterMin(i);
            float maxValue = (float)activeEffect->GetParameterMax(i);
            float fineStep = activeEffect->GetParameterFineStepSize(i);
            // Raw32 value or Float Magnitude Types
            m_activeEffectSettingsMenuItems[i].type = AbstractMenu::ItemType::valueItem;
            m_activeEffectSettingFloatValues[i] = new MyMappedFloatValue(minValue, maxValue, activeEffect->GetParameterAsFloat(i));
            m_activeEffectSettingFloatValues[i]->SetFineStepSize(fineStep);
            m_activeEffectSettingsMenuItems[i].asMappedValueItem.valueToModify = m_activeEffectSettingFloatValues[i];
        }
        else if (parameterType == ParameterValueType::Bool)
        {
            // Boolean Type
            m_activeEffectSettingsMenuItems[i].type = AbstractMenu::ItemType::checkboxItem;
            m_activeEffectSettingBoolValues[i] = activeEffect->GetParameterAsBool(i);
            m_activeEffectSettingsMenuItems[i].asCheckboxItem.valueToModify = &m_activeEffectSettingBoolValues[i];
        }
        else if (parameterType == ParameterValueType::Binned)
        {
            // Binned Value Type
            m_activeEffectSettingsMenuItems[i].type = AbstractMenu::ItemType::valueItem;
            int binnedValue = activeEffect->GetParameterAsBinnedValue(i);
            const char** binNames = activeEffect->GetParameterBinNames(i);

            if (binNames == NULL)
            {
                m_activeEffectSettingIntValues[i] = new MappedIntValue(1, activeEffect->GetParameterBinCount(i), binnedValue, 1, 5);
                m_activeEffectSettingsMenuItems[i].asMappedValueItem.valueToModify = m_activeEffectSettingIntValues[i];
            }
            else
            {
                m_activeEffectSettingStringValues[i] = new MappedStringListValue(binNames, activeEffect->GetParameterBinCount(i), binnedValue-1);
                m_activeEffectSettingsMenuItems[i].asMappedValueItem.valueToModify = m_activeEffectSettingStringValues[i];
            }
            
        }
    }

    m_activeEffectSettingsMenuItems[m_numActiveEffectSettingsItems].type = AbstractMenu::ItemType::closeMenuItem;
    m_activeEffectSettingsMenuItems[m_numActiveEffectSettingsItems].text = "Back";

    m_activeEffectSettingsMenu.Init(m_activeEffectSettingsMenuItems, m_numActiveEffectSettingsItems + 1);
}

void GuitarPedalUI::InitGlobalSettingsUIPages()
{
    // Get a handle to the persitance storage settings
    Settings &settings = storage.GetSettings();

    // ====================================================================
    // The "Global Settings" menu
    // ====================================================================
    if (m_availableEffectListMappedValues != NULL)
    {
        delete m_availableEffectListMappedValues;
    }

    if (m_availableEffectNames != NULL)
    {
        delete [] m_availableEffectNames;
    }

    m_availableEffectNames = new const char*[availableEffectsCount];
    int m_activeEffectIndex = -1;

    for (int i = 0; i < availableEffectsCount; i++)
    {
        m_availableEffectNames[i] = availableEffects[i]->GetName();

        if (availableEffects[i] == activeEffect)
        {
            m_activeEffectIndex = i;
        }
    }
    
    m_availableEffectListMappedValues = new MappedStringListValue(m_availableEffectNames, availableEffectsCount, m_activeEffectIndex);

    m_globalSettingsMenuItems[0].type = AbstractMenu::ItemType::valueItem;
    m_globalSettingsMenuItems[0].text = "Effect";
    m_globalSettingsMenuItems[0].asMappedValueItem.valueToModify = m_availableEffectListMappedValues;

    m_globalSettingsMenuItems[1].type = AbstractMenu::ItemType::checkboxItem;
    m_globalSettingsMenuItems[1].text = "True Bypass";
    m_globalSettingsMenuItems[1].asCheckboxItem.valueToModify = &settings.globalRelayBypassEnabled;

    m_globalSettingsMenuItems[2].type = AbstractMenu::ItemType::checkboxItem;
    m_globalSettingsMenuItems[2].text = "Split Mono";
    m_globalSettingsMenuItems[2].asCheckboxItem.valueToModify = &settings.globalSplitMonoInputToStereo;

    m_globalSettingsMenuItems[3].type = AbstractMenu::ItemType::checkboxItem;
    m_globalSettingsMenuItems[3].text = "Midi On";
    m_globalSettingsMenuItems[3].asCheckboxItem.valueToModify = &settings.globalMidiEnabled;

    m_globalSettingsMenuItems[4].type = AbstractMenu::ItemType::checkboxItem;
    m_globalSettingsMenuItems[4].text = "Midi Thru";
    m_globalSettingsMenuItems[4].asCheckboxItem.valueToModify = &settings.globalMidiThrough;

    m_globalSettingsMenuItems[5].type = AbstractMenu::ItemType::valueItem;
    m_globalSettingsMenuItems[5].text = "Midi Ch";
    m_midiChannelSettingValue.Set(settings.globalMidiChannel);
    m_globalSettingsMenuItems[5].asMappedValueItem.valueToModify = &m_midiChannelSettingValue;

    m_globalSettingsMenuItems[6].type = AbstractMenu::ItemType::closeMenuItem;
    m_globalSettingsMenuItems[6].text = "Back";

    m_globalSettingsMenu.Init(m_globalSettingsMenuItems, kNumGlobalSettingsMenuItems);
    

    m_presetsMenuItems[0].type = AbstractMenu::ItemType::valueItem;
    m_presetsMenuItems[0].text = "Preset #";
    m_presetsMenuItems[0].asMappedValueItem.valueToModify = &m_activePresetSettingIntValue;

    m_presetsMenuItems[1].type = AbstractMenu::ItemType::callbackFunctionItem;
    m_presetsMenuItems[1].text = "Erase All";
    m_presetsMenuItems[1].asCallbackFunctionItem.callbackFunction = &FactoryReset;    
    m_presetsMenuItems[1].asCallbackFunctionItem.context = this;

    m_presetsMenuItems[2].type = AbstractMenu::ItemType::closeMenuItem;
    m_presetsMenuItems[2].text = "Back";

    m_presetsMenu.Init(m_presetsMenuItems, kNumPresetSettingsItems);
    
    m_activePresetSettingIntValue.Set(activeEffect->GetCurrentPreset());
}

void GuitarPedalUI::GenerateUIEvents()
{
    if (!hardware.SupportsDisplay())
    {
        return;
    }

    if(hardware.encoders[0].RisingEdge())
    {
        m_eventQueue.AddButtonPressed(0, 1);
    }

    if(hardware.encoders[0].FallingEdge())
    {
        m_eventQueue.AddButtonReleased(0);
    }

    const auto increments = hardware.encoders[0].Increment();

    if(increments != 0)
    {
        m_eventQueue.AddEncoderTurned(0, increments, 12);
    }
}

void GuitarPedalUI::UpdateUI(float elapsedTime)
{
    // Get a handle to the persitance storage settings
    Settings &settings = storage.GetSettings();

    activeEffect->UpdateUI(elapsedTime);

    // Properly Handle returning the screen from a parameter change
    if (m_secondsTilReturnFromParamChange > 0.0f)
    {
        m_secondsTilReturnFromParamChange -= elapsedTime;

        if (m_secondsTilReturnFromParamChange <= 0.0f)
        {
            // Close the menu if we opened it to display the Parameter change
            if (m_needToCloseActiveEffectSettingsMenu)
            {
                m_ui.ClosePage(m_activeEffectSettingsMenu);
                m_needToCloseActiveEffectSettingsMenu = false;
            }

            // If we were already on param menu return to the item we had selected before.
            if (m_paramIdToReturnTo != -1)
            {
                m_activeEffectSettingsMenu.SelectItem(m_paramIdToReturnTo);
                m_paramIdToReturnTo = -1;
            }
        }
    }        

    // Handle Displaying the Saving notication
    if (m_displayingSaveSettingsNotification)
    {
        m_secondsSinceLastActiveEffectSettingsSave += elapsedTime;

        // Change the main menu text to say saved
        m_effectModuleMenuItem.SetIsSavingData(true);

        if (m_secondsSinceLastActiveEffectSettingsSave > 2.0f)
        {
            m_displayingSaveSettingsNotification = false;
            m_effectModuleMenuItem.SetIsSavingData(false);
        }
    }
    
    m_activePresetSelected = m_activePresetSettingIntValue.Get();
    // Set the target preset from the menu, the ui will be the "brains" and figure out what actual preset number makes sense here, since Base Effects does not know its effect id.
    if(m_activePresetSelected != activeEffect->GetCurrentPreset())
    {
        uint32_t temp = activeEffect->GetPresetCount();
        if(m_activePresetSelected < temp)
        {
            activeEffect->SetCurrentPreset(m_activePresetSelected);
            LoadPresetFromPersistentStorage(activeEffectID, m_activePresetSelected);
        }
        else
        {
            // Basically set 1 index higher than the actual presets, expecting the user to save the new preset in the usual way
            m_activePresetSelected = temp;
            activeEffect->SetCurrentPreset(m_activePresetSelected);
            m_activePresetSettingIntValue.Set(m_activePresetSelected);
        }
        // Update all menu system parameters to the current Active Effect Parameter Settings
        for (int i = 0; i < m_numActiveEffectSettingsItems; i++)
        {
            int parameterType = activeEffect->GetParameterType(i);
    
            if (parameterType == -1 || parameterType == 0)
            {
                // Unknown or Raw value Types
                m_activeEffectSettingIntValues[i]->Set(activeEffect->GetParameterRaw(i));
            }
            else if (parameterType == 1)
            {
                // Float Magnitude Types
                m_activeEffectSettingFloatValues[i]->Set(activeEffect->GetParameterAsFloat(i));
            }
            else if (parameterType == 2)
            {
                // Bool Type
                m_activeEffectSettingBoolValues[i] = activeEffect->GetParameterAsBool(i);
            }
            else if (parameterType == 3)
            {
                // Binned Value Type
                if (activeEffect->GetParameterBinNames(i) == NULL)
                {
                    // Handle when Bins have no String Name
                    activeEffect->SetParameterAsBinnedValue(i, m_activeEffectSettingIntValues[i]->Get());
                }
                else
                {
                    // Handle when Bins are using String Name
                    activeEffect->SetParameterAsBinnedValue(i, m_activeEffectSettingStringValues[i]->GetIndex() + 1);
                }
            }
        }
    }
    else
    {
        // Update all Active Effect Parameter Settings to values from the menu system
        for (int i = 0; i < m_numActiveEffectSettingsItems; i++)
        {
            int parameterType = activeEffect->GetParameterType(i);
    
            if (parameterType == -1 || parameterType == 0)
            {
                // Unknown, Raw value or Float Magnitude Types
                activeEffect->SetParameterRaw(i, m_activeEffectSettingIntValues[i]->Get());
            }
            else if (parameterType == 1)
            {
                // Unknown, Raw value or Float Magnitude Types
                activeEffect->SetParameterAsFloat(i, m_activeEffectSettingFloatValues[i]->Get());
            }
            else if (parameterType == 2)
            {
                // Bool Type
                activeEffect->SetParameterAsBool(i, m_activeEffectSettingBoolValues[i]);
            }
            else if (parameterType == 3)
            {
                // Binned Value Type
                if (activeEffect->GetParameterBinNames(i) == NULL)
                {
                    // Handle when Bins have no String Name
                    m_activeEffectSettingIntValues[i]->Set(activeEffect->GetParameterAsBinnedValue(i));
                }
                else
                {
                    // Handle when Bins are using String Name
                    m_activeEffectSettingStringValues[i]->SetIndex(activeEffect->GetParameterAsBinnedValue(i) - 1);
                }
            }
        }
    }
    

    // Update the Midi Channel if the value was changed in the Menu
    settings.globalMidiChannel = m_midiChannelSettingValue.Get();

    // Process the UI
    m_ui.Process();
}

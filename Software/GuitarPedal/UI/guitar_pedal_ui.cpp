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

void GuitarPedalUI::UpdateActiveEffectParameterValue(int paramID, uint8_t paramValue, bool showChangeOnDisplay)
{
    if (hardware.SupportsDisplay())
    {
        m_activeEffectSettingValues[paramID]->Set(paramValue);

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

    m_mainMenu.Init(m_mainMenuItems, kNumMainMenuItems);

    // ====================================================================
    // The "Settings" menu for the Active Effect (depends on what effect is active)
    // ====================================================================

    // Clean up any dynamically allocated memory
    if (m_activeEffectSettingValues != NULL)
    {
        for(int i = 0; i < m_numActiveEffectSettingsItems; ++i)
            delete m_activeEffectSettingValues[i];

        delete [] m_activeEffectSettingValues;
        m_activeEffectSettingValues = NULL;
        m_numActiveEffectSettingsItems = 0;
    }

    if (m_activeEffectSettingsMenuItems != NULL)
    {
        delete [] m_activeEffectSettingsMenuItems;
    }

    m_numActiveEffectSettingsItems = activeEffect->GetParameterCount();
    m_activeEffectSettingValues = new MappedIntValue*[m_numActiveEffectSettingsItems];
    m_activeEffectSettingsMenuItems = new AbstractMenu::ItemConfig[m_numActiveEffectSettingsItems + 1];
    
    for (int i = 0; i < m_numActiveEffectSettingsItems; i++)
    {
        m_activeEffectSettingsMenuItems[i].type = AbstractMenu::ItemType::valueItem;
        m_activeEffectSettingsMenuItems[i].text = activeEffect->GetParameterName(i);
        m_activeEffectSettingValues[i] = new MappedIntValue(0, 127, activeEffect->GetParameter(i), 1, 5);
        m_activeEffectSettingsMenuItems[i].asMappedValueItem.valueToModify = m_activeEffectSettingValues[i];
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

    // Update all Active Effect Parameter Settings to values from the menu system
    for (int i = 0; i < m_numActiveEffectSettingsItems; i++)
    {
        activeEffect->SetParameter(i, m_activeEffectSettingValues[i]->Get());
    }

    // Update the Midi Channel if the value was changed in the Menu
    settings.globalMidiChannel = m_midiChannelSettingValue.Get();

    // Process the UI
    m_ui.Process();
}

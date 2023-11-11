#include "effect_module_menu_item.h"

using namespace bkshepherd;

// Default Constructor
EffectModuleMenuItem::EffectModuleMenuItem() : m_parentUI(NULL),
                                                m_effectSettingsPage(NULL),
                                                m_effectModule(NULL),
                                                m_isSavingData(false)

{

}

// Destructor
EffectModuleMenuItem::~EffectModuleMenuItem()
{

}

void EffectModuleMenuItem::SetActiveEffectSettingsPage(UI *parentUI, UiPage *page)
{
    m_parentUI = parentUI;
    m_effectSettingsPage = page;
}

void EffectModuleMenuItem::SetActiveEffectModule(BaseEffectModule *effectModule)
{
    m_effectModule = effectModule;
}

void EffectModuleMenuItem::SetIsSavingData(bool isSavingData)
{
    m_isSavingData = isSavingData;
}

bool EffectModuleMenuItem::CanBeEnteredForEditing() const
{
    return false;
}

void EffectModuleMenuItem::ModifyValue(int16_t increments, uint16_t stepsPerRevolution, bool isFunctionButtonPressed)
{

}

void EffectModuleMenuItem::ModifyValue(float valueSliderPosition0To1,  bool isFunctionButtonPressed)
{

}

void EffectModuleMenuItem::OnOkayButton()
{
    // Default Behavior is to open the effect settings page when the encoder button is pressed
    if (m_parentUI != NULL && m_effectSettingsPage != NULL)
    {
        m_parentUI->OpenPage(*m_effectSettingsPage);
    }        
}
    
void EffectModuleMenuItem::UpdateUI(float elapsedTime)
{
    if (m_effectModule != NULL)
    {
        m_effectModule->UpdateUI(elapsedTime);
    }
}

void EffectModuleMenuItem::Draw(OneBitGraphicsDisplay& display, int currentIndex, int numItemsTotal, Rectangle boundsToDrawIn, bool isEditing)
{
    // If there is nothing to draw fill in some basic information.
    if (m_effectModule == NULL)
    {
        display.WriteStringAligned("Daisy Guitar Pedal", Font_7x10, boundsToDrawIn, Alignment::topCentered, true);
        display.WriteStringAligned("Made by", Font_7x10, boundsToDrawIn, Alignment::centered, true);
        display.WriteStringAligned("Keith Shepherd", Font_7x10, boundsToDrawIn, Alignment::bottomCentered, true);
        
        return;
    }
    
    // Handle Indicating that data is saving
    if (m_isSavingData)
    {
        display.WriteStringAligned("Saving...", Font_11x18, boundsToDrawIn, Alignment::centered, true);
        return;
    }

    // Let the Effect Module Handle any custom drawing (currently has the full screen to work with)
    m_effectModule->DrawUI(display, currentIndex, numItemsTotal, boundsToDrawIn, isEditing);
}
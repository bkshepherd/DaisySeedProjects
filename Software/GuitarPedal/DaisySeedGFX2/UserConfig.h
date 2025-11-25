//*************************************************************************
// Copyright(c) 2024 Dad Design.
//   UserConfig.h - Adapted for Emerald 125B + ST7735 & changed by Gemini 11/24/2025
//*************************************************************************
#pragma once 

//-------------------------------------------------------------------------
// Screen dimensions (ST7735 Standard)
#define TFT_WIDTH       128       
#define TFT_HEIGHT      160       

//-------------------------------------------------------------------------
// Display controller type
#define TFT_CONTROLEUR_TFT  7735  // <--- The file you found confirms this is the correct ID

//-------------------------------------------------------------------------
// Color encoding
#define TFT_COLOR 16              
//#define INV_COLOR               // <--- Uncomment this line later if Blue appears as Red

//-------------------------------------------------------------------------
// Screen Offsets 
// (ST7735s often need 1-2 pixel offsets. Keep at 0 for now, tweak if image is shifted)
#define XSCREEN_OFFSET 0
#define YSCREEN_OFFSET 0

//-------------------------------------------------------------------------
// SPI bus configuration
#define TFT_SPI_PORT SPI_1        
#define TFT_SPI_MODE Mode0        // <--- The file you found suggests Mode0. This is safer for ST7735.
#define TFT_SPI_BaudPrescaler PS_8 

//-------------------------------------------------------------------------
// GPIO configuration for SPI pins
// CRITICAL: These must match guitar_pedal_125b.cpp, NOT the default ST7735 file.
#define TFT_MOSI D10              
#define TFT_SCLK D8               
#define TFT_DC   D9              // <--- EMERALD HARDWARE PIN
#define TFT_RST  D11             // <--- EMERALD HARDWARE PIN

//-------------------------------------------------------------------------
// Block size optimization
// 128 width / 8 blocks = 16px
// 160 height / 8 blocks = 20px
#define NB_BLOC_WIDTH   8               
#define NB_BLOC_HEIGHT  8               
#define NB_BLOCS        NB_BLOC_WIDTH * NB_BLOC_HEIGHT 
#define BLOC_WIDTH      TFT_WIDTH / NB_BLOC_WIDTH      
#define BLOC_HEIGHT     TFT_HEIGHT / NB_BLOC_HEIGHT    

//-------------------------------------------------------------------------
// FIFO size 
#define SIZE_FIFO 5
#pragma once
// --------------------------------------------------------------------------
// Initialisation de l'écran de type ST7735
//   thank:  Adafruit https://github.com/adafruit/Adafruit-ST7735-Library
//           and
//           TFT_eSPI https://github.com/Bodmer/TFT_eSPI
// 

// Generic commands
#define TFT_NOP     0x00
#define TFT_SWRST   0x01

#define TFT_INVOFF  0x20
#define TFT_INVON   0x21

#define TFT_DISPOFF 0x28
#define TFT_DISPON  0x29

#define TFT_CASET   0x2A
#define TFT_RASET   0x2B
#define TFT_RAMWR   0x2C

#define TFT_MADCTL  0x36
#define TFT_MAD_MY  0x80
#define TFT_MAD_MX  0x40
#define TFT_MAD_MV  0x20
#define TFT_MAD_ML  0x10
#define TFT_MAD_BGR 0x08
#define TFT_MAD_MH  0x04
#define TFT_MAD_RGB 0x00

#define TFT_MAD_COLOR_ORDER TFT_MAD_RGB
//#define TFT_MAD_COLOR_ORDER TFT_MAD_BGR

// ST7735 specific commands used in init
#define ST7735_NOP			0x00
#define ST7735_SWRESET		0x01
#define ST7735_RDDID		0x04
#define ST7735_RDDST		0x09

#define ST7735_RDDPM		0x0A        // Read display power mode
#define ST7735_RDD_MADCTL	0x0B        // Read display MADCTL
#define ST7735_RDD_COLMOD	0x0C        // Read display pixel format
#define ST7735_RDDIM		0x0D        // Read display image mode
#define ST7735_RDDSM		0x0E        // Read display signal mode

#define ST7735_SLPIN		0x10
#define ST7735_SLPOUT		0x11
#define ST7735_PTLON		0x12
#define ST7735_NORON		0x13

#define ST7735_INVOFF		0x20
#define ST7735_INVON		0x21
#define ST7735_GAMSET		0x26        // Gamma set
#define ST7735_DISPOFF	    0x28
#define ST7735_DISPON		0x29

#define ST7735_CASET		0x2A
#define ST7735_RASET		0x2B
#define ST7735_RAMWR		0x2C
#define ST7735_RAMRD		0x2E

#define ST7735_PTLAR		0x30
#define ST7735_TEOFF		0x34      // Tearing effect line off
#define ST7735_TEON			0x35      // Tearing effect line on
#define ST7735_MADCTL		0x36      // Memory data access control
#define ST7735_IDMOFF		0x38      // Idle mode off
#define ST7735_IDMON		0x39      // Idle mode on
#define ST7735_COLMOD		0x3A

#define ST7735_RDID1		0xDA
#define ST7735_RDID2		0xDB
#define ST7735_RDID3		0xDC

#define ST7735_FRMCTR1      0xB1
#define ST7735_FRMCTR2      0xB2
#define ST7735_FRMCTR3      0xB3
#define ST7735_INVCTR       0xB4
#define ST7735_DISSET5      0xB6

#define ST7735_PWCTR1       0xC0
#define ST7735_PWCTR2       0xC1
#define ST7735_PWCTR3       0xC2
#define ST7735_PWCTR4       0xC3
#define ST7735_PWCTR5       0xC4
#define ST7735_VMCTR1       0xC5

#define ST7735_RDID1        0xDA
#define ST7735_RDID2        0xDB
#define ST7735_RDID3        0xDC
#define ST7735_RDID4        0xDD

#define ST7735_GMCTRP1      0xE0
#define ST7735_GMCTRN1      0xE1

#define ST7735_PWCTR6       0xFC

// --------------------------------------------------------------------------  
// Initialization of the ST7735-type screen  
void DadGFX::TFT_SPI::Initialise() {
    // Init for 7735R, part 1 (red or green tab)
    // -----------------------------------------
    SendCommand(ST7735_SWRESET);        //  1: Software reset, 0 args, w/delay
      System::Delay(150);                
    SendCommand(ST7735_SLPOUT);         //  2: Out of sleep mode, 0 args, w/delay
      System::Delay(500);                
    
    SendCommand(ST7735_FRMCTR1);        //  3: Frame rate ctrl - normal mode
      SendData(0x01); 
      SendData(0x2C); 
      SendData(0x2D);                   //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
    
    SendCommand(ST7735_FRMCTR2);        //  4: Frame rate control - idle mode
      SendData(0x01); 
      SendData(0x2C); 
      SendData(0x2D);                   //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
    
    SendCommand(ST7735_FRMCTR3);        //  5: Frame rate ctrl - partial mode
      SendData(0x01); 
      SendData(0x2C); 
      SendData(0x2D);                   //     Dot inversion mode
      SendData(0x01); 
      SendData(0x2C); 
      SendData(0x2D);                   //     Line inversion mode

    SendCommand(ST7735_INVCTR);         //  6: Display inversion ctrl
      SendData(0x07);                   //     No inversion
    
    SendCommand(ST7735_PWCTR1);         //  7: Power control
      SendData(0xA2);
      SendData(0x02);                   //     -4.6V
      SendData(0x84);                   //     AUTO mode
    
    SendCommand(ST7735_PWCTR2);         //  8: Power control
      SendData(0xC5);                   //     VGH25 = 2.4C VGSEL = -10 VGH = 3 * AVDD
    
    SendCommand(ST7735_PWCTR3 );        //  9: Power control
      SendData(0x0A);                   //     Opamp current small
      SendData(0x00);                   //     Boost frequency
    
    SendCommand(ST7735_PWCTR4 );        // 10: Power control
      SendData(0x8A);                   //     BCLK/2); Opamp current small & Medium low
      SendData(0x2A);  

    SendCommand(ST7735_PWCTR5 );        // 11: Power control
      SendData(0x8A); SendData(0xEE);
    
    SendCommand(ST7735_VMCTR1 );        // 12: Power control
      SendData(0x0E);
    
    SendCommand(ST7735_INVOFF );        // 13: Don't invert display
    
    SendCommand(ST7735_MADCTL );        // 14: Memory access control (directions)
      SendData(0xC0 | TFT_MAD_COLOR_ORDER); //     row addr/col addr); bottom to top refresh
    
    SendCommand(ST7735_COLMOD );        // 15: set color mode
#if TFT_COLOR == 16
        SendData(0x05);
#else
        SendData(0x06);
#endif

    SendCommand(ST7735_GMCTRP1);        // 16: 
      SendData(0x02); SendData(0x1c); SendData(0x07); SendData(0x12);
      SendData(0x37); SendData(0x32); SendData(0x29); SendData(0x2d);
      SendData(0x29); SendData(0x25); SendData(0x2B); SendData(0x39);
      SendData(0x00); SendData(0x01); SendData(0x03); SendData(0x10);
    
    SendCommand(ST7735_GMCTRN1);        // 17:
      SendData(0x03); SendData(0x1d); SendData(0x07); SendData(0x06);
      SendData(0x2E); SendData(0x2C); SendData(0x29); SendData(0x2D);
      SendData(0x2E); SendData(0x2E); SendData(0x37); SendData(0x3F);
      SendData(0x00); SendData(0x00); SendData(0x02); SendData(0x10);
    
    SendCommand(ST7735_NORON);          // 18: Normal display on
      System::Delay(10);
    
    SendCommand(ST7735_DISPON);         // 19: Main screen turn on
      System::Delay(100);
}

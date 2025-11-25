//------------------------------------------------------------------------
// TFT_SPI.h
//  Management of an SPI connection to a TFT screen
// Copyright(c) 2024 Dad Design.
//------------------------------------------------------------------------
#pragma once
#include "daisy_seed.h"
#include "per/spi.h"
#include "per/gpio.h"
#include "sys/system.h"
#include "../UserConfig.h"

// TFT Generic commands
#define TFT_NOP     0x00    // No operation
#define TFT_SWRST   0x01    // Software reset

#define TFT_INVOFF  0x20    // Disable inversion
#define TFT_INVON   0x21    // Enable inversion

#define TFT_DISPOFF 0x28    // Turn display off
#define TFT_DISPON  0x29    // Turn display on

#define TFT_CASET   0x2A    // Set column address
#define TFT_RASET   0x2B    // Set row address
#define TFT_RAMWR   0x2C    // Write to display RAM

#define TFT_MADCTL  0x36    // Memory access control
#define TFT_MAD_MY  0x80    // Row address order
#define TFT_MAD_MX  0x40    // Column address order
#define TFT_MAD_MV  0x20    // Row/Column exchange
#define TFT_MAD_ML  0x10    // Vertical refresh order
#define TFT_MAD_BGR 0x08    // Blue-Green-Red pixel order
#define TFT_MAD_MH  0x04    // Horizontal refresh order
#define TFT_MAD_RGB 0x00    // Red-Green-Blue pixel order

using namespace daisy;
extern DaisySeed hw;

enum class SPIMode {
    Mode0, // SPI Mode 0
    Mode1, // SPI Mode 1
    Mode2, // SPI Mode 2
    Mode3  // SPI Mode 3
};

enum class Rotation {
    Degre_0,     // 0-degree rotation
    Degre_90,    // 90-degree rotation
    Degre_180,   // 180-degree rotation
    Degre_270    // 270-degree rotation
};

// Configuration of GPIOs used
#define _TFT_SPI_PORT SpiHandle::Config::Peripheral::TFT_SPI_PORT
#define _TFT_SPI_MODE SPIMode::TFT_SPI_MODE

// GPIO configurations for TFT
#define _TFT_MOSI seed::TFT_MOSI  // MOSI pin
#define _TFT_SCLK seed::TFT_SCLK  // Clock pin
#define _TFT_DC   seed::TFT_DC    // Data/Command pin
#define _TFT_RST  seed::TFT_RST   // Reset pin

namespace DadGFX {

//***********************************************************************************
// TFT_SPI
//  SPI management for communication with the TFT screen
//*********************************************************************************** 
class TFT_SPI {
    public :

    // --------------------------------------------------------------------------
    // Initialize SPI communication
    void Init_TFT_SPI();

    // --------------------------------------------------------------------------
    // Initialize the TFT screen
    void Initialise();

    // --------------------------------------------------------------------------
    // Change the screen orientation
    void setTFTRotation(Rotation r);

    // --------------------------------------------------------------------------
    // Send a command to the TFT screen
    inline void SendCommand(uint8_t cmd) {
        m_dc.Write(false);  // Set DC pin to command mode
        m_spi.BlockingTransmit(&cmd, 1); // Transmit the command
    }

    // --------------------------------------------------------------------------
    // Send a single data byte to the TFT screen
    inline void SendData(uint8_t Data) {
        m_dc.Write(true);  // Set DC pin to data mode
        m_spi.BlockingTransmit(&Data, 1); // Transmit the data
    }

    // --------------------------------------------------------------------------
    // Send a block of data to the TFT screen
    inline void SendData(uint8_t* buff, size_t size) {
        m_dc.Write(true);  // Set DC pin to data mode
        m_spi.BlockingTransmit(buff, size); // Transmit the data block
    }

    // --------------------------------------------------------------------------
    // Send a command using DMA (non-blocking)
    inline void SendDMACommand(uint8_t *cmd, SpiHandle::EndCallbackFunctionPtr end_callback = NULL, void* callback_context = NULL) {
        m_dc.Write(false);  // Set DC pin to command mode
        m_spi.DmaTransmit(cmd, 1, NULL, end_callback, callback_context); // Transmit command using DMA
    }

    // --------------------------------------------------------------------------
    // Send a block of data using DMA (non-blocking)
    inline void SendDMAData(uint8_t* buff, size_t size, SpiHandle::EndCallbackFunctionPtr end_callback = NULL, void* callback_context = NULL) {   
        m_dc.Write(true);  // Set DC pin to data mode
        m_spi.DmaTransmit(buff, size, NULL, end_callback, callback_context); // Transmit data block using DMA
    }

    // --------------------------------------------------------------------------
    // Delay in milliseconds
    inline void Delay(uint32_t msTime) {
        System::Delay(msTime); // Use system delay
    }

    // --------------------------------------------------------------------------
    // Control the Data/Command pin
    inline void setDC() {
        m_dc.Write(true);  // Set DC pin to high (data mode)
    }
    inline void resetDC() {
        m_dc.Write(false); // Set DC pin to low (command mode)
    }

    // --------------------------------------------------------------------------
    // Control the Reset pin
    inline void setRST() {
        m_reset.Write(true);  // Set Reset pin to high (inactive)
    }
    inline void resetRST() {
        m_reset.Write(false); // Set Reset pin to low (active)
    }

    protected :
    SpiHandle           m_spi;         // SPI handle for communication
    SpiHandle::Config   m_spi_config;  // SPI configuration

    GPIO                m_reset;       // GPIO for Reset pin
    GPIO                m_dc;          // GPIO for Data/Command pin
};
} // DadGFX

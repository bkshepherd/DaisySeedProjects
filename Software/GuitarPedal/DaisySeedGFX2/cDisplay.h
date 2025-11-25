//====================================================================================
// cDisplay
//  Management of an SPI-based display on an STM32 processor
//  Features:
//  - Transfers only modified blocks to optimize performance
//  - Uses a framebuffer and multiple layers for rendering
//  
// Copyright (c) 2025 Dad Design.
//====================================================================================
#pragma once 
#include <stdint.h>             // Standard integer types
#include <vector>               // STL vector for dynamic arrays
#include "TFT_SPI.h"            // SPI driver for the TFT display
#include "GFX.h"                // GFX library

namespace DadGFX {  

//***********************************************************************************
// Macro utility
//***********************************************************************************
#define DECLARE_DISPLAY(DisplayName)\
DadGFX::sFIFO_Data   DMA_BUFFER_MEM_SECTION  DisplayName##FIFO;									 /*FIFO pour émission SPI en DMA*/\
DadGFX::sColor       DSY_SDRAM_BSS           DisplayName##BlocFrame[BLOC_HEIGHT][BLOC_WIDTH];    /* Frame Blocs*/\
DadGFX::cDisplay 				 			 DisplayName;					                     /* Screen */

#define INIT_DISPLAY(DisplayName) DisplayName.init(&DisplayName##FIFO, &DisplayName##BlocFrame[0][0])

#define DECLARE_LAYER(LayerName, ValWidth, ValHeight) \
static DadGFX::sColor DSY_SDRAM_BSS __Layer##LayerName[ValWidth][ValHeight]; \
constexpr int Width##LayerName = ValWidth; \
constexpr int Height##LayerName = ValHeight;

#define ADD_LAYER(LayerName, PosX, PosY, PosZ) \
__Display.addLayer(&__Layer##LayerName[0][0], PosX, PosY, Width##LayerName, Height##LayerName, PosZ);

//***********************************************************************************
// Block size definition based on color depth
//***********************************************************************************
#if TFT_COLOR == 16
    #define TAILLE_BLOC (BLOC_WIDTH * BLOC_HEIGHT * 2)  // Block size for 16-bit color
#else
    #define TAILLE_BLOC (BLOC_WIDTH * BLOC_HEIGHT * 3)  // Block size for 24-bit color
#endif

class cDisplay;
class cLayer;

//***********************************************************************************
// Enumerations
//***********************************************************************************

// --------------------------------------------------------------------------
// Display orientation modes
enum class ORIENTATION {
    Portrait,      // Vertical orientation
    Landscape      // Horizontal orientation
};

// Layer write modes
enum class DRAW_MODE {
    Blend,       // Blend layers with transparency
    Overwrite    // Overwrite completely
};

//***********************************************************************************
// Cmd_CASET
//   SPI Command for column selection
//*********************************************************************************** 
class Cmd_CASET {
public:
    friend class cDisplay;

    // --------------------------------------------------------------------------
    // Constructor
    Cmd_CASET() {
        m_Commande = TFT_CASET;  // Initialize with the TFT_CASET command
    }

    // --------------------------------------------------------------------------
    // Define the horizontal window (column range)
    void setData(uint16_t x, uint16_t dx) {
        m_Data[0] = x >> 8;       // High byte of the starting column
        m_Data[1] = x & 0xFF;     // Low byte of the starting column
        m_Data[2] = dx >> 8;      // High byte of the ending column
        m_Data[3] = dx & 0xFF;    // Low byte of the ending column
    }

    // --------------------------------------------------------------------------
    // Class data
protected:
    uint8_t m_Commande;  // SPI command identifier
    uint8_t m_Data[4];   // Command data for the column range
};

//***********************************************************************************
// Cmd_RASET
//   SPI Command for row selection
//*********************************************************************************** 
class Cmd_RASET {
public:
    friend class cDisplay;

    // --------------------------------------------------------------------------
    // Constructor
    Cmd_RASET() {
        m_Commande = TFT_RASET;  // Initialize with the TFT_RASET command
    }

    // --------------------------------------------------------------------------
    // Define the vertical window (row range)
    void setData(uint16_t y, uint16_t dy) {
        m_Data[0] = y >> 8;       // High byte of the starting row
        m_Data[1] = y & 0xFF;     // Low byte of the starting row
        m_Data[2] = dy >> 8;      // High byte of the ending row
        m_Data[3] = dy & 0xFF;    // Low byte of the ending row
    }

    // --------------------------------------------------------------------------
    // Class data
protected:
    uint8_t m_Commande;  // SPI command identifier
    uint8_t m_Data[4];   // Command data for the row range
};

//***********************************************************************************
// Cmd_RAMWR
//   SPI Command for pixel data writing
//***********************************************************************************
class Cmd_RAMWR {
public:
    friend class cDisplay;

    // --------------------------------------------------------------------------
    // Constructor
    Cmd_RAMWR() {
        m_Commande = TFT_RAMWR;  // Initialize with the TFT_RAMWR command
    }

    // --------------------------------------------------------------------------
    // Get adresse of buffer data transfert
    inline uint8_t* getData(){
        return m_Data;
    }

    // --------------------------------------------------------------------------
    // Class data
protected:
    uint8_t m_Commande;           // SPI command identifier
    uint8_t m_Data[TAILLE_BLOC];  // Pixel data block to be transferred
};

//***********************************************************************************
// sFIFO_Data 
//   Stores a command and a pixel block to transmit to the display
//   For DMA operation, this structure must be instantiated in the 
//   DMA_BUFFER_MEM_SECTION memory
//*********************************************************************************** 

struct sFIFO_Data {
    Cmd_CASET m_CmdCASET[SIZE_FIFO];  // FIFO for column selection commands
    Cmd_RASET m_CmdRASET[SIZE_FIFO]; // FIFO for row selection commands
    Cmd_RAMWR m_CmdRAWWR[SIZE_FIFO]; // FIFO for pixel data write commands
};

class cLayerBase;
class cImageLayer;
//***********************************************************************************
// cDisplay
//  Display Manager Class
//***********************************************************************************
class cDisplay : protected TFT_SPI {
public:
friend class cLayer;
friend class cLayerBase;
friend class cImageLayer;
    // --------------------------------------------------------------------------
    // Constructor
    // Initializes the display manager, clears layers, and resets layer change flag
    cDisplay();
    
    // Destructor
    // Cleans up resources, disables DMA2D, and deletes allocated layers
    ~cDisplay();
    
    // --------------------------------------------------------------------------
    // Initialize the display manager
    // Configures layers, screen size, dirty blocks, and initializes hardware
    void init(sFIFO_Data *pFIFO_Data, sColor* pDitryBlocFrame);

    // --------------------------------------------------------------------------
    // Add a new layer to the display
    // Parameters:
    //   pLayerFrame: Framebuffer pointer
    //   x, y: Position of the layer
    //   Width, Height: Dimensions of the layer
    //   zPos: Z-order of the layer (stacking order)
    cLayer* addLayer(sColor* pLayerFrame, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint8_t zPos);
    
    // --------------------------------------------------------------------------
    // Add a new Image layer to the display
    // Parameters:
    //   pLayerFrame: Framebuffer pointer
    //   x, y: Position of the layer
    //   Width, Height: Dimensions of the layer
    //   zPos: Z-order of the layer (stacking order)
    cImageLayer* addLayer(const uint8_t * pLayerFrame, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint8_t zPos);
    
    // --------------------------------------------------------------------------
    // Set the screen's rotation and adjust dirty block configuration
    void setOrientation(Rotation r);
    
    // --------------------------------------------------------------------------
    // Flush all dirty blocks to the display
    void flush();

    // --------------------------------------------------------------------------
    // Get the width of the Display
    inline uint16_t getWith(){
        return m_Width;
    }

    // --------------------------------------------------------------------------
    // Get the height of the Display
    inline uint16_t getHeight(){
        return m_Height;
    }

protected :
    // -----------------------------------------------------------------------------
    // Blends two rectangular blocks of pixels using alpha compositing.
    // 
    // This function processes two pixel sources (`pSource1` and `pSource2`) over a 
    // specified width and height, blending them and storing the result in `pDest`.
    // Alpha transparency is managed, handling fully opaque, semi-transparent, and
    // transparent cases. Row padding for each buffer is accounted for using offsets.
    // 
    // Parameters:
    // - OutputOffset: Offset to skip at the end of each row in the destination buffer.
    // - InputOffset1: Offset to skip at the end of each row in the first source buffer.
    // - InputOffset2: Offset to skip at the end of each row in the second source buffer.
    // - pSource1: Pointer to the first source pixel data (foreground).
    // - pSource2: Pointer to the second source pixel data (background).
    // - pDest: Pointer to the destination pixel data.
    // - Width: Width of the block to process, in pixels.
    // - Height: Height of the block to process, in pixels.
    void Blend2Bloc( uint32_t OutputOffset, uint32_t  InputOffset1, uint32_t  InputOffset2, DadGFX::sColor* pSource1, DadGFX::sColor* pSource2, DadGFX::sColor* pDest, uint32_t Width,  uint32_t Height);

    // --------------------------------------------------------------------------
    // Mark layers as changed, requiring re-sorting
    inline void setLayersChange() {
        m_LayersChange = 1;
    }
    
    // --------------------------------------------------------------------------
    // Invalidate a rectangular region of the screen
    // Marks the corresponding dirty blocks for refresh
    void invalidateRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    
    // --------------------------------------------------------------------------
    // Invalidate a single point on the screen
    // Marks the corresponding dirty block for refresh
    inline void invalidatePoint(uint16_t x0, uint16_t y0) {
        m_DirtyBlocks[x0 / m_DitryBlocWidth][y0 / m_DitryBlocHeight] = 1;
    }
private :
    // --------------------------------------------------------------------------
    // Mark all blocks as dirty (require refresh)
    inline void invalidateAll() {
        memset(m_DirtyBlocks, 1, sizeof(m_DirtyBlocks));
    }

    // --------------------------------------------------------------------------
    // Validate all blocks (no refresh required)
    inline void validateAll() {
        memset(m_DirtyBlocks, 0, sizeof(m_DirtyBlocks));
    }

    // --------------------------------------------------------------------------
    // Adjust frame dimensions for the screen orientation
    void switchOrientation(ORIENTATION Orientation);

    // ==========================================================================
    // Management of transmission blocks

    // --------------------------------------------------------------------------
    // Check if the FIFO buffer is full
    // Returns true if the FIFO buffer can accept more blocks
    inline bool isFull() {
        // Protect m_FIFO_NbElements to avoid race conditions
        if (m_FIFO_NbElements >= SIZE_FIFO) {
            return false;  // FIFO is full
        }
        return true;  // FIFO is not full
    }

    // --------------------------------------------------------------------------
    // Check if a transmission is in progress
    // Returns true if the display is currently busy with a transmission
    inline bool isBusy() {
        return m_Busy;
    }

    // --------------------------------------------------------------------------
    // Add a block to the FIFO for transmission
    // Parameters:
    //   x, y: Coordinates of the block to add
    // Returns:
    //   true if the block was successfully added to the FIFO
    bool AddBloc(uint16_t x, uint16_t y);

    // --------------------------------------------------------------------------
    // Send the blocks currently in the FIFO using DMA
    // Returns:
    //   true if the transmission started successfully
    bool sendDMA();

    // --------------------------------------------------------------------------
    // Callbacks for SPI and DMA transmissions
    // These methods are called when specific SPI or DMA transmission steps complete
    static void sendCASETDMAData(void* context, daisy::SpiHandle::Result result);
    static void sendRASETDMACmd(void* context, daisy::SpiHandle::Result result);
    static void sendRASETDMAData(void* context, daisy::SpiHandle::Result result);
    static void sendRAWWRDMACmd(void* context, daisy::SpiHandle::Result result);
    static void sendRAWWRDMAData(void* context, daisy::SpiHandle::Result result);
    static void endDMA(void* context, daisy::SpiHandle::Result result);

    // --------------------------------------------------------------------------
    // Attributes for screen and layer management
    uint16_t m_Width;             // Width of the display
    uint16_t m_Height;            // Height of the display

    std::vector<cLayerBase*> m_TabLayers;  // List of active layers
    uint8_t m_LayersChange;       // Flag to indicate if layers need re-sorting

    ORIENTATION m_Orientation;   // Current screen orientation (Portrait/Landscape)

    // --------------------------------------------------------------------------
    // Dirty block management
    uint8_t m_DirtyBlocks[NB_BLOC_WIDTH][NB_BLOC_HEIGHT];  // Dirty block flags
    uint8_t m_DitryBlocWidth;       // Width of a dirty block in pixels
    uint8_t m_DitryBlocHeight;      // Height of a dirty block in pixels
    uint8_t m_NbDitryBlocX;         // Number of dirty blocks horizontally
    uint8_t m_NbDitryBlocY;         // Number of dirty blocks vertically
    sColor* m_pDitryBlocFrame;      // Temporary framebuffer for a dirty block

    // --------------------------------------------------------------------------
    // FIFO buffer for block transmission
    sFIFO_Data* m_pFIFO = nullptr;  // Pointer to the FIFO buffer
    uint16_t m_FIFO_in = 0;         // Index of the first free block (FIFO input)
    uint16_t m_FIFO_out = 0;        // Index of the next block to transmit (FIFO output)
    volatile uint16_t m_FIFO_NbElements = 0; // Number of elements currently in the FIFO
    volatile bool m_Busy = false;            // Flag to indicate if a transmission is ongoing
};

//***********************************************************************************
// cLayer Base
//  Manages a base graphical layer
//***********************************************************************************
class cLayerBase {
friend cDisplay;
public:
    cLayerBase(){}  // Constructor
    virtual ~cLayerBase() {}  // Destructor

    // --------------------------------------------------------------------------
    // Move the layer to a new position (x, y)
    DAD_GFX_ERROR moveLayer(uint16_t x, uint16_t y);

    // --------------------------------------------------------------------------
    // Change the Z-order of the layer
    inline void changeZOrder(uint8_t z){
        m_Z = z;  // Update Z-order
        m_pDisplay->setLayersChange();  // Notify the display about Z-order change
        m_pDisplay->invalidateRect(m_X, m_Y, m_X + m_Width-1, m_Y + m_Height-1);  // Invalidate the layer
    }

    // --------------------------------------------------------------------------
    // Get the X position of the layer
    inline uint16_t getX(){
        return m_X;
    }

    // --------------------------------------------------------------------------
    // Get the Y position of the layer
    inline uint16_t getY(){
        return m_Y;
    }

    // --------------------------------------------------------------------------
    // Get the Z-order of the layer
    inline uint16_t getZ(){
        return m_Z;
    }

    // --------------------------------------------------------------------------
    // Get the width of the layer
    inline uint16_t getWith(){
        return m_Width;
    }

    // --------------------------------------------------------------------------
    // Get the height of the layer
    inline uint16_t getHeight(){
        return m_Height;
    }
    
protected :
    // --------------------------------------------------------------------------
    // Get the pointer to the layer's frame buffer
    inline sColor* getFrame(){
        return m_pLayerFrame;
    }

    // --------------------------------------------------------------------------
    // 
    cDisplay*               m_pDisplay;     // Pointer to the display
    uint16_t                m_X;            // X position of the layer
    uint16_t                m_Y;            // Y position of the layer
    uint16_t                m_Width;        // Width of the layer
    uint16_t                m_Height;       // Height of the layer
    uint8_t                 m_Z;            // Z-order of the layer
    union {
        sColor*             m_pLayerFrame;
        const uint8_t*      m_pImageLayerFrame;
    };
};

//***********************************************************************************
// cLayer
//  Manages a graphical layer
//***********************************************************************************
class cLayer : public cLayerBase, public cGFX {
friend cDisplay;
public:
    cLayer(){}  // Constructor
    virtual ~cLayer() {}  // Destructor
   
    // -----------------------------------------------------------------------------
    // Erase the layer
    virtual DAD_GFX_ERROR eraseLayer(const sColor& Color = sColor(0,0,0,0));

    // -----------------------------------------------------------------------------
    // Set the layer write mode
    inline void setMode(DRAW_MODE Mode){
        m_Mode = Mode;    
    }

protected :
    // --------------------------------------------------------------------------
    // Initialize the layer with display, DMA2D handler, frame buffer, dimensions, and Z position
    void init(cDisplay* pDisplay, sColor* pLayerFrame, uint16_t y, uint16_t x, uint16_t Width, uint16_t Height, uint8_t zPos);

    // --------------------------------------------------------------------------
    // Set a pixel in the layer at (x, y) to the specified color
    virtual DAD_GFX_ERROR setPixel(uint16_t x, uint16_t y, const sColor& Color);

    // --------------------------------------------------------------------------
    // Draw a rectangle in the layer starting at (x, y) with specified width, height, and color
    virtual DAD_GFX_ERROR setRectangle(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, const sColor& Color);

    // -----------------------------------------------------------------------------
    // Fill a rectangle with either a foreground or background color based on a bitmap
    virtual DAD_GFX_ERROR fillRectWithBitmap(uint16_t x0, uint16_t y0, const uint8_t* pBitmap, uint16_t BitmapWidth, uint16_t BitmapBmpHeight,
                                             const sColor& ForegroundColor, const sColor& BackgroundColor);

protected :
    DRAW_MODE   m_Mode = DRAW_MODE::Blend;
};

//***********************************************************************************
// cImageLayer
//  Manages a graphical layer
//***********************************************************************************
class cImageLayer : public cLayerBase {
friend cDisplay;
public:
    cImageLayer(){}  // Constructor
    virtual ~cImageLayer() {}  // Destructor
   
protected :
    // --------------------------------------------------------------------------
    // Initialize the layer with display, DMA2D handler, frame buffer, dimensions, and Z position
    void init(cDisplay* pDisplay, const uint8_t* pLayerFrame, uint16_t y, uint16_t x, uint16_t Width, uint16_t Height, uint8_t zPos);

};

} // DadGFX
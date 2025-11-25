//====================================================================================
// cDisplay
//  Management of an SPI-based display on an STM32 processor
//  Features:
//  - Transfers only modified blocks to optimize performance
//  - Uses a framebuffer and multiple layers for rendering
//  
// Copyright (c) 2025 Dad Design.
//====================================================================================
#include "cDisplay.h"

namespace DadGFX {  

//***********************************************************************************
// cImageLayer
//  Manages a graphical layer
//***********************************************************************************

// --------------------------------------------------------------------------
// Initialize the layer with display, frame buffer, dimensions, and Z position
void cImageLayer::init(cDisplay* pDisplay, const uint8_t* pLayerFrame, uint16_t y, uint16_t x, uint16_t Width, uint16_t Height, uint8_t zPos){
    m_pDisplay = pDisplay;            // Pointer to display object
    m_Width = Width;                  // Layer width
    m_Height = Height;                // Layer height
    m_X = x;                          // X position of layer
    m_Y = y;                          // Y position of layer
    m_Z = zPos;                       // Z order of layer
    m_pImageLayerFrame = pLayerFrame;
}

//***********************************************************************************
// cLayer
//  Manages a graphical layer
//***********************************************************************************

// --------------------------------------------------------------------------
// Initialize the layer with display, frame buffer, dimensions, and Z position
void cLayer::init(cDisplay* pDisplay, sColor* pLayerFrame, uint16_t y, uint16_t x, uint16_t Width, uint16_t Height, uint8_t zPos){
    m_pDisplay = pDisplay;            // Pointer to display object
    m_pLayerFrame = pLayerFrame;      // Pointer to layer's ARGB frame buffer
    m_Width = Width;                  // Layer width
    m_Height = Height;                // Layer height
    m_X = x;                          // X position of layer
    m_Y = y;                          // Y position of layer
    m_Z = zPos;                       // Z order of layer
    memset((void *)pLayerFrame, 0, sizeof(sColor[Width*Height]));
}

// --------------------------------------------------------------------------
// Draw a rectangle in the layer starting at (x, y) with specified width, height, and color
DAD_GFX_ERROR cLayer::setRectangle(uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, const sColor& Color) {
    // ----------------------------------------------------------------------
    // Check bounds and validity of frame buffer
    if (!m_pLayerFrame || x >= m_Width || y >= m_Height) {
        return DAD_GFX_ERROR::Size_Error;  // Return error if frame buffer is invalid or rectangle is out of bounds
    }

    // Adjust width and height if rectangle exceeds boundaries
    if (x + Width > m_Width) {
        Width = m_Width - x;
    }
    if (y + Height > m_Height) {
        Height = m_Height - y;
    }

    // If the alpha component is zero, rectangle is fully transparent, nothing to draw
    if ((Color.m_A == 0) &&  (m_Mode == DRAW_MODE::Blend)) {
        return DAD_GFX_ERROR::OK;  // Operation successful but no modification
    }

    // ----------------------------------------------------------------------
    // Draw rectangle pixel by pixel
    sColor* pFrame;  // Pointer to current frame buffer pixel

    for (uint16_t indexY = 0; indexY < Height; indexY++) {
        // Calculate starting address of the row in the frame buffer
        pFrame = &m_pLayerFrame[((y + indexY) * m_Width) + x];

        for (uint16_t indexX = 0; indexX < Width; indexX++) {
            if ((Color.m_A == 255) | (m_Mode == DRAW_MODE::Overwrite)) {
                // Fully opaque color: overwrite the pixel
                pFrame->m_ARGB = Color.m_ARGB;
            } else {
                // Alpha blending for semi-transparent colors
                sColor Pixel;
                Pixel.m_ARGB = pFrame->m_ARGB;  // Get current pixel color

                // Normalize alpha values to [0, 1]
                float alpha2 = pFrame->m_A / 255.0f;  // Existing pixel alpha
                float alpha1 = Color.m_A / 255.0f;    // New color alpha

                // Compute the blended alpha value
                float outAlpha = alpha1 + alpha2 * (1 - alpha1);

                // Perform per-channel blending
                Pixel.m_R = static_cast<uint8_t>((Color.m_R * alpha1 + Pixel.m_R * alpha2 * (1 - alpha1)) / outAlpha);
                Pixel.m_G = static_cast<uint8_t>((Color.m_G * alpha1 + Pixel.m_G * alpha2 * (1 - alpha1)) / outAlpha);
                Pixel.m_B = static_cast<uint8_t>((Color.m_B * alpha1 + Pixel.m_B * alpha2 * (1 - alpha1)) / outAlpha);

                // Update alpha channel
                Pixel.m_A = static_cast<uint8_t>(outAlpha * 255);

                // Write blended color back to the frame buffer
                pFrame->m_ARGB = Pixel.m_ARGB;
            }

            // Move to the next pixel in the row
            pFrame++;
        }
    }

    // ----------------------------------------------------------------------
    // Invalidate the modified screen zone
    // Notify the display system of the updated region for redraw
    m_pDisplay->invalidateRect(m_X + x, m_Y + y, m_X + x + Width - 1, m_Y + y + Height - 1);

    // Operation successful
    return DAD_GFX_ERROR::OK;
}

// --------------------------------------------------------------------------
// Set a pixel in the layer at (x, y) to the specified color
DAD_GFX_ERROR cLayer::setPixel(uint16_t x, uint16_t y, const sColor& Color) {
    // ----------------------------------------------------------------------
    // Check bounds and validity of frame buffer
    if (x >= m_Width || y >= m_Height || !m_pLayerFrame) {
        return DAD_GFX_ERROR::Size_Error;  // Out of bounds or frame buffer is invalid
    }

    // ----------------------------------------------------------------------
    // Update the pixel color in the frame buffer
    sColor* pFrame = &m_pLayerFrame[(y * m_Width) + x];  // Calculate the address of the pixel

    if ((Color.m_A == 0) && (m_Mode == DRAW_MODE::Blend)) {
        // Fully transparent color, nothing to update
        return DAD_GFX_ERROR::OK;
    } else if ((Color.m_A == 255) | (m_Mode == DRAW_MODE::Overwrite)) {
        // Fully opaque color, directly overwrite the pixel
        pFrame->m_ARGB = Color.m_ARGB;
    } else {
        // Semi-transparent color, apply alpha blending
        sColor Pixel;
        Pixel.m_ARGB = pFrame->m_ARGB;  // Get the current pixel color

        // Normalize alpha values to [0, 1]
        float alpha2 = Pixel.m_A / 255.0f;  // Existing pixel alpha
        float alpha1 = Color.m_A / 255.0f;  // New color alpha

        // Compute the blended alpha value
        float outAlpha = alpha1 + alpha2 * (1 - alpha1);

        // Perform per-channel blending
        Pixel.m_R = static_cast<uint8_t>((Color.m_R * alpha1 + Pixel.m_R * alpha2 * (1 - alpha1)) / outAlpha);
        Pixel.m_G = static_cast<uint8_t>((Color.m_G * alpha1 + Pixel.m_G * alpha2 * (1 - alpha1)) / outAlpha);
        Pixel.m_B = static_cast<uint8_t>((Color.m_B * alpha1 + Pixel.m_B * alpha2 * (1 - alpha1)) / outAlpha);

        // Update the alpha channel
        Pixel.m_A = static_cast<uint8_t>(outAlpha * 255);

        // Write the blended color back to the frame buffer
        pFrame->m_ARGB = Pixel.m_ARGB;
    }

    // ----------------------------------------------------------------------
    // Invalidate the point to trigger a redraw in the display
    m_pDisplay->invalidatePoint(x + m_X, y + m_Y);

    return DAD_GFX_ERROR::OK;  // Operation successful
}

// -----------------------------------------------------------------------------
// Fill a rectangle with either a foreground or background color based on a bitmap
//
// This function fills a rectangle using a bitmap (1 bit per pixel, packed) to determine
// whether each pixel should take the foreground color (for bits set to 1) or the background
// color (for bits set to 0). It supports alpha blending for semi-transparent colors.
//
// Parameters:
// - x0, y0: Top-left corner of the rectangle.
// - pBitmap: Pointer to the bitmap data (1 bit per pixel, packed).
// - BitmapWidth, BitmapHeight: Dimensions of the bitmap in pixels.
// - ForegroundColor: The color used for bits set to 1 in the bitmap.
// - BackgroundColor: The color used for bits set to 0 in the bitmap.
// -----------------------------------------------------------------------------
DAD_GFX_ERROR cLayer::fillRectWithBitmap(
    uint16_t x0, uint16_t y0, 
    const uint8_t* pBitmap, uint16_t BitmapWidth, uint16_t BitmapHeight,
    const sColor& ForegroundColor, const sColor& BackgroundColor) {

    // -------------------------------------------------------------------------
    // Bounds check and framebuffer validation
    if (x0 >= m_Width || y0 >= m_Height || !m_pLayerFrame) {
        return DAD_GFX_ERROR::Size_Error;  // Out of bounds or invalid framebuffer
    }

    // Adjust BitmapWidth and BitmapHeight to ensure they fit within the display
    if (x0 + BitmapWidth > m_Width) {
        //BitmapWidth = m_Width - x0;
        return DAD_GFX_ERROR::Size_Error;  // Out of bounds 
    }
    if (y0 + BitmapHeight > m_Height) {
        //BitmapHeight = m_Height - y0;
        return DAD_GFX_ERROR::Size_Error;  // Out of bounds 
    }

    // -------------------------------------------------------------------------
    // Iterate through each row of the bitmap
    const uint8_t* pCurrentBitmap = pBitmap;  // Pointer to the current byte in the bitmap
    uint8_t currentByte = *pCurrentBitmap++;  // Load the first byte of the row
    uint8_t bitIndex = 0;  // Track the bit within the current byte
    
    for (uint16_t y = 0; y < BitmapHeight; ++y) {
        sColor* pFrame = &m_pLayerFrame[(y + y0) * m_Width + x0];  // Start of the row in the framebuffer

        // Iterate through each column of the bitmap
        for (uint16_t x = 0; x < BitmapWidth; ++x) {
            // Determine the pixel color based on the current bitmap bit
            const sColor& Color = (currentByte & 0x80) ? ForegroundColor : BackgroundColor;

            // Apply the color with optional alpha blending
            if ((Color.m_A != 0) || (m_Mode == DRAW_MODE::Overwrite)) {  // Skip fully transparent colors
                if ((Color.m_A == 255) | (m_Mode == DRAW_MODE::Overwrite)) {
                    // Fully opaque color, overwrite directly
                    pFrame->m_ARGB = Color.m_ARGB;
                } else {
                    // Perform alpha blending for semi-transparent colors
                    sColor Pixel;
                    Pixel.m_ARGB = pFrame->m_ARGB;  // Current pixel color in framebuffer

                    float alpha1 = Color.m_A / 255.0f;  // Foreground alpha
                    float alpha2 = Pixel.m_A / 255.0f;  // Background alpha
                    float outAlpha = alpha1 + alpha2 * (1 - alpha1);

                    // Blend each channel
                    Pixel.m_R = static_cast<uint8_t>((Color.m_R * alpha1 + Pixel.m_R * alpha2 * (1 - alpha1)) / outAlpha);
                    Pixel.m_G = static_cast<uint8_t>((Color.m_G * alpha1 + Pixel.m_G * alpha2 * (1 - alpha1)) / outAlpha);
                    Pixel.m_B = static_cast<uint8_t>((Color.m_B * alpha1 + Pixel.m_B * alpha2 * (1 - alpha1)) / outAlpha);
                    Pixel.m_A = static_cast<uint8_t>(outAlpha * 255);

                    pFrame->m_ARGB = Pixel.m_ARGB;  // Write the blended color back
                }
            }

            // Advance to the next pixel
            ++pFrame;
            ++bitIndex;

            // Move to the next byte in the bitmap if all bits in the current byte are used
            if (bitIndex > 7) {
                bitIndex = 0;
                currentByte = *pCurrentBitmap++;
            } else {
                currentByte <<= 1;  // Shift to the next bit
            }
        }
    }

    // -------------------------------------------------------------------------
    // Invalidate the region to trigger a redraw in the display
    m_pDisplay->invalidateRect(x0 + m_X, y0 + m_Y, x0 + m_X + BitmapWidth - 1, y0 + m_Y + BitmapHeight - 1);

    return DAD_GFX_ERROR::OK;  // Successful operation
}

// -----------------------------------------------------------------------------
// Erase the layer
DAD_GFX_ERROR cLayer::eraseLayer(const sColor& Color){
    DAD_GFX_ERROR Result;
    DRAW_MODE OldMode = m_Mode;
    setMode(DRAW_MODE::Overwrite);
    Result = setRectangle(0, 0, m_Width, m_Height, Color);
    setMode(OldMode);
    return Result;
}

// --------------------------------------------------------------------------
// Move the layer to a new position (x, y)
DAD_GFX_ERROR cLayerBase::moveLayer(uint16_t x, uint16_t y){
    // Check if the new position is within screen boundaries
    if(x >= m_pDisplay->getWith()) x = m_pDisplay->getWith()-1;
    if(y >= m_pDisplay->getHeight()) y = m_pDisplay->getHeight()-1;

    // Invalidate the current position to erase the layer
    m_pDisplay->invalidateRect(m_X, m_Y, m_X + m_Width -1, m_Y + m_Height -1);

    // Update layer position
    m_X = x;
    m_Y = y;

    // Invalidate the new position to redraw the layer
    m_pDisplay->invalidateRect(m_X, m_Y, m_X + m_Width-1, m_Y + m_Height-1);
    return DAD_GFX_ERROR::OK;  // Operation successful
}

//***********************************************************************************
// cDisplay
//  Display Manager Class
//***********************************************************************************

// --------------------------------------------------------------------------
// Constructor
// Initializes the display manager, clears layers, and resets layer change flag
cDisplay::cDisplay() {
    m_TabLayers.clear();   // Clear the vector of layers
    m_LayersChange = 0;    // Reset layer change flag
}

// Destructor
// Cleans up resources, disables DMA2D, and deletes allocated layers
cDisplay::~cDisplay() {
    for (auto& Layer : m_TabLayers) {
        delete Layer;  // Free memory for each layer
    }
    m_TabLayers.clear();  // Clear the layer list
    m_LayersChange = 0;   // Reset layer change flag
}

// --------------------------------------------------------------------------
// Initialize the display manager
// Configures layers, screen size, dirty blocks, and initializes hardware
void cDisplay::init(sFIFO_Data *pFIFO_Data, sColor* pDitryBlocFrame) {
    // Clear layers and reset layer change flag
    m_TabLayers.clear();
    m_LayersChange = 0;

    // Initialize screen dimensions
    m_Width = TFT_WIDTH;
    m_Height = TFT_HEIGHT;

    // Configure dirty block dimensions
    m_Orientation = ORIENTATION::Portrait;
    m_DitryBlocWidth = BLOC_WIDTH;
    m_DitryBlocHeight = BLOC_HEIGHT;
    m_NbDitryBlocX = NB_BLOC_WIDTH;
    m_NbDitryBlocY = NB_BLOC_HEIGHT;

    invalidateAll();  // Mark all blocks as valid (not dirty)

    // Initialize FIFO memory
    m_pFIFO  = pFIFO_Data;

    // Initialize DitryBlocFrame memory
    m_pDitryBlocFrame = pDitryBlocFrame;
    
    // Initialize SPI bus for the display
    Init_TFT_SPI();
}

// --------------------------------------------------------------------------
// Add a new layer to the display
// Parameters:
//   pLayerFrame: Framebuffer pointer
//   x, y: Position of the layer
//   Width, Height: Dimensions of the layer
//   zPos: Z-order of the layer (stacking order)
cLayer* cDisplay::addLayer(sColor* pLayerFrame, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint8_t zPos) {
    // Check if the new position is within screen boundaries
    //if(x >= m_Width) x = m_Width-1;
    //if(y >= m_Height) y = m_Height-1;

    cLayer* pNewLayer = new cLayer();
    if (!pNewLayer) {
        return pNewLayer;  // Return nullptr if memory allocation fails
    }
    pNewLayer->init(this, pLayerFrame, 0 , 0, Width, Height, zPos);
    m_TabLayers.push_back(static_cast<cLayerBase*>(pNewLayer));  // Add the layer to the list
    m_LayersChange = 1;                                          // Mark layers as changed
    pNewLayer->moveLayer(x,y);    
    return pNewLayer;
}

// --------------------------------------------------------------------------
// Add a new layer to the display
// Parameters:
//   pLayerFrame: Framebuffer pointer
//   x, y: Position of the layer
//   Width, Height: Dimensions of the layer
//   zPos: Z-order of the layer (stacking order)
cImageLayer* cDisplay::addLayer(const uint8_t* pLayerFrame, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint8_t zPos) {
    // Check if the new position is within screen boundaries
    if(x >= m_Width) x = m_Width-1;
    if(y >= m_Height) y = m_Height-1;

    cImageLayer* pNewLayer = new cImageLayer();
    if (!pNewLayer) {
        return pNewLayer;  // Return nullptr if memory allocation fails
    }
    pNewLayer->init(this, pLayerFrame, x, y, Width, Height, zPos);

    m_TabLayers.push_back(static_cast<cLayerBase*>(pNewLayer));  // Add the layer to the list
    m_LayersChange = 1;                // Mark layers as changed
    invalidateRect(x, y, x + Width-1, y + Height -1);
    return pNewLayer;
}

// --------------------------------------------------------------------------
// Invalidate a rectangular region of the screen
// Marks the corresponding dirty blocks for refresh
void cDisplay::invalidateRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    
    // Calculate the start and end column indices of the dirty blocks
    uint16_t startCol = x0 / m_DitryBlocWidth;
    uint16_t endCol = x1 / m_DitryBlocWidth;
    
    // Calculate the start and end row indices of the dirty blocks
    uint16_t startRow = y0 / m_DitryBlocHeight;
    uint16_t endRow = y1 / m_DitryBlocHeight;

    // Clamp the column indices to stay within the valid range
    if (startCol >= m_NbDitryBlocX) startCol = m_NbDitryBlocX - 1;
    if (endCol >= m_NbDitryBlocX) endCol = m_NbDitryBlocX - 1;
    
    // Clamp the row indices to stay within the valid range
    if (startRow >= m_NbDitryBlocY) startRow = m_NbDitryBlocY - 1;
    if (endRow >= m_NbDitryBlocY) endRow = m_NbDitryBlocY - 1;

    // Mark all blocks overlapping the rectangle as dirty
    for (uint16_t row = startRow; row <= endRow; ++row) {
        for (uint16_t col = startCol; col <= endCol; ++col) {
            m_DirtyBlocks[row][col] = 1;
        }
    }
}
    
// --------------------------------------------------------------------------
// Set the screen's rotation 
void cDisplay::setOrientation(Rotation r) {
    while (m_Busy == true) {
        Delay(1);  // Wait until the display is no longer busy
    }
    setTFTRotation(r);  // Apply the rotation to the display hardware
    switch (r) {
    case Rotation::Degre_0:   // Portrait
    case Rotation::Degre_180: // Inverted portrait
        switchOrientation(ORIENTATION::Portrait);
        invalidateAll();  // Mark all blocks as dirty
        break;

    case Rotation::Degre_90:  // Landscape
    case Rotation::Degre_270: // Inverted landscape
        switchOrientation(ORIENTATION::Landscape);
        invalidateAll();  // Mark all blocks as dirty
        break;
    }
}

// --------------------------------------------------------------------------
// Adjust frame dimensions for the screen orientation
void cDisplay::switchOrientation(ORIENTATION Orientation) {
    if (Orientation != m_Orientation) {
        uint16_t memHeight = m_Height;
        m_Height = m_Width;
        m_Width = memHeight;

        uint8_t Mem = m_DitryBlocHeight;
        m_DitryBlocHeight = m_DitryBlocWidth;
        m_DitryBlocWidth = Mem;
        
        Mem = m_NbDitryBlocX;
        m_NbDitryBlocX = m_NbDitryBlocY;
        m_NbDitryBlocY = Mem;
        m_Orientation = Orientation;
    }
}

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
void cDisplay::Blend2Bloc( uint32_t OutputOffset, uint32_t  InputOffset1, uint32_t  InputOffset2, DadGFX::sColor* pSource1, DadGFX::sColor* pSource2, DadGFX::sColor* pDest, uint32_t Width,  uint32_t Height){
    
  // Loop through each pixels 
  for (uint16_t indexY = 0; indexY < Height; indexY++) {
    for (uint16_t indexX = 0; indexX < Width; indexX++) {
      // Check if the first source pixel has non-zero alpha
      if (pSource1->m_A != 0) {
        
        // If the alpha of the first source pixel is fully opaque
        if (pSource1->m_A == 255) {
          // Copy the RGB and alpha values from source1 directly to the destination
          pDest->m_R = pSource1->m_R;
          pDest->m_G = pSource1->m_G;
          pDest->m_B = pSource1->m_B;
          pDest->m_A = 255;
        
        } else {
          // Perform alpha blending when the first source pixel is semi-transparent
          float alpha2 = pSource2->m_A / 255.0f; // Normalize alpha of source2
          float alpha1 = pSource1->m_A / 255.0f; // Normalize alpha of source1
          float outAlpha = alpha1 + alpha2 * (1 - alpha1); // Compute blended alpha

          // Compute the blended RGB values using the alpha values
          pDest->m_R = static_cast<uint8_t>((pSource1->m_R * alpha1 + pSource2->m_R * alpha2 * (1 - alpha1)) / outAlpha);
          pDest->m_G = static_cast<uint8_t>((pSource1->m_G * alpha1 + pSource2->m_G * alpha2 * (1 - alpha1)) / outAlpha);
          pDest->m_B = static_cast<uint8_t>((pSource1->m_B * alpha1 + pSource2->m_B * alpha2 * (1 - alpha1)) / outAlpha);
          pDest->m_A = static_cast<uint8_t>(outAlpha * 255); // Convert alpha back to 0-255 range
        }
      }
      // Move to the next pixel in the row for both sources and the destination
      pSource1++;
      pSource2++;
      pDest++;
    }
    // Skip the padding at the end of each row for source1, source2, and destination
    pSource1 += InputOffset1;
    pSource2 += InputOffset2;
    pDest += OutputOffset;
  }   
}

// --------------------------------------------------------------------------
// Flush all dirty blocks to the display
void cDisplay::flush() {
    // Sort layers by Z-order if they have changed
    if (m_LayersChange == 1) {
        std::stable_sort(m_TabLayers.begin(), m_TabLayers.end(), [](cLayerBase* a, cLayerBase* b) {
            return a->getZ() < b->getZ();
        });
        m_LayersChange = 0;  // Reset the flag
    }

    // Update dirty blocks
    for (uint8_t yIndexBloc = 0; yIndexBloc < m_NbDitryBlocY; yIndexBloc++) {
        for (uint8_t xIndexBloc = 0; xIndexBloc < m_NbDitryBlocX; xIndexBloc++) {
            // Check if the block is marked as dirty (needs updating)
            if (m_DirtyBlocks[yIndexBloc][xIndexBloc] == 1) {
                m_DirtyBlocks[yIndexBloc][xIndexBloc] = 0; // Mark block as clean

                // Calculate X and Y positions of the block in the screen
                uint16_t blocX = xIndexBloc * m_DitryBlocWidth;
                uint16_t blocY = yIndexBloc * m_DitryBlocHeight;

                // Clear the memory used for the dirty block frame before updating
                memset((void*)m_pDitryBlocFrame, 0x00, sizeof(sColor[BLOC_WIDTH][BLOC_HEIGHT]));

                // Iterate over the layers to blend them into the dirty block
                for (auto& layer : m_TabLayers) {
                   if (layer->getZ() == 0) continue; // Skip layers with Z = 0 (invisible layers)

                    // Get the position and dimensions of the layer
                    uint16_t layerX = layer->getX();
                    uint16_t layerY = layer->getY();
                    uint16_t layerWidth = layer->getWith();
                    uint16_t layerHeight = layer->getHeight();

                    // Calculate the intersection between the block and the layer
                    uint16_t intersectX = std::max(blocX, layerX);
                    uint16_t intersectY = std::max(blocY, layerY);
                    int16_t intersectWidth = std::min(blocX + m_DitryBlocWidth, layerX + layerWidth) - intersectX;
                    int16_t intersectHeight = std::min(blocY + m_DitryBlocHeight, layerY + layerHeight) - intersectY;

                    // Skip if there is no intersection
                    if ((intersectWidth > 0) && (intersectHeight > 0)) {
                        // Calculate offsets for the block and the layer
                        uint16_t offsetX = intersectX - blocX;
                        uint16_t offsetY = intersectY - blocY;
                        uint16_t layerOffsetX = intersectX - layerX;
                        uint16_t layerOffsetY = intersectY - layerY;

                        // Start the blending operation
                        sColor* pSource = &(layer->getFrame()[(layerOffsetY * layer->getWith()) + layerOffsetX]);
                        sColor* pDest = &m_pDitryBlocFrame[(offsetY * m_DitryBlocWidth) + offsetX];
                        Blend2Bloc(
                            m_DitryBlocWidth - intersectWidth,
                            layerWidth - intersectWidth,
                            m_DitryBlocWidth - intersectWidth,
                            pSource,
                            pDest,
                            pDest,
                            intersectWidth,
                            intersectHeight
                        );
                    }
                }
                // Add the processed block to the FIFO for transmission
                while (AddBloc(blocX, blocY) == false) {
                    System::Delay(1); // Wait if the FIFO is full
                }
                sendDMA(); // Transmit the block
            }
        }
    }
}

// --------------------------------------------------------------------------
// Add a block to the FIFO for transmission
// This function prepares a display block (defined by its coordinates) for 
// transmission to the screen and places it in the FIFO queue.
//
// Parameters:
//   x, y: Coordinates of the top-left corner of the block to be added
// Returns:
//   true if the block was successfully added to the FIFO, false otherwise
bool cDisplay::AddBloc(uint16_t x, uint16_t y) {
    // Disable interrupts to safely access shared resources
    __disable_irq();

    // Check if the FIFO is full; if so, re-enable interrupts and return false
    if (m_FIFO_NbElements >= SIZE_FIFO) {
        __enable_irq();
        return false;
    }

    // Re-enable interrupts once the safety check is complete
    __enable_irq();

    // Calculate the bottom-right coordinates of the block
    uint16_t dx = x + m_DitryBlocWidth - 1;
    uint16_t dy = y + m_DitryBlocHeight - 1;

    // Configure the necessary commands to define and transmit the block
    // CASET: Sets the column range (x to dx) for the block
    m_pFIFO->m_CmdCASET[m_FIFO_in].setData(x, dx);

    // RASET: Sets the row range (y to dy) for the block
    m_pFIFO->m_CmdRASET[m_FIFO_in].setData(y, dy);

    // RAWWR: Prepares the block's raw data for transfer
    sColor *pFrame;
    uint8_t *pBloc = m_pFIFO->m_CmdRAWWR[m_FIFO_in].getData();


    for(uint16_t IndexY=0; IndexY < m_DitryBlocHeight; IndexY++){
        pFrame = &m_pDitryBlocFrame[IndexY*m_DitryBlocWidth];
        for(uint16_t IndexX=0; IndexX < m_DitryBlocWidth; IndexX ++){
#if TFT_COLOR == 16
                *pBloc++ = (pFrame->m_R & 0xF8) | (pFrame->m_G >> 5 );
                *pBloc++ = (pFrame->m_B >> 3) | ((pFrame->m_G << 3 )  & 0xE0);
#else
                *pBloc++ = pFrame->m_R();
                *pBloc++ = pFrame->m_G();
                *pBloc++ = pFrame->m_B();
#endif
        pFrame++;
        }
    }    

    // Increment the FIFO input index, wrapping around if necessary
    m_FIFO_in += 1;
    if (m_FIFO_in >= SIZE_FIFO) {
        m_FIFO_in = 0;
    }

    // Increment the count of elements in the FIFO
    m_FIFO_NbElements += 1;

    // Return true to indicate successful addition to the FIFO
    return true;
}

// --------------------------------------------------------------------------
// Send the blocks currently in the FIFO using DMA
// This function initiates the DMA transmission process for blocks in the FIFO. 
// The DMA controller handles the transfer of display data to the screen.
//
// Returns:
//   true if the transmission started successfully, false otherwise
bool cDisplay::sendDMA() {
    // Disable interrupts to safely check and modify shared resources
    __disable_irq();

    // If the FIFO is empty or a transmission is already in progress, re-enable
    // interrupts and return false
    if ((m_FIFO_NbElements == 0) || (m_Busy == true)) {
        __enable_irq();
        return false;
    }

    // Re-enable interrupts after the safety check
    __enable_irq();

    // Mark the display as busy to indicate that a transmission is in progress
    m_Busy = true;

    // Initiate the transmission of the first block in the FIFO
    // CASET (Column Address Set) command is sent first
    // - The callback function `sendCASETDMAData` will handle subsequent steps
    // - `this` provides context to the callback for handling other DMA stages
    SendDMACommand(&m_pFIFO->m_CmdCASET[m_FIFO_out].m_Commande, 
                   cDisplay::sendCASETDMAData, 
                   this);

    // Return true to indicate that transmission was successfully started
    return true;
}

// --------------------------------------------------------------------------
// Handles the completion of the CASET (Column Address Set) DMA command
// This method initiates the transfer of CASET data.
//
// Parameters:
//   context: Pointer to the cDisplay instance (used for accessing class members)
//   result:  Result of the DMA operation (success/failure)
void cDisplay::sendCASETDMAData(void* context, daisy::SpiHandle::Result result) {
    cDisplay *pthis = (cDisplay *)context;  // Retrieve the cDisplay instance
    // Start transferring the CASET data and set the next callback to sendRASETDMACmd
    pthis->SendDMAData(pthis->m_pFIFO->m_CmdCASET[pthis->m_FIFO_out].m_Data, 4, cDisplay::sendRASETDMACmd, context);
}

// --------------------------------------------------------------------------
// Handles the completion of the RASET (Row Address Set) DMA command
// This method initiates the setup for sending RASET data.
//
// Parameters:
//   context: Pointer to the cDisplay instance
//   result:  Result of the DMA operation
void cDisplay::sendRASETDMACmd(void* context, daisy::SpiHandle::Result result) {
    cDisplay *pthis = (cDisplay *)context;  // Retrieve the cDisplay instance
    // Initiate the DMA transfer of the RASET command and set the next callback to sendRASETDMAData
    pthis->SendDMACommand(&pthis->m_pFIFO->m_CmdRASET[pthis->m_FIFO_out].m_Commande, cDisplay::sendRASETDMAData, context);
}

// --------------------------------------------------------------------------
// Handles the completion of the RASET data transfer
// This method prepares the next DMA operation for the RAWWR (Write Memory) command.
//
// Parameters:
//   context: Pointer to the cDisplay instance
//   result:  Result of the DMA operation
void cDisplay::sendRASETDMAData(void* context, daisy::SpiHandle::Result result) {
    cDisplay *pthis = (cDisplay *)context;  // Retrieve the cDisplay instance
    // Start transferring the RASET data and set the next callback to sendRAWWRDMACmd
    pthis->SendDMAData(pthis->m_pFIFO->m_CmdRASET[pthis->m_FIFO_out].m_Data, 4, cDisplay::sendRAWWRDMACmd, context);
}

// --------------------------------------------------------------------------
// Handles the completion of the RAWWR (Write Memory) DMA command
// This method starts the transfer of RAWWR data (the pixel data for the block).
//
// Parameters:
//   context: Pointer to the cDisplay instance
//   result:  Result of the DMA operation
void cDisplay::sendRAWWRDMACmd(void* context, daisy::SpiHandle::Result result) {
    cDisplay *pthis = (cDisplay *)context;  // Retrieve the cDisplay instance
    // Start transferring the RAWWR data and set the next callback to sendRAWWRDMAData
    pthis->SendDMACommand(&pthis->m_pFIFO->m_CmdRAWWR[pthis->m_FIFO_out].m_Commande, cDisplay::sendRAWWRDMAData, context);
}

// --------------------------------------------------------------------------
// Handles the completion of the RAWWR data transfer
// This method processes the block's pixel data and triggers the next step.
//
// Parameters:
//   context: Pointer to the cDisplay instance
//   result:  Result of the DMA operation
void cDisplay::sendRAWWRDMAData(void* context, daisy::SpiHandle::Result result) {
    cDisplay *pthis = (cDisplay *)context;  // Retrieve the cDisplay instance
    // Transfer the pixel data and set the next callback to endDMA
    pthis->SendDMAData(pthis->m_pFIFO->m_CmdRAWWR[pthis->m_FIFO_out].m_Data, TAILLE_BLOC, cDisplay::endDMA, context);
}

// --------------------------------------------------------------------------
// Finalizes the transmission of the current block
// This method removes the transmitted block from the FIFO and starts the 
// next block's transmission if the FIFO is not empty.
//
// Parameters:
//   context: Pointer to the cDisplay instance
//   result:  Result of the DMA operation
void cDisplay::endDMA(void* context, daisy::SpiHandle::Result result) {
    cDisplay *pthis = (cDisplay *)context;  // Retrieve the cDisplay instance
    
    // Move to the next block in the FIFO
    pthis->m_FIFO_out++;
    if (pthis->m_FIFO_out >= SIZE_FIFO) {
        pthis->m_FIFO_out = 0;  // Wrap the index around if it exceeds the FIFO size
    }
    pthis->m_FIFO_NbElements -= 1;  // Decrease the number of elements in the FIFO

    // If there are more blocks in the FIFO, start the transmission of the next block
    if (pthis->m_FIFO_NbElements != 0) {
        // Start the next block's transmission with the CASET command
        pthis->SendDMACommand(&pthis->m_pFIFO->m_CmdCASET[pthis->m_FIFO_out].m_Commande, cDisplay::sendCASETDMAData, context);
    } else {
        // If the FIFO is empty, mark the display as no longer busy
        pthis->m_Busy = false;
    }
}

} // DadGFX
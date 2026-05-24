#pragma once

#include <Arduino.h>
#include <ssd1306xled.h>

// enable collision checks for the player sprite
// without collision checks we don't need a mask buffer
#define _ENABLE_COLLISION_CHECKS_

// enable support for inverted sprites (24 bytes of flash)
#define _ENABLE_SPRITE_INVERSION_SUPPORT_

// enable support for horizontal flipping the sprites (32 bytes of flash)
#define _ENABLE_SPRITE_HFLIP_SUPPORT_

// for performance testing without i2c transfers
//#define _NO_DATA_TRANSFER_

// sprite header resides in flash [3 bytes per sprite]
struct SSD1306_SPRITE_HEADER
{
  uint8_t width;
  int8_t  heightInPages;
  int8_t  spriteFlags;
  // the sprite data is following the header
  // ....
};

// management structure is 6 bytes of RAM per sprite 
struct SSD1306_SPRITE
{
  int16_t x;
  int8_t  y;
  int8_t  frameAndFlags; // [0..3] frame number
                         // [4]    horizontal flip
                         // [5]    vertical flip
                         // [6]    invert sprite (only when mask is available)
                         // [7]    undraw - restore background/clear area
  const uint8_t *header;
};

enum SSD1306_SPRITE_FLAGS
{
  hFlip  = 0x10,
  vFlip  = 0x20,
  invert = 0x40,
  undraw = 0x80,
};

const uint8_t spriteFrameMask = 0x0f;

/////////////////////////////////////////////////////////////////////////////
// Faster sprites routine using a configurable RAM buffer.
// The idea is to use three buffers (background, collision, dirty) of the same size
// and clip the sprites to the appropriate buffer regions.
// A buffer size of 32 bytes per buffer is a good choice to start with,
// below 8 bytes per buffer performance starts to degrade fast.
// The work buffer size is required to be an integer divisor of the screen width!
bool ssd1306_draw_sprites_px( uint8_t *workBuffer, const uint8_t workBufferSize,
                              SSD1306_SPRITE *spriteList, const uint8_t maxSprites,
                            #ifdef _ENABLE_COLLISION_CHECKS_  
                              const uint8_t playerSprite /*= 255*/,
                            #endif
                              const uint8_t *background /*= nullptr*/,
                              const uint8_t screenStartX /*= 0*/, const uint8_t screenWidth /*= 128*/,
                              const uint8_t screenStartPage /*= 0*/, const uint8_t screenEndPage /*= 7*/ )
{
  // assign buffers
#if defined( _ENABLE_COLLISION_CHECKS_ )
  const uint8_t bufferSize = workBufferSize / 3; // TODO: not happy with division here
#else
  const uint8_t bufferSize = workBufferSize / 2;
#endif
  uint8_t *buffer = workBuffer;
  uint8_t *dirtyFlags = workBuffer + bufferSize;
#if defined( _ENABLE_COLLISION_CHECKS_ )
  uint8_t *collisionBuffer = dirtyFlags + bufferSize;
#endif

  // no collision yet
  bool collision = false;

  // calculate last valid column
  const int16_t screenEndPosX = screenStartX + screenWidth - 1;

  for ( int8_t page = screenStartPage; page <= screenEndPage; page++ )
  {
    // split the screen width in chunks of <bufferSize>
    for ( int16_t x_chunk = screenStartX; x_chunk <= screenEndPosX; x_chunk += bufferSize )
    {
      // initialize all buffers with background or empty
      if ( background ) { memcpy_P( buffer, background + x_chunk - screenStartX, bufferSize ); }
      else { memset( buffer, 0, bufferSize ); }
      // no data written yet
      memset( dirtyFlags, 0, bufferSize );
    #if defined( _ENABLE_COLLISION_CHECKS_ )
      // no collisions yet
      memset( collisionBuffer, 0, bufferSize );
    #endif

      const int16_t x_chunk_end = x_chunk + bufferSize;

      // iterate through all sprites
      for ( uint8_t n = 0; n < maxSprites; n++ )
      {
        // for readability
        SSD1306_SPRITE *sprite = &spriteList[n];
        int16_t _spriteStartX = sprite->x;

        // is the sprite visible in this buffer (check chunk end first)?
        if ( _spriteStartX < x_chunk_end )
        {
          // read sprite width from flash
          uint8_t spriteWidth = pgm_read_byte( sprite->header + 0 );

          // is the sprite visible in this buffer (check start position)?
          if ( _spriteStartX + spriteWidth >= x_chunk )
          {
            const int8_t spriteHeightInPages = pgm_read_byte( sprite->header + 1 );
            const int8_t spriteVerticalOffset = sprite->y & 0x07;

            const int8_t spriteFlags = pgm_read_byte( sprite->header + 2 );
            const bool useMask = ( spriteFlags < 0 );

            // calculate start and end page
            const int8_t startPage = sprite->y >> 3;

            // is the sprite visible on this page?
            if ( page >= startPage )
            {
              int8_t endPage = startPage + spriteHeightInPages;
              if ( !spriteVerticalOffset ) { endPage--; }

              if ( page <= endPage )
              {
                // calculate offset to next sprite data row
                uint8_t spriteLineOffset = spriteWidth;
                uint16_t bitmapSize = spriteLineOffset * uint8_t( spriteHeightInPages );

                uint16_t bitmapOffset = 0;
                // clip left
                if ( _spriteStartX < x_chunk )
                {
                  bitmapOffset = x_chunk - _spriteStartX;
                  spriteWidth -= bitmapOffset;
                  _spriteStartX = x_chunk;
                }
                // clip right
                if ( _spriteStartX + spriteWidth >= x_chunk_end )
                {
                  spriteWidth = x_chunk_end - _spriteStartX;
                }

                // normalize sprite x position
                _spriteStartX -= x_chunk;

                // now x is >= 0 and < bufferSize -> uint8_t is good enough
                const uint8_t spriteStartX = uint8_t( _spriteStartX );
                const uint8_t spriteEndX = spriteStartX + spriteWidth;

                // should the sprite be drawn?
                if ( !( sprite->frameAndFlags & SSD1306_SPRITE_FLAGS::undraw ) )
                {
                  // add frame offset
                  bitmapOffset += ( sprite->frameAndFlags & spriteFrameMask ) * bitmapSize;

                  // calculate bitmap data address
                  uint16_t addr = bitmapOffset;
                #ifdef _ENABLE_SPRITE_HFLIP_SUPPORT_
                  // horizontal flip?
                  bool hFlip = ( sprite->frameAndFlags & SSD1306_SPRITE_FLAGS::hFlip );
                  if ( hFlip )
                  {
                    // flip to the other side of the sprite
                    addr = spriteLineOffset - 1 - bitmapOffset;
                  }
                #endif
                  if ( useMask ) { spriteLineOffset <<= 1;
                                   addr <<= 1; }
                  addr += uint16_t( spriteList[n].header + sizeof( SSD1306_SPRITE_HEADER ) + uint8_t( page - startPage ) * spriteLineOffset );

                  for ( uint8_t x = 0; x < uint8_t( spriteWidth ); x++ )
                  {
                    uint8_t maskValue = 0;;
                    uint8_t pixels = 0;

                    if ( spriteVerticalOffset == 0 )
                    {
                      if ( useMask )
                      {
                        maskValue = pgm_read_byte( addr + 1 ); 
                      }
                      pixels = pgm_read_byte( addr );
                    }
                    else
                    {
                      if ( page < endPage )
                      {
                        if ( useMask )
                        {
                          maskValue = pgm_read_byte( addr + 1 ) << spriteVerticalOffset;
                        }
                        pixels = pgm_read_byte( addr ) << spriteVerticalOffset;
                      }
                      if ( page > startPage )
                      {
                        // look one row up
                        if ( useMask )
                        {
                          maskValue |= pgm_read_byte( addr - spriteLineOffset + 1 ) >> ( 8 - spriteVerticalOffset );
                        }
                        pixels |= pgm_read_byte( addr - spriteLineOffset ) >> ( 8 - spriteVerticalOffset );
                      }
                    }

                  #ifdef _ENABLE_COLLISION_CHECKS_
                    // check for collision with player sprite(s)
                    if ( n >= playerSprite )
                    {
                      //if ( collisionBuffer[spriteStartX + x] & maskValue ) { collision = true; }
                      if ( collisionBuffer[spriteStartX + x] & pixels ) { collision = true; }
                    }
                    // add pixel data to collision buffer?
                    if ( spriteFlags & SpriteFlag::solid )
                    {
                      //collisionBuffer[spriteStartX + x] |= maskValue;
                      collisionBuffer[spriteStartX + x] |= pixels;
                    }
                  #endif
                    if ( useMask )
                    {
                      // remove background under the sprite
                      buffer[spriteStartX + x] &= ~maskValue;
                    #ifdef _ENABLE_SPRITE_HFLIP_SUPPORT_  
                      if ( hFlip ) { addr--; }
                      else 
                    #endif
                      { addr++; }
                    }
                  #ifdef _ENABLE_SPRITE_INVERSION_SUPPORT_
                    if ( sprite->frameAndFlags & SSD1306_SPRITE_FLAGS::invert )
                    {
                      pixels = ~pixels & maskValue;
                    }
                  #endif
                    buffer[spriteStartX + x] |= pixels;
                  #ifdef _ENABLE_SPRITE_HFLIP_SUPPORT_  
                    if ( hFlip ) { addr--; }
                    else 
                  #endif
                    { addr++; }
                  }
                }

                // mark the sprite position as "dirty", so the background will be sent to the display
                for ( uint8_t x = uint8_t( spriteStartX ); x < spriteEndX; x++ ) { dirtyFlags[x] = true; }

              }
            } // sprite is on page

          }
        } // sprite is in buffer

      } // for n

      // transfer all dirty data to the display
      bool transferActive = false;
      
      for ( uint8_t x = 0; x < bufferSize; x++ )
      {
        // is this byte dirty?
        if ( dirtyFlags[x] )
        {
          // transfer already active?
          if ( !transferActive )
          {
            // start a new transfer
            transferActive = true;
          #ifndef _NO_DATA_TRANSFER_
            SSD1306.ssd1306_setpos( x + x_chunk, page );
            SSD1306.ssd1306_send_data_start();
          #endif
          }
        #ifndef _NO_DATA_TRANSFER_
          SSD1306.ssd1306_send_byte( buffer[x] );
        #endif
        }
        // was there an active transfer?
        else if ( transferActive )
        {
          // stop transfer
          transferActive = false;
        #ifndef _NO_DATA_TRANSFER_
          SSD1306.ssd1306_send_data_stop();
        #endif
        }
      }
      // is a transfer still active?
      if ( transferActive )
      {
      #ifndef _NO_DATA_TRANSFER_
        // stop transfer
        SSD1306.ssd1306_send_data_stop();
      #endif
      }

    } // for x_chunk

    if ( background )
    { 
      // next background row
      background += screenWidth;
    }
  } // for page

  return( collision );
}
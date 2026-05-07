#include <Arduino.h>
#include <font6x8.h>
#include <font8x16.h>
#include <ssd1306xled.h>
#include "Sprites.h"

// enable this to use the second byte of the sprite data as a mask
// (for collision detection and better sprite rendering)
// The interleaved bitmap/mask format is compatible with Arduboy sprites
#define _ENABLE_MASK_

// enable collision checks for the player sprite
// without collision checks we don't need a mask buffer
#ifdef _ENABLE_MASK_
  #define _ENABLE_COLLISION_CHECKS_
#endif

// calculate frame times and show max. frame time on the screen
// checks and displays also collision of player sprite with other sprites
#define _ENABLE_DIAGNOSTICS_

struct SSD1306_SPRITE_HEADER
{
  int8_t  width;          // for easier clipping, limits sprite width to 127 px ;)
  uint8_t heightInPages;
};

// management structure is 6 bytes of RAM per sprite 
struct SSD1306_SPRITE
{
  int16_t x;
  int8_t  y;
  int8_t  frame;
  const uint8_t *header;
  // the sprite data is following the header
  // ....
};

constexpr int8_t SPRITE_DELETE = -1;

// global RAM buffer
constexpr uint8_t _spriteBufferSize = 64;
#ifdef _ENABLE_COLLISION_CHECKS_
  constexpr uint8_t _workBufferSize = 3 * _spriteBufferSize;
#else
  constexpr uint8_t _workBufferSize = 2 * _spriteBufferSize;
#endif
uint8_t _workBuffer[_workBufferSize];

bool ssd1306_draw_sprites_px( uint8_t *workBuffer, const uint8_t workBufferSize,
                              SSD1306_SPRITE *spriteList, const uint8_t maxSprites,
                            #ifdef _ENABLE_COLLISION_CHECKS_  
                              const uint8_t playerSprite = 255, 
                            #endif
                              const uint8_t *background = nullptr, 
                            #ifdef _ENABLE_MASK_
                              const bool useMask = false,
                            #endif
                              const uint8_t screenStartX = 0, const uint8_t screenWidth = 128,
                              const uint8_t screenStartPage = 0, const uint8_t screenEndPage = 7 );

char str[10];

SSD1306_SPRITE _spriteList[] = { 
#ifdef _ENABLE_MASK_
  { 100, -4, 0, meteor_w_mask_16x16 },
  {  54, 43, 1, meteor_w_mask_16x16 },
  {  74, 19, 0, moon_w_mask_30x32 },
  { 140, 28, 3, meteor_w_mask_16x16 },
  { 114, 38, 0, moon_w_mask_30x32 },
  {  90, 52, 0, meteor_w_mask_16x16 },
  {  67, 32, 2, meteor_w_mask_16x16 },
  {  10,  0, 0, ship_w_mask_18x16 },
#else
  {  60, 32, 0, meteor_16x16 },
  { 104, 10, 0, meteor_16x16 },
  {  54, 43, 0, meteor_16x16 },
  {  84, 27, 0, moon_30x32 },
  {  94, 58, 0, moon_30x32 },
  { 127, 28, 0, meteor_16x16 },
  { 118, 52, 0, meteor_16x16 },
  {  20,  1, 0, ship_16x16 },
#endif
  {},
  {},
  {},
  {},
  {},
  {},
  {},
  {},
};

void setup() {
  _delay_ms(100);
  SSD1306.ssd1306_init();
}

void loop() {

  for ( uint8_t run = 0; run <= 1; run++ )
  {
    // -- Sprites ---
    SSD1306.ssd1306_fillscreen(0);
    SSD1306.ssd1306_setpos(0, 0);
    SSD1306.ssd1306_string_font6x8( !run ? "Sprites" : "Sprites /w background" );
    _delay_ms(1500);

    SSD1306.ssd1306_fillscreen(0);

    const uint8_t *background = !run ? nullptr : Moon128x64;

  #ifdef _ENABLE_DIAGNOSTICS_
    int16_t maxFrameTime = 0;
  #endif

    if ( background ) { SSD1306.ssd1306_draw_bmp( 0, 0, 128, 8, background ); }

    uint8_t spriteCount = sizeof( _spriteList ) / sizeof( _spriteList[0] ) / 2;

    for ( uint8_t step = 0; step < 128; step++ )
    {
      // copy all active sprites to the second half and mark them for clearing
      memcpy( &_spriteList[spriteCount], _spriteList, sizeof( _spriteList ) >> 1 );
      for ( uint8_t n = spriteCount; n < spriteCount << 1; n++ )
      {
        _spriteList[n].frame = SPRITE_DELETE;
      }

      // move spaceship down and up again
      _spriteList[spriteCount - 1].y = ( step < 64 ) ? step : 128 - step;

      // move moons and asteroids
      for ( uint8_t n = 0; n < spriteCount - 1; n++ )
      {
        if ( !run )
        { 
          _spriteList[n].x -= ( n >> 2 ) + 1;
          if ( _spriteList[n].header == meteor_w_mask_16x16 )
          {
            if ( ( ( step + n ) & 0xf ) == 0 )
            {
              _spriteList[n].frame ++;
              _spriteList[n].frame &= 0x03;
            }
          }
        }
        else
        { 
          _spriteList[n].x += ( n >> 2 ) + 1;
          if ( _spriteList[n].header == meteor_w_mask_16x16 )
          {
            if ( ( ( step + n ) & 0x0f ) == 0 )
            {
              _spriteList[n].frame --;
              _spriteList[n].frame &= 0x03;
            }
          }
        }
      }

    #ifdef _ENABLE_DIAGNOSTICS_
      uint16_t startTime = millis();
    #endif

    #ifdef _ENABLE_MASK_
      if ( ssd1306_draw_sprites_px( _workBuffer, _workBufferSize, _spriteList, spriteCount << 1, 
                                  #ifdef _ENABLE_COLLISION_CHECKS_
                                    spriteCount - 1,
                                  #endif
                                    background, true, 0, 128, 0, 7 ) )
      {
    #ifdef _ENABLE_DIAGNOSTICS_
        SSD1306.ssd1306_setpos( 96, 0 );
        SSD1306.ssd1306_string_font6x8( "BOOM!" );
      }
      else
      {
        SSD1306.ssd1306_setpos( 96, 0 );
        SSD1306.ssd1306_string_font6x8( "ok   " );
    #endif
      }
    #else
      ssd1306_draw_sprites_px( _workBuffer, _workBufferSize, _spriteList, spriteCount << 1,
                             #ifdef _ENABLE_COLLISION_CHECKS_
                               spriteCount - 1,
                             #endif
                               background, 0, 128, 0, 7 );
    #endif

    #ifdef _ENABLE_DIAGNOSTICS_
      int frameTime = millis() - startTime;
      if ( frameTime > maxFrameTime ) { maxFrameTime = frameTime; }

      SSD1306.ssd1306_setpos( 110, 6 );
      itoa( frameTime, str, 10 );
      SSD1306.ssd1306_string_font6x8( str ); SSD1306.ssd1306_string_font6x8( " " );
      SSD1306.ssd1306_setpos( 110, 7 );
      itoa( maxFrameTime, str, 10 );
      SSD1306.ssd1306_string_font6x8( str ); SSD1306.ssd1306_string_font6x8( " " );

      while ( frameTime < 50 )
      {
        _delay_ms( 1 );
        frameTime++;
      }
    #endif
    }
  }

  _delay_ms( 5000 );
}

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
                            #ifdef _ENABLE_MASK_  
                              const bool useMask /*= false*/,
                            #endif  
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
  const uint8_t screenEndPosX = screenStartX + screenWidth - 1;

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
        int16_t spriteStartX = sprite->x;
        int8_t spriteWidth = pgm_read_byte( sprite->header + 0 );

        // is the sprite visible in this buffer?
        if ( spriteStartX + spriteWidth >= x_chunk && spriteStartX < x_chunk_end )
        {
          int8_t spriteHeightInPages = pgm_read_byte( sprite->header + 1 );
          uint8_t spriteVerticalOffset = sprite->y & 0x07;

          // calculate start and end page
          int8_t startPage = sprite->y >> 3;
          int8_t endPage = startPage + spriteHeightInPages;
          if ( !spriteVerticalOffset ) { endPage--; }

          // is the sprite visible on this page?
          if ( page >= startPage && page <= endPage )
          {
            // calculate offset to next sprite data row
            uint8_t spriteLineOffset = spriteWidth;
          #ifdef _ENABLE_MASK_  
            if ( useMask ) { spriteLineOffset <<= 1; }
          #endif

            uint16_t bitmapOffset = 0;
            // clip left
            if ( spriteStartX < x_chunk )
            {
              bitmapOffset = x_chunk - spriteStartX;
              spriteWidth -= bitmapOffset;
              spriteStartX = x_chunk;
            }
            // clip right
            if ( spriteStartX + spriteWidth >= x_chunk_end )
            {
              spriteWidth = x_chunk_end - spriteStartX;
            }

            // normalize sprite x position
            spriteStartX -= x_chunk;
            const int8_t spriteEndX = spriteStartX + spriteWidth;

            // if the frame isn't negativ, the sprite will be drawn, otherwise it will be removed
            if ( sprite->frame >= 0 )
            {
              // add frame offset
              bitmapOffset += sprite->frame * spriteLineOffset * spriteHeightInPages >> 1;

              // calculate bitmap data address
              uint16_t addr = bitmapOffset;
            #ifdef _ENABLE_MASK_  
              if ( useMask ) { addr <<= 1; }
            #endif
              addr += uint16_t( spriteList[n].header + sizeof( SSD1306_SPRITE_HEADER ) + ( page - startPage ) * spriteLineOffset );

              for ( int8_t x = 0; x < spriteWidth; x++ )
              {
                uint8_t maskValue = 0;;
                uint8_t pixels = 0;

                if ( spriteVerticalOffset == 0 )
                {
                #ifdef _ENABLE_MASK_  
                  if ( useMask )
                  {
                    maskValue = pgm_read_byte( addr + 1 ); 
                  }
                #endif
                  pixels = pgm_read_byte( addr );
                }
                else
                {
                  if ( page < endPage )
                  {
                  #ifdef _ENABLE_MASK_  
                    if ( useMask )
                    {
                      maskValue = pgm_read_byte( addr + 1 ) << spriteVerticalOffset;
                    }
                  #endif
                    pixels = pgm_read_byte( addr ) << spriteVerticalOffset;
                  }
                  if ( page > startPage )
                  {
                    // look one row up
                  #ifdef _ENABLE_MASK_  
                    if ( useMask )
                    {
                      maskValue |= pgm_read_byte( addr - spriteLineOffset + 1 ) >> ( 8 - spriteVerticalOffset );
                    }
                  #endif
                    pixels |= pgm_read_byte( addr - spriteLineOffset ) >> ( 8 - spriteVerticalOffset );
                  }
                }

              #ifdef _ENABLE_COLLISION_CHECKS_
                // check for collision with player sprite(s)
                if ( n >= playerSprite )
                {
                  if ( collisionBuffer[spriteStartX + x] & maskValue ) { collision = true; }
                }
                // add this mask to the previous masks
                collisionBuffer[spriteStartX + x] |= maskValue;
              #endif
              #ifdef _ENABLE_MASK_
                // remove background under the sprite
                buffer[spriteStartX + x] &= ~maskValue;
                addr++;
              #endif
                buffer[spriteStartX + x] |= pixels;
                addr++;
              }
            }

            // mark the sprite position as "dirty", so the background will be sent to the display
            for ( uint8_t x = uint8_t( spriteStartX ); x < uint8_t( spriteEndX ); x++ ) { dirtyFlags[x] = true; }

          } // sprite is on page

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
            SSD1306.ssd1306_setpos( x + x_chunk, page );
            SSD1306.ssd1306_send_data_start();
          }
          SSD1306.ssd1306_send_byte( buffer[x] );
        }
        // was there an active transfer?
        else if ( transferActive )
        {
          // stop transfer
          transferActive = false;
          SSD1306.ssd1306_send_data_stop();
        }
      }
      // is a transfer still active?
      if ( transferActive )
      {
        // stop transfer
        SSD1306.ssd1306_send_data_stop();
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
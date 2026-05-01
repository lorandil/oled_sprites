#include <Arduino.h>
#include <font6x8.h>
#include <font8x16.h>
#include <ssd1306xled.h>
#include "Sprites.h"

#define _ENABLE_MASK_

// enabling bitfield saves 7/8th of the buffer size, but costs 42 bytes
// of flash and decreases the performance significantly. Most of the time it is more
// efficient to reduce the buffer size by half and use bytes for the dirty flags
// which saves even more RAM ;)
//#define _USE_DIRTY_BITFIELD_

#define _USE_FAST_SPRITES_

constexpr uint8_t SPRITE_DELETE   = 0x01;
constexpr uint8_t SPRITE_FINISHED = 0x80;

bool ssd1306_draw_sprites_px( SSD1306_SPRITE *spriteList, const uint8_t maxSprites, const uint8_t playerSprite = 255, 
                              const uint8_t *background = nullptr, 
                            #ifdef _ENABLE_MASK_
                              const bool useMask = false,
                            #endif
                              const uint8_t screenStartX = 0, const uint8_t screenWidth = 128,
                              const uint8_t screenStartPage = 0, const uint8_t screenEndPage = 7 );

unsigned char str[10];

SSD1306_SPRITE _spriteList[] = { 
#ifdef _ENABLE_MASK_
  { 104, 10, 0, meteor_w_mask_16x16 },
  {  54, 43, 0, meteor_w_mask_16x16 },
  {  84, 27, 0, moon_w_mask_30x32 },
  {  94, 58, 0, moon_w_mask_30x32 },
  { 127, 28, 0, meteor_w_mask_16x16 },
  { 118, 52, 0, meteor_w_mask_16x16 },
  {  60, 32, 0, meteor_w_mask_16x16 },
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

  for ( uint8_t run = 0; run < 2; run++ )
  {
    // -- Sprites ---
    SSD1306.ssd1306_fillscreen(0);
    SSD1306.ssd1306_setpos(0, 0);
    SSD1306.ssd1306_string_font6x8( !run ? "Sprites" : "Sprites /w background" );
    _delay_ms(1500);

    SSD1306.ssd1306_fillscreen(0);

    const uint8_t *background = !run ? nullptr : moon128x128;

    uint16_t maxFrameTime = 0;

    if ( background ) { SSD1306.ssd1306_draw_bmp( 0, 0, 128, 8, background ); }

    uint8_t spriteCount = sizeof( _spriteList ) / sizeof( _spriteList[0] ) / 2;

    for ( uint8_t step = 0; step < 128; step++ )
    {
      // copy all active sprites to the second half and mark them for clearing
      memcpy( &_spriteList[spriteCount], _spriteList, sizeof( _spriteList ) >> 1 );
      for ( uint8_t n = spriteCount; n < spriteCount << 1; n++ )
      {
        _spriteList[n].flags = SPRITE_DELETE;
      }

      // move spaceship down and up again
      _spriteList[spriteCount - 1].y = ( step < 64 ) ? step : 128 - step;

      // move moons and asteroids
      for ( uint8_t n = 0; n < spriteCount - 1; n++ )
      {
        if ( !run ) { _spriteList[n].x -= 1; }
        else { _spriteList[n].x += 1; }
      }

      uint16_t startTime = millis();

    #ifdef _ENABLE_MASK_
      if ( ssd1306_draw_sprites_px( _spriteList, spriteCount << 1, spriteCount - 1, background, true, 0, 128, 0, 7 ) )
      {
        SSD1306.ssd1306_setpos( 96, 0 );
        SSD1306.ssd1306_string_font6x8( "BOOM!" );
      }
      else
      {
        SSD1306.ssd1306_setpos( 96, 0 );
        SSD1306.ssd1306_string_font6x8( "ok   " );
      }
    #else
      ssd1306_draw_sprites_px( _spriteList, spriteCount << 1, spriteCount - 1, background, 0, 128, 0, 7 );
    #endif
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
    }
  }

  _delay_ms( 5000 );
}

#ifndef _USE_FAST_SPRITES_
/////////////////////////////////////////////////////////////////////////////
// Idea for a fast sprite engine without a frame buffer (with slow i2c!)
// - three nested loops
// - run over all pages
//   - run over all columns ('x')
//      - run over all sprites ('n')
//          - get background bitmap at (x, page) (if available)
//          - check for each sprite if it is in the column and in the page
//          - if so, check if the sprite has a bitmap - if not, the sprite background should be restored
//            (address page and column and start data transfer if required)
//          - if the sprite has a bitmap, mask the background, or the bitmap data
//          - if 'n' is less than 'playerSprite', or the mask into the mask byte, 
//            else check if the mask byte collides with the player sprite's mask.
//            If a collision is detected, set the collision flag
//          - send the pixel data to the display
//    - close the data transfer, if a non sprite column or the end of the screen is reached
bool ssd1306_draw_sprites_px( SSD1306_SPRITE *spriteList, const uint8_t maxSprites, const uint8_t playerSprite, 
                              const uint8_t *background /*= nullptr*/,
                            #ifdef _ENABLE_MASK_  
                              const bool useMask /*= false*/,
                            #endif  
                              const uint8_t screenStartX /*= 0*/, const uint8_t screenWidth /*= 128*/,
                              const uint8_t screenStartPage /*= 0*/, const uint8_t screenEndPage /*= 7*/ )
{
  bool collision = false;

  uint8_t screenEndPosX = screenStartX + screenWidth - 1;

  for ( int8_t page = screenStartPage; page <= screenEndPage; page++ )
  {
    // no active transfer
    bool transferActive = false;

    // check all columns
    for ( uint8_t x = screenStartX; x < screenEndPosX; x++ )
    {
      uint8_t next_x = screenWidth;

      // initialize pixels
      uint8_t pixels = ( background ? pgm_read_byte( background + x )  : 0 );
    #ifdef _ENABLE_MASK_
      uint8_t mask = 0;
    #endif

      // find next sprite position/draw sprite
      for ( uint8_t n = 0; n < maxSprites; n++ )
      {
        // for readability
        SSD1306_SPRITE *sprite = &spriteList[n];

        // is the sprite not finished yet?
        if ( !( sprite->flags & SPRITE_FINISHED ) )
        {
          // calculate start/end column
          int16_t sprite_start_x = sprite->x;

          // is the sprite visible in this column?
          if ( x >= sprite_start_x )
          {
            int8_t  spriteWidth = pgm_read_byte( sprite->header + 0 );
            int16_t sprite_end_x = sprite_start_x + spriteWidth - 1;

            if ( x <= sprite_end_x )
            {
              // we are up to something!
              next_x = x;

              // calculate start/end page
              int8_t sprite_start_y = sprite->y;
              int8_t sprite_start_page = sprite_start_y >> 3;

              if ( page >= sprite_start_page )
              {
                uint8_t spriteHeight = pgm_read_byte( sprite->header + 1 );
                uint8_t sprite_bit_offset_y = sprite_start_y & 0x07;
                // add sprite height in pages
                int8_t sprite_end_page = sprite_start_page + spriteHeight;
                // if the sprite is page aligned, that was one to many ;)
                if ( sprite_bit_offset_y == 0 ) { sprite_end_page--; }

                // is the sprite visible on this page?
                if ( page <= sprite_end_page )
                {
                  // calculate offset to next sprite data row
                  uint8_t sprite_line_offset = spriteWidth;
                #ifdef _ENABLE_MASK_  
                  if ( useMask ) { sprite_line_offset <<= 1; }
                #endif

                  // ok, we need a transfer
                  if ( !transferActive )
                  {
                    // set display position
                    SSD1306.ssd1306_setpos( x, page );
                    // start transfer
                    SSD1306.ssd1306_send_data_start();
                    transferActive = true;
                  }

                  // should this sprite be drawn or removed?
                  // at this point flags can only be NULL or "SPRITE_DELETE"
                  if ( !sprite->flags )
                  {
                    // calculate bitmap data address
                    uint8_t value = 0;
                    uint8_t maskValue = 0;
                    uint16_t addr = uint16_t( x - sprite_start_x );
                  #ifdef _ENABLE_MASK_  
                    if ( useMask ) { addr <<= 1; }
                  #endif
                    addr += uint16_t( spriteList[n].header + sizeof( SSD1306_SPRITE_HEADER ) + ( page - sprite_start_page ) * sprite_line_offset );

                    if ( sprite_bit_offset_y == 0 )
                    {
                    #ifdef _ENABLE_MASK_  
                      if ( useMask )
                      {
                        maskValue = pgm_read_byte( addr + 1 ); 
                      }
                    #endif
                      value = pgm_read_byte( addr );
                    }
                    else
                    {
                      if ( page < sprite_end_page )
                      {
                      #ifdef _ENABLE_MASK_  
                        if ( useMask )
                        {
                          maskValue = pgm_read_byte( addr + 1 ) << sprite_bit_offset_y;
                        }
                      #endif
                        value = pgm_read_byte( addr ) << sprite_bit_offset_y;
                      }
                      if ( page > sprite_start_page )
                      {
                        // look one row up
                        addr -= sprite_line_offset;

                      #ifdef _ENABLE_MASK_  
                        if ( useMask )
                        {
                          maskValue |= pgm_read_byte( addr + 1 ) >> ( 8 - sprite_bit_offset_y );
                        }
                      #endif
                        value |= pgm_read_byte( addr ) >> ( 8 - sprite_bit_offset_y );
                      }
                    }

                  #ifdef _ENABLE_MASK_  
                    pixels &= ~maskValue;
                    // check for collision with player sprite
                    if ( n >= playerSprite )
                    {
                      if ( mask & maskValue ) { collision = true; }
                    }
                    // add this mask to the previous masks
                    mask |= maskValue;
                  #endif
                    pixels |= value;
                  }

                  // is this sprite fully drawn?
                  if ( ( x == sprite_end_x ) && ( page == sprite_end_page ) )
                  {
                    sprite->flags |= SPRITE_FINISHED;
                  }
                }
              }
            }
          }
          else
          {
            // skip to the next sprite start position (if no other sprite is in this column)
            if ( ( sprite_start_x > x ) && ( sprite_start_x < next_x ) ) { next_x = sprite_start_x - 1; }
          }
        }

      } // for n

      // was there no new sprite data?
      if ( next_x > x )
      {
        // finalize transfer
        transferActive = false;
        SSD1306.ssd1306_send_data_stop();
        // skip empty columns
        x = next_x;
      }

      // write data to display
      if ( transferActive )
      {
        SSD1306.ssd1306_send_byte( pixels );
      }
    } // for x

    // stop transfer (if required)
    if ( transferActive ) { 
      SSD1306.ssd1306_send_data_stop();
    }

    if ( background )
    { 
      // next background row
      background += screenWidth;
    }
  } // for page

  // clear finished flag on all sprites
  for ( int n = 0; n < maxSprites; n++ ) { spriteList[n].flags &= ~SPRITE_FINISHED; }

  return( collision );
}

#else

/////////////////////////////////////////////////////////////////////////////
// Faster version using more RAM for buffering
// The idea is to use three buffers (background, mask, dirty) of the same size and clip the sprites
// to the appropriate regions. With a reasonable buffer size this version is much faster than the
// no RAM version above.
// !For simplicity's sake it's assumed that the screen size is a multiple of the buffer size!

// for testing the RAM buffer size is 64 bytes * 2 + 64 *1/8th bytes for the dirty flags
constexpr uint8_t bufferSize = 64;
#ifdef _USE_DIRTY_BITFIELD_
  constexpr uint8_t dirtyFlagsSize = ( bufferSize + 7 ) / 8;
#else
  constexpr uint8_t dirtyFlagsSize = bufferSize;
#endif
uint8_t buffer[bufferSize];
uint8_t mask[bufferSize];
uint8_t dirtyFlags[dirtyFlagsSize];

bool ssd1306_draw_sprites_px( SSD1306_SPRITE *spriteList, const uint8_t maxSprites, const uint8_t playerSprite, 
                              const uint8_t *background /*= nullptr*/,
                            #ifdef _ENABLE_MASK_  
                              const bool useMask /*= false*/,
                            #endif  
                              const uint8_t screenStartX /*= 0*/, const uint8_t screenWidth /*= 128*/,
                              const uint8_t screenStartPage /*= 0*/, const uint8_t screenEndPage /*= 7*/ )
{
  bool collision = false;

  uint8_t screenEndPosX = screenStartX + screenWidth - 1;

  for ( int8_t page = screenStartPage; page <= screenEndPage; page++ )
  {
    // split the screen width in chunks of <bufferSize>
    for ( int16_t x_chunk = screenStartX; x_chunk < screenEndPosX; x_chunk += bufferSize )
    {
      // initialize all buffers with background or empty
      if ( background ) { memcpy_P( buffer, background + x_chunk - screenStartX, bufferSize ); }
      else { memset( buffer, 0, bufferSize ); }
      // no data written yet
      memset( dirtyFlags, 0, dirtyFlagsSize );
    #ifdef _ENABLE_MASK_
      // no mask here
      memset( mask, 0, bufferSize );
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

            uint8_t bitmapOffset = 0;
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

            // should this sprite be drawn or removed?
            // at this point flags can only be NULL or "SPRITE_DELETE"
            if ( !sprite->flags )
            {
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

              #ifdef _ENABLE_MASK_  
                // check for collision with player sprite
                if ( n >= playerSprite )
                {
                  if ( mask[spriteStartX + x] & maskValue ) { collision = true; }
                }
                // add this mask to the previous masks
                mask[spriteStartX + x] |= maskValue;
                // remove background under the sprite
                buffer[spriteStartX + x] &= ~maskValue;
                addr++;
              #endif
                buffer[spriteStartX + x] |= pixels;
                addr++;
              }
            }

            // mark the sprite position as "dirty", so the background will be sent to the display
            for ( int8_t x = spriteStartX; x < spriteEndX; x++ )
            {
            #ifdef _USE_DIRTY_BITFIELD_
              dirtyFlags[(x >> 3)] |= ( 1 << ( x & 0x07 ) );
            #else
              dirtyFlags[x] = true;
            #endif
            }

          } // sprite is on page

        } // sprite is in buffer

      } // for n

      // transfer all dirty data to the display
      bool transferActive = false;
      
      for ( uint8_t x = 0; x < bufferSize; x++ )
      {
        // is this byte dirty?
      #ifdef _USE_DIRTY_BITFIELD_
        if ( dirtyFlags[(x >> 3)] & ( 1 << ( x & 0x07 ) ) )
      #else
        if ( dirtyFlags[x] )
      #endif
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

  // clear finished flag on all sprites
  for ( int n = 0; n < maxSprites; n++ ) { spriteList[n].flags &= ~SPRITE_FINISHED; }

  return( collision );
}

#endif
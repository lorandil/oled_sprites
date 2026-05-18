#include <Arduino.h>
#include <font6x8.h>
#include <font8x16.h>
#include <ssd1306xled.h>
#include "Sprites.h"
#include "DrawSpritesPx.hpp"

// calculate frame times and show max. frame time on the screen
// checks and displays also collision of player sprite with other sprites
#define _ENABLE_DIAGNOSTICS_

// add the background image as a sprite - because we can
//#define _CRAZY_DEMO_

// global RAM buffer
constexpr uint8_t _spriteBufferSize = 64;
#ifdef _ENABLE_COLLISION_CHECKS_
  constexpr uint8_t _workBufferSize = 3 * _spriteBufferSize;
#else
  constexpr uint8_t _workBufferSize = 2 * _spriteBufferSize;
#endif
uint8_t _workBuffer[_workBufferSize];

SSD1306_SPRITE _spriteList[] = { 
  { 100, -4, 0, meteor_w_mask_16x16 },
  {  54, 43, 1, meteor_w_mask_16x16 },
  {  74, 19, 0, moon_w_mask_30x32 },
  { 140, 28, 3, meteor_w_mask_16x16 },
  { 114, 38, 0, moon_w_mask_30x32 },
  {  90, 52, 0, meteor_w_mask_16x16 },
  {  67, 32, 2, meteor_w_mask_16x16 },
  {  84, 27, 0, moon_30x32 },
#ifdef _CRAZY_DEMO_
  { 128,  0, 0, Moon128x64 },
#endif
  {  10,  0, 0, ship_w_mask_18x16 },
  // placeholders for removal of old sprites
  {},
  {},
  {},
  {},
  {},
  {},
  {},
  {},
  {},
#ifdef _CRAZY_DEMO_
  {},
#endif
};

void setup() {
  _delay_ms(100);
  SSD1306.ssd1306_init();
}

#ifdef _ENABLE_DIAGNOSTICS_
  // for diagnostic output
  char str[10];
#endif

void loop() {

  for ( uint8_t run = 0; run <= 1; run++ )
  {
    // -- Sprites ---
    SSD1306.ssd1306_fillscreen(0);
    SSD1306.ssd1306_setpos(0, 0);
    SSD1306.ssd1306_string_font6x8( !run ? "Sprites" : "Sprites /w background" );
    _delay_ms(1500);

    SSD1306.ssd1306_fillscreen(0);

    const uint8_t *background = !run ? nullptr : Moon128x64 + sizeof( SSD1306_SPRITE_HEADER );

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
        _spriteList[n].frameAndFlags = SSD1306_SPRITE_FLAGS::undraw;
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
            if ( ( ( step + n ) & 0x07 ) == 0 )
            {
              _spriteList[n].frameAndFlags++;
              _spriteList[n].frameAndFlags &= 0x03;
            }
          }
        }
        else
        { 
          _spriteList[n].x += ( n >> 2 ) + 1;
          if ( _spriteList[n].header == meteor_w_mask_16x16 )
          {
            if ( ( ( step + n ) & 0x07 ) == 0 )
            {
              _spriteList[n].frameAndFlags--;
              _spriteList[n].frameAndFlags &= 0x03;
            }
          }
        }
      }

    #ifdef _ENABLE_DIAGNOSTICS_
      uint16_t startTime = millis();
    #endif

      if ( ssd1306_draw_sprites_px( _workBuffer, _workBufferSize, _spriteList, spriteCount << 1, 
                                  #ifdef _ENABLE_COLLISION_CHECKS_
                                    spriteCount - 1,
                                  #endif
                                    background, 0, 128, 0, 7 ) )
      {
        // "player" was hit
        _spriteList[spriteCount - 1].frameAndFlags = SSD1306_SPRITE_FLAGS::invert;
      #ifdef _ENABLE_DIAGNOSTICS_
        SSD1306.ssd1306_setpos( 96, 0 );
        SSD1306.ssd1306_string_font6x8( "BOOM!" );
      #endif
      }
      else
      {
      #ifdef _ENABLE_DIAGNOSTICS_
        SSD1306.ssd1306_setpos( 96, 0 );
        SSD1306.ssd1306_string_font6x8( "ok   " );
      #endif
        // everything is fine!
        _spriteList[spriteCount - 1].frameAndFlags = 0;
      }
    
    #ifdef _ENABLE_DIAGNOSTICS_
      int frameTime = millis() - startTime;
      if ( frameTime > maxFrameTime ) { maxFrameTime = frameTime; }

      SSD1306.ssd1306_setpos( 110, 6 );
      itoa( frameTime, str, 10 );
      SSD1306.ssd1306_string_font6x8( str ); SSD1306.ssd1306_string_font6x8(" ");
      SSD1306.ssd1306_setpos( 110, 7 );
      itoa( maxFrameTime, str, 10 );
      SSD1306.ssd1306_string_font6x8( str ); SSD1306.ssd1306_string_font6x8(" ");

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
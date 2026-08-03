/** @file ssd1315.h
 *
 * Copyright (c) 2026 IACE
 */
#pragma once
#include <sys/i2c.h>
#include <comm/frameregistry.h>

namespace Display {
    struct SSD1315: public I2C::Device {
        bool hasExternalVCC;
        int16_t height;
        int16_t width;
        uint8_t contrast;

        enum ControlByte {
            COMMANDS_ONLY   = 0b00000000,     // only commands follow in transmission
            IMAGEDATA_ONLY  = 0b01000000,     // only image data follows in transmission
            COMMANDS        = 0b10000000,     // commands follow, same transmission will have (an) additional control byte(s)
            IMAGEDATA       = 0b11000000      // image data follows, same transmission will have (an) additional control byte(s)
        };

        enum Command {
            // Setter commands needing one of a list of other commands next
            SET_MEMORY_ADDRESSINGMODE   = 0x20,
            SET_CHARGE_PUMP_VOLTAGE     = 0x8D,     // disable if external VCC is used
            SET_COM_PINS_HW             = 0xDA,     // COM Pins Hardware Configuration
            SET_VCOMH_DESELECT_LEVEL    = 0xDB,

            // Setter commands according to next two bytes (start and end)
            SET_COLUMN_ADDRESS          = 0x21,
            SET_PAGE_ADDRESS            = 0x22,

            // Setter commands according to next byte
            SET_DISPLAY_OFFSET          = 0xD3,     // vertical shift by COM (from 0-63)
            SET_DISPLAY_CLOCKDIV        = 0xD5,
            SET_CONTRAST_CONTROL        = 0x81,     // between 0x01 and 0xFF
            SET_PRECHARGE_PERIOD        = 0xD9,     // two phases, least significant 4 bits for first phase, most significant 4 bits for second (neither can be 0)
            SET_MUX_RATIO               = 0xA8,     // Set Multiplex ratio to N+1 MUX

            // Setter commands according to bitwise-AND byte 
            SET_START_LINE              = 0x40,     // RAM display start line register (from 0-63)
            SET_START_PAGE              = 0xB0,     // Set GDDRAM Page Start Address for Page Addressing Mode (from 0-7)

            SEGMENT_REMAP_OFF           = 0xA0,     // column address 0 is mapped to SEG0 (RESET)
            SEGMENT_REMAP_ON            = 0xA1,     // column address 127 is mapped to SEG0
            PIXELS_RAM                  = 0xA4,     // pixels ON according to RAM
            PIXELS_ALL                  = 0xA5,     // pixels ON regardless of RAM
            DISPLAY_OFF                 = 0xAE,
            DISPLAY_ON                  = 0xAF,     // in normal mode
            INVERSION_OFF               = 0xA6,     // HIGH bit in RAM means pixel ON
            INVERSION_ON                = 0xA7,     // HIGH bit in RAM means pixel OFF
            SCAN_DIRECTION_NORMAL       = 0xC0,     // scan from COM0 to COM[N–1] (where N is multiplex ratio from SET_MUX_RATIO)
            SCAN_DIRECTION_REMAPPED     = 0xC8,     // scan from COM[N–1] to COM0
            SCROLL_ON                   = 0x2F,
            SCROLL_OFF                  = 0x2E
        };

        enum AddressingMode {
            HORIZONTAL  = 0b00,     // Horizontal Addressing Mode
            VERTICAL    = 0b01,     // Vertical Addressing Mode
            PAGE        = 0b10,     // Page Addressing Mode (RESET)
            INVALID     = 0b11      // Invalid
        };
        enum ChargePumpVoltage {
            DISABLE         = 0x10,     // disable
            SEVENPOINTFIVE  = 0x14,     // 7.5 V
            EIGHTPOINTFIVE  = 0x94,     // 8.5 V
            NINE            = 0x95      // 9.0 V
        };
        enum COMPinConfiguration {
            SEQUENTAL_NORMAL = 0x02,    // Sequential
            ALTERNATE_NORMAL = 0x12,    // Alternative
            SEQUENTAL_REMAP  = 0x22,    // Sequential, with Left/Right Remap
            ALTERNATE_REMAP  = 0x32     // Alternative, with Left/Right Remap
        };
        enum DeselectionLevelPercent {
            SIXTYFIVE       = 0x00,
            SEVENTYONE      = 0x10,
            SEVENTYSEVEN    = 0x20,
            EIGHTYTHREE     = 0x30
        };
        

        /** default conf Config  */
        struct Config {
            Sink<I2C::Request> &bus;
            uint8_t address;
            bool hasExternalVCC;
            int16_t height;
            int16_t width;
            uint8_t contrast;
        };

        SSD1315(const Config &conf) : 
            I2C::Device(conf.bus, conf.address), 
            hasExternalVCC(conf.hasExternalVCC), 
            height(conf.height), 
            width(conf.width), 
            contrast(conf.contrast) {}

        void init() {
            bus.push({
                .dev = this,
                .data = Frame{}
                    .pack<uint8_t>( ControlByte::COMMANDS_ONLY)
                    .pack<uint8_t>( Command::DISPLAY_OFF )
                    .pack<uint8_t>( Command::SET_DISPLAY_CLOCKDIV      ).pack<uint8_t>( 0x80 )
                    .pack<uint8_t>( Command::SET_MUX_RATIO             ).pack<uint8_t>( (height - 1) )
                    .pack<uint8_t>( Command::SET_DISPLAY_OFFSET        ).pack<uint8_t>( 0x00 )
                    .pack<uint8_t>( Command::SET_START_LINE            ).pack<uint8_t>( 0x00 )
                    .pack<uint8_t>( Command::SET_CHARGE_PUMP_VOLTAGE   ).pack<uint8_t>( hasExternalVCC ? ChargePumpVoltage::DISABLE : ChargePumpVoltage::SEVENPOINTFIVE )
                    .pack<uint8_t>( Command::SET_MEMORY_ADDRESSINGMODE ).pack<uint8_t>( AddressingMode::HORIZONTAL )
                    .pack<uint8_t>( Command::SEGMENT_REMAP_OFF )
                    .pack<uint8_t>( Command::SCAN_DIRECTION_NORMAL )
                    .pack<uint8_t>( Command::SET_COM_PINS_HW           ).pack<uint8_t>( COMPinConfiguration::ALTERNATE_NORMAL )
                    .pack<uint8_t>( Command::SET_CONTRAST_CONTROL      ).pack<uint8_t>( contrast )
                    .pack<uint8_t>( Command::SET_PRECHARGE_PERIOD      ).pack<uint8_t>( hasExternalVCC ? 0x22 : 0xF1 )
                    .pack<uint8_t>( Command::SET_VCOMH_DESELECT_LEVEL  ).pack<uint8_t>( DeselectionLevelPercent::SIXTYFIVE )
                    .pack<uint8_t>( Command::PIXELS_RAM )
                    .pack<uint8_t>( Command::INVERSION_OFF )
                    .pack<uint8_t>( Command::DISPLAY_ON )
                    .b,
            });
        }

        void setContrast(const uint8_t value) {
            bus.push({
                .dev = this,
                .data = Frame{}
                    .pack<uint8_t>( ControlByte::COMMANDS_ONLY)
                    .pack<uint8_t>( Command::SET_CONTRAST_CONTROL)
                    .pack<uint8_t>( value )
                    .b,
            });
        }
        
        void invertDisplay(const bool invert) {
            bus.push({
                .dev = this,
                .data = Frame{}
                    .pack<uint8_t>( ControlByte::COMMANDS_ONLY)
                    .pack<uint8_t>( invert ? Command::INVERSION_ON : Command::INVERSION_OFF)
                    .b,
            });
        }
        
        void lightAll(const bool light) {
            bus.push({
                .dev = this,
                .data = Frame{}
                    .pack<uint8_t>( ControlByte::COMMANDS_ONLY)
                    .pack<uint8_t>( light ? Command::PIXELS_ALL : Command::PIXELS_RAM)
                    .b,
            });
        }
        
        void callback(const I2C::Request &rq) override {
            // the SSD1315 can't send data through the I2C bus
        }
    };
}
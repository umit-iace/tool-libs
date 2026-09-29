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

        uint8_t colStart = 0;
        uint8_t colEnd = 127;
        uint8_t pageStart = 0;
        uint8_t pageEnd = 7;

        /** Control Bytes 
        * need to be sent at the start of every transmission
        */
        enum ControlByte {
            COMMAND_STREAM     = 0b00000000, // all following bytes in transmission will be interpreted as commands
            IMAGEDATA_STREAM   = 0b01000000, // all following bytes in transmission will be interpreted as image data
            SINGLE_COMMAND     = 0b10000000, // next byte will be interpreted as command
            SINGLE_IMAGEBYTE   = 0b11000000  // next byte will be interpreted as image data
        };

        /** Command Bytes 
        * list of commands
        */
        enum Command {
            // commands configuring a setting according to one of a list of configuration bytes
            SET_MEMORY_ADDRESSINGMODE   = 0x20,
            SET_CHARGE_PUMP_VOLTAGE     = 0x8D,     // disable if external VCC is used
            SET_COM_PINS_HW             = 0xDA,     // COM Pins Hardware Configuration
            SET_VCOMH_DESELECT_LEVEL    = 0xDB,

            // commands configuring a setting according to the next two bytes (start and end)
            SET_COLUMN_ADDRESS          = 0x21,
            SET_PAGE_ADDRESS            = 0x22,

            // commands configuring a setting according to the next byte
            SET_DISPLAY_OFFSET          = 0xD3,     // vertical shift by COM (from 0-63)
            SET_DISPLAY_CLOCKDIV        = 0xD5,
            SET_CONTRAST_CONTROL        = 0x81,     // between 0x01 and 0xFF
            SET_PRECHARGE_PERIOD        = 0xD9,     // two phases, least significant 4 bits for first phase, most significant 4 bits for second (neither can be 0)
            SET_MUX_RATIO               = 0xA8,     // Set Multiplex ratio to N+1 MUX

            // commands configuring a setting according to bitwise-AND byte
            SET_START_LINE              = 0x40,     // RAM display start line register (from 0-63)
            SET_START_PAGE              = 0xB0,     // Set GDDRAM Page Start Address for Page Addressing Mode (from 0-7)

            // pairs of commands respectively enabling or disabling a setting
            SCROLL_ON                   = 0x2F,
            SCROLL_OFF                  = 0x2E,
            SEGMENT_REMAP_OFF           = 0xA0,     // column address 0 is mapped to SEG0 (RESET)
            SEGMENT_REMAP_ON            = 0xA1,     // column address 127 is mapped to SEG0
            PIXELS_RAM                  = 0xA4,     // pixels ON according to RAM
            PIXELS_ALL                  = 0xA5,     // pixels ON regardless of RAM
            DISPLAY_OFF                 = 0xAE,
            DISPLAY_ON                  = 0xAF,     // in normal mode
            INVERSION_OFF               = 0xA6,     // HIGH bit in RAM means pixel ON
            INVERSION_ON                = 0xA7,     // HIGH bit in RAM means pixel OFF
            SCAN_DIRECTION_REMAP_OFF    = 0xC0,     // scan from COM0 to COM[N–1] (where N is multiplex ratio from SET_MUX_RATIO)
            SCAN_DIRECTION_REMAP_ON     = 0xC8      // scan from COM[N–1] to COM0
        };

        /** Configuration of Addressing Mode */
        enum AddressingMode {
            HORIZONTAL  = 0b00,     // Horizontal Addressing Mode
            VERTICAL    = 0b01,     // Vertical Addressing Mode
            PAGE        = 0b10,     // Page Addressing Mode (RESET)
            INVALID     = 0b11      // Invalid
        };
        /** Configuration of internal charge pump for panel voltage */
        enum ChargePumpVoltage {
            DISABLE         = 0x10,     // disable
            SEVENPOINTFIVE  = 0x14,     // 7.5 V
            EIGHTPOINTFIVE  = 0x94,     // 8.5 V
            NINE            = 0x95      // 9.0 V
        };
        /** Configuration of COM pins */
        enum COMPinConfiguration {
            SEQUENTAL_NORMAL = 0x02,    // Sequential
            ALTERNATE_NORMAL = 0x12,    // Alternative
            SEQUENTAL_REMAP  = 0x22,    // Sequential, with Left/Right Remap
            ALTERNATE_REMAP  = 0x32     // Alternative, with Left/Right Remap
        };
        /** deselection level
        * decides at which percentage of VCC a voltage is considered LOW
        */
        enum DeselectionLevelPercent {
            SIXTYFIVE       = 0x00,
            SEVENTYONE      = 0x10,
            SEVENTYSEVEN    = 0x20,
            EIGHTYTHREE     = 0x30
        };
        
        /** 
        * default config struct  
        * @param bus I2C request queue
        * @param address 7bit I2C address of ADS1115 device
        * @param hasExternalVCC hardware source of voltage
        * @param height display height (64)
        * @param width display width (128)
        * @param contrast initial contrast setting
        */
        struct Config {
            Sink<I2C::Request> &bus;
            uint8_t address;
            bool hasExternalVCC;
            int16_t height;
            int16_t width;
            uint8_t contrast;
        };

        /**
        * constructor
        * @param conf SSD1315 config struct
        */
        SSD1315(const Config &conf) : 
            I2C::Device(conf.bus, conf.address), 
            hasExternalVCC(conf.hasExternalVCC), 
            height(conf.height), 
            width(conf.width), 
            contrast(conf.contrast) {}

        /**
        * initialize SSD1315 hardware
        * call in Support::init()
        */
        void init() {
            bus.push({
                .dev = this,
                .data = Frame{}
                    .pack<uint8_t>( ControlByte::COMMAND_STREAM )
                    .pack<uint8_t>( Command::DISPLAY_OFF )
                    .pack<uint8_t>( Command::SET_DISPLAY_CLOCKDIV )
                    .pack<uint8_t>( 0x80 )
                    .pack<uint8_t>( Command::SET_MUX_RATIO )
                    .pack<uint8_t>( (height - 1) )
                    .pack<uint8_t>( Command::SET_DISPLAY_OFFSET )
                    .pack<uint8_t>( 0x00 )
                    .pack<uint8_t>( Command::SET_START_LINE | (uint8_t) 0x00 )
                    .pack<uint8_t>( Command::SET_CHARGE_PUMP_VOLTAGE )
                    .pack<uint8_t>( hasExternalVCC ? ChargePumpVoltage::DISABLE : ChargePumpVoltage::SEVENPOINTFIVE )
                    .pack<uint8_t>( Command::SET_MEMORY_ADDRESSINGMODE )
                    .pack<uint8_t>( AddressingMode::HORIZONTAL )
                    .pack<uint8_t>( Command::SEGMENT_REMAP_ON )
                    .pack<uint8_t>( Command::SCAN_DIRECTION_REMAP_ON )
                    .pack<uint8_t>( Command::SET_COLUMN_ADDRESS )
                    .pack<uint8_t>( 0x00 )
                    .pack<uint8_t>( 0x7F )
                    .pack<uint8_t>( Command::SET_PAGE_ADDRESS )
                    .pack<uint8_t>( 0x00 )
                    .pack<uint8_t>( 0x07 )
                    .pack<uint8_t>( Command::SET_COM_PINS_HW )
                    .pack<uint8_t>( COMPinConfiguration::ALTERNATE_NORMAL )
                    .pack<uint8_t>( Command::SET_CONTRAST_CONTROL )
                    .pack<uint8_t>( contrast )
                    .pack<uint8_t>( Command::SET_PRECHARGE_PERIOD )
                    .pack<uint8_t>( hasExternalVCC ? 0x22 : 0xF1 )
                    .pack<uint8_t>( Command::SET_VCOMH_DESELECT_LEVEL )
                    .pack<uint8_t>( DeselectionLevelPercent::SIXTYFIVE )
                    .pack<uint8_t>( Command::PIXELS_RAM )
                    .pack<uint8_t>( Command::INVERSION_OFF )
                    .pack<uint8_t>( Command::DISPLAY_ON )
                    .b,
            });
        }
        
        /**
        * sends a section of arbitraty size via I2C into GDDRAM
        * @param image pointer to imagedata
        * @param new_colStart starting column
        * @param new_colEnd end column
        * @param new_pageStart starting page
        * @param new_pageEnd end page
        */
        void sendSection(uint8_t* image, uint8_t new_colStart, uint8_t new_colEnd, uint8_t new_pageStart, uint8_t new_pageEnd, bool fill = false) {
            Frame frameBuffer{};

            uint16_t bytesInFrame = 0;

            if ((colStart != new_colStart) || (colEnd != new_colEnd)) {
                colEnd = new_colEnd;
                colStart = new_colStart;

                frameBuffer
                    .pack<uint8_t>( ControlByte::SINGLE_COMMAND ).pack<uint8_t>( Command::SET_COLUMN_ADDRESS )
                    .pack<uint8_t>( ControlByte::SINGLE_COMMAND ).pack<uint8_t>( colStart )
                    .pack<uint8_t>( ControlByte::SINGLE_COMMAND ).pack<uint8_t>( colEnd );
                bytesInFrame += 6;
            }

            if ((pageStart != new_pageStart) || (pageEnd != new_pageEnd)) {
                pageEnd = new_pageEnd;
                pageStart = new_pageStart;

                frameBuffer
                    .pack<uint8_t>( ControlByte::SINGLE_COMMAND ).pack<uint8_t>( Command::SET_PAGE_ADDRESS )
                    .pack<uint8_t>( ControlByte::SINGLE_COMMAND ).pack<uint8_t>( pageStart )
                    .pack<uint8_t>( ControlByte::SINGLE_COMMAND ).pack<uint8_t>( pageEnd );
                bytesInFrame += 6;
            }
            
            frameBuffer.pack<uint8_t>( ControlByte::IMAGEDATA_STREAM);
            bytesInFrame += 1;
            
            uint16_t imageSize = (colEnd + 1 - colStart) * (pageEnd + 1 - pageStart);
            for (uint16_t i = 0; i<imageSize; ++i) {
                frameBuffer.pack<uint8_t>( fill ? image[0] : image[i] );
                ++bytesInFrame;

                // if Frame is full
                if (bytesInFrame == 127) {
                    // send
                    bus.push({
                        .dev = this,
                        .data = frameBuffer.b,
                    });

                    // reset Frame
                    frameBuffer = Frame{}.pack<uint8_t>( ControlByte::IMAGEDATA_STREAM);
                    bytesInFrame = 1;
                }
            }

            // send leftovers
            if (bytesInFrame != 1) {
                bus.push({
                    .dev = this,
                    .data = frameBuffer.b,
                });
            }
        }

        /**
        * fills GDDRAM with all zeros and resets cursor address
        */
        void reset() {
            uint8_t zero[1] = {0};
            sendSection(zero, 0, 127, 0, 7, true);
        }

        /**
        * sets contrast of display
        * @param value contrast value between 1-255
        */
        void setContrast(const uint8_t value) {
            bus.push({
                .dev = this,
                .data = Frame{}
                    .pack<uint8_t>( ControlByte::COMMAND_STREAM )
                    .pack<uint8_t>( Command::SET_CONTRAST_CONTROL )
                    .pack<uint8_t>( value )
                    .b,
            });
        }
        
        /**
        * turns inversion on or off
        * with inversion on, HIGH bits in GDDRAM will be off and LOW bits will be on
        * does not affect values in GDDDRAM
        * @param invert boolean of desired inversion state
        */
        void invertDisplay(const bool invert) {
            bus.push({
                .dev = this,
                .data = Frame{}
                    .pack<uint8_t>( ControlByte::COMMAND_STREAM )
                    .pack<uint8_t>( invert ? Command::INVERSION_ON : Command::INVERSION_OFF )
                    .b,
            });
        }
        
        /**
        * turns all pixels on or according to GDDRAM
        * does not affect values in GDDDRAM
        * @param light boolean of desired pixel state
        */
        void lightAll(const bool light) {
            bus.push({
                .dev = this,
                .data = Frame{}
                    .pack<uint8_t>( ControlByte::COMMAND_STREAM )
                    .pack<uint8_t>( light ? Command::PIXELS_ALL : Command::PIXELS_RAM )
                    .b,
            });
        }

        /*
        * callback dummy
        * demanded by inheritance but the SSD1315 can't send data through the I2C bus
        */
        void callback(const I2C::Request &rq) override { }
    };
}
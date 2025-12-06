// Credits to https://github.com/Gifford47/HCPBridgeMqtt for the initial code base

#ifndef HOERMANN_H_
#define HOERMANN_H_

#include <Arduino.h>
#include <Stream.h>

#define SLAVE_ID 2
#define SIMULATEKEYPRESSDELAYMS 100
#define DEADREPORTTIMEOUT 60000

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define PIN_TXD 17
#define PIN_RXD 18
#else
#define PIN_TXD 17 // UART 2 TXT - G17
#define PIN_RXD 16 // UART 2 RXD - G16 
#endif

static const char *TAG_HCI = "HCI-BUS";



class HoermannCommand
{
public:
    static const HoermannCommand STARTOPENDOOR;
    static const HoermannCommand STARTCLOSEDOOR;
    static const HoermannCommand STARTIMPULSE;
    static const HoermannCommand STARTOPENDOORHALF;
    static const HoermannCommand STARTVENTPOSITION;
    static const HoermannCommand STARTTOGGLELAMP;
    static const HoermannCommand WAITING;

public:
    uint16_t commandRegPlus2Value;
    uint16_t commandEndPlus2Value;
    uint16_t commandRegPlus3Value;
    uint16_t commandEndPlus3Value;

private:
    HoermannCommand(
        uint16_t commandRegPlus2Value,
        uint16_t commandEndPlus2Value,
        uint16_t commandRegPlus3Value,
        uint16_t commandEndPlus3Value)
    {
        this->commandRegPlus2Value = commandRegPlus2Value;
        this->commandEndPlus2Value = commandEndPlus2Value;
        this->commandRegPlus3Value = commandRegPlus3Value;
        this->commandEndPlus3Value = commandEndPlus3Value;
    }
};

class HoermannState
{
public:
    enum State
    {
        OPEN,
        OPENING,
        CLOSED,
        CLOSING,
        HALFOPEN,
        MOVE_VENTING,
        VENT,
        MOVE_HALF,
        STOPPED
    };
    // All position values are from 0 to 100
    float targetPosition = 0;
    float currentPosition = 0;
    bool lightOn = false;
    bool relayOn = false;
    State state = CLOSED;
    String debugMessage = "initial";
    unsigned long lastModbusRespone = 0;
    bool changed = false;
    bool debMessage = false;
    float gotoPosition = 0.0f;
    bool valid = false;

    void setTargetPosition(float targetPosition);
    void setGotoPosition(float setPosition);
    void setCurrentPosition(float currentPosition);
    void setLigthOn(bool lightOn);
    void setRelayOn(bool relayOn);
    void recordModbusResponse();
    void clearChanged();
    void clearDebug();
    long responseAge();
    void setState(State state);
    void setValid(bool isValid);

};

class HoermannGarageEngine
{
public:
    HoermannState *state = new HoermannState();

    static HoermannGarageEngine& getInstance();

    void setup(int8_t rx, int8_t tx, int8_t rts);
    
    // Modbus register access methods for use by modbus_server callbacks
    uint16_t getRegister9CB9(uint16_t offset);
    void setRegister9CB9(uint16_t offset, uint16_t value);
    uint16_t getRegister9C41(uint16_t offset);
    void setRegister9C41(uint16_t offset, uint16_t value);
    uint16_t getRegister9D31(uint16_t offset);
    void setRegister9D31(uint16_t offset, uint16_t value);
    
    // Callbacks for modbus server events
    void onModbusRequest();
    void setCommandValuesToRead();
    uint16_t onDoorPositionChanged(uint16_t val);
    uint16_t onCurrentStateChanged(uint16_t val);
    uint16_t onRegSevenChanged(uint16_t val);

    /**
     * Write on 0x9C41 , byte1: counter, byte2: command
     */
    uint16_t onCounterWrite(uint16_t val);

    /**
     * Helper to set next Command and *not* skip Current Command before end was sent
     */
    void setCommand(bool cond, const HoermannCommand *command);

    /**
     * Control Functions
     */
    void stopDoor();
    void closeDoor();
    void openDoor();
    void impulseDoor();
    void halfPositionDoor();
    void ventilationPositionDoor();
    void turnLight(bool on);
    void toggleLight();
    void setPosition(int setPosition);

private:
    HoermannGarageEngine(){};
    const HoermannCommand *nextCommand = nullptr; // Next Command to transmit
    unsigned long commandWrittenOn = 0;           // When was last command written (wait 100ms before end of command is transmitted)
    
    // Internal register storage (replacing ModbusRTU internal storage)
    uint16_t reg_9CB9[8] = {0};  // Internal State registers
    uint16_t reg_9C41[3] = {0};  // Command registers
    uint16_t reg_9D31[9] = {0};  // Broadcast registers
};
#endif
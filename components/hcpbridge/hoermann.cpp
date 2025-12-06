// Credits to https://github.com/Gifford47/HCPBridgeMqtt for the initial code base

#include "hoermann.h"

// arg1,arg4> command start value
// arg2,arg5> command end   value
const HoermannCommand HoermannCommand::STARTOPENDOOR = HoermannCommand(0x0210, 0x0110, 0x0000, 0x0000); // Typo 0201
const HoermannCommand HoermannCommand::STARTCLOSEDOOR = HoermannCommand(0x0220, 0x0120, 0x0000, 0x0000);
const HoermannCommand HoermannCommand::STARTIMPULSE = HoermannCommand(0x0240, 0x0140, 0x0000, 0x0000);
const HoermannCommand HoermannCommand::STARTOPENDOORHALF = HoermannCommand(0x0200, 0x0100, 0x0400, 0x0400);
const HoermannCommand HoermannCommand::STARTVENTPOSITION = HoermannCommand(0x0200, 0x0100, 0x4000, 0x4000);
const HoermannCommand HoermannCommand::STARTTOGGLELAMP = HoermannCommand(0x0100, 0x0800, 0x0200, 0x0200);
const HoermannCommand HoermannCommand::WAITING = HoermannCommand(0x0000, 0x0000, 0x0000, 0x0000);

HoermannGarageEngine &HoermannGarageEngine::getInstance()
{
  static HoermannGarageEngine instance;
  return instance;
}

void HoermannGarageEngine::setup(int8_t rx, int8_t tx, int8_t rts)
{
  // Setup is now handled by ESPHome's modbus_server component
  // The UART and modbus server will be configured in the YAML file
  ESP_LOGI(TAG_HCI, "HoermannGarageEngine initialized (modbus configured via YAML)");
}

// Register access methods for modbus_server callbacks
uint16_t HoermannGarageEngine::getRegister9CB9(uint16_t offset) {
  if (offset < 8) {
    return reg_9CB9[offset];
  }
  return 0;
}

void HoermannGarageEngine::setRegister9CB9(uint16_t offset, uint16_t value) {
  if (offset < 8) {
    reg_9CB9[offset] = value;
  }
}

uint16_t HoermannGarageEngine::getRegister9C41(uint16_t offset) {
  if (offset < 3) {
    return reg_9C41[offset];
  }
  return 0;
}

void HoermannGarageEngine::setRegister9C41(uint16_t offset, uint16_t value) {
  if (offset < 3) {
    reg_9C41[offset] = value;
  }
}

uint16_t HoermannGarageEngine::getRegister9D31(uint16_t offset) {
  if (offset < 9) {
    return reg_9D31[offset];
  }
  return 0;
}

void HoermannGarageEngine::setRegister9D31(uint16_t offset, uint16_t value) {
  if (offset < 9) {
    reg_9D31[offset] = value;
  }
}

void HoermannGarageEngine::onModbusRequest()
{
  this->state->recordModbusResponse();

  // Update internal state registers based on current command
  // This is called when the master reads from 0x9CB9, preparing the response registers
  setRegister9CB9(0, 0x0000);
  setRegister9CB9(1, 0x0001);
  setCommandValuesToRead();
  setRegister9CB9(4, 0x0000);
  setRegister9CB9(5, 0x0000);
  setRegister9CB9(6, 0x0000);
  setRegister9CB9(7, 0x0000);

  // Note: The old implementation had special handling for BusScan requests
  // (returning 0x0000, 0x0005, 0x0430, 0x10ff, 0xa845 for a specific request pattern).
  // With the new modbus_server approach, this would need to be detected based on
  // the write pattern to 0x9C41 if needed. BusScan appears to be a device discovery
  // feature that may only be used during initial setup.
  // TODO: If BusScan functionality is required, add detection logic here

  this->state->setValid(true);
}

void HoermannGarageEngine::setCommandValuesToRead()
{
  uint16_t regPlug2Value = 0x0000;
  uint16_t regPlug3Value = 0x0000;

  // Command was set
  if (nextCommand != nullptr)
  {
    // But not yet sent
    if (commandWrittenOn == 0)
    {
      // Send it
      regPlug2Value = nextCommand->commandRegPlus2Value;
      regPlug3Value = nextCommand->commandRegPlus3Value;
      ESP_LOGI(TAG_HCI, "command start %x %x", regPlug2Value, regPlug3Value);
      commandWrittenOn = millis();
      // It was written and it can be cleared
    }
    else if (commandWrittenOn != 0 && (commandWrittenOn + SIMULATEKEYPRESSDELAYMS) < millis())
    {
      regPlug2Value = nextCommand->commandEndPlus2Value;
      regPlug3Value = nextCommand->commandEndPlus3Value;
      ESP_LOGI(TAG_HCI, "command dispose %x %x", regPlug2Value, regPlug3Value);
      // Reset Variables
      commandWrittenOn = 0;
      nextCommand = nullptr;
    }
  }
  setRegister9CB9(2, regPlug2Value);
  setRegister9CB9(3, regPlug3Value);
}

uint16_t HoermannGarageEngine::onDoorPositionChanged(uint16_t val)
{
  uint16_t oldVal = getRegister9D31(1);
  
  // on First Byte changed (current)
  if ((oldVal & 0x00FF) != (val & 0x00FF))
  {
    this->state->setCurrentPosition((float)(val & 0x00FF) / 200.0f);
    if ((this->state->gotoPosition > 0.0f && this->state->state == HoermannState::State::CLOSING && this->state->gotoPosition >= this->state->currentPosition) ||
        (this->state->gotoPosition > 0.0f && this->state->state == HoermannState::State::OPENING && this->state->gotoPosition <= this->state->currentPosition))
    {
      this->stopDoor();
      this->state->setGotoPosition(0.0f);
    }
  }
  // on Second Byte changed (target)
  if ((oldVal & 0xFF00) != (val & 0xFF00))
  {
    this->state->setTargetPosition((float)((val & 0xFF00) >> 8) / 200.0f);
  }

  return val;
}

uint16_t HoermannGarageEngine::onCurrentStateChanged(uint16_t val)
{
  uint16_t oldVal = getRegister9D31(2);
  
  // on First Byte changed
  if ((oldVal & 0xFF00) != (val & 0xFF00))
  {
    ESP_LOGI(TAG_HCI, "onCurrentStateChanged. value=%x (actual: %x)", val, (val & 0xFF00) >> 8);

    switch ((val & 0xFF00) >> 8)
    {
    case 0x1:
      this->state->setState(HoermannState::State::OPENING);
      break;
    case 0x2:
      this->state->setState(HoermannState::State::CLOSING);
      break;
    case 0x20:
      this->state->setState(HoermannState::State::OPEN);
      break;
    case 0x40:
      this->state->setState(HoermannState::State::CLOSED);
      break;
    case 0x80:
      this->state->setState(HoermannState::State::HALFOPEN);
      break;
    case 0x09:
      this->state->setState(HoermannState::State::MOVE_VENTING);
      break;
    case 0x05:
      this->state->setState(HoermannState::State::MOVE_HALF);
      break;
    case 0x0A:
      this->state->setState(HoermannState::State::VENT);
      break;
    case 0x00:
      // Additional check on the low byte when the high byte is 0x00
      if ((val & 0x00FF) == 0x61) {
        this->state->setState(HoermannState::State::VENT);
      } else {
        this->state->setState(HoermannState::State::STOPPED);
      } 
      break;
    default:
      ESP_LOGW(TAG_HCI, "unknown State %x", (val & 0xFF00) >> 8);
    }
  }
  return val;
}

uint16_t HoermannGarageEngine::onRegSevenChanged(uint16_t val)
  //Observed Values, last bit 4 is assumed could not be tested as I have no UAP HCP.
  //0x00 0x00 Relay off - Light off
  //0x02 0x00 Relay on  - light off
  //0x02 0x10 Relay on  - light on
  //0x00 0x10 Relay off - light on
  //0x00 0x14 Relay on  - light on 
  //0x00 0x04 Relay on  - light off

{
  uint16_t oldVal = getRegister9D31(6);
  
  if ((oldVal & 0xFF00) != (val & 0xFF00)){
    // 0x02 happen when relay menu 30 is set to 06, 07, 10 
    this->state->setRelayOn((val & 0xFF00) >> 8 == 0x02);
  }
  // On second byte changed
  if ((oldVal & 0x00FF) != (val & 0x00FF))
  {
    ESP_LOGI(TAG_HCI, "onRegSixChanged. value=%x", val);
    this->state->setLigthOn((val & 0x00FF) == 0x14 || (val & 0x00FF) == 0x10);
    this->state->setRelayOn((val & 0xFF00) >> 8 == 0x02 || (val & 0x00FF) == 0x14 || (val & 0x00FF) == 0x04); 
  }
  return val;
}

/**
 * Write on 0x9C41 , byte1: counter, byte2: command
 */
uint16_t HoermannGarageEngine::onCounterWrite(uint16_t val)
{
  uint16_t counter = val & 0xFF00;
  uint16_t command = (val & 0x00FF) << 8;
  setRegister9CB9(0, getRegister9CB9(0) | counter);
  setRegister9CB9(1, getRegister9CB9(1) | command);
  return val;
}

/**
 * Helper to set next Command and *not* skip Current Command before end was sent
 */
void HoermannGarageEngine::setCommand(bool cond, const HoermannCommand *command)
{
  if (cond)
  {
    if (nextCommand != nullptr)
    {
      ESP_LOGW(TAG_HCI, "Last Command was not yet fetched by modbus!");
    }
    else
    {
      nextCommand = command;
    }
  }
}

/**
 * Control Functions
 */
void HoermannGarageEngine::stopDoor()
{
  //only send impulse if door is in a moving state
  setCommand( this->state->state == HoermannState::State::CLOSING || 
              this->state->state == HoermannState::State::OPENING ||
              this->state->state == HoermannState::State::MOVE_HALF ||
              this->state->state == HoermannState::State::MOVE_VENTING , &HoermannCommand::STARTIMPULSE);
}
void HoermannGarageEngine::closeDoor()
{
  setCommand(true, &HoermannCommand::STARTCLOSEDOOR);
}
void HoermannGarageEngine::openDoor()
{
  setCommand(true, &HoermannCommand::STARTOPENDOOR);
}
void HoermannGarageEngine::impulseDoor()
{
  setCommand(true, &HoermannCommand::STARTIMPULSE);
}
void HoermannGarageEngine::halfPositionDoor()
{
  setCommand(true, &HoermannCommand::STARTOPENDOORHALF);
}
void HoermannGarageEngine::ventilationPositionDoor()
{
  setCommand(true, &HoermannCommand::STARTVENTPOSITION);
}
void HoermannGarageEngine::turnLight(bool on)
{
  setCommand((on && !this->state->lightOn) || (!on && this->state->lightOn), &HoermannCommand::STARTTOGGLELAMP);
}
void HoermannGarageEngine::toggleLight()
{
  setCommand(true, &HoermannCommand::STARTTOGGLELAMP);
}
void HoermannGarageEngine::setPosition(int setPosition)
{
  // First and last movement segments seem a bit inconsistent on Promatic4, so it's better to leave it to fully open or close.
  if (setPosition <= 5)
    closeDoor();
  else if (setPosition >= 95)
    openDoor();
  else if ((setPosition > 5) && (setPosition < 95))
  {
    this->state->setGotoPosition(static_cast<float>(setPosition) / 100.0f);
    setCommand(this->state->currentPosition < this->state->gotoPosition, &HoermannCommand::STARTOPENDOOR);
    setCommand(this->state->currentPosition > this->state->gotoPosition, &HoermannCommand::STARTCLOSEDOOR);
  }
}

void HoermannState::setTargetPosition(float targetPosition)
{
  this->targetPosition = targetPosition;
  this->changed = true;
}
void HoermannState::setGotoPosition(float setPosition)
{
  this->gotoPosition = setPosition;
  this->changed = true;
}
void HoermannState::setCurrentPosition(float currentPosition)
{
  this->currentPosition = currentPosition;
  this->changed = true;
}
void HoermannState::setLigthOn(bool lightOn)
{
  this->lightOn = lightOn;
  this->changed = true;
}
void HoermannState::setRelayOn(bool relayOn)
{
  this->relayOn = relayOn;
  this->changed = true;
}
void HoermannState::recordModbusResponse()
{
  this->lastModbusRespone = millis();
}
void HoermannState::clearChanged()
{
  this->changed = false;
}
void HoermannState::clearDebug()
{
  this->debMessage = false;
  this->debugMessage = "Initial";
}
long HoermannState::responseAge()
{
  if (this->lastModbusRespone == 0)
  {
    return -1;
  }
  long diff = millis() - lastModbusRespone;
  if (diff < 0)
  {
    return -2;
  }
  return diff / 1000;
}
void HoermannState::setState(State state)
{
  this->state = state;
  this->changed = true;
}
void HoermannState::setValid(bool isValid)
{
  this->valid = isValid;
}

#include <string.h>
#include <unity.h>

#include "control.h"

void setUp(void) {}
void tearDown(void) {}

// The point of injecting the sink: the whole control layer is testable with no radio.
class FakeSink : public PacketSink {
 public:
  void send(const uint8_t *packet) override {
    memcpy(last, packet, V2_PACKET_LEN);
    count++;
  }
  uint8_t last[V2_PACKET_LEN] = {0};
  int count = 0;

  Packet decoded() const {
    Packet p;
    decodePacket(last, &p);
    return p;
  }
};

static void sends_on_and_updates_its_own_state(void) {
  FakeSink sink;
  Control control(sink, 0xabcd, 1);

  control.turnOn();

  TEST_ASSERT_EQUAL_INT(1, sink.count);
  const Packet sent = sink.decoded();
  TEST_ASSERT_EQUAL_HEX16(0xabcd, sent.deviceId);
  TEST_ASSERT_EQUAL_HEX8(V2_CMD_ON_OFF, sent.command);
  TEST_ASSERT_EQUAL_UINT8(v2OnArg(1), sent.argument);
  // A command we send must move our own state, or the light and the UI disagree.
  TEST_ASSERT_TRUE(control.state().on());
}

static void sends_off(void) {
  FakeSink sink;
  Control control(sink, 0xabcd, 1);
  control.turnOn();
  control.turnOff();
  TEST_ASSERT_EQUAL_UINT8(v2OffArg(1), sink.decoded().argument);
  TEST_ASSERT_FALSE(control.state().on());
}

static void brightness_is_clamped_before_transmission(void) {
  FakeSink sink;
  Control control(sink, 0xabcd, 1);
  control.setBrightness(200);
  TEST_ASSERT_EQUAL_UINT8(100, sink.decoded().argument);
}

static void colour_and_white(void) {
  FakeSink sink;
  Control control(sink, 0xabcd, 1);

  control.setHue(0x55);
  TEST_ASSERT_EQUAL_HEX8(V2_CMD_COLOR, sink.decoded().command);
  TEST_ASSERT_EQUAL(COLOUR_MODE_RGB, control.state().mode());

  control.setWhite();
  TEST_ASSERT_EQUAL_UINT8(V2_ARG_WHITE_MODE, sink.decoded().argument);
  TEST_ASSERT_EQUAL(COLOUR_MODE_WHITE, control.state().mode());
}

// A press on the physical remote arrives as a packet and must move state the same way a
// command of ours does — one path, so the two can never drift apart.
static void overheard_packets_update_state(void) {
  FakeSink sink;
  Control control(sink, 0xabcd, 1);

  Packet remote;
  remote.deviceId = 0xabcd;
  remote.group = 1;
  remote.command = V2_CMD_BRIGHTNESS;
  remote.argument = 70;
  remote.sequence = 5;
  remote.held = false;
  control.onReceived(remote);

  TEST_ASSERT_EQUAL_UINT8(70, control.state().brightness());
  TEST_ASSERT_EQUAL_INT(0, sink.count);  // overhearing must not transmit
}

// Someone else's light on the same protocol must not move our state.
static void ignores_other_devices(void) {
  FakeSink sink;
  Control control(sink, 0xabcd, 1);

  Packet other;
  other.deviceId = 0x1234;
  other.group = 1;
  other.command = V2_CMD_ON_OFF;
  other.argument = v2OnArg(1);
  other.sequence = 0;
  other.held = false;
  control.onReceived(other);

  TEST_ASSERT_FALSE(control.state().on());
}

// The speed keys ride on the on/off command with their own arguments, which is why they
// are not a separate command in the protocol.
static void effect_speed_buttons(void) {
  FakeSink sink;
  Control control(sink, 0xabcd, 1);

  control.effectFaster();
  TEST_ASSERT_EQUAL_HEX8(V2_CMD_ON_OFF, sink.decoded().command);
  TEST_ASSERT_EQUAL_HEX8(V2_ARG_SPEED_UP, sink.decoded().argument);

  control.effectSlower();
  TEST_ASSERT_EQUAL_HEX8(V2_ARG_SPEED_DOWN, sink.decoded().argument);
}

// They adjust a running effect, so they must not read as OFF in our own state either.
static void speed_does_not_turn_the_light_off(void) {
  FakeSink sink;
  Control control(sink, 0xabcd, 1);
  control.turnOn();
  control.effectFaster();
  TEST_ASSERT_TRUE(control.state().on());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(sends_on_and_updates_its_own_state);
  RUN_TEST(sends_off);
  RUN_TEST(brightness_is_clamped_before_transmission);
  RUN_TEST(colour_and_white);
  RUN_TEST(overheard_packets_update_state);
  RUN_TEST(ignores_other_devices);
  RUN_TEST(effect_speed_buttons);
  RUN_TEST(speed_does_not_turn_the_light_off);
  return UNITY_END();
}

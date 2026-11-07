#include "obd/decoders.hpp"

namespace {
// QByteArray::at() returns a signed char, OBD data bytes are unsigned
int byte(const Response &resp, int i) { return static_cast<quint8>(resp.data.at(i)); }
}

double percentage(Response resp) { return (100.0 / 255.0) * byte(resp, 0); }

double temp(Response resp) { return byte(resp, 0) - 40; }

double rpm(Response resp) { return ((256.0 * byte(resp, 0)) + byte(resp, 1)) / 4.0; }

double speed(Response resp) { return byte(resp, 0); }

double flow(Response resp) { return ((256.0 * byte(resp, 0)) + byte(resp, 1)) / 100.0; }

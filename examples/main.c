#include "bare.h"
#include "protocol.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static BareStatus server_handle(const uint8_t req_wire[], size_t req_len, uint8_t res_wire[],
                                size_t res_cap, size_t *res_len) {
  Request req = {0};
  BARE_TRY(request_decode(&req, req_wire, req_len));
  Response res = {0};
  switch (req.tag) {
  case RequestTag_PING:
    res.tag = ResponseTag_PONG;
    break;
  case RequestTag_READ_TEMPERATURE:
    res.tag = ResponseTag_TEMPERATURE;
    res.value.temperature.device = req.value.read_temperature.device;
    res.value.temperature.reading = 21.5F;
    res.value.temperature.timestamp = 1757203200;
    break;
  case RequestTag_SET_LABEL:
    if (req.value.set_label.label.len == 0) {
      res.tag = ResponseTag_ERROR;
      BARE_STR_LIT(res.value.error.message, "label must not be empty");
    } else {
      res.tag = ResponseTag_OK;
    }
    break;
  }
  return response_encode(&res, res_wire, res_cap, res_len);
}

static void print_response(const Response *res) {
  switch (res->tag) {
  case ResponseTag_PONG:
    printf("client: pong\n");
    break;
  case ResponseTag_TEMPERATURE:
    printf("client: device %u reads %.1f degrees at %llu\n", res->value.temperature.device,
           (double)res->value.temperature.reading,
           (unsigned long long)res->value.temperature.timestamp);
    break;
  case ResponseTag_OK:
    printf("client: ok\n");
    break;
  case ResponseTag_ERROR:
    printf("client: error: %.*s\n", BARE_STR_ARG(&res->value.error.message));
    break;
  }
}

static int exchange(const Request *req) {
  uint8_t req_wire[256];
  uint8_t res_wire[256];
  size_t req_len = 0;
  size_t res_len = 0;
  if (request_encode(req, req_wire, sizeof(req_wire), &req_len) != BareStatus_OK) {
    return 1;
  }
  printf("client -> server: %zu bytes\n", req_len);
  if (server_handle(req_wire, req_len, res_wire, sizeof(res_wire), &res_len) != BareStatus_OK) {
    return 1;
  }
  printf("server -> client: %zu bytes\n", res_len);
  Response res = {0};
  if (response_decode(&res, res_wire, res_len) != BareStatus_OK) {
    return 1;
  }
  print_response(&res);
  return 0;
}

int main(void) {
  Request ping = {0};
  ping.tag = RequestTag_PING;

  Request read_temp = {0};
  read_temp.tag = RequestTag_READ_TEMPERATURE;
  read_temp.value.read_temperature.device = 7;

  Request set_label = {
      .tag = RequestTag_SET_LABEL,
      .value.set_label = {.device = 7, .label = BARE_STR64("greenhouse")},
  };

  Request bad_label = {0};
  bad_label.tag = RequestTag_SET_LABEL;
  bad_label.value.set_label.device = 7;

  return exchange(&ping) + exchange(&read_temp) + exchange(&set_label) + exchange(&bad_label);
}

#include "utils.h"
#include <amqp.h>
#include <amqp_framing.h>
#include <amqp_tcp_socket.h>
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef MAX_SECRET_LENGTH
#define MAX_SECRET_LENGTH 100
#endif
static void get_mac(char *mac_address, const char *interface);
static void get_temp(float *j);
static void mark_temp(char const *hostname, int port, char const *user,
                      char const *pass, char *mac);
static void get_user_pass(char *user, char *pass, const char *file_name);
static void get_interface(char *interface);
void send_message(amqp_connection_state_t conn, char *name, float value);
// host port value
int main(int argc, char const *const *argv) {

  float j = 0;
  get_temp(&j);
  const char *hostname = "rabbit.centurionx.net";
  char user[MAX_SECRET_LENGTH];
  char pass[MAX_SECRET_LENGTH];
  char interface[MAX_SECRET_LENGTH];

  get_interface(interface);
  int port = 5672;

  get_user_pass(user, pass, "/home/node/usagesecrets");

  char mac[15] = {};
  get_mac(mac, interface);

  long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);

  while (1) {
    mark_temp(hostname, port, user, pass, mac);
    sleep(60 * 5);
  }
}

void mark_temp(char const *hostname, int port, char const *user,
               char const *pass, char *mac) {
  amqp_socket_t *socket = NULL;
  amqp_connection_state_t conn;
  int status;
  conn = amqp_new_connection();

  socket = amqp_tcp_socket_new(conn);
  if (!socket) {
    die("died creating TCP socket");
  }
  status = amqp_socket_open(socket, hostname, port);
  if (status) {
    die("died opening TCP socket");
  }

  die_on_amqp_error(
      amqp_login(conn, "/", 0, 4096, 0, AMQP_SASL_METHOD_PLAIN, user, pass),
      "Logging in");

  float temperature;

  get_temp(&temperature);

  amqp_channel_open(conn, 1);
  die_on_amqp_error(amqp_get_rpc_reply(conn), "Opening channel");

  send_message(conn, mac, temperature);
  die_on_amqp_error(amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS),
                    "Closing channel");
  die_on_amqp_error(amqp_connection_close(conn, AMQP_REPLY_SUCCESS),
                    "Closing connection");
  die_on_error(amqp_destroy_connection(conn), "Ending connection");
}

void get_temp(float *temperature) {
  FILE *fptr;
  fptr = fopen("/sys/class/thermal/thermal_zone0/temp", "r");

  if (!fptr) {
    printf("Temp file wasn't there");
    exit(-1);
  }

  fscanf(fptr, "%f", temperature);

  fclose(fptr);
  *temperature = (*temperature) / 1000;
}

void send_message(amqp_connection_state_t conn, char *name, float value) {
  char message[255];
  amqp_basic_properties_t props;

  time_t utc_now = time(NULL);
  sprintf(message, "{'HostName':'%s', 'TimeStamp': %lu,'Temperature': %f}",
          name, utc_now, value);

  props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
  props.content_type = amqp_cstring_bytes("text/plain");
  props.delivery_mode = 2; /* persistent delivery mode */
  die_on_error(amqp_basic_publish(conn, 1, amqp_cstring_bytes("InterTopic"),
                                  amqp_cstring_bytes("node.temp"), 0, 0, &props,
                                  amqp_cstring_bytes(message)),
               "Publishing");
}

static void get_user_pass(char *user, char *pass, const char *file_name) {
  FILE *fptr;
  fptr = fopen(file_name, "r");

  if (!fptr) {
    printf("Secret file {%s} doesn't exist", file_name);
    exit(-2);
  }

  // read user
  if (!fgets(user, MAX_SECRET_LENGTH, fptr)) {
    printf("Secret file is empty?");
    exit(-3);
  }
  user[strcspn(user, "\n")] = 0;
  // read pass
  if (!fgets(pass, MAX_SECRET_LENGTH, fptr)) {
    printf("Secret file is too short?");
    exit(-4);
  }
  pass[strcspn(pass, "\n")] = 0;

  fclose(fptr);
}

static void get_mac(char *mac_address, const char *interface) {
  FILE *fptr;
  char unfiltered[19] = {};
  char file_name[100];

  sprintf(file_name, "/sys/class/net/%s/address", interface);

  fptr = fopen(file_name, "r");

  if (fptr) {
    fgets(unfiltered, 18, fptr);
    fclose(fptr);
  } else {
    printf("Interface %s doesn't exist", interface);
    exit(-1);
  }
  int output_index = 0;
  for (int i = 0; i < 18; i++) {
    if ((i % 3) == 2) {
      i++;
    }
    mac_address[output_index++] = unfiltered[i];
  }
}

static void get_interface(char *interface) {
  DIR *dr;
  struct dirent *current;
  dr = opendir("/sys/class/net/");

  if (!dr) {
    printf("What even happened? why is there no /sys/class/net ?");
    exit(-2);
  }

  while ((current = readdir(dr))) {
    if (strcmp(current->d_name, "wlan0") == 0) {
      strcpy(interface, "wlan0");
      interface = "wlan0";
      continue;
    }
    if (strcmp(current->d_name, "eth0") == 0) {
      strcpy(interface, "eth0");
      closedir(dr);
      return;
    }
  }

  if (strcmp(interface, "wlan0") == 0) {
    // This is good enough
    closedir(dr);
    return;
  }

  closedir(dr);
  printf("wlan0 and eth0 couldn't be found");

  exit(-4);
}

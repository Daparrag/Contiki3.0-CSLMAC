#include "contiki.h"
#include <stdio.h>

#ifdef IOTLAB_M3
#include "dev/light-sensor.h"
#include "dev/pressure-sensor.h"
#endif
#include "dev/acc-mag-sensor.h"
#include "dev/gyr-sensor.h"

#include "dev/leds.h"

/*
 * Print the value of each available sensors once every second.
 */


PROCESS(sensor_collection, "Sensors collection");
AUTOSTART_PROCESSES(&sensor_collection);

#ifdef IOTLAB_M3

/* Light sensor */
static void config_light()
{
  light_sensor.configure(LIGHT_SENSOR_SOURCE, ISL29020_LIGHT__AMBIENT);
  light_sensor.configure(LIGHT_SENSOR_RESOLUTION, ISL29020_RESOLUTION__16bit);
  light_sensor.configure(LIGHT_SENSOR_RANGE, ISL29020_RANGE__1000lux);
  SENSORS_ACTIVATE(light_sensor);
}
static void process_light()
{
  int light_val = light_sensor.value(0);
  float light = ((float)light_val) / LIGHT_SENSOR_VALUE_SCALE;
  printf("light: %f lux\n", light);
}

/* Pressure */
static void config_pressure()
{
  pressure_sensor.configure(PRESSURE_SENSOR_DATARATE, LPS331AP_P_12_5HZ_T_1HZ);
  SENSORS_ACTIVATE(pressure_sensor);
}

static void process_pressure()
{
  int pressure;
  pressure = pressure_sensor.value(0);
  printf("press: %f mbar\n", (float)pressure / PRESSURE_SENSOR_VALUE_SCALE);
}


#endif

/* Accelerometer / magnetometer */
static unsigned acc_freq = 0;
static void config_acc()
{
  acc_sensor.configure(ACC_MAG_SENSOR_DATARATE,
      LSM303DLHC_ACC_RATE_1344HZ_N_5376HZ_LP);
  acc_freq = 1344;
  acc_sensor.configure(ACC_MAG_SENSOR_SCALE, LSM303DLHC_ACC_SCALE_2G);
  acc_sensor.configure(ACC_MAG_SENSOR_MODE, LSM303DLHC_ACC_UPDATE_ON_READ);
  SENSORS_ACTIVATE(acc_sensor);
}

static void process_acc()
{
  int xyz[3];
  static unsigned count = 0;
  if ((++count % acc_freq) == 0) {
    xyz[0] = acc_sensor.value(ACC_MAG_SENSOR_X);
    xyz[1] = acc_sensor.value(ACC_MAG_SENSOR_Y);
    xyz[2] = acc_sensor.value(ACC_MAG_SENSOR_Z);

    printf("accel: %d %d %d xyz mg\n", xyz[0], xyz[1], xyz[2]);
  }
}

static unsigned mag_freq = 0;
static void config_mag()
{
  mag_sensor.configure(ACC_MAG_SENSOR_DATARATE, LSM303DLHC_MAG_RATE_220HZ);
  mag_freq = 220;
  mag_sensor.configure(ACC_MAG_SENSOR_SCALE, LSM303DLHC_MAG_SCALE_1_3GAUSS);
  mag_sensor.configure(ACC_MAG_SENSOR_MODE, LSM303DLHC_MAG_MODE_CONTINUOUS);
  SENSORS_ACTIVATE(mag_sensor);
}

static void process_mag()
{
  int xyz[3];
  static unsigned count = 0;
  if ((++count % mag_freq) == 0) {
    xyz[0] = mag_sensor.value(ACC_MAG_SENSOR_X);
    xyz[1] = mag_sensor.value(ACC_MAG_SENSOR_Y);
    xyz[2] = mag_sensor.value(ACC_MAG_SENSOR_Z);

    printf("magne: %d %d %d xyz mgauss\n", xyz[0], xyz[1], xyz[2]);
  }
}

/* Gyroscope */
static unsigned gyr_freq = 0;
static void config_gyr()
{
  gyr_sensor.configure(GYR_SENSOR_DATARATE, L3G4200D_800HZ);
  gyr_freq = 800;
  gyr_sensor.configure(GYR_SENSOR_SCALE, L3G4200D_250DPS);
  SENSORS_ACTIVATE(gyr_sensor);
}

static void process_gyr()
{
  int xyz[3];
  static unsigned count = 0;
  if ((++count % gyr_freq) == 0) {
    xyz[0] = gyr_sensor.value(GYR_SENSOR_X);
    xyz[1] = gyr_sensor.value(GYR_SENSOR_Y);
    xyz[2] = gyr_sensor.value(GYR_SENSOR_Z);
    printf("gyros: %d %d %d xyz m°/s\n", xyz[0], xyz[1], xyz[2]);
  }
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(sensor_collection, ev, data)
{
  PROCESS_BEGIN();
  static struct etimer timer;

#ifdef IOTLAB_M3
  config_light();
  config_pressure();
#endif

  config_acc();
  config_mag();
  config_gyr();

  etimer_set(&timer, CLOCK_SECOND);

  while(1) {
    PROCESS_WAIT_EVENT();
    if (ev == PROCESS_EVENT_TIMER) {
#ifdef IOTLAB_M3
      process_light();
      process_pressure();
#endif

      etimer_restart(&timer);
    } else if (ev == sensors_event && data == &acc_sensor) {
      process_acc();
    } else if (ev == sensors_event && data == &mag_sensor) {
      process_mag();
    } else if (ev == sensors_event && data == &gyr_sensor) {
      process_gyr();
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

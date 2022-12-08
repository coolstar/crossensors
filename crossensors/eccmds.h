#ifndef __CROS_EC_REGS_H__
#define __CROS_EC_REGS_H__

#define BIT(nr) (1UL << (nr))

/* Command version mask */
#define EC_VER_MASK(version) (1UL << (version))

#define EC_FEATURE_MOTION_SENSE 6

/*****************************************************************************/
/*
 * Motion sense commands. We'll make separate structs for sub-commands with
 * different input args, so that we know how much to expect.
 */
#define EC_CMD_MOTION_SENSE_CMD 0x002B

 /* Motion sense commands */
enum motionsense_command {
	/*
	* Dump command returns all motion sensor data including motion sense
	* module flags and individual sensor flags.
	*/
	MOTIONSENSE_CMD_DUMP = 0,

	/*
	* Info command returns data describing the details of a given sensor,
	* including enum motionsensor_type, enum motionsensor_location, and
	* enum motionsensor_chip.
	*/
	MOTIONSENSE_CMD_INFO = 1,

	/*
	* EC Rate command is a setter/getter command for the EC sampling rate
	* in milliseconds.
	* It is per sensor, the EC run sample task  at the minimum of all
	* sensors EC_RATE.
	* For sensors without hardware FIFO, EC_RATE should be equals to 1/ODR
	* to collect all the sensor samples.
	* For sensor with hardware FIFO, EC_RATE is used as the maximal delay
	* to process of all motion sensors in milliseconds.
	*/
	MOTIONSENSE_CMD_EC_RATE = 2,

	/*
	* Sensor ODR command is a setter/getter command for the output data
	* rate of a specific motion sensor in millihertz.
	*/
	MOTIONSENSE_CMD_SENSOR_ODR = 3,

	/*
	* Sensor range command is a setter/getter command for the range of
	* a specified motion sensor in +/-G's or +/- deg/s.
	*/
	MOTIONSENSE_CMD_SENSOR_RANGE = 4,

	/*
	* Setter/getter command for the keyboard wake angle. When the lid
	* angle is greater than this value, keyboard wake is disabled in S3,
	* and when the lid angle goes less than this value, keyboard wake is
	* enabled. Note, the lid angle measurement is an approximate,
	* un-calibrated value, hence the wake angle isn't exact.
	*/
	MOTIONSENSE_CMD_KB_WAKE_ANGLE = 5,

	/*
	* Returns a single sensor data.
	*/
	MOTIONSENSE_CMD_DATA = 6,

	/*
	* Return sensor fifo info.
	*/
	MOTIONSENSE_CMD_FIFO_INFO = 7,

	/*
	* Insert a flush element in the fifo and return sensor fifo info.
	* The host can use that element to synchronize its operation.
	*/
	MOTIONSENSE_CMD_FIFO_FLUSH = 8,

	/*
	* Return a portion of the fifo.
	*/
	MOTIONSENSE_CMD_FIFO_READ = 9,

	/*
	* Perform low level calibration.
	* On sensors that support it, ask to do offset calibration.
	*/
	MOTIONSENSE_CMD_PERFORM_CALIB = 10,

	/*
	* Sensor Offset command is a setter/getter command for the offset
	* used for factory calibration.
	* The offsets can be calculated by the host, or via
	* PERFORM_CALIB command.
	*/
	MOTIONSENSE_CMD_SENSOR_OFFSET = 11,

	/*
	* List available activities for a MOTION sensor.
	* Indicates if they are enabled or disabled.
	*/
	MOTIONSENSE_CMD_LIST_ACTIVITIES = 12,

	/*
	* Activity management
	* Enable/Disable activity recognition.
	*/
	MOTIONSENSE_CMD_SET_ACTIVITY = 13,

	/*
	* Lid Angle
	*/
	MOTIONSENSE_CMD_LID_ANGLE = 14,

	/*
	* Allow the FIFO to trigger interrupt via MKBP events.
	* By default the FIFO does not send interrupt to process the FIFO
	* until the AP is ready or it is coming from a wakeup sensor.
	*/
	MOTIONSENSE_CMD_FIFO_INT_ENABLE = 15,

	/*
	* Spoof the readings of the sensors.  The spoofed readings can be set
	* to arbitrary values, or will lock to the last read actual values.
	*/
	MOTIONSENSE_CMD_SPOOF = 16,

	/* Set lid angle for tablet mode detection. */
	MOTIONSENSE_CMD_TABLET_MODE_LID_ANGLE = 17,

	/*
		* Sensor Scale command is a setter/getter command for the calibration
		* scale.
		*/
		MOTIONSENSE_CMD_SENSOR_SCALE = 18,

		/*
		* Read the current online calibration values (if available).
		*/
		MOTIONSENSE_CMD_ONLINE_CALIB_READ = 19,

		/*
		* Activity management
		* Retrieve current status of given activity.
		*/
		MOTIONSENSE_CMD_GET_ACTIVITY = 20,

		/* Number of motionsense sub-commands. */
		MOTIONSENSE_NUM_CMDS,
};

/* List of motion sensor types. */
enum motionsensor_type {
	MOTIONSENSE_TYPE_ACCEL = 0,
	MOTIONSENSE_TYPE_GYRO = 1,
	MOTIONSENSE_TYPE_MAG = 2,
	MOTIONSENSE_TYPE_PROX = 3,
	MOTIONSENSE_TYPE_LIGHT = 4,
	MOTIONSENSE_TYPE_ACTIVITY = 5,
	MOTIONSENSE_TYPE_BARO = 6,
	MOTIONSENSE_TYPE_SYNC = 7,
	MOTIONSENSE_TYPE_LIGHT_RGB = 8,
	MOTIONSENSE_TYPE_MAX,
};

/* List of motion sensor locations. */
enum motionsensor_location {
	MOTIONSENSE_LOC_BASE = 0,
	MOTIONSENSE_LOC_LID = 1,
	MOTIONSENSE_LOC_CAMERA = 2,
	MOTIONSENSE_LOC_MAX,
};

/* List of motion sensor chips. */
enum motionsensor_chip {
	MOTIONSENSE_CHIP_KXCJ9 = 0,
	MOTIONSENSE_CHIP_LSM6DS0 = 1,
	MOTIONSENSE_CHIP_BMI160 = 2,
	MOTIONSENSE_CHIP_SI1141 = 3,
	MOTIONSENSE_CHIP_SI1142 = 4,
	MOTIONSENSE_CHIP_SI1143 = 5,
	MOTIONSENSE_CHIP_KX022 = 6,
	MOTIONSENSE_CHIP_L3GD20H = 7,
	MOTIONSENSE_CHIP_BMA255 = 8,
	MOTIONSENSE_CHIP_BMP280 = 9,
	MOTIONSENSE_CHIP_OPT3001 = 10,
	MOTIONSENSE_CHIP_BH1730 = 11,
	MOTIONSENSE_CHIP_GPIO = 12,
	MOTIONSENSE_CHIP_LIS2DH = 13,
	MOTIONSENSE_CHIP_LSM6DSM = 14,
	MOTIONSENSE_CHIP_LIS2DE = 15,
	MOTIONSENSE_CHIP_LIS2MDL = 16,
	MOTIONSENSE_CHIP_LSM6DS3 = 17,
	MOTIONSENSE_CHIP_LSM6DSO = 18,
	MOTIONSENSE_CHIP_LNG2DM = 19,
	MOTIONSENSE_CHIP_TCS3400 = 20,
	MOTIONSENSE_CHIP_LIS2DW12 = 21,
	MOTIONSENSE_CHIP_LIS2DWL = 22,
	MOTIONSENSE_CHIP_LIS2DS = 23,
	MOTIONSENSE_CHIP_BMI260 = 24,
	MOTIONSENSE_CHIP_ICM426XX = 25,
	MOTIONSENSE_CHIP_ICM42607 = 26,
	MOTIONSENSE_CHIP_BMA422 = 27,
	MOTIONSENSE_CHIP_BMI323 = 28,
	MOTIONSENSE_CHIP_BMI220 = 29,
	MOTIONSENSE_CHIP_CM32183 = 30,
	MOTIONSENSE_CHIP_MAX,
};

/* List of orientation positions */
enum motionsensor_orientation {
	MOTIONSENSE_ORIENTATION_LANDSCAPE = 0,
	MOTIONSENSE_ORIENTATION_PORTRAIT = 1,
	MOTIONSENSE_ORIENTATION_UPSIDE_DOWN_PORTRAIT = 2,
	MOTIONSENSE_ORIENTATION_UPSIDE_DOWN_LANDSCAPE = 3,
	MOTIONSENSE_ORIENTATION_UNKNOWN = 4,
};

#include <pshpack1.h>
struct ec_response_activity_data {
	UINT8 activity; /* motionsensor_activity */
	UINT8 state;
};

struct ec_response_motion_sensor_data {
	/* Flags for each sensor. */
	UINT8 flags;
	/* Sensor number the data comes from. */
	UINT8 sensor_num;
	/* Each sensor is up to 3-axis. */
	union {
		INT16 data[3];
		/* for sensors using unsigned data */
		UINT16 udata[3];
		struct {
			UINT16 reserved;
			UINT32 timestamp;
		};
		struct {
			struct ec_response_activity_data activity_data;
			INT16 add_info[2];
		};
	};
};
#include <poppack.h>

/* Response to AP reporting calibration data for a given sensor. */
struct ec_response_online_calibration_data {
	/** The calibration values. */
	INT16 data[3];
};

#include <pshpack1.h>
/* Note: used in ec_response_get_next_data */
struct ec_response_motion_sense_fifo_info {
	/* Size of the fifo */
	UINT16 size;
	/* Amount of space used in the fifo */
	UINT16 count;
	/* Timestamp recorded in us.
	 * aka accurate timestamp when host event was triggered.
	 */
	UINT32 timestamp;
	/* Total amount of vector lost */
	UINT16 total_lost;
	/* Lost events since the last fifo_info, per sensors */
	UINT16 lost[0];
};

struct ec_response_motion_sense_fifo_data {
	UINT32 number_data;
	struct ec_response_motion_sensor_data data[0];
};
#include <poppack.h>

/* List supported activity recognition */
enum motionsensor_activity {
	MOTIONSENSE_ACTIVITY_RESERVED = 0,
	MOTIONSENSE_ACTIVITY_SIG_MOTION = 1,
	MOTIONSENSE_ACTIVITY_DOUBLE_TAP = 2,
	MOTIONSENSE_ACTIVITY_ORIENTATION = 3,
	MOTIONSENSE_ACTIVITY_BODY_DETECTION = 4,
};

#include <pshpack1.h>
struct ec_motion_sense_activity {
	UINT8 sensor_num;
	UINT8 activity; /* one of enum motionsensor_activity */
	UINT8 enable; /* 1: enable, 0: disable */
	UINT8 reserved;
	UINT16 parameters[3]; /* activity dependent parameters */
};
#include <poppack.h>

/* Module flag masks used for the dump sub-command. */
#define MOTIONSENSE_MODULE_FLAG_ACTIVE BIT(0)

/* Sensor flag masks used for the dump sub-command. */
#define MOTIONSENSE_SENSOR_FLAG_PRESENT BIT(0)

/*
 * Flush entry for synchronization.
 * data contains time stamp
 */
#define MOTIONSENSE_SENSOR_FLAG_FLUSH BIT(0)
#define MOTIONSENSE_SENSOR_FLAG_TIMESTAMP BIT(1)
#define MOTIONSENSE_SENSOR_FLAG_WAKEUP BIT(2)
#define MOTIONSENSE_SENSOR_FLAG_TABLET_MODE BIT(3)
#define MOTIONSENSE_SENSOR_FLAG_ODR BIT(4)

#define MOTIONSENSE_SENSOR_FLAG_BYPASS_FIFO BIT(7)

 /*
  * Send this value for the data element to only perform a read. If you
  * send any other value, the EC will interpret it as data to set and will
  * return the actual value set.
  */
#define EC_MOTION_SENSE_NO_VALUE -1

#define EC_MOTION_SENSE_INVALID_CALIB_TEMP 0x8000

  /* MOTIONSENSE_CMD_SENSOR_OFFSET subcommand flag */
  /* Set Calibration information */
#define MOTION_SENSE_SET_OFFSET BIT(0)

/* Default Scale value, factor 1. */
#define MOTION_SENSE_DEFAULT_SCALE BIT(15)

#define LID_ANGLE_UNRELIABLE 500

enum motionsense_spoof_mode {
	/* Disable spoof mode. */
	MOTIONSENSE_SPOOF_MODE_DISABLE = 0,

	/* Enable spoof mode, but use provided component values. */
	MOTIONSENSE_SPOOF_MODE_CUSTOM,

	/* Enable spoof mode, but use the current sensor values. */
	MOTIONSENSE_SPOOF_MODE_LOCK_CURRENT,

	/* Query the current spoof mode status for the sensor. */
	MOTIONSENSE_SPOOF_MODE_QUERY,
};

#include <pshpack1.h>
struct ec_params_motion_sense {
	UINT8 cmd;
	union {
		/* Used for MOTIONSENSE_CMD_DUMP. */
		struct {
			/*
			 * Maximal number of sensor the host is expecting.
			 * 0 means the host is only interested in the number
			 * of sensors controlled by the EC.
			 */
			UINT8 max_sensor_count;
		} dump;

		/*
		 * Used for MOTIONSENSE_CMD_KB_WAKE_ANGLE.
		 */
		struct {
			/* Data to set or EC_MOTION_SENSE_NO_VALUE to read.
			 * kb_wake_angle: angle to wakup AP.
			 */
			INT16 data;
		} kb_wake_angle;

		/*
		 * Used for MOTIONSENSE_CMD_INFO, MOTIONSENSE_CMD_DATA
		 */
		struct {
			UINT8 sensor_num;
		} info, info_3, info_4, data, fifo_flush, list_activities;

		/*
		 * Used for MOTIONSENSE_CMD_PERFORM_CALIB:
		 * Allow entering/exiting the calibration mode.
		 */
		struct {
			UINT8 sensor_num;
			UINT8 enable;
		} perform_calib;

		/*
		 * Used for MOTIONSENSE_CMD_EC_RATE, MOTIONSENSE_CMD_SENSOR_ODR
		 * and MOTIONSENSE_CMD_SENSOR_RANGE.
		 */
		struct {
			UINT8 sensor_num;

			/* Rounding flag, true for round-up, false for down. */
			UINT8 roundup;

			UINT16 reserved;

			/* Data to set or EC_MOTION_SENSE_NO_VALUE to read. */
			INT32 data;
		} ec_rate, sensor_odr, sensor_range;

		/* Used for MOTIONSENSE_CMD_SENSOR_OFFSET */
		struct {
			UINT8 sensor_num;

			/*
			 * bit 0: If set (MOTION_SENSE_SET_OFFSET), set
			 * the calibration information in the EC.
			 * If unset, just retrieve calibration information.
			 */
			UINT16 flags;

			/*
			 * Temperature at calibration, in units of 0.01 C
			 * 0x8000: invalid / unknown.
			 * 0x0: 0C
			 * 0x7fff: +327.67C
			 */
			INT16 temp;

			/*
			 * Offset for calibration.
			 * Unit:
			 * Accelerometer: 1/1024 g
			 * Gyro:          1/1024 deg/s
			 * Compass:       1/16 uT
			 */
			INT16 offset[3];
		} sensor_offset;

		/* Used for MOTIONSENSE_CMD_SENSOR_SCALE */
		struct {
			UINT8 sensor_num;

			/*
			 * bit 0: If set (MOTION_SENSE_SET_OFFSET), set
			 * the calibration information in the EC.
			 * If unset, just retrieve calibration information.
			 */
			UINT16 flags;

			/*
			 * Temperature at calibration, in units of 0.01 C
			 * 0x8000: invalid / unknown.
			 * 0x0: 0C
			 * 0x7fff: +327.67C
			 */
			INT16 temp;

			/*
			 * Scale for calibration:
			 * By default scale is 1, it is encoded on 16bits:
			 * 1 = BIT(15)
			 * ~2 = 0xFFFF
			 * ~0 = 0.
			 */
			UINT16 scale[3];
		} sensor_scale;

		/* Used for MOTIONSENSE_CMD_FIFO_INFO */
		/* (no params) */

		/* Used for MOTIONSENSE_CMD_FIFO_READ */
		struct {
			/*
			 * Number of expected vector to return.
			 * EC may return less or 0 if none available.
			 */
			UINT32 max_data_vector;
		} fifo_read;

		/* Used for MOTIONSENSE_CMD_SET_ACTIVITY */
		struct ec_motion_sense_activity set_activity;

		/* Used for MOTIONSENSE_CMD_LID_ANGLE */
		/* (no params) */

		/* Used for MOTIONSENSE_CMD_FIFO_INT_ENABLE */
		struct {
			/*
			 * 1: enable, 0 disable fifo,
			 * EC_MOTION_SENSE_NO_VALUE return value.
			 */
			INT8 enable;
		} fifo_int_enable;

		/* Used for MOTIONSENSE_CMD_SPOOF */
		struct {
			UINT8 sensor_id;

			/* See enum motionsense_spoof_mode. */
			UINT8 spoof_enable;

			/* Ignored, used for alignment. */
			UINT8 reserved;

			union {
				/* Individual component values to spoof. */
				INT16 components[3];

				/* Used when spoofing an activity */
				struct {
					/* enum motionsensor_activity */
					UINT8 activity_num;

					/* spoof activity state */
					UINT8 activity_state;
				};
			};
		} spoof;

		/* Used for MOTIONSENSE_CMD_TABLET_MODE_LID_ANGLE. */
		struct {
			/*
			 * Lid angle threshold for switching between tablet and
			 * clamshell mode.
			 */
			INT16 lid_angle;

			/*
			 * Hysteresis degree to prevent fluctuations between
			 * clamshell and tablet mode if lid angle keeps
			 * changing around the threshold. Lid motion driver will
			 * use lid_angle + hys_degree to trigger tablet mode and
			 * lid_angle - hys_degree to trigger clamshell mode.
			 */
			INT16 hys_degree;
		} tablet_mode_threshold;

		/*
		 * Used for MOTIONSENSE_CMD_ONLINE_CALIB_READ:
		 * Allow reading a single sensor's online calibration value.
		 */
		struct {
			UINT8 sensor_num;
		} online_calib_read;

		/*
		 * Used for MOTIONSENSE_CMD_GET_ACTIVITY.
		 */
		struct {
			UINT8 sensor_num;
			UINT8 activity; /* enum motionsensor_activity */
		} get_activity;
	};
};
#include <poppack.h>

enum motion_sense_cmd_info_flags {
	/* The sensor supports online calibration */
	MOTION_SENSE_CMD_INFO_FLAG_ONLINE_CALIB = BIT(0),
};

#include <pshpack1.h>
struct ec_response_motion_sense {
	union {
		/* Used for MOTIONSENSE_CMD_DUMP */
		struct {
			/* Flags representing the motion sensor module. */
			UINT8 module_flags;

			/* Number of sensors managed directly by the EC. */
			UINT8 sensor_count;

			/*
			 * Sensor data is truncated if response_max is too small
			 * for holding all the data.
			 */
			struct ec_response_motion_sensor_data sensor[0];
		} dump;

		/* Used for MOTIONSENSE_CMD_INFO. */
		struct {
			/* Should be element of enum motionsensor_type. */
			UINT8 type;

			/* Should be element of enum motionsensor_location. */
			UINT8 location;

			/* Should be element of enum motionsensor_chip. */
			UINT8 chip;
		} info;

		/* Used for MOTIONSENSE_CMD_INFO version 3 */
		struct {
			/* Should be element of enum motionsensor_type. */
			UINT8 type;

			/* Should be element of enum motionsensor_location. */
			UINT8 location;

			/* Should be element of enum motionsensor_chip. */
			UINT8 chip;

			/* Minimum sensor sampling frequency */
			UINT32 min_frequency;

			/* Maximum sensor sampling frequency */
			UINT32 max_frequency;

			/* Max number of sensor events that could be in fifo */
			UINT32 fifo_max_event_count;
		} info_3;

		/* Used for MOTIONSENSE_CMD_INFO version 4 */
		struct {
			/* Should be element of enum motionsensor_type. */
			UINT8 type;

			/* Should be element of enum motionsensor_location. */
			UINT8 location;

			/* Should be element of enum motionsensor_chip. */
			UINT8 chip;

			/* Minimum sensor sampling frequency */
			UINT32 min_frequency;

			/* Maximum sensor sampling frequency */
			UINT32 max_frequency;

			/* Max number of sensor events that could be in fifo */
			UINT32 fifo_max_event_count;

			/*
			 * Should be elements of
			 * enum motion_sense_cmd_info_flags
			 */
			UINT32 flags;
		} info_4;

		/* Used for MOTIONSENSE_CMD_DATA */
		struct ec_response_motion_sensor_data data;

		/*
		 * Used for MOTIONSENSE_CMD_EC_RATE, MOTIONSENSE_CMD_SENSOR_ODR,
		 * MOTIONSENSE_CMD_SENSOR_RANGE,
		 * MOTIONSENSE_CMD_KB_WAKE_ANGLE,
		 * MOTIONSENSE_CMD_FIFO_INT_ENABLE and
		 * MOTIONSENSE_CMD_SPOOF.
		 */
		struct {
			/* Current value of the parameter queried. */
			INT32 ret;
		} ec_rate, sensor_odr, sensor_range, kb_wake_angle,
			fifo_int_enable, spoof;

		/*
		 * Used for MOTIONSENSE_CMD_SENSOR_OFFSET,
		 * PERFORM_CALIB.
		 */
		struct {
			INT16 temp;
			INT16 offset[3];
		} sensor_offset, perform_calib;

		/* Used for MOTIONSENSE_CMD_SENSOR_SCALE */
		struct {
			INT16 temp;
			UINT16 scale[3];
		} sensor_scale;

		struct ec_response_motion_sense_fifo_info fifo_info, fifo_flush;

		struct ec_response_motion_sense_fifo_data fifo_read;

		struct ec_response_online_calibration_data online_calib_read;

		struct {
			UINT16 reserved;
			UINT32 enabled;
			UINT32 disabled;
		} list_activities;

		/* No params for set activity */

		/* Used for MOTIONSENSE_CMD_LID_ANGLE */
		struct {
			/*
			 * Angle between 0 and 360 degree if available,
			 * LID_ANGLE_UNRELIABLE otherwise.
			 */
			UINT16 value;
		} lid_angle;

		/* Used for MOTIONSENSE_CMD_TABLET_MODE_LID_ANGLE. */
		struct {
			/*
			 * Lid angle threshold for switching between tablet and
			 * clamshell mode.
			 */
			UINT16 lid_angle;

			/* Hysteresis degree. */
			UINT16 hys_degree;
		} tablet_mode_threshold;

		/* USED for MOTIONSENSE_CMD_GET_ACTIVITY. */
		struct {
			UINT8 state;
		} get_activity;
	};
};
#include <poppack.h>

/*****************************************************************************/


/* Read versions supported for a command */
#define EC_CMD_GET_CMD_VERSIONS 0x0008

/**
 * struct ec_params_get_cmd_versions - Parameters for the get command versions.
 * @cmd: Command to check.
 */
#include <pshpack1.h>
struct ec_params_get_cmd_versions {
	UINT8 cmd;
};
#include <poppack.h>

/**
 * struct ec_params_get_cmd_versions_v1 - Parameters for the get command
 *         versions (v1)
 * @cmd: Command to check.
 */
#include <pshpack2.h>
struct ec_params_get_cmd_versions_v1 {
	UINT16 cmd;
};
#include <poppack.h>

/**
 * struct ec_response_get_cmd_version - Response to the get command versions.
 * @version_mask: Mask of supported versions; use EC_VER_MASK() to compare with
 *                a desired version.
 */
#include <pshpack4.h>
struct ec_response_get_cmd_versions {
	UINT32 version_mask;
};
#include <poppack.h>


/* Get protocol information */
#define EC_CMD_GET_PROTOCOL_INFO	0x0b

/* Flags for ec_response_get_protocol_info.flags */
/* EC_RES_IN_PROGRESS may be returned if a command is slow */
#define EC_PROTOCOL_INFO_IN_PROGRESS_SUPPORTED (1 << 0)

#include <pshpack4.h>

struct ec_response_get_protocol_info {
	/* Fields which exist if at least protocol version 3 supported */

	/* Bitmask of protocol versions supported (1 << n means version n)*/
	UINT32 protocol_versions;

	/* Maximum request packet size, in bytes */
	UINT16 max_request_packet_size;

	/* Maximum response packet size, in bytes */
	UINT16 max_response_packet_size;

	/* Flags; see EC_PROTOCOL_INFO_* */
	UINT32 flags;
};

#include <poppack.h>

#endif /* __CROS_EC_REGS_H__ */
#ifndef GCODE_H
#define GCODE_H

#include "base/vector.h"
#include "base/array.h"
#include "base/strings.h"
#include "extra/date.h"
#include "maths/geometry.h"

namespace iso {
/*
Gnnn	Standard GCode command, such as move to a point
Mnnn	RepRap-defined command, such as turn on a cooling fan
Tnnn	Select tool nnn. In RepRap, a tool is typically associated with a nozzle, which may be fed by one or more extruders.
Snnn	Command parameter, such as time in seconds; temperatures; voltage to send to a motor
Pnnn	Command parameter, such as time in milliseconds; proportional (Kp) in PID Tuning
Xnnn	A X coordinate, usually to move to. This can be an Integer or Fractional number.
Ynnn	A Y coordinate, usually to move to. This can be an Integer or Fractional number.
Znnn	A Z coordinate, usually to move to. This can be an Integer or Fractional number.
Innn	Parameter - X-offset in arc move; integral (Ki) in PID Tuning
Jnnn	Parameter - Y-offset in arc move
Dnnn	Parameter - used for diameter; derivative (Kd) in PID Tuning
Hnnn	Parameter - used for heater number in PID Tuning
Fnnn	Feedrate in mm per minute. (Speed of print head movement)
Rnnn	Parameter - used for temperatures
Qnnn	Parameter - not currently used
Ennn	Length of extrudate. This is exactly like X, Y and Z, but for the length of filament to consume.
Nnnn	Line number. Used to request repeat transmission in the case of communications errors.
*nnn	Checksum. Used to check for communications errors.
*/

#define GCODE(L,N)	('L' << 24) | int(N * 10)
enum GCODES {
	MOVE														= GCODE(G, 0),
	MOVE_RAPID													= GCODE(G, 1),
	CONTROLLED_ARC_MOVE_CW										= GCODE(G, 2),
	CONTROLLED_ARC_MOVE_CCW										= GCODE(G, 3),
	DWELL														= GCODE(G, 4),
	TOOL_OFFSET													= GCODE(G, 10),
	RETRACT														= GCODE(G, 10),
	UNRETRACT													= GCODE(G, 11),
//	PLANE_SELECTION_(CNC_SPECIFIC)								= GCODE(G, 17..19),
	SET_UNITS_TO_INCHES											= GCODE(G, 20),
	SET_UNITS_TO_MILLIMETERS									= GCODE(G, 21),
//	FIRMWARE_CONTROLLED_RETRACT/PRECHARGE						= G22_&_G23,
	MOVE_TO_ORIGIN_(HOME)										= GCODE(G, 28),
	DETAILED_ZPROBE												= GCODE(G, 29),
	SET_Z_PROBE_HEAD_OFFSET										= GCODE(G, 29.1),
	SET_Z_PROBE_HEAD_OFFSET_CALCULATED_FROM_TOOLHEAD_POSITION	= GCODE(G, 29.2),
	SINGLE_ZPROBE												= GCODE(G, 30),
	SET_OR_REPORT_CURRENT_PROBE_STATUS							= GCODE(G, 31),
	DOCK_Z_PROBE_SLED											= GCODE(G, 31),
	PROBE_Z_AND_CALCULATE_Z_PLANE								= GCODE(G, 32),
	UNDOCK_Z_PROBE_SLED											= GCODE(G, 32),
	MEASURE/LIST/ADJUST_DISTORTION_MATRIX						= GCODE(G, 33),
	STRAIGHT_PROBE_(CNC_SPECIFIC)													= G38.x,
	PROBE_TOWARD_WORKPIECE,_STOP_ON_CONTACT,_SIGNAL_ERROR_IF_FAILUre				= GCODE(G, 38.2),
	PROBE_TOWARD_WORKPIECE,_STOP_ON_CONTACT											= GCODE(G, 38.3),
	PROBE_AWAY_FROM_WORKPIECE,_STOP_ON_LOSS_OF_CONTACT,_SIGNAL_ERRor_if_failure		= GCODE(G, 38.4),
	PROBE_AWAY_FROM_WORKPIECE,_STOP_ON_LOSS_OF_CONTACT								= GCODE(G, 38.5),
	COMPENSATION_OFF_(CNC_SPECIFIC)								= GCODE(G, 40),
//	COORDINATE_SYSTEM_SELECT_(CNC_SPECIFIC)						= GCODE(G, 54..59),
	CANCEL_CANNED_CYCLE_(CNC_SPECIFIC)							= GCODE(G, 80),
	SET_TO_ABSOLUTE_POSITIONING									= GCODE(G, 90),
	SET_TO_RELATIVE_POSITIONING									= GCODE(G, 91),
	SET_POSITION_												= GCODE(G, 92),
	RESET_COORDINATE_SYSTEM_OFFSETS_(CNC_SPECIFIC)				= G92.x,
	FEED_RATE_MODE_(INVERSE_TIME_MODE)_(CNC_SPECIFIC)			= GCODE(G, 93),
	FEED_RATE_MODE_(UNITS_PER_MINUTE)_(CNC_SPECIFIC)			= GCODE(G, 94),
	CALIBRATE_FLOOR_OR_ROD_RADIUS								= GCODE(G, 100),
	SET_DIGITAL_POTENTIOMETER_VALUE								= GCODE(G, 130),
	REMOVE_OFFSET												= GCODE(G, 131),
	CALIBRATE_ENDSTOP_OFFSETS									= GCODE(G, 132),
	MEASURE_STEPS_TO_TOP										= GCODE(G, 133),
	HOME_AXES_TO_MINIMUM										= GCODE(G, 161),
	HOME_AXES_TO_MAXIMUM										= GCODE(G, 162),
	//M
	STOP_OR_UNCONDITIONAL_STOP									= GCODE(M, 0),
	SLEEP_OR_CONDITIONAL_STOP									= GCODE(M, 1),
	PROGRAM_END													= GCODE(M, 2),
	SPINDLE_ON,_CLOCKWISE_(CNC_SPECIFIC)						= GCODE(M, 3),
	SPINDLE_ON,_COUNTERCLOCKWISE_(CNC_SPECIFIC)					= GCODE(M, 4),
	SPINDLE_OFF_(CNC_SPECIFIC)									= GCODE(M, 5),
	TOOL_CHANGE													= GCODE(M, 6),
	MIST_COOLANT_ON_(CNC_SPECIFIC)								= GCODE(M, 7),
	FLOOD_COOLANT_ON_(CNC_SPECIFIC)								= GCODE(M, 8),
	COOLANT_OFF_(CNC_SPECIFIC)									= GCODE(M, 9),
	VACUUM_ON_(CNC_SPECIFIC)									= GCODE(M, 10),
	VACUUM_OFF_(CNC_SPECIFIC)									= GCODE(M, 11),
	ENABLE/POWER_ALL_STEPPER_MOTORS								= GCODE(M, 17),
	DISABLE_ALL_STEPPER_MOTORS									= GCODE(M, 18),
	LIST_SD_CARD												= GCODE(M, 20),
	INITIALIZE_SD_CARD											= GCODE(M, 21),
	RELEASE_SD_CARD												= GCODE(M, 22),
	SELECT_SD_FILE												= GCODE(M, 23),
	START/RESUME_SD_PRINT										= GCODE(M, 24),
	PAUSE_SD_PRINT												= GCODE(M, 25),
	SET_SD_POSITION												= GCODE(M, 26),
	REPORT_SD_PRINT_STATUS										= GCODE(M, 27),
	BEGIN_WRITE_TO_SD_CARD										= GCODE(M, 28),
	STOP_WRITING_TO_SD_CARD										= GCODE(M, 29),
	DELETE_A_FILE_ON_THE_SD_CARD_								= GCODE(M, 30),
	OUTPUT_TIME_SINCE_LAST_M109_OR_SD_CARD_START_TO_SERIAL		= GCODE(M, 31),
	SELECT_FILE_AND_START_SD_PRINT								= GCODE(M, 32),
	GET_THE_LONG_NAME_FOR_AN_SD_CARD_FILE_OR_FOLDER				= GCODE(M, 33),
	SET_SD_FILE_SORTING_OPTIONS									= GCODE(M, 34),
	RETURN_FILE_INFORMATION										= GCODE(M, 36),
	SIMULATION_MODE												= GCODE(M, 37),
	COMPUTE_SHA1_HASH_OF_TARGET_FILE							= GCODE(M, 38),
	EJECT														= GCODE(M, 40),
	LOOP														= GCODE(M, 41),
	SWITCH_I/O_PIN												= GCODE(M, 42),
	STAND_BY_ON_MATERIAL_EXHAUSTED								= GCODE(M, 43),
	MEASURE_ZPROBE_REPEATABILITY								= GCODE(M, 48),
	DISPLAY_MESSAGE												= GCODE(M, 70),
	PLAY_A_TONE_OR_SONG											= GCODE(M, 72),
	SET_BUILD_PERCENTAGE										= GCODE(M, 73),
	ATX_POWER_ON												= GCODE(M, 80),
	ATX_POWER_OFF												= GCODE(M, 81),
	SET_EXTRUDER_TO_ABSOLUTE_MODE								= GCODE(M, 82),
	SET_EXTRUDER_TO_RELATIVE_MODE								= GCODE(M, 83),
	STOP_IDLE_HOLD												= GCODE(M, 84),
	SET_INACTIVITY_SHUTDOWN_TIMER								= GCODE(M, 85),
	SET_AXIS_STEPS_PER_UNIT										= GCODE(M, 92),
	SEND_AXIS_STEPS_PER_UNIT									= GCODE(M, 93),
	CALL_MACRO/SUBPROGRAM										= GCODE(M, 98),
	RETURN_FROM_MACRO/SUBPROGRAM								= GCODE(M, 99),
	GET_AXIS_HYSTERESIS_MM										= GCODE(M, 98),
	SET_AXIS_HYSTERESIS_MM										= GCODE(M, 99),
	TURN_EXTRUDER_1_ON_(FORWARD),_UNDO_RETRACTION				= GCODE(M, 101),
	TURN_EXTRUDER_1_ON_(REVERSE)								= GCODE(M, 102),
	TURN_ALL_EXTRUDERS_OFF,_EXTRUDER_RETRACTION					= GCODE(M, 103),
	SET_EXTRUDER_TEMPERATURE_									= GCODE(M, 104),
	GET_EXTRUDER_TEMPERATURE									= GCODE(M, 105),
	FAN_ON_														= GCODE(M, 106),
	FAN_OFF														= GCODE(M, 107),
	CANCEL_HEATING_(MARLIN)										= GCODE(M, 108),
	SET_EXTRUDER_SPEED											= GCODE(M, 108),
	SET_EXTRUDER_TEMPERATURE_AND_WAIT							= GCODE(M, 109),
	SET_CURRENT_LINE_NUMBER										= GCODE(M, 110),
	SET_DEBUG_LEVEL_											= GCODE(M, 111),
	EMERGENCY_STOP												= GCODE(M, 112),
	SET_EXTRUDER_PWM											= GCODE(M, 113),
	GET_CURRENT_POSITION										= GCODE(M, 114),
	GET_FIRMWARE_VERSION_AND_CAPABILITIES						= GCODE(M, 115),
	WAIT														= GCODE(M, 116),
	GET_ZERO_POSITION											= GCODE(M, 117),
	DISPLAY_MESSAGE												= GCODE(M, 117),
	NEGOTIATE_FEATURES											= GCODE(M, 118),
	GET_ENDSTOP_STATUS											= GCODE(M, 119),
	PUSH														= GCODE(M, 120),
	POP															= GCODE(M, 121),
	ENABLE_ENDSTOP_DETECTION									= GCODE(M, 120),
	DISABLE_ENDSTOP_DETECTION									= GCODE(M, 121),
	DIAGNOSE													= GCODE(M, 122),
	TACHOMETER_VALUE											= GCODE(M, 123),
	IMMEDIATE_MOTOR_STOP										= GCODE(M, 124),
	OPEN_VALVE_													= GCODE(M, 126),
	CLOSE_VALVE_												= GCODE(M, 127),
	EXTRUDER_PRESSURE_PWM										= GCODE(M, 128),
	EXTRUDER_PRESSURE_OFF										= GCODE(M, 129),
	SET_PID_P_VALUE												= GCODE(M, 130),
	SET_PID_I_VALUE												= GCODE(M, 131),
	SET_PID_D_VALUE_											= GCODE(M, 132),
	SET_PID_I_LIMIT_VALUE_										= GCODE(M, 133),
	WRITE_PID_VALUES_TO_EEPROM_									= GCODE(M, 134),
	SET_PID_SAMPLE_INTERVAL_									= GCODE(M, 135),
	PRINT_PID_SETTINGS_TO_HOST									= GCODE(M, 136),
	SET_BED_TEMPERATURE_(FAST)									= GCODE(M, 140),
	SET_CHAMBER_TEMPERATURE_(FAST)								= GCODE(M, 141),
	HOLDING_PRESSURE											= GCODE(M, 142),
	MAXIMUM_HOTEND_TEMPERATURE									= GCODE(M, 143),
	STAND_BY_YOUR_BED											= GCODE(M, 144),
	SET_CHAMBER_HUMIDITY										= GCODE(M, 146),
	SET_TEMPERATURE_UNITS										= GCODE(M, 149),
	SET_DISPLAY_COLOR											= GCODE(M, 150),
	NUMBER_OF_MIXED_MATERIALS									= GCODE(M, 160),
	SET_WEIGHT_OF_MIXED_MATERIAL								= GCODE(M, 163),
	STORE_WEIGHTS												= GCODE(M, 164),
	WAIT_FOR_BED_TEMPERATURE_TO_REACH_TARGET_TEMP				= GCODE(M, 190),
	WAIT_FOR_CHAMBER_TEMPERATURE_TO_REACH_TARGET_TEMP			= GCODE(M, 191),
	SET_FILAMENT_DIAMETER										= GCODE(M, 200),
	SET_MAX_PRINTING_ACCELERATION								= GCODE(M, 201),
	SET_MAX_TRAVEL_ACCELERATION									= GCODE(M, 202),
	SET_MAXIMUM_FEEDRATE_										= GCODE(M, 203),
	SET_DEFAULT_ACCELERATION_									= GCODE(M, 204),
	ADVANCED_SETTINGS_											= GCODE(M, 205),
	OFFSET_AXES													= GCODE(M, 206),
	CALIBRATE_Z_AXIS_BY_DETECTING_Z_MAX_LENGTH					= GCODE(M, 207),
	SET_RETRACT_LENGTH											= GCODE(M, 207),
	SET_AXIS_MAX_TRAVEL											= GCODE(M, 208),
	SET_UNRETRACT_LENGTH										= GCODE(M, 208),
	ENABLE_AUTOMATIC_RETRACT									= GCODE(M, 209),
	SET_HOMING_FEEDRATES										= GCODE(M, 210),
	DISABLE/ENABLE_SOFTWARE_ENDSTOPS							= GCODE(M, 211),
	SET_BED_LEVEL_SENSOR_OFFSET									= GCODE(M, 212),
	SET_HOTEND_OFFSET											= GCODE(M, 218),
	SET_SPEED_FACTOR_OVERRIDE_PERCENTAGE						= GCODE(M, 220),
	SET_EXTRUDE_FACTOR_OVERRIDE_PERCENTAGE						= GCODE(M, 221),
	TURN_OFF_AUX_V1.0.5											= GCODE(M, 220),
	TURN_ON_AUX_V1.0.5											= GCODE(M, 221),
	SET_SPEED_OF_FAST_XY_MOVES									= GCODE(M, 222),
	SET_SPEED_OF_FAST_Z_MOVES									= GCODE(M, 223),
	ENABLE_EXTRUDER_DURING_FAST_MOVES							= GCODE(M, 224),
	DISABLE_ON_EXTRUDER_DURING_FAST_MOVES						= GCODE(M, 225),
	GCODE_INITIATED_PAUSE										= GCODE(M, 226),
	WAIT_FOR_PIN_STATE											= GCODE(M, 226),
	ENABLE_AUTOMATIC_REVERSE_AND_PRIME							= GCODE(M, 227),
	DISABLE_AUTOMATIC_REVERSE_AND_PRIME							= GCODE(M, 228),
	ENABLE_AUTOMATIC_REVERSE_AND_PRIME							= GCODE(M, 229),
	DISABLE_/_ENABLE_WAIT_FOR_TEMPERATURE_CHANGE				= GCODE(M, 230),
	SET_OPS_PARAMETER											= GCODE(M, 231),
	READ_AND_RESET_MAX._ADVANCE_VALUES							= GCODE(M, 232),
	TRIGGER_CAMERA												= GCODE(M, 240),
	START_CONVEYOR_BELT_MOTOR_ECHO_OFF							= GCODE(M, 240),
	STOP_CONVEYOR_BELT_MOTOR_ECHO_ON							= GCODE(M, 241),
	START_COOLER												= GCODE(M, 245),
	STOP_COOLER													= GCODE(M, 246),
	SET_LCD_CONTRAST											= GCODE(M, 250),
	SET_SERVO_POSITION											= GCODE(M, 280),
	PLAY_BEEP_SOUND												= GCODE(M, 300),
	SET_PID_PARAMETERS_											= GCODE(M, 301),
	ALLOW_COLD_EXTRUDES											= GCODE(M, 302),
	RUN_PID_TUNING												= GCODE(M, 303),
	SET_PID_PARAMETERS_BED										= GCODE(M, 304),
//	SET_THERMISTOR_VALUES										= M304_in_RepRapPro_version_of_Marlin,
	SET_THERMISTOR_AND_ADC_PARAMETERS							= GCODE(M, 305),
	SET_HOME_OFFSET_CALCULATED_FROM_TOOLHEAD_POSITION			= GCODE(M, 306),
	SET_OR_REPORT_HEATING_PROCESS_PARAMETERS					= GCODE(M, 307),
	CONTROL_THE_SERVOS											= GCODE(M, 340),
	SET_MICROSTEPPING_MODE										= GCODE(M, 350),
	TOGGLE_MS1_MS2_PINS_DIRECTLY								= GCODE(M, 351),
	TURN_CASE_LIGHTS_ON/OFF										= GCODE(M, 355),
	REPORT_FIRMWARE_CONFIGURATION								= GCODE(M, 360),
	MOVE_TO_THETA_0_DEGREE_POSITION								= GCODE(M, 360),
	MOVE_TO_THETA_90_DEGREE_POSITION							= GCODE(M, 361),
	MOVE_TO_PSI_0_DEGREE_POSITION								= GCODE(M, 362),
	MOVE_TO_PSI_90_DEGREE_POSITION								= GCODE(M, 363),
	MOVE_TO_PSI_+_THETA_90_DEGREE_POSITION						= GCODE(M, 364),
	SCARA_SCALING_FACTOR										= GCODE(M, 365),
	SCARA_CONVERT_TRIM											= GCODE(M, 366),
	MORGAN_MANUAL_BED_LEVEL_CLEAR_MAP							= GCODE(M, 370),
	MOVE_TO_NEXT_CALIBRATION_POSITION							= GCODE(M, 371),
	RECORD_CALIBRATION_VALUE,_AND_MOVE_TO_NEXT_POSITION			= GCODE(M, 372),
	END_BED_LEVEL_CALIBRATION_MODE								= GCODE(M, 373),
	SAVE_CALIBRATION_GRID										= GCODE(M, 374),
	DISPLAY_LOAD_MATRIX											= GCODE(M, 375),
	ACTIVATE_SOLENOID											= GCODE(M, 380),
	DISABLE_ALL_SOLENOIDS										= GCODE(M, 381),
	WAIT_FOR_CURRENT_MOVES_TO_FINISH							= GCODE(M, 400),
	LOWER_ZPROBE												= GCODE(M, 401),
	RAISE_ZPROBE												= GCODE(M, 402),
	FILAMENT_WIDTH_AND_NOZZLE_DIAMETER							= GCODE(M, 404),
	FILAMENT_SENSOR_ON											= GCODE(M, 405),
	FILAMENT_SENSOR_OFF											= GCODE(M, 406),
	DISPLAY_FILAMENT_DIAMETER									= GCODE(M, 407),
	REPORT_JSONSTYLE_RESPONSE									= GCODE(M, 408),
	SET_RGB_COLORS_AS_PWM_(MACHINEKIT)							= GCODE(M, 420),
	ENABLE/DISABLE_MESH_LEVELING_(MARLIN)						= GCODE(M, 420),
	SET_A_MESH_BED_LEVELING_Z_COORDINATE						= GCODE(M, 421),
	REPORT_PRINTER_MODE											= GCODE(M, 450),
	SELECT_FFF_PRINTER_MODE										= GCODE(M, 451),
	SELECT_LASER_PRINTER_MODE									= GCODE(M, 452),
	SELECT_CNC_PRINTER_MODE										= GCODE(M, 453),
	DEFINE_TEMPERATURE_RANGE_FOR_THERMISTOR_CONTROLLED_FAN		= GCODE(M, 460),
	STORE_PARAMETERS_IN_EEPROM									= GCODE(M, 500),
	READ_PARAMETERS_FROM_EEPROM									= GCODE(M, 501),
	REVERT_TO_THE_DEFAULT_"FACTORY_SETTINGS."					= GCODE(M, 502),
	PRINT_SETTINGS												= GCODE(M, 503),
	ENABLE/DISABLE_"STOP_SD_PRINT_ON_ENDSTOP_HIT"				= GCODE(M, 540),
	SET_MAC_ADDRESS												= GCODE(M, 540),
	SET_NAME													= GCODE(M, 550),
	SET_PASSWORD												= GCODE(M, 551),
	SET_IP_ADDRESS												= GCODE(M, 552),
	SET_NETMASK													= GCODE(M, 553),
	SET_GATEWAY													= GCODE(M, 554),
	SET_COMPATIBILITY											= GCODE(M, 555),
	AXIS_COMPENSATION											= GCODE(M, 556),
	SET_Z_PROBE_POINT											= GCODE(M, 557),
	SET_Z_PROBE_TYPE											= GCODE(M, 558),
	UPLOAD_CONFIGURATION_FILE									= GCODE(M, 559),
	UPLOAD_WEB_PAGE_FILE										= GCODE(M, 560),
	SET_IDENTITY_TRANSFORM										= GCODE(M, 561),
	RESET_TEMPERATURE_FAULT										= GCODE(M, 562),
	DEFINE_OR_REMOVE_A_TOOL										= GCODE(M, 563),
	LIMIT_AXES													= GCODE(M, 564),
	SET_Z_PROBE_OFFSET											= GCODE(M, 565),
	SET_ALLOWABLE_INSTANTANEOUS_SPEED_CHANGE					= GCODE(M, 566),
	SET_TOOL_MIX_RATIOS											= GCODE(M, 567),
	TURN_OFF/ON_TOOL_MIX_RATIOS									= GCODE(M, 568),
	SET_AXIS_DIRECTION_AND_ENABLE_VALUES						= GCODE(M, 569),
	SET_HEATER_TIMEOUT											= GCODE(M, 570),
	SET_OUTPUT_ON_EXTRUDE										= GCODE(M, 571),
	SET_OR_REPORT_EXTRUDER_PRESSURE_ADVANCE						= GCODE(M, 572),
	REPORT_HEATER_PWM											= GCODE(M, 573),
	SET_ENDSTOP_CONFIGURATION									= GCODE(M, 574),
	SET_SERIAL_COMMS_PARAMETERS									= GCODE(M, 575),
	WAIT_UNTIL_ENDSTOP_IS_TRIGGERED								= GCODE(M, 577),
	FIRE_INKJET_BITS											= GCODE(M, 578),
	SCALE_CARTESIAN_AXES										= GCODE(M, 579),
	SELECT_ROLAND												= GCODE(M, 580),
	CONFIGURE_EXTERNAL_TRIGGER									= GCODE(M, 581),
	CHECK_EXTERNAL_TRIGGER										= GCODE(M, 582),
	WAIT_FOR_PIN												= GCODE(M, 583),
	SET_DRIVE_MAPPING											= GCODE(M, 584),
	PROBE_TOOL													= GCODE(M, 585),
	SET_LINE_CROSS_SECTION										= GCODE(M, 600),
	FILAMENT_CHANGE_PAUSE										= GCODE(M, 600),
	SET_DUAL_XCARRIAGE_MOVEMENT_MODE							= GCODE(M, 605),
	SET_DELTA_CONFIGURATION										= GCODE(M, 665),
	SET_DELTA_ENDSTOP_ADJUSTMENT								= GCODE(M, 666),
	SELECT_COREXY_MODE											= GCODE(M, 667),
	SET_ZOFFSET_COMPENSATIONS_POLYNOMIAL						= GCODE(M, 668),
	LEVEL_PLATE													= GCODE(M, 700),
	LOAD_FILAMENT												= GCODE(M, 701),
	UNLOAD_FILAMENT												= GCODE(M, 702),
	GET_BOARD_TYPE												= GCODE(M, 703),
	ERASE_THE_EEPROM_AND_RESET_THE_BOARD						= GCODE(M, 710),
	FIRE_START_PRINT_PROCEDURE									= GCODE(M, 800),
	FIRE_END_PRINT_PROCEDURE									= GCODE(M, 801),
	SET_ZPROBE_OFFSET											= GCODE(M, 851),
	SET_MOTOR_CURRENTS											= GCODE(M, 906),
	SET_DIGITAL_TRIMPOT_MOTOR									= GCODE(M, 907),
	CONTROL_DIGITAL_TRIMPOT_DIRECTLY							= GCODE(M, 908),
	SET_MICROSTEPPING											= GCODE(M, 909),
	SET_DECAY_MODE												= GCODE(M, 910),
	SET_POWER_MONITOR_THRESHOLD_VOLTAGES						= GCODE(M, 911),
	SET_ELECTRONICS_TEMPERATURE_MONITOR_ADJUSTMENT				= GCODE(M, 912),
	SET_MOTOR_PERCENTAGE_OF_NORMAL_CURRENT						= GCODE(M, 913),
	START_SD_LOGGING											= GCODE(M, 928),
	PERFORM_INAPPLICATION_FIRMWARE_UPDATE						= GCODE(M, 997),
	REQUEST_RESEND_OF_LINE										= GCODE(M, 998),
	RESTART_AFTER_BEING_STOPPED_BY_ERROR						= GCODE(M, 999),
};


struct Command {
	GCodes		Code;
	float3		where;
	float3		arcIJK; // I,J,K (dx, dy, dz)
	bool		is_value; // M commands
	double		value; // M commands S value code
	double		f, e; // Feedrate f=speed, e=extrusion to perform while moving (Pythagoras)
	uint32		extruder_no;

	double		abs_extr;			// for debugging/painting
	double		travel_length;		// for debugging
	bool		not_layerchange;	// don't record as layerchange for lifted moves

	string		explicit_arg;
	string		comment;

	static int getChecksum(const char *s) {
		int c = 0;
		while (*s && *s != '*')
			c ^= *s++;
		return c;
	}

	static GCodes getCode(const char *commstr) {
		for (int i = 0; i < NUM_GCODES; i++) {
			if (str(MCODES[i]) == commstr)
				return (GCodes)i;
		}
		return COMMENT;
	}

	Command() : Code(UNKNOWN), where(zero), is_value(false), f(0), e(0), extruder_no(0), abs_extr(0), travel_length(0), not_layerchange(false) {
	}
	Command(GCodes code, const char *_explicit_arg) : Code(code), explicit_arg(_explicit_arg), where(zero), is_value(false), f(0), e(0), extruder_no(0), abs_extr(0), travel_length(0), not_layerchange(false) {
	}
	Command(GCodes code, const float3 &_where = float3(zero), double E = 0, double F = 0) : Code(code), where(_where), is_value(false), f(F), e(E), extruder_no(0), abs_extr(0), travel_length(0), not_layerchange(false) {
		if (where.z < 0)
		where.z = 0;
	}
	// for letter-without-number codes like "T" the value is not an "S" value, it belongs to the command
	Command(GCodes code, double _value) : Code(code), where(0, 0, 0), is_value(strlen(MCODES[code]) != 1), value(_value), f(0), e(0), extruder_no(0), abs_extr(0), travel_length(0), not_layerchange(false) {
	}

	Command(char *_comment) : Code(COMMENT), where(zero), is_value(true), value(0), f(0), e(0), extruder_no(0), abs_extr(0), travel_length(0), not_layerchange(true), comment(_comment) {}
	Command(char *gcodeline, const float3 &defaultpos, const char *E_letters) : where(defaultpos), arcIJK(0, 0, 0), is_value(false), f(0), e(0), extruder_no(0), abs_extr(0), travel_length(0) {
		string_scan	s(gcodeline);

		while (s.remaining()) {
			// GCode is always <LETTER> <NUMBER>
			char	ch = toupper(s.getc());
			float	num = s.get();

			Code = getCode(format_string("%c%f", ch, num));
			if (Code != COMMENT) {
				is_value = ch == 'M';

			} else switch (ch) {
				case 'S': value = num; break;
				case 'F': f = num; break;
				case 'X': where.x = num; break;
				case 'Y': where.y = num; break;
				case 'Z': where.z = num; break;
				case 'I': arcIJK.x = num; break;
				case 'J': arcIJK.y = num; break;
				case 'K':
				case 'R':
					break;
				case 'T':
					Code = SELECTEXTRUDER;
					extruder_no = num;
					break;
				default:
				{
					bool foundExtr = false;
					if (const char *e = string_find(E_letters, ch)) {
						extruder_no = e - E_letters;
					} else {
						ISO_TRACEF("cannot parse GCode line ") << gcodeline << '\n';
					}
					break;
				}
			}
		}
		if (where.z < 0)
		where.z = 0;
	}

	bool hasNoEffect(const float3 LastPos, const double lastE, const double lastF, const bool relativeEcode) const {
		return (Code == COORDINATEDMOTION || Code == RAPIDMOTION)
			&& len2(where - LastPos) < 0.000001f
			&& ((relativeEcode && abs(e) < 0.00001f) || (!relativeEcode && abs(e - lastE) < 0.00001f))
			&& abs(abs_extr) < 0.00001f;
	}

	string GetGCodeText(float3 &LastPos, double &lastE, double &lastF, bool relativeEcode, const char E_letter = 'E', bool speedAlways = false) const {
		string			s;
		string_builder	b(s);

		b << MCODES[Code];

		if (is_value && Code != COMMENT) {
			b << " S" << value;
			if (comment)
				b << " ; " << comment;
			return b;
		}

		double	thisE = lastE - e;	// extraction of this command amount only
		double	length = len(where - LastPos);
		string	comm = comment;

		switch (Code) {
			case ARC_CW:
			case ARC_CCW:
				if (arcIJK.x != 0) b << " I" << arcIJK.x;
				if (arcIJK.y != 0) b << " J" << arcIJK.y;
				if (arcIJK.z != 0) b << " K" << arcIJK.z;
			case RAPIDMOTION:
			case COORDINATEDMOTION:
			{ // going down? -> split xy and z movements
				float3	delta = where - LastPos;
				const double RETRACT_E = 2; //mm
				if ((where.z < 0 || delta.z < 0) && delta.xy != zero) {
					Command		xycommand(*this); // copy
					xycommand.comment = comment + " xy part";
					Command		zcommand(*this); // copy
					zcommand.comment = comment + " z part";
					if (where.z < 0) { // z<0 cannot be absolute -> positions are relative
						xycommand.where.z = 0.;
						zcommand.where.xy = zero; // this command will be z-only
					} else {
						xycommand.where.z = LastPos.z;
					}
					if (relativeEcode) {
						xycommand.e = -RETRACT_E;	// retract filament at xy move
						zcommand.e = 0;				// all extrusion done in xy
					} else {
						xycommand.e = lastE - RETRACT_E;
						zcommand.e = lastE - RETRACT_E;
					}
					b << xycommand.GetGCodeText(LastPos, lastE, lastF, relativeEcode, E_letter) << '\n';
					b << zcommand.GetGCodeText(LastPos, lastE, lastF, relativeEcode, E_letter);
					return b;
				}
			}
			if (where.x != LastPos.x) {
				b << " X" << where.x;
				LastPos.x = where.x;
			}
			if (where.y != LastPos.y) {
				b << " Y" << where.y;
				LastPos.y() = where.y;
			}
			case ZMOVE:
				if (where.z != LastPos.z) {
					b << " Z" << where.z;
					LastPos.z = where.z;
					comm += " Z-Change";
				}
				if (relativeEcode ? e != 0 : e != lastE) {
					(b << " " << E_letter).format("%.5f", e);
					lastE = e;
				}
			case SETSPEED:
				if (speedAlways || abs(f - lastF) > 0.1)
					b.format(f > 10 ? " F%.0f" : " F%.4f", f);
				lastF = f;
				break;
			case SELECTEXTRUDER:
				b.format("%.0f", value);
				comm += " Select Extruder";
				break;
			case RESET_E:
				b << " " << E_letter << "0";
				comm += " Reset Extrusion";
				lastE = 0;
				break;

			default:
				break;
		}
		if (explicit_arg)
			b << " " << explicit_arg;

		if (comm) {
			if (Code != COMMENT)
				b << " ; ";
			b << comm;
		}
		if (abs_extr != 0) {
			b << " ; AbsE " << abs_extr;
			if (travel_length != 0) {
				const double espeed = abs_extr / travel_length * f / 60;
				b.format(" (%.2f", espeed);
				if (thisE != 0)
					b.format("/%.2f", (thisE + abs_extr) / travel_length * f / 60);
				b << " mm/s) ";
			}
		}

		return b;
	}

	void addToPosition(float3 &from, bool relative) {
		if (relative)
			from += where;
		else
			from = (where == zero).select(from, where);
	}

	string info() const {
		string	s;
		string_builder	b(s);
		b << "Command";
		if (comment)
			b << " '" << comment << "'";
		b << ": Extr=" << extruder_no << ", Code=" << Code << ", where=" << where << ", f=" << f << ", e=" << e;
		if (explicit_arg)
			b << " Explicit: " << explicit_arg;
		return b;
	}
};

struct Settings;
struct Model;
struct ViewProgress;

class GCodeIter {
public:
	uint32		num_lines, cur_line;
	DateTime	time_used;
	DateTime	time_started;
	DateTime	time_estimation;

	GCodeIter();
	string	next_line();
	string	next_line_stripped();
	bool	finished();

	Command getCurrentCommand(float3 defaultwhere, const char *E_letters);
	void	set_to_lineno(int lineno);
};

class GCode {
	uint32	unconfirmed_blocks;
	cuboid	box;
	float3	center;
	float3	currentCursorWhere;
	float3	currentCursorFrom;
	Command currentCursorCommand;
	dynamic_array<uint32> buffer_zpos_lines; // line numbers where a z position is set
	dynamic_array<uint32> layerchanges;
public:
	GCode() : box(empty), center(zero) {}

	void Read(Model *model, const char *E_letters, ViewProgress *progress, string filename);
	void draw(const Settings &settings, int layer = -1, bool liveprinting = false, int linewidth = 3);
	void drawCommands(const Settings &settings, uint32 start, uint32 end, bool liveprinting, int linewidth, bool arrows, bool boundary = false, bool onlyZChange = false);
	void MakeText(string &GcodeTxt, const Settings &settings, ViewProgress * progress);

	string	get_text() const;
	void	clear();

	dynamic_array<Command> commands;
	uint32 size() { return commands.size(); };

	void translate(float3 trans);

	GCodeIter *get_iter();

	double GetTotalExtruded(bool relativeEcode) const;
	double GetTimeEstimation() const;

	void updateWhereAtCursor(const dynamic_array<char> &E_letters);
	int getLayerNo(const double z) const;
	int getLayerNo(const uint32 commandno) const;
	uint32 getLayerStart(const uint32 layerno) const;
	uint32 getLayerEnd(const uint32 layerno) const;
};



void GCode::clear() {
	commands.clear();
	layerchanges.clear();
	buffer_zpos_lines.clear();
	box		= empty;
	center	= float3(zero);
}

double GCode::GetTotalExtruded(bool relativeEcode) const {
	if (commands.size()) {
		if (relativeEcode) {
			double E = 0;
			for (uint32 i = 0; i < commands.size(); i++)
				E += commands[i].e;
			return E;
		} else {
			for (uint32 i = commands.size() - 1; i > 0; i--)
				if (commands[i].e > 0)
					return commands[i].e;
		}
	}
	return 0;
}

void GCode::translate(float3 trans) {
	for (uint32 i = 0; i < commands.size(); i++)
		commands[i].where += trans;
	box		+= trans;
	center	+= trans;
}


double GCode::GetTimeEstimation() const {
	float3 where(zero);
	double time = 0, feedrate = 0, distance = 0;
	for (uint32 i = 0; i < commands.size(); i++) {
		if (commands[i].f != 0)
			feedrate = commands[i].f;
		if (feedrate != 0) {
			distance = len(commands[i].where - where);
			time	+= distance / feedrate * 60;
		}
		where = commands[i].where;
	}
	return time;
}

string getLineAt(const Glib::RefPtr<Gtk::TextBuffer> buffer, int lineno) {
	Gtk::TextBuffer::iterator from, to;
	from = buffer->get_iter_at_line(lineno);
	to = buffer->get_iter_at_line(lineno + 1);
	return buffer->get_text(from, to);
}

void GCode::updateWhereAtCursor(const dynamic_array<char> &E_letters) {
	int line = buffer->get_insert()->get_iter().get_line();
	// Glib::RefPtr<Gtk::TextBuffer> buf = iter.get_buffer();
	if (line == 0) return;
	string text = getLineAt(buffer, line - 1);
	Command commandbefore(text, zero, E_letters);
	float3 where = commandbefore.where;
	// complete position of previous line
	int l = line;
	while (l > 0 && where.x() == 0) {
		l--;
		text = getLineAt(buffer, l);
		where.x = Command(text, zero, E_letters).where.x;
	}
	l = line;
	while (l > 0 && where.y() == 0) {
		l--;
		text = getLineAt(buffer, l);
		where.y() = Command(text, zero, E_letters).where.y;
	}
	l = line;
	// find last z pos fast
	if (buffer_zpos_lines.size() > 0) {
		for (uint32 i = buffer_zpos_lines.size() - 1; i > 0; i--) {
			if (int(buffer_zpos_lines[i]) <= l) {
				text = getLineAt(buffer, buffer_zpos_lines[i]);
				//cerr << text << endl;
				Command c(text, zero, E_letters);
				where.z() = c.where.z();
				if (where.z() != 0) break;
			}
		}
	}
	while (l > 0 && where.z() == 0) {
		l--;
		text = getLineAt(buffer, l);
		Command c(text, zero, E_letters);
		where.z() = c.where.z;
	}
	// current move:
	text = getLineAt(buffer, line);
	Command command(text, where, E_letters);
	float3 dwhere = command.where - where;
	where.z() -= 0.0000001;
	currentCursorWhere = where + dwhere;
	currentCursorCommand = command;
	currentCursorFrom = where;
}

void GCode::Read(Model *model, const char *E_letters, ViewProgress *progress, string filename) {
	clear();

	ifstream file;
	file.open(filename.c_str());		//open a file
	file.seekg(0, ios::end);
	double filesize = double(file.tellg());
	file.seekg(0);

	progress->start(_("Loading GCode"), filesize);
	int progress_steps = (int)(filesize / 1000);
	if (progress_steps == 0) progress_steps = 1;

	buffer_zpos_lines.clear();

	if (!file.good())
		return;

	set_locales("C");

	uint32 LineNr = 0;

	string s;

	bool	relativePos = false;
	float3	globalPos(zero);
	Min.set(99999999.0, 99999999.0, 99999999.0);
	Max.set(-99999999.0, -99999999.0, -99999999.0);

	dynamic_array<Command> loaded_commands;

	double lastZ = 0;
	double lastE = 0;
	double lastF = 0;
	layerchanges.clear();

	stringstream alltext;

	int current_extruder = 0;

	while (getline(file, s)) {
		alltext << s << endl;

		LineNr++;
		uint32 fpos = file.tellg();
		if (fpos%progress_steps == 0) if (!progress->update(fpos)) break;

		Command command;

		if (relativePos)
			command = Command(s, zero, E_letters);
		else
			command = Command(s, globalPos, E_letters);

		if (command.Code == COMMENT) {
			continue;
		}
		if (command.Code == UNKNOWN) {
			cerr << "Unknown GCode " << s << endl;
			continue;
		}
		if (command.Code == RELATIVEPOSITIONING) {
			relativePos = true;
			continue;
		}
		if (command.Code == ABSOLUTEPOSITIONING) {
			relativePos = false;
			continue;
		}
		if (command.Code == SELECTEXTRUDER) {
			current_extruder = command.extruder_no;
			continue;
		}
		command.extruder_no = current_extruder;

		// not used yet
		//	if (command.Code == ABSOLUTE_ECODE) {
		//		relativeE = false;
		//		continue;
		//	}
		//	if (command.Code == RELATIVE_ECODE) {
		//		relativeE = true;
		//		continue;
		//	}

		if (command.e == 0)
			command.e = lastE;
		else
			lastE = command.e;

		if (command.f != 0)
			lastF = command.f;
		else
			command.f = lastF;

		// cout << s << endl;
		//cerr << command.info()<< endl;
		// if(command.where.x() < -100)
		//   continue;
		// if(command.where.y() < -100)
		//   continue;

		if (command.Code == SETCURRENTPOS) {
			continue;//if (relativePos) globalPos = command.where;
		} else {
			command.addToPosition(globalPos, relativePos);
		}

		if (globalPos.z() < 0) {
			cerr << "GCode below zero!" << endl;
			continue;
		}

		if (command.Code == RAPIDMOTION ||
			command.Code == COORDINATEDMOTION ||
			command.Code == ARC_CW ||
			command.Code == ARC_CCW ||
			command.Code == GOHOME) {

			if (globalPos.x() < Min.x())
				Min.x() = globalPos.x();
			if (globalPos.y() < Min.y())
				Min.y() = globalPos.y();
			if (globalPos.z() < Min.z())
				Min.z() = globalPos.z();
			if (globalPos.x() > Max.x())
				Max.x() = globalPos.x();
			if (globalPos.y() > Max.y())
				Max.y() = globalPos.y();
			if (globalPos.z() > Max.z())
				Max.z() = globalPos.z();
			if (globalPos.z() > lastZ) {
				// if (lastZ > 0){ // don't record first layer
				uint32 num = loaded_commands.size();
				layerchanges.push_back(num);
				loaded_commands.push_back(Command(LAYERCHANGE, layerchanges.size()));
				// }
				lastZ = globalPos.z();
				buffer_zpos_lines.push_back(LineNr - 1);
			} else if (globalPos.z() < lastZ) {
				lastZ = globalPos.z();
				if (layerchanges.size() > 0)
					layerchanges.erase(layerchanges.end() - 1);
			}
		}
		loaded_commands.push_back(command);
	}

	file.close();
	reset_locales();

	commands = loaded_commands;

	buffer->set_text(alltext.str());

	Center = (Max + Min) / 2;

	model->m_signal_gcode_changed.emit();

	double time = GetTimeEstimation();
	int h = (int)time / 3600;
	int min = ((int)time % 3600) / 60;
	int sec = ((int)time - 3600 * h - 60 * min);
	cerr << "GCode Time Estimation " << h << "h " << min << "m " << sec << "s" << endl;
	//??? to statusbar or where else?
}

int GCode::getLayerNo(const double z) const {
	if (layerchanges.size() > 0) // have recorded layerchange indices -> draw whole layers
		for (uint32 i = 0; i < layerchanges.size(); i++) {
			if (commands.size() > layerchanges[i]) {
				if (commands[layerchanges[i]].where.z >= z)
					return i;
			}
		}
	return -1;
}

int GCode::getLayerNo(const uint32 commandno) const {
	if (commandno < 0)
		return commandno;
	if (layerchanges.size() > 0) {// have recorded layerchange indices -> draw whole layers
		if (commandno > layerchanges.back() && commandno < commands.size()) // last layer?
			return layerchanges.size() - 1;
		for (uint32 i = 0; i < layerchanges.size(); i++)
			if (layerchanges[i] > commandno)
				return (i - 1);
	}
	return -1;
}

uint32 GCode::getLayerStart(const uint32 layerno) const {
	return layerchanges.size() > layerno ? layerchanges[layerno] : 0;
}
uint32 GCode::getLayerEnd(const uint32 layerno) const {
	return layerchanges.size() > layerno + 1 ? layerchanges[layerno + 1] - 1 : commands.size() - 1;
}

void GCode::MakeText(string &GcodeTxt, const Settings &settings, ViewProgress * progress) {
	string GcodeStart = settings.get_string("GCode", "Start");
	string GcodeLayer = settings.get_string("GCode", "Layer");
	string GcodeEnd = settings.get_string("GCode", "End");

	double lastE = -10;
	double lastF = 0; // last Feedrate (can be omitted when same)
	float3 pos(zero);
	float3 LastPos(-10, -10, -10);
	stringstream oss;

	Glib::Date date;
	date.set_time_current();
	Glib::TimeVal time;
	time.assign_current_time();
	GcodeTxt += "; GCode by Repsnapper, " +
		date.format_string("%a, %x") +
		//time.as_iso8601() +
		"\n";

	GcodeTxt += "\n; Startcode\n" + GcodeStart + "; End Startcode\n\n";

	layerchanges.clear();
	if (progress) progress->restart(_("Collecting GCode"), commands.size());
	int progress_steps = (int)(commands.size() / 100);
	if (progress_steps == 0) progress_steps = 1;

	double speedalways = settings.get_boolean("Hardware", "SpeedAlways");
	bool useTcommand = settings.get_boolean("Slicing", "UseTCommand");

	const bool relativeecode = settings.get_boolean("Slicing", "RelativeEcode");
	uint32 currextruder = 0;
	const uint32 numExt = settings.getNumExtruders();
	string extLetters = "";

	for (uint32 i = 0; i < numExt; i++)
		extLetters += settings.get_string(settings.numberedExtruder("Extruder", i), "GCLetter")[0];

	for (uint32 i = 0; i < commands.size(); i++) {
		char E_letter;
		if (useTcommand) // use first extruder's code for all extuders
			E_letter = extLetters[0];
		else {
			// extruder change?
			if (i == 0 || commands[i].extruder_no != commands[i - 1].extruder_no)
				currextruder = commands[i].extruder_no;
			E_letter = extLetters[currextruder];
		}
		if (progress && i%progress_steps == 0 && !progress->update(i))
			break;

		if (commands[i].Code == LAYERCHANGE) {
			layerchanges.push_back(i);
			if (GcodeLayer.length() > 0)
				GcodeTxt += "\n; Layerchange GCode\n" + GcodeLayer + "; End Layerchange GCode\n\n";
		}

		if (commands[i].where.z() < 0) {
			cerr << i << " Z < 0 " << commands[i].info() << endl;
		} else {
			GcodeTxt += commands[i].GetGCodeText(LastPos, lastE, lastF, relativeecode, E_letter, speedalways) + "\n";
		}
	}

	GcodeTxt += "\n; End GCode\n" + GcodeEnd + "\n";

	buffer->set_text(GcodeTxt);

	// save zpos line numbers for faster finding
	buffer_zpos_lines.clear();
	uint32 blines = buffer->get_line_count();
	for (uint32 i = 0; i < blines; i++) {
		const string line = getLineAt(buffer, i);
		if (line.find("Z") != string::npos || line.find("z") != string::npos)
			buffer_zpos_lines.push_back(i);
	}

	if (progress)
		progress->stop();

}

GCodeIter::GCodeIter(Glib::RefPtr<Gtk::TextBuffer> buffer) : m_buffer(buffer), m_it(buffer->begin()), num_lines(buffer->get_line_count()), cur_line(1) {
}

void GCodeIter::set_to_lineno(int lineno) {
	cur_line = max((int)0, lineno);
	m_it = m_buffer->get_iter_at_line(cur_line);
}

string GCodeIter::next_line() {
	Gtk::TextBuffer::iterator last = m_it;
	m_it = m_buffer->get_iter_at_line(cur_line++);
	return m_buffer->get_text(last, m_it);
}
string GCodeIter::next_line_stripped() {
	string line = next_line();
	size_t pos = line.find(";");
	if (pos != string::npos) {
		line = line.slice(0, pos);
	}
	size_t newline = line.find("\n");
	if (newline == string::npos)
		line += "\n";
	return line;
}

bool GCodeIter::finished() {
	return cur_line > num_lines;
}

GCodeIter *GCode::get_iter() {
	GCodeIter *iter = new GCodeIter(buffer);
	iter->time_estimation = GetTimeEstimation();
	return iter;
}

Command GCodeIter::getCurrentCommand(float3 defaultwhere, const dynamic_array<char> &E_letters) {
	Gtk::TextBuffer::iterator from, to;
	// cerr <<"currline" << defaultwhere << endl;
	// cerr <<"currline" << (int) cur_line << endl;
	from = m_buffer->get_iter_at_line(cur_line);
	to = m_buffer->get_iter_at_line(cur_line + 1);
	return Command(m_buffer->get_text(from, to), defaultwhere, E_letters);
}



} // namespace iso

#endif //GCODE_H


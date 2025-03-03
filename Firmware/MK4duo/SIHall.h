#pragma once
/**
 * SIHall.h
 * 
 * Scribit hall data handling
 *
 * Created by: Alessio Graziano (alessio.graziano@shinsekai.it)
 * Created: 2019-06-17
 */
#define HOMING_Z_MAX_MOVEMENT 720 //Max mm to move Z if no hall detected (Should be 720° to properly work in any case)
#define HOMING_SPEED_MMM 2400 //Homing speed in mm/min
#define HOMING_MIN_TIME 2000 //Minimum time it should be away from hall sensor befor triggering it

#define MAG A4

//The variable below are updated in HAL::Tick()
//Hall data
extern unsigned int hall_temp;
//New data available flag
extern bool hall_newData;
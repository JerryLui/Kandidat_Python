#include <Servo.h>

// Global Constants
# define analogMax 1023
# define pi 3.1416

// Servo Constants
Servo servo;
const int servoResolution = 2;		// Number of steps per servo degree
const int minAngle = 0;
const int maxAngle = 180*servoResolution;

// IR-sensor Constants
const int sensorPin = A5;

const int sensorThreshold = 80; 	// Threshold value for a read
const int spanSizeThreshold = 3;
const int sensorRead = 20;				// Times to read the sensor data
const int sensorReadThreshold = analogMax*sensorRead*0.10;	// Upperbound for ir

// Direction Variables
#define lDirection 4
#define lStep 3
#define lOutput 5

#define rDirection 7
#define rStep 6
#define rOutput 2

// Motor Constants
const int stepsPerTurn = 391;							// Nmbr of steps per turn __NOT PRECISE__

// Motor Variables
const int motorDelayUpperBound = 6000;		// Delaytime for motor
const int motorDelayLowerBound = 2000;
const int delayIncrement = 40;
const int accelerationLength = (motorDelayUpperBound-motorDelayLowerBound)/delayIncrement;

// Global Variables
int globalMotorDelay = motorDelayLowerBound;

// Ultrasound Constants
const int trigPin = 2;					// Respective pins for ultrasound
const int echoPin = 3;

const int maxDistance = 100;		// Maximum distance in cm
const int minDistance = 10;

// Line Sensor Constants
#define lineThreshold 30

const int lineLeftPin = A3;
const int lineRightPin = A4;

// Directions
enum Direction {
	LEFT, RIGHT, STRAIGHT, BACK, UNKNOWN
};

// Setup
void setup() {
	// Initiate Serial Contact (for debug purposes)
	Serial.begin(9600);

	// Motor Setup
	pinMode(lOutput, OUTPUT);pinMode(lStep, OUTPUT);pinMode(lDirection, OUTPUT);
	pinMode(rOutput, OUTPUT);pinMode(rStep, OUTPUT);pinMode(rDirection, OUTPUT);
	digitalWrite(lOutput, LOW);digitalWrite(lDirection, HIGH);
	digitalWrite(rOutput, LOW);digitalWrite(rDirection, LOW);

	// Servo Setup
	pinMode(sensorPin, INPUT_PULLUP);
	servo.attach(13);

	// Ultrasound Setup
	pinMode(trigPin, OUTPUT);pinMode(echoPin, INPUT);

	// Line Sensor Setup
	pinMode(lineLeftPin, INPUT);pinMode(lineRightPin, INPUT);
}

int test = 1;
// Main Loop
void loop() {
	if (test == 1) {
		debugMotorFunctions();
		test++;
	}
	else if (test == 2) {
		debugPrintDirection(getLineDirection());
		test++;
	}
	else if (test == 3) {
		debugPrintServoScan() {
		test++;
	}
}

/*~~~~~~~~~~ Robot Logic ~~~~~~~~~~*/
void navigate() {
	// Robo Logic
	// Scan for signal in 180 degrees in front of robot
	int angle = servoScan();

	// Check if signal out of bounds
	if (angle < 0 || angle > 180)
		turnAround();	
	else if (angle > 95) 
		turn(LEFT, abs(90-angle));	// Rotate robot by given angular difference
	else if (angle < 85) 
		turn(RIGHT, abs(90-angle)); // Rotate robot by given anlgular difference
	else {												// Else, signal is straight ahead
		if (getLineDirection() == STRAIGHT) {	// While no line detected 
			distanceNavigation(angle);
		} else {
			dockingNavigation(getLineDirection());
		}
	}
}

// Navigation for long distance
void distanceNavigation(int angle) {
	int distance;
	Direction lineDirection = getLineDirection();

	// Turns IR-sensor towoards signal
	servoTurn(angle);

	// While there's a signal and no lines are detected
	while (lineDirection == STRAIGHT && readSensor()) {
		// Retrieve distance from ultrasound sensor
		distance = getDistance();
		// If distance longer than 1m: walk towoards signal with small steps 
		if (distance > 100 || distance < 10) {
			longWalk(STRAIGHT, 10);
		}
		// If distance longer than ultrasound lower bound walk all the way towoards it
		else if (distance > 20) {
			longWalk(STRAIGHT, 0.95*distance);
		}
	}
}

// Navigation for docking 
void dockingNavigation(Direction dir) {
	while (!inDock(dir)) {
		shortWalk(dir, 10);
		dir = getLineDirection();
	}
}

// Checks if in dock
bool inDock(Direction dir) {
	if (dir == UNKNOWN) 
		return true;
	else
		return false;
}


/*~~~~~~~~~~ Line Sensor Functions ~~~~~~~~~~*/
// Line Sensor Test Function
void debugPrintDirection(Direction dir) {
	Serial.print("Direction: ");
	switch (dir) {
		case LEFT:
			Serial.println("Left");
			break;
		case RIGHT:
			Serial.println("Right");
			break;
		case STRAIGHT:
			Serial.println("Straight");
			break;
		case BACK:
			Serial.println("Back");
			break;
		case UNKNOWN:
			Serial.println("Unknown");
			break;
	}
}

// Reads data from line sensors, returns direction to go towoards
Direction getLineDirection() {
	bool leftData = readLeftLineSensor();
	bool rightData = readRightLineSensor();

	if (!leftData && !rightData)
		return STRAIGHT; // When no line found
	else if (leftData && !rightData)
		return RIGHT; 	// When line detected on left sensor
	else if (!leftData && rightData)
		return LEFT;		// When line detected on right sensor
	else
		return UNKNOWN;	// When line found
}
// Reads data from left line sensor, returns true if on a line
bool readLeftLineSensor() {
	return analogRead(lineLeftPin) > lineThreshold;
}

// Reads data from right line sensor, returns true if on a line
bool readRightLineSensor() {
	return analogRead(lineRightPin) > lineThreshold;
}

/*~~~~~~~~~~ Servo Functions ~~~~~~~~~~*/
// Debug Servo
void debugPrintServoScan() {
	Serial.print("Angle: ");
	Serial.println(servoScan());
}

// Scans 0-180 degrees in front of the robot, returns angle for which there is an signal
int servoScan() {
	bool clockwiseReadings [maxAngle];
	bool counterClockwiseReadings [maxAngle];
	bool combinedReadings [maxAngle];

	// Sweeps the servo from minAngle to maxAngle while collecting the reading
	for (int i = minAngle; i<maxAngle; i++) {
		servoTurn(i/servoResolution);
		clockwiseReadings[i] = readSensor();
	}
	// does another sweep backwards
	for (int i = maxAngle; i>minAngle; i--) {
		servoTurn(i/servoResolution);
		counterClockwiseReadings[i] = readSensor();
	}

	int spanSize = 0;
	int spanEnd = 0;
	int maxSpanSize = 0;

	// Experimental method of calculating signal pos
	int simpleMovingAverage = 0;
	int elements = 0;

	// Compares the readings from the two sweeps and finds the largest span
	for (int i = minAngle; i<maxAngle; i++) {
		// Sets combinedReadings to true if both sweeeps show an positive signal at i
		if (clockwiseReadings[i] && counterClockwiseReadings[i])
			combinedReadings[i] = true;

		// Increases the spanSize for each run
		if (combinedReadings[i]) {
			spanSize++;
			if (spanSize > 1) {							// TODO: Test experimental method of calculating signal pos
				simpleMovingAverage += i; 		// Current method finds the maximum span of signals found
				elements++;										// SMA finds the average signal pos from measurements
			}
		} else if (spanSize != 0) {
			if (spanSize > maxSpanSize) {
				maxSpanSize = spanSize;
				spanEnd = i-1;
			}
			spanSize = 0;
		}
	}

	// Returns the angle for which there's a signal, returns -1 if nothing found
	if (maxSpanSize < spanSizeThreshold) {
		return -1;
	} else {
		return (spanEnd-maxSpanSize/2)/servoResolution;
	}
}

// Turns servo to the given angle
void servoTurn(double angle) {
	servo.write(angle);
}

// Reads sensor data
bool readSensor() {
	int reading = 0;

	// Reads sensor data sensorRead times
	for (int i = 0; i < sensorRead; i++) {
		reading += analogRead(sensorPin);	// Data from IR is HIGH when no signal
	}

	// If reading is less than sensorReadThreshold return true
	if (reading < sensorReadThreshold)
		return true;
	else
		return false;
}

/*~~~~~~~~~~ Ultrasound Functions ~~~~~~~~~~*/
// Print distance data from ultrasound
void debugPrintUltrasound() {
	Serial.print("Distance: ");
	Serial.println(getDistance());
}

// Gets the distance to object in front of robot
int getDistance() {
	sendEcho();
	return recieveEcho();
}

// Sends an ultrasound pulse
void sendEcho() {
	digitalWrite(trigPin, LOW);
	delayMicroseconds(2);
	digitalWrite(trigPin, HIGH);			// Send an exiting pulse
	delayMicroseconds(10);
	digitalWrite(trigPin, LOW);
}

// Recieves an ultrasound pulse and returns the distance
int recieveEcho() {
	int distance = pulseIn(echoPin, HIGH)/29/2;

	pulseIn(echoPin, LOW);
	if (distance < maxDistance && distance > minDistance) {
		return distance;
	} else {
		return 0;
	}
}

/*~~~~~~~~~~ Motor Functions ~~~~~~~~~~~~*/
// Tests motor functions
void debugMotorFunctions() {
	int debugDelayMillis = 800;

	// Long Walk Functions
	longWalk(LEFT, 5);
	delay(debugDelayMillis);
	longWalk(RIGHT, 5);
	delay(debugDelayMillis);
	longWalk(STRAIGHT, 20);
	delay(debugDelayMillis);
	longWalk(STRAIGHT, 5);
	delay(debugDelayMillis);
	longWalk(BACK, 10);
	delay(debugDelayMillis);

	// Short Walk Functions
	shortWalk(LEFT, 20);
	delay(debugDelayMillis);
	shortWalk(RIGHT, 20);
	delay(debugDelayMillis);
	shortWalk(STRAIGHT, 20);
	delay(debugDelayMillis);
	shortWalk(BACK, 20);
	delay(debugDelayMillis);

	// Turn Functions
	turn(LEFT, 45);
	delay(debugDelayMillis);
	turn(LEFT, 135);
	delay(debugDelayMillis);
	turn(RIGHT, 45);
	delay(debugDelayMillis);
	turn(RIGHT, 135);
	delay(debugDelayMillis);
	turn(STRAIGHT, 45);
	delay(debugDelayMillis);
}

// Walks towoards direction in given length in centimeters
void longWalk(Direction dir, int length) {
	direction(dir);

	int steps = lengthToSteps(length);
	// Walks given steps, if nmbr of steps is longer than 2*accelerationLenght
	// skip constant run.
	if ((steps-2*accelerationLength) < 0) {
		accelerate(steps/2);
		deaccelerate(steps/2);
	} else {
		accelerate(accelerationLength);
		runMotor(steps-2*accelerationLength);
		deaccelerate(accelerationLength);
	}
}

// Short walk without acceleration and deacceleration
void shortWalk(Direction dir, int steps) {
	globalMotorDelay = motorDelayUpperBound;
	direction(dir);
	runMotor(steps);
}

/*	Converts length into motor steps

		The wheels have a radius of 3.4 cm, which results in a circumference
		of 6.8*pi cm. To convert from length to steps we use the constant steps-
		PerTurn seen in the return formula below. 															*/
int lengthToSteps(int length) {
	return length/(6.8*pi) * stepsPerTurn;
}

// Turns towoards direction by given angle
void turn(Direction dir, int angle) {
	if (dir != LEFT || dir != RIGHT) {}
	else {
		direction(dir);
		globalMotorDelay = motorDelayLowerBound;
		runMotor(angle/180*stepsPerTurn);
	}
}
// Runs motor for given amount of steps
void runMotor(int steps) {
	for (int i = 0; i < steps; i++) {
		step(globalMotorDelay);
	}
}

// Accelerates in given number of steps, returns the current motor delay
void accelerate(int steps) {
	int currentDelay = motorDelayUpperBound;
	for (int i = 0; i < steps; i++) {
		step(currentDelay);
		currentDelay = decrementDelay(currentDelay);
	}
	globalMotorDelay = currentDelay;
}

// Deaccelerates in given number of steps, returns the current motor delay
void deaccelerate(int steps) {
	int currentDelay = globalMotorDelay;
	for (int i = 0; i < steps; i++) {
		step(currentDelay);
		currentDelay = incrementDelay(currentDelay);
	}
	globalMotorDelay = currentDelay;
}

// Increments motor delay time by delayIncrement
int incrementDelay(int delayTime) {
	if (delayTime < motorDelayUpperBound) {
		return delayTime + delayIncrement;
	} else {
		return delayTime;
	}
}

// Decrements motor delay time by delayIncrement
int decrementDelay(int delayTime) {
	if (delayTime > motorDelayLowerBound) {
		return delayTime - delayIncrement;
	} else {
		return delayTime;
	}
}

// Turns around 180 degrees anti clockwise 
void turnAround() {
	turnAround(LEFT);
}

// Turns around 180 degree in the given direction
void turnAround(Direction dir) {
	turn(dir, 180);
}

// Moves one step by sending out a square wave with the input motorDelay
void step(int motorDelay) {
	delayMicroseconds(motorDelay);
	digitalWrite(lStep, HIGH);digitalWrite(rStep, HIGH);
	delayMicroseconds(motorDelay);
	digitalWrite(lStep, LOW);digitalWrite(rStep, LOW);
}

// Changes the direction of motors
void direction(Direction dir) {
	switch(dir) {
		case LEFT:
			digitalWrite(lDirection, LOW);
			digitalWrite(rDirection, LOW);
			break;
		case RIGHT:
			digitalWrite(lDirection, HIGH);
			digitalWrite(rDirection, HIGH);
			break;
		case STRAIGHT:
			digitalWrite(lDirection, HIGH);
			digitalWrite(rDirection, LOW);
			break;
		case BACK:
			digitalWrite(lDirection, LOW);
			digitalWrite(rDirection, HIGH);
			break;
	}
}
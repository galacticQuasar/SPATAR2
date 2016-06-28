

// FROM FILE: main.c
///hjiliouh
// Game data:
float origin[3];
float sphereRadius;
float asteroidRadius;
float dangerZoneRadius;
float innerZoneRadius;
float outerZoneRadius;
float shadowZoneTarget[3];

// Motion data:
float velocity[3];
float position[3];
float ZRState[12];

// Movement data:
float mass;
float lastVelocity[3];
float acceleration[3];
float maxForce;
float lastForceMagnitude;

// Utility Data:
float atPointTollerance;
float wayPointSpeed;

//Camera Data:
bool takenInner[3];
bool takenOuter[3];
int poi;

//State Data:
int fState;

void init(){
    // Setting game data:
    origin[0] = 0.f;
    origin[1] = 0.f;
    origin[2] = 0.f;

    sphereRadius = 0.11f;
    asteroidRadius = 0.2f;
    dangerZoneRadius = 0.31f;
    innerZoneRadius = 0.42f;
    outerZoneRadius = 0.53f;

    shadowZoneTarget[0] = 0.32;
    shadowZoneTarget[1] = 0.0;
    shadowZoneTarget[2] = 0.0;

    // Setting movement data:
    mass = 5.248637919480517f;
    assignVectorToVector(lastVelocity, origin);
	maxForce = 0.05f;
    lastForceMagnitude = 0.f;
	
    // Setting utility data:
    atPointTollerance = 0.01f;
    wayPointSpeed = 0.03f;

    // Setting up Finite State Machine:
    fState = 99;

    // Setting camera data:
    takenInner[0] = false;
    takenInner[1] = false;
    takenInner[2] = false;
    takenOuter[0] = false;
    takenOuter[1] = false;
    takenOuter[2] = false;
    poi = 0;
}

void loop(){
    if (canUpload(position)) {
        DEBUG(("canUpload!!!"));
    }
    movementStart();
    FSM();
    movementEnd();
}

// FROM FILE: fsm/fsm.c

void FSM() {
    //Finite State Machine:
    /*
        00: Take photos,
            00: Move to photo taking position,
            01: Take a photo,
        10: Avoid Solar Damage,
            10: Move into shadow zone and upload,
        20: Upload images,
            20: go to upload position and upload,
    */
    // If solar flare or the like, go to case 10?
    bool _continue = true;
    while (_continue) {
        DEBUG(("[fState: %i]", fState));
        switch(fState){
            //Make a decision
            case 99:
                // Decisions should be made here.
                if (game.getNextFlare() == -1) {
                    // If there is no flare coming.
                    fState = 00;
                } else {
                    fState = 10;
                }
                break;
            //Take Photos
            case 00:
                if (possiblePhotos() < 1) {
                    // If you can't take photo's, move to somewhere where you can.
                    moveToPhotoPos();
                    _continue = false;
                } else {
                    // If you can take photos, change to the photo taking state.
                    fState = 01;
                }
                break;
            case 01:
                takePhoto(poi); // Take the photo.
                if (game.getMemoryFilled() == game.getMemorySize()) {
                    // If memory is full, try to upload:
                    fState = 20;
                } else {
                    fState = 99;
                    _continue = false;
                }
                break;
            //Avoid Solar Damage
            case 10:
                if(inShadow(position)) {
                    // If you are already in the shadow zone...
                    game.uploadPic();
                }
                move(shadowZoneTarget, 0); // Move to the shadow zone.
                fState = 99;
                _continue = false;
                break;
            //Upload Images
            case 20:
                if(canUpload(position)) {
                    game.uploadPic();
                    fState = 99;
                    _continue = false;
                } else {
                    moveToUploadPos();
                }
                break;
        }
    }
}

// FROM FILE: movement/movement.c

void basicMove(float targetPosition[3], float targetVelocityMagnitude) {
    // This function is designed to take a targetPosition, and a targetVelocityMagnitude to be at when it reaches that position.
    // If you want to stop there, set the targetVelocity to 0, and this function will simply use setPositionTarget.
    // If, however, you do not wish to stop, this will try to be as close as possible to the target velocity when it arrives.
    // If is entirely possible, that even with a reasonably low targetVelocityMagitude it will not be able to reach it.
    if (targetVelocityMagnitude == 0) {
        // Setting position target if you want to stop there.
        api.setPositionTarget(targetPosition);
    } else {
        // Setting up the required vectors.
        float targetVelocity[3];
        float requiredAcceleration[3];
        float requiredForce[3];
        // Initially just gets the direction of the required velocity, which is identical to the displacement between where you are and where you want to be.
        mathVecSubtract(targetVelocity, targetPosition, position, 3);
        if (!mathVecMagnitude(targetVelocity, 3)) {
            // If silly stuff is happening and this is zero, don't try to apply forces.
            api.setPositionTarget(targetPosition);
            return;
        }
        // Then it sets the magnitude of the targetVelocity to what you want it to be.
        setMagnitude(targetVelocity, targetVelocityMagnitude);
        // Then it works out how much you have to accelerate to make it to that this second.
        // a = (v - u) / t (But t is 1).
        mathVecSubtract(requiredAcceleration, targetVelocity, velocity, 3);
        // Then, F = ma
        multiplyVectorByScalar(requiredForce, requiredAcceleration, mass);
        // But, if it couldn't actually apply this much force, it reduces it to the maximum force it can apply.
        // (Found to be ~0.05N through experimentation).
        // While acctually applying more would be fine, it would make mass predictions wildly inaccurate.
        if (mathVecMagnitude(requiredForce, 3) > maxForce) {
            setMagnitude(requiredForce, maxForce);
        }
        // Here it actually sets the force.
        api.setForces(requiredForce);
        // And here it records it for use in updateMass()
        lastForceMagnitude = mathVecMagnitude(requiredForce, 3);
    }
}

void move(float targetPosition[3], float targetVelocityMagnitude) {
    // This function is designed to get from place to place, using basicMove(), but to account for the asteroid.
    float wayPoint[3]; // The current way point to be used.
    bool isWayPoint = false; // Is the point you end up going to a wayPoint?
    closestPointInIntervalToPoint(wayPoint, origin, position, targetPosition); // The wayPoint is the closes point to the asteroid.
    // But, if it is inside the raidus, it needs to be adjusted.
    int x = 0;
    while (mathVecMagnitude(wayPoint, 3) <= dangerZoneRadius + sphereRadius + atPointTollerance && x < 5) {
        x ++;
        isWayPoint = true;
        setMagnitude(wayPoint, dangerZoneRadius + sphereRadius + atPointTollerance);
        closestPointInIntervalToPoint(wayPoint, origin, position, wayPoint); // Setting to check if there is a point along this line.
    }
    // If the path passes through the origin for instance, the wayPoint will be null
    // Don't use paths which pass through the origin.
    if (isWayPoint && mathVecMagnitude(wayPoint, 3)) {
        basicMove(wayPoint, wayPointSpeed);
    } else {
        basicMove(targetPosition, targetVelocityMagnitude);
    }
}

// Face a particular point:
void facePoint(float point[3]) {
    float direction[3];
    // The vector b - the vector a is the vector which points from a to b, or the displacement of b, from a.
    // Hence, point - position = the direction of point from position.
    mathVecSubtract(direction, point, position, 3);
    // This makes it a unit vector (i.e. magnitude of 1), which is what setAttitudeTarget wants.
    mathVecNormalize(direction, 3);
    // Sets the attitude target.
    api.setAttitudeTarget(direction);
}

void updateMass() {
    // This function takes the force applied last second, and the acceleration experiences since then, and uses it to predict your mass.
    // THIS FUNCTION WILL BE INCORRECT IF BLACK BOX FUNCTIONALITY HAS OVERRIDDEN MOTION!
    // e.g. Run out of fuel, hit asteroid, hit other sphere.
    float accelerationMagnitude = mathVecMagnitude(acceleration, 3);
    // If either lastForceMagnitude or accelerationMagnitude is zero, this function should not run.
    // If lastForceMagnitude is zero, a force was not applied last frame, and as such, the equation would be invalid.
    // Acceleration magntidue should never be zero while lastForceMagnitude is not zero, but weird things happen, and it is not worth the risk.
    // As such, if either or them are zero, them multiplied together will be zero, and this will not run.
    if (lastForceMagnitude * accelerationMagnitude) {
        // F = ma
        // a = F / m
        float newMass = lastForceMagnitude / accelerationMagnitude;
        // Takes the average of the mass predicted by this, and what you thought the mass was before,
        // to prevent anomolous [or black box] motion from having too large an effect.
        mass = (mass + newMass) / 2;
    }
}

// This must be called at the start of every loop.
void movementStart() {
    // Setting motion data:
    api.getMyZRState(ZRState);
    //api.getOtherZRState(OZRState);
    for (int i = 0; i < 3; i++) {
        position[i] = ZRState[i];
        velocity[i] = ZRState[i + 3];
    }
    mathVecSubtract(acceleration, velocity, lastVelocity, 3);
    updateMass();
    lastForceMagnitude = 0.f; // Very important in mass calculation [Positioning in loop included].
}

// This must be called at the end of every loop.
void movementEnd() {
    // More setting motion data:
    assignVectorToVector(lastVelocity, velocity);
}

// FROM FILE: photos/photos.c

void moveToPhotoPos() {
  bool outer;

  float targetPos[3];
  getNextNode(&outer, &poi);
  getTargetPos(targetPos, &outer, &poi);
  move(targetPos, 0);
  // Face the point.
  float poiPos[3];
  game.getPOILoc(poiPos, poi);
  facePoint(poiPos);
}

void moveToUploadPos() {
	float displacementToShadow[3];
	mathVecSubtract(displacementToShadow, shadowZoneTarget, position, 3);
	float distanceToShadow = mathVecMagnitude(displacementToShadow, 3);
	float distanceToOutside = outerZoneRadius - mathVecMagnitude(position, 3);
	if (distanceToShadow <= distanceToOutside && !lineFromPointToPointPassesThroughDanger(position, shadowZoneTarget)) {
		move(shadowZoneTarget, 0.f);
		return;
	}
	float outerZonePoint[3];
	assignVectorToVector(outerZonePoint, position);
	setMagnitude(outerZonePoint, outerZoneRadius + atPointTollerance);
	move(outerZonePoint, 0.f);
}

void getNextNode(bool* outer, int* poi) {
  for(int i = 0; i < 3; i++) {
    if(!takenInner[i]) {
      *outer = false;
      *poi = i;
      break;
    }
  }
  for(int i = 0; i < 3; i++) {
    if(!takenOuter[i]) {
      *outer = true;
      *poi = i;
      break;
    }
  }
}

void getTargetPos(float pos[3], bool* outer, int* poi) {
  game.getPOILoc(pos, *poi);
  setMagnitude(pos, 0.35 + 0.12 * int(*outer));
}

int possiblePhotos() {
  // Returns number of possible photos.
  if (game.getMemoryFilled() == game.getMemorySize()) {
        // If memory is full.
        return 0;
    }
  int num = 0;
  for (int i = 0; i < 3; i++) {
      float POILoc[3];
      float playerVector[3];
      game.getPOILoc(POILoc, i);
      mathVecSubtract(playerVector, position, POILoc, 3);
    if (game.alignLine(i)) {
        if(inInner(position) && angleBetweenVectors(playerVector, POILoc) <= 0.8f){
            num += 1;
        }
        else if(inOuter(position) && angleBetweenVectors(playerVector, POILoc) <= 0.4f){
            num += 1;
        }
    }
  }
  return num;
}

void takePhoto(int poid) {
    game.takePic(poid);
    if(inOuter(position)) {
        takenOuter[poid] = true;
    } else if(inInner(position)) {
        takenInner[poid] = true;
    }
}

bool canUpload(float position[3]) {
	bool allowed = false;
	//outer max radius
	float photoRadius = 0.53;


	//check if not within photo-zones
	if (mathVecMagnitude(position, 3) > photoRadius) {
		allowed = true;
	}

	//check if in shadow zone
	if (inShadow(position)) {
		allowed = true;
	}

	return allowed;
}

// FROM FILE: util/util.c

// Multiply the magnitude of a vector by a particular amount [api appeared to only do vector vector multiplication]:
void multiplyVectorByScalar(float final[3], float vector[3], float scalar) {
	// Multiplying each component of a vector will multiply the magnitude by that much.
    for (int i = 0; i < 3; i++) {
        final[i] = vector[i] * scalar;
    }
}

// Set the magnitude of a vector to be a particular amount:
void setMagnitude(float vector[3], float magnitude) {
	// Normalising a vector is to make its magnitude one, but preserve its direction.
	// As such, if you normalise it, its magnitude is now one, and so if you then multiply it by a particular amount, that will be the magnitude.
    mathVecNormalize(vector, 3);
    multiplyVectorByScalar(vector, vector, magnitude);
}

// Functions returning booleans based on whether or not the point passed to them is in a zone.
bool inDanger (float point[3]) {
	return (
		mathVecMagnitude(point, 3) <= dangerZoneRadius
	);
}

bool inInner (float point[3]) {
	return (
		!inDanger(point) && mathVecMagnitude(point, 3) <= innerZoneRadius
	);
}

bool inOuter (float point[3]) {
	return (
		!inInner(point) && mathVecMagnitude(point, 3) <= outerZoneRadius
	);
}

// returns true if in the shadow zone, where it is treated as a CYLINDER
bool inShadow (float point[3]) {
	//if on shadow side of the asteroid and within yz radius
	if (point[0] > 0 && sqrtf(mathSquare(point[1]) + mathSquare(point[2]) <= asteroidRadius)) {
		return true;
	} else {
		return false;
	}
}

float distanceFromPointToInterval(float point[3], float lineStart[3], float lineEnd[3]) {
	float q[3]; // The closest point in the interval to point.
	closestPointInIntervalToPoint(q, point, lineStart, lineEnd);
	float displacement[3];
	mathVecSubtract(displacement, point, q, 3);
	return mathVecMagnitude(displacement, 3);
}

void closestPointInIntervalToPoint(float pointToFill[3], float point[3], float lineStart[3], float lineEnd[3]) {
	// See documentation/distanceFromPointToLine.txt to see what I am doing here.
	float n[3]; // The normal vector of the plane, the line passing through lineStart and lineEnd.
	mathVecSubtract(n, lineEnd, lineStart, 3);
	float a[3]; // Point. A point on the place, sorry to do this, but I wanted nice inputs but also couldn't stand the maths with long names.
	for (int i = 0; i < 3; i++) {
		a[i] = point[i];
	}
	float q[3]; // The point of intersection of the plane and the line. <-- What we are looking for.
	float b[3]; // Any point on the line.
	for (int i = 0; i < 3; i++) {
		b[i] = lineStart[i];
	}
	// You really need to read the documentation for this (and the stuff below the if statement):
	float t = (n[0]*a[0] + n[1]*a[1] + n[2]*a[2] - n[0]*b[0] - n[1]*b[1] - n[2]*b[2]) / (mathSquare(n[0]) + mathSquare(n[1]) + mathSquare(n[2]));
	// But, if t < 0 or t > 1, the point q is not between lineStart and lineEnd.
	// Which I believe means that the closest point on the line to the point must be one of the ends.
	if (t < 0 || t > 1) {
		float displacement1[3];
		mathVecSubtract(displacement1, point, lineStart, 3);
		float displacement2[3];
		mathVecSubtract(displacement2, point, lineEnd, 3);
		if (mathVecMagnitude(displacement1, 3) <= mathVecMagnitude(displacement2, 3)) {
			for (int i = 0; i < 3; i++) {
				pointToFill[i] = lineStart[i];
			}
			return;
		}
		for (int i = 0; i < 3; i++) {
			pointToFill[i] = lineEnd[i];
		}
		return;
	}
	// But, if the point is between them...
	float tn[3];
	multiplyVectorByScalar(tn, n, t);
	mathVecAdd(q, b, tn, 3);
	for (int i = 0; i < 3; i++) {
		pointToFill[i] = q[i];
	}
}

// Returns true if traveling between point1 and point2 would take the sphere through the danger zone.
bool lineFromPointToPointPassesThroughDanger(float point1[3], float point2[3]) {
    return (
        distanceFromPointToInterval(origin, point1, point2) <= dangerZoneRadius + sphereRadius
    );
}

// Sets one vector to equal another.
void assignVectorToVector(float vectorToChange[3], float vectorToChangeOtherVectorInto[3]) {
	for (int i = 0; i < 3; i++) {
		vectorToChange[i] = vectorToChangeOtherVectorInto[i];
	}
}

float angleBetweenVectors(float vector1[3], float vector2[3]){
    float angle = 0;
    angle = acosf((mathVecInner(vector1, vector2, 3))/(fabsf(mathVecMagnitude(vector1, 3)) * fabsf(mathVecMagnitude(vector2, 3))));
    angle = angle * (3.141593 / 180);
    if(angle > 0){
        return angle;
    }
    return angle * -1;
}

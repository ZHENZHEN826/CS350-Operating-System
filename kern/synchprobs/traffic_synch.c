#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <array.h>
#include <opt-A1.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */


// 1 lock for 1 intersection
static struct lock *intersectionLock;
// 12 cv for 12 different directions
static struct cv *NorthEast_CV, *NorthWest_CV, *NorthSouth_CV;
static struct cv *SouthWest_CV, *SouthNorth_CV, *SouthEast_CV;
static struct cv *WestEast_CV, *WestNorth_CV, *WestSouth_CV;
static struct cv *EastSouth_CV, *EastWest_CV, *EastNorth_CV;
// 12 int: For each direction, count number of cars in the intersection
static volatile int NorthEast_dir, NorthWest_dir, NorthSouth_dir;
static volatile int SouthWest_dir, SouthNorth_dir, SouthEast_dir;
static volatile int WestEast_dir, WestNorth_dir, WestSouth_dir;
static volatile int EastSouth_dir, EastWest_dir, EastNorth_dir;

// Determine vehicle's turn: left turn, right turn, or straight
Turn determineTurn(Direction origin, Direction destination){
  if (((origin == west) && (destination == south)) ||\
      ((origin == south) && (destination == east)) ||\
      ((origin == east) && (destination == north)) ||\
      ((origin == north) && (destination == west))) {
    return right;
  } else if (((origin == north) && (destination == south)) ||\
      ((origin == south) && (destination == north)) ||\
      ((origin == east) && (destination == west)) ||\
      ((origin == west) && (destination == east))){
    return straight;
  } else if (((origin == south) && (destination == west)) ||\
      ((origin == west) && (destination == north)) ||\
      ((origin == north) && (destination == east)) ||\
      ((origin == east) && (destination == south))) {
    return left;
  } else {
    panic("car turn not exist");
    return -1;
  }
}

/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
  intersectionLock = lock_create("intersectionLock");
  if (intersectionLock == NULL) {
    panic("could not create intersection lock");
  }
  NorthEast_CV = cv_create("NE");
  NorthWest_CV = cv_create("NW");
  NorthSouth_CV = cv_create("NS");
  SouthWest_CV = cv_create("SW");
  SouthNorth_CV = cv_create("SN");
  SouthEast_CV = cv_create("SE");
  WestEast_CV = cv_create("WE");
  WestNorth_CV = cv_create("WN");
  WestSouth_CV = cv_create("WS");
  EastSouth_CV = cv_create("ES");
  EastWest_CV = cv_create("EW");
  EastNorth_CV = cv_create("EN");
  if ((NorthEast_CV == NULL) || (NorthWest_CV == NULL) || (NorthSouth_CV == NULL)\
    || (SouthWest_CV == NULL) || (SouthNorth_CV == NULL) || (SouthEast_CV == NULL)\
    || (WestEast_CV == NULL) || (WestNorth_CV == NULL) || (WestSouth_CV == NULL)\
    || (EastSouth_CV == NULL) || (EastWest_CV == NULL) || (EastNorth_CV == NULL)){
    panic("could not create some cv");
  }
  NorthEast_dir = 0; 
  NorthWest_dir = 0;
  NorthSouth_dir = 0; 
  SouthWest_dir = 0; 
  SouthNorth_dir = 0; 
  SouthEast_dir = 0;
  WestEast_dir = 0; 
  WestNorth_dir = 0; 
  WestSouth_dir = 0;
  EastSouth_dir = 0;
  EastWest_dir = 0;
  EastNorth_dir = 0;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  KASSERT(intersectionLock != NULL);
  KASSERT(NorthEast_CV);
  KASSERT(NorthWest_CV);
  KASSERT(NorthSouth_CV);
  KASSERT(SouthWest_CV);
  KASSERT(SouthNorth_CV);
  KASSERT(SouthEast_CV);
  KASSERT(WestEast_CV);
  KASSERT(WestNorth_CV);
  KASSERT(WestSouth_CV);
  KASSERT(EastSouth_CV);
  KASSERT(EastWest_CV);
  KASSERT(EastNorth_CV);
  lock_destroy(intersectionLock);
  cv_destroy(NorthEast_CV);
  cv_destroy(NorthWest_CV);
  cv_destroy(NorthSouth_CV);
  cv_destroy(SouthWest_CV);
  cv_destroy(SouthNorth_CV);
  cv_destroy(SouthEast_CV);
  cv_destroy(WestEast_CV);
  cv_destroy(WestNorth_CV);
  cv_destroy(WestSouth_CV);
  cv_destroy(EastSouth_CV);
  cv_destroy(EastWest_CV);
  cv_destroy(EastNorth_CV);
  return;
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
  KASSERT(intersectionLock != NULL);
  lock_acquire(intersectionLock);
    Turn thisTurn = determineTurn(origin,destination); 
    if (thisTurn == right){
      // 4 Right Turn

      if ((origin == south) && (destination == east)) {
        while ((WestEast_dir != 0) || (NorthEast_dir != 0)){
          cv_wait(SouthEast_CV, intersectionLock);
        }
        SouthEast_dir += 1;
      } else if ((origin == east) && (destination == north)) {
        while ((SouthNorth_dir != 0) || (WestNorth_dir != 0)){
          cv_wait(EastNorth_CV, intersectionLock);
        }
        EastNorth_dir += 1;
      } else if ((origin == north) && (destination == west)) {
        while ((EastWest_dir != 0) || (SouthWest_dir != 0)){
          cv_wait(NorthWest_CV, intersectionLock);
        }
        NorthWest_dir += 1;
      } else if ((origin == west) && (destination == south)) {
        while ((NorthSouth_dir != 0) || (EastSouth_dir != 0)){
          cv_wait(WestSouth_CV, intersectionLock);
        }
        WestSouth_dir += 1;
      } else {
        panic("this right turn not exist");
      }

    } else if (thisTurn == straight){
      // 4 straight

      if ((origin == south) && (destination == north)) {
        while ((EastWest_dir != 0) || (EastNorth_dir != 0) || (EastSouth_dir != 0) \
          || (WestEast_dir != 0) || (WestNorth_dir != 0) || (NorthEast_dir != 0)){
          cv_wait(SouthNorth_CV, intersectionLock);
        }
        SouthNorth_dir += 1;
      } else if ((origin == east) && (destination == west)) {
        while ((NorthWest_dir != 0) || (NorthEast_dir != 0) || (NorthSouth_dir != 0) \
          || (SouthWest_dir != 0) || (SouthNorth_dir != 0) || (WestNorth_dir != 0)){
          cv_wait(EastWest_CV, intersectionLock);
        }
        EastWest_dir += 1;
      } else if ((origin == north) && (destination == south)) {
        while ((WestNorth_dir != 0) || (WestEast_dir != 0) || (WestSouth_dir != 0) \
          || (EastWest_dir != 0) || (EastSouth_dir != 0) || (SouthWest_dir != 0)){
          cv_wait(NorthSouth_CV, intersectionLock);
        }
        NorthSouth_dir += 1;
      } else if ((origin == west) && (destination == east)) {
        while ((SouthNorth_dir != 0) || (SouthEast_dir != 0) || (SouthWest_dir != 0) \
          || (NorthEast_dir != 0) || (NorthSouth_dir != 0) || (EastSouth_dir != 0)){
          cv_wait(WestEast_CV, intersectionLock);
        }
        WestEast_dir += 1;
      } else {
        panic("this straight not exist");
      }

    } else if (thisTurn == left){
        // 4 left turn

        if ((origin == south) && (destination == west)) {
        while ((EastWest_dir != 0) || (EastSouth_dir != 0) \
          || (WestNorth_dir != 0) || (WestEast_dir != 0) \
          || (NorthEast_dir != 0) || (NorthSouth_dir != 0) || (NorthWest_dir != 0)){
          cv_wait(SouthWest_CV, intersectionLock);
        }
        SouthWest_dir += 1;
      } else if ((origin == east) && (destination == south)) {
        while ((SouthNorth_dir != 0) || (SouthWest_dir != 0) \
          || (NorthSouth_dir != 0) || (NorthEast_dir != 0) \
          || (WestNorth_dir != 0) || (WestEast_dir != 0) || (WestSouth_dir != 0)){
          cv_wait(EastSouth_CV, intersectionLock);
        }
        EastSouth_dir += 1;
      } else if ((origin == north) && (destination == east)) {
        while ((EastWest_dir != 0) || (EastSouth_dir != 0) \
          || (WestEast_dir != 0) || (WestNorth_dir != 0) \
          || (SouthWest_dir != 0) || (SouthNorth_dir != 0) || (SouthEast_dir != 0)){
          cv_wait(NorthEast_CV, intersectionLock);
        }
        NorthEast_dir += 1;
      } else if ((origin == west) && (destination == north)) {
        while ((NorthSouth_dir != 0) || (NorthEast_dir != 0) \
          || (SouthNorth_dir != 0) || (SouthWest_dir != 0) \
          || (EastSouth_dir != 0) || (EastWest_dir != 0) || (EastNorth_dir != 0)){
          cv_wait(WestNorth_CV, intersectionLock);
        }
        WestNorth_dir += 1;
      } else {
        panic("this left turn not exist");
      }

    } else {
      panic("this turn not exist");
    }
  lock_release(intersectionLock);
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
  KASSERT(intersectionLock != NULL);
  lock_acquire(intersectionLock);
    Turn thisTurn = determineTurn(origin,destination); 

    if (thisTurn == right){
      // 4 right turn
      if (origin == south){
        SouthEast_dir -= 1;
        cv_broadcast(WestEast_CV, intersectionLock);
        cv_broadcast(NorthEast_CV, intersectionLock);
      } else if (origin == east){
        EastNorth_dir -= 1;
        cv_broadcast(SouthNorth_CV, intersectionLock);
        cv_broadcast(WestNorth_CV, intersectionLock);
      } else if (origin == north){
        NorthWest_dir -= 1;
        cv_broadcast(EastWest_CV, intersectionLock);
        cv_broadcast(SouthWest_CV, intersectionLock);
      } else if (origin == west){
        WestSouth_dir -= 1;
        cv_broadcast(NorthSouth_CV, intersectionLock);
        cv_broadcast(EastSouth_CV, intersectionLock);
      } else {
        panic ("this right turn cannot delete");
      }
    } else if (thisTurn == straight){
      // 4 straight
      if (origin == south){
        SouthNorth_dir -= 1;
        cv_broadcast(EastNorth_CV, intersectionLock);
        cv_broadcast(EastWest_CV, intersectionLock);
        cv_broadcast(WestEast_CV, intersectionLock);
        cv_broadcast(EastSouth_CV, intersectionLock);
        cv_broadcast(NorthEast_CV, intersectionLock);
        cv_broadcast(WestNorth_CV, intersectionLock);
      } else if (origin == east){
        EastWest_dir -= 1;
        cv_broadcast(NorthWest_CV, intersectionLock);
        cv_broadcast(SouthNorth_CV, intersectionLock);
        cv_broadcast(NorthSouth_CV, intersectionLock);
        cv_broadcast(SouthWest_CV, intersectionLock);
        cv_broadcast(NorthEast_CV, intersectionLock);
        cv_broadcast(WestNorth_CV, intersectionLock);
      } else if (origin == north){
        NorthSouth_dir -= 1;
        cv_broadcast(WestSouth_CV, intersectionLock);
        cv_broadcast(EastWest_CV, intersectionLock);
        cv_broadcast(WestEast_CV, intersectionLock);
        cv_broadcast(SouthWest_CV, intersectionLock);
        cv_broadcast(EastSouth_CV, intersectionLock);
        cv_broadcast(WestNorth_CV, intersectionLock);
      } else if (origin == west){
        WestEast_dir -= 1;
        cv_broadcast(SouthEast_CV, intersectionLock);
        cv_broadcast(SouthNorth_CV, intersectionLock);
        cv_broadcast(NorthSouth_CV, intersectionLock);
        cv_broadcast(SouthWest_CV, intersectionLock);
        cv_broadcast(EastSouth_CV, intersectionLock);
        cv_broadcast(NorthEast_CV, intersectionLock);
      } else {
        panic ("this straight cannot delete");
      }
    } else if (thisTurn == left){
      // 4 left turn
      if (origin == south){
        SouthWest_dir -= 1;
        cv_broadcast(EastWest_CV, intersectionLock);
        cv_broadcast(NorthSouth_CV, intersectionLock);
        cv_broadcast(WestEast_CV, intersectionLock);
        cv_broadcast(EastSouth_CV, intersectionLock);
        cv_broadcast(WestNorth_CV, intersectionLock);
	cv_broadcast(NorthEast_CV, intersectionLock);
	cv_broadcast(NorthWest_CV, intersectionLock);
      } else if (origin == east){
        EastSouth_dir -= 1;
        cv_broadcast(SouthNorth_CV, intersectionLock);
        cv_broadcast(NorthSouth_CV, intersectionLock);
        cv_broadcast(WestEast_CV, intersectionLock);
        cv_broadcast(SouthWest_CV, intersectionLock);
        cv_broadcast(NorthEast_CV, intersectionLock);
	cv_broadcast(WestSouth_CV, intersectionLock);
	cv_broadcast(WestNorth_CV, intersectionLock);
      } else if (origin == north){
        NorthEast_dir -= 1;
        cv_broadcast(SouthNorth_CV, intersectionLock);
        cv_broadcast(EastWest_CV, intersectionLock);
        cv_broadcast(WestEast_CV, intersectionLock);
        cv_broadcast(EastSouth_CV, intersectionLock);
        cv_broadcast(WestNorth_CV, intersectionLock);
	cv_broadcast(SouthEast_CV, intersectionLock);
	cv_broadcast(SouthWest_CV, intersectionLock);
      } else if (origin == west){
        WestNorth_dir -= 1;
        cv_broadcast(SouthNorth_CV, intersectionLock);
        cv_broadcast(EastWest_CV, intersectionLock);
        cv_broadcast(NorthSouth_CV, intersectionLock);
        cv_broadcast(SouthWest_CV, intersectionLock);
        cv_broadcast(NorthEast_CV, intersectionLock);
	cv_broadcast(EastNorth_CV, intersectionLock);
	cv_broadcast(EastSouth_CV, intersectionLock);
      } else {
        panic ("this left turn cannot delete");
      }
    } else {
      panic ("this turn cannot delete");
    }

  lock_release(intersectionLock);
}

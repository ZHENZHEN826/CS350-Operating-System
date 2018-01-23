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

#define MAX_CARS 100

enum Turns
  { 
    straight = 0,
    left = 1,
    right = 2,
  };
typedef enum Turns Turn;

typedef struct cars {
  Direction origin;
  Direction destination;
  Turn carTurn;
}car;

struct array *carOrigin = array_create();
struct array *carDestination = array_create();
array_init(carOrigin);
array_init(carDestination);

unsigned *carNumber = 0;
int determineTurn(Direction origin, Direction destination);
int determineTurn(Direction origin, Direction destination){
  if (((origin == west) && (destination == south)) ||
      ((origin == south) && (destination == east)) ||
      ((origin == east) && (destination == north)) ||
      ((origin == north) && (destination == west))) {
    return right;
  } else if (((origin == north) && (destination == south)) ||
      ((origin == south) && (destination == north)) ||
      ((origin == east) && (destination == west)) ||
      ((origin == west) && (destination == east))){
    return straight;
  } else if (((origin == south) && (destination == west)) ||
      ((origin == west) && (destination == north)) ||
      ((origin == north) && (destination == east)) ||
      ((origin == east) && (destination == south))) {
    return left;
  } else {
    panic("car turn not exist");
    return -1;
  }
}

//static struct semaphore *intersectionSem;

 struct lock *intersectionLock;
//static struct cv *canGoNorth;
//static struct cv *canGoSouth;
//static struct cv *canGoWest;
//static struct cv *canGoEast;
struct cv *emptyIntersection;
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
  /* replace this default implementation with your own implementation */

  //intersectionSem = sem_create("intersectionSem",1);
  //if (intersectionSem == NULL) {
  //  panic("could not create intersection semaphore");
  //}

  intersectionLock = lock_create("intersectionLock");
  return;
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
  /* replace this default implementation with your own implementation */
  //KASSERT(intersectionSem != NULL);
  //sem_destroy(intersectionSem);

  lock_destroy(intersectionLock);
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
  /* replace this default implementation with your own implementation */
  //(void)origin;  /* avoid compiler complaint about unused parameter */
  //(void)destination; /* avoid compiler complaint about unused parameter */
  //KASSERT(intersectionSem != NULL);
  //P(intersectionSem);

  KASSERT(intersectionLock != NULL);
  lock_acquire(intersectionLock);
    // number of car in intersection
    if (carNumber == 0){
      // No car in the intersection, add the car to the array
      car oneCar = {origin, destination, determineTurn(origin, destination)};
      //array_add(carInIntersection, oneCar, carNumber);
      array_add(carOrigin,&origin, &carNumber);
      array_add(carDestination, &destination, &carNumber);
      carNumber = carNumber + 1;
    } else if (carNumber == 1){
      if (carInIntersection[0].origin == origin){
        car oneCar = {origin, destination, determineTurn(origin, destination)};
        //array_add(carInIntersection, oneCar, carNumber);
        array_add(carOrigin,&origin, &carNumber);
        array_add(carDestination, &destination, &carNumber);
        carNumber = carNumber + 1;
      } else if ((carInIntersection[0].origin == destination) && (carInIntersection[0].destination == origin)) {
        car oneCar = {origin, destination, determineTurn(origin, destination)};
        //array_add(carInIntersection, oneCar, carNumber);
        array_add(carOrigin,&origin, &carNumber);
        array_add(carDestination, &destination, &carNumber);
        carNumber = carNumber + 1;
      } else{
        while(carNumber > 0) {
          cv_wait(emptyIntersection,intersectionLock);
        }
      }
    }
    while (carNumber > 1){
      cv_wait(emptyIntersection, intersectionLock);
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
  /* replace this default implementation with your own implementation */
  //(void)origin;  /* avoid compiler complaint about unused parameter */
  //(void)destination; /* avoid compiler complaint about unused parameter */
  //KASSERT(intersectionSem != NULL);
  //V(intersectionSem);
  KASSERT(intersectionLock != NULL);
  lock_acquire(intersectionLock);
    for(int i = 0; i < carNumber; i++){
      if ((carInIntersection[i].origin == origin) && (carInIntersection[i].destination == destination)){
        // delete the car
        array_remove(carOrigin, i);
	array_remove(carDestination, i);
        carNumber --;
        break;
      }
    }
    if (carNumber == 0){
      cv_signal(emptyIntersection, intersectionLock);
    }
  lock_release(intersectionLock);
}

// A satellite path modelled in geocentric equitorial inertial
// coordinates using cubic splines.  An orbit propagator is used to
// fill in the blanks at a number of position along the orbital path,
// and then splines are fit to these known positions to allow fast
// lookup of the platform position and velocity at different points in
// time.  See orbital_state_vector.h for some indications of the
// errors involved in the propagation.

#ifndef PLATFORM_PATH_H
#define PLATFORM_PATH_H

#include <gsl/gsl_spline.h>

#include "orbital_state_vector.h"
#include "vector.h"

// It may be useful to read the start_time and end_time members
// directly, everything else in this instance structure is private.
typedef struct {
  double start_time;
  double end_time;
  gsl_interp_accel *xp_accel;
  gsl_spline *xp;
  gsl_interp_accel *yp_accel;
  gsl_spline *yp;
  gsl_interp_accel *zp_accel;
  gsl_spline *zp;
} PlatformPath;


// Create a new instance modeling platform position with
// control_point_count point splines from start_time to end_time using
// the observation_count observations having times observation_times
// and positions observation_positions.
PlatformPath *
platform_path_new (int control_point_count, double start_time, double end_time,
		   int observation_count, double *observation_times, 
		   OrbitalStateVector **observation_positions);

// Set position to the position of platform at time.
void
platform_path_position_at_time (PlatformPath *self, double time, 
				Vector *position);

// Set velocity to the velocity of platform at time.
void
platform_path_velocity_at_time (PlatformPath *self, double time, 
				Vector *velocity);

#endif // PLATFORM_PATH_H

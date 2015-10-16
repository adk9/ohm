                  
               // N-body system in 3D                      //
               // Basic Velocity Verlet                    //
               // interaction: 6-12                        //

                                  // Michel Vallieres //

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define Npartmax 2000    // dimension of arrays ( Npart < Npartmax )


typedef struct{
  double x, y, z;
} position;

typedef struct{
  double vx, vy, vz;
} velocity;

typedef struct{
  double px, py, pz;
} momentum;

typedef struct{
  double ax, ay, az;
} acceleration;

int Npart;                  // No of bodies
double mass[Npartmax];      // masses
position R[Npartmax];       // positions
velocity V[Npartmax];       // velocities
acceleration acc[Npartmax]; // accelerations
double tot_mass;            // total mass
FILE *fp_traj;
position Rp[Npartmax], Rnew[Npartmax];
double boxsize;
double dt, tmin, tmax;      // time grid

// current time-step
static int cur_ts;

// total time-steps
static int total_ts;

#define ONE_LARGE_ERROR  1
#define ERROR_PERCENT 0.05  // 1%

                            // gaussian distributed random 
                            // numbers - Koonin & Meredith 1986
void GAUSS( double *gauss1, double *gauss2 )
{
  double x1, x2, twou, radius, theta;

  x1 = (double)rand()/(double)RAND_MAX;
  x2 = (double)rand()/(double)RAND_MAX;
  twou = -2*log( 1.0-x1 );
  radius = sqrt( twou);
  theta = 2*M_PI*x2;
  *gauss1 = radius*cos(theta);
  *gauss2 = radius*sin(theta);
}


                            // random gaussian mass distribution
                            // all 1 for special case
void mass_setup( int iprint, int special )
{
  int i;
  double g1, dumb;

  for ( i=0 ; i<Npart ; i++ )
       mass[i] = 1.0;

  if ( special == 1 ) return;

  if ( iprint == 1 )
         fprintf( stderr, "Mass matrix\n" );

  for ( i=0 ; i<Npart ; i++ )
    {
      GAUSS( &g1, &dumb );
      mass[i] = 1.0 + 0.1 * g1;
      if ( mass[i] < 0.0 )
        mass[i] = - mass[i];

      if ( iprint == 1 )
        {
          fprintf( stderr, " %f", mass[i] );
	  if ( i>0 && 5*(i/5) == i ) printf( "\n" );
	}
    }
  if ( iprint == 1 ) 
         fprintf( stderr, "\n" );
}


                            // output routine
void output_x_v()
{
  int i;

  for ( i=0 ; i<Npart ; i++ )
    {
      fprintf( stderr, "Position & velocity:  %d %f %f %f %f %f %f\n", 
           i, R[i].x, R[i].y, R[i].z, V[i].vx, V[i].vy, V[i].vz ); 
    }
}


                            // record trajectory in file
void record_trajectories( )
{
  int i;

  for ( i=0 ; i<Npart ; i++ )
    {
      fprintf( fp_traj, "%f %f %f\n", R[i].x, R[i].y, R[i].z );
    }
}


                         // setup a compact square 3D grid
                         // with unit spacing and N vertices
void initial_grid( int N, int xyz[Npartmax][3] )
{
  int gen, i, j, k, ii, pass, n, nnewlist, noldlist, same;
  FILE *fp;

  n = 0;
  xyz[n][0] = 0;
  xyz[n][1] = 0;
  xyz[n][2] = 0;
  n++;

  for ( gen = 1; gen<5 ; gen++ )
    {
      nnewlist = 0;

  for ( pass=0 ; pass<3 ; pass++ )
    {
      noldlist = nnewlist;
      nnewlist = n;

      for ( i=noldlist ; i<nnewlist ; i++ )
        {
          for ( j=0 ; j<3 ; j++ )
            {
              if ( xyz[i][j] == gen - 1 )
                {
                  xyz[n][0] = xyz[i][0];
                  xyz[n][1] = xyz[i][1];
                  xyz[n][2] = xyz[i][2];
                  xyz[n][j] = gen;
                  n++;

                  for ( k=0; k<n-1 ; k++ )
                    {
                      same = 0;
                      if ( xyz[n-1][0] == xyz[k][0] &&
                           xyz[n-1][1] == xyz[k][1] &&
                           xyz[n-1][2] == xyz[k][2] )
                        {
                          n--;
                          same = 1;
                        }
                      if ( same == 1 ) break;
                    } // k loop

                  if ( n == N )
                    {
                        fp = fopen( "list_initial", "w" );
                        for ( ii=0 ; ii<N ; ii++ )
                           fprintf( fp, "%d %d %d %d\n", ii,
                                   xyz[ii][0], xyz[ii][1], xyz[ii][2] );
                        fclose(fp);
                        return;
                    }

                }

            }  // j loop

        }  // i loop

    }  // pass loop

    }  // gen loop

}

                            // initial x on square lattice
                            // initial gaussian random v
                            // v adjusted for center of mass
                            // with 0 momentum (stationary)
                            // center of mass and located close to (0,0)
                            // because gaussian random centered at zero
                            // gaussian distribution not realistic?
void initial_conditions( )
{
  int i;
  double g1, size, speed_scale, r1, r2, r3;
  double sq3, v;
  momentum P;
  int xyz[Npartmax][3];

                        // 3D lattice positions
  initial_grid( Npart, xyz );

  size = 1.2;
  for ( i=0 ; i<Npart ; i++ )
    {
      R[i].x = size*xyz[i][0];
      R[i].y = size*xyz[i][1];
      R[i].z = size*xyz[i][2];
    }

                        // random velocities
  speed_scale = 1.0;
  for ( i=0 ; i<Npart ; i++ )
    {
      r1 = (double)rand()/(double)RAND_MAX;
      r2 = (double)rand()/(double)RAND_MAX;
      r3 = (double)rand()/(double)RAND_MAX;
      V[i].vx = speed_scale * ( 2*r1 - 1.0 );
      V[i].vy = speed_scale * ( 2*r2 - 1.0 );
      V[i].vz = speed_scale * ( 2*r3 - 1.0 );
    }

                        // remove CM motion
  P.px = 0.0;
  P.py = 0.0;
  P.pz = 0.0;
  tot_mass = 0.0;
  for ( i=0 ; i<Npart ; i++ )
    {
      tot_mass = tot_mass + mass[i];
      P.px = P.px + mass[i] * V[i].vx;
      P.py = P.py + mass[i] * V[i].vy;
      P.pz = P.pz + mass[i] * V[i].vz;
    }
  for ( i=0 ; i<Npart ; i++ )
    {
      V[i].vx = V[i].vx - P.px/tot_mass;
      V[i].vy = V[i].vy - P.py/tot_mass;
      V[i].vz = V[i].vz - P.pz/tot_mass;
    }

                        // previous value (needed in Verlet step)
                        // Euler step
  for ( i=0 ; i<Npart ; i++ )
    {
      Rp[i].x = R[i].x - dt*V[i].vx;
      Rp[i].y = R[i].y - dt*V[i].vy;
      Rp[i].z = R[i].z - dt*V[i].vz;
    }
}


                            // print acc & forces (debug)
void output_force()
{
  int i;
  double Ftotx, Ftoty, Ftotz, fx, fy, fz;

  Ftotx = 0.0;
  Ftoty = 0.0;
  Ftotz = 0.0;
  for ( i=0 ; i<Npart ; i++ )
    {
      fx =  mass[i]*acc[i].ax;
      fy =  mass[i]*acc[i].ay; 
      fz =  mass[i]*acc[i].az;
      printf("Mass & acceleration:  %d %f %f %f %f -- %f %f %f\n", 
	     i, mass[i], acc[i].ax, acc[i].ay, acc[i].az, fx, fy, fz );
      Ftotx = Ftotx + fx;
      Ftoty = Ftoty + fy;
      Ftotz = Ftotz + fz;
    }
  printf("Total force (x & y): %f %f %f\n", Ftotx, Ftoty, Ftotz );
}


                            // force (acceleration) due to 6-12 potential
                            // on each body caused by
                            // all other bodies
void accelerations( int iprint )
{
  int i, j;
  double xij, yij, zij, rij2, rij4, rij8, rij14;
  double Fijx, Fijy, Fijz;
  double Fx[Npartmax], Fy[Npartmax], Fz[Npartmax];
  double factor;


   for ( i=0 ; i<Npart ; i++ )
    {
      Fx[i] = 0.0;
      Fy[i] = 0.0;
      Fz[i] = 0.0;
    }

  for ( i=0 ; i<Npart-1 ; i++ )
    {
      for ( j= i+1 ; j<Npart ; j++ )
	{
              xij = R[i].x - R[j].x;
              yij = R[i].y - R[j].y;
	      zij = R[i].z - R[j].z;

              rij2 = xij*xij + yij*yij + zij*zij ;
              rij4 = rij2*rij2;
              rij8 = rij4*rij4;
              rij14 = rij2*rij4*rij8;

              factor = 4.0 * ( 12.0/rij14 - 6.0/rij8 );

              Fijx = factor * xij;
              Fijy = factor * yij;
              Fijz = factor * zij;

              Fx[i] = Fx[i] + Fijx;
	      Fy[i] = Fy[i] + Fijy;
	      Fz[i] = Fz[i] + Fijz;
              Fx[j] = Fx[j] - Fijx;
	      Fy[j] = Fy[j] - Fijy;
	      Fz[j] = Fz[j] - Fijz;

	} // end j loop
    } // i loop

   for ( i=0 ; i<Npart ; i++ )
    {
      acc[i].ax = Fx[i] / mass[i];
      acc[i].ay = Fy[i] / mass[i];
      acc[i].az = Fz[i] / mass[i];
    }

  if ( iprint == 1 )
       output_force();
}


                            // Velocity Verlet time step
time_step_verlet( int iprint )
{
  int i, j;
                            // accelerations at t
  accelerations( iprint );

  if (ONE_LARGE_ERROR) {
    if ((((double)2.5*cur_ts)/total_ts) == 1.0) {
      acc[0].ax *= 3;
      acc[0].ay *= 3; 
      acc[0].az /= 3;
    }
  } else {
    if (cur_ts % (int)(total_ts/((double)ERROR_PERCENT * total_ts)) == 0) {
      acc[0].ax += ((double)rand()/(double)RAND_MAX);
      acc[0].ay += ((double)rand()/(double)RAND_MAX);
      acc[0].az -= ((double)rand()/(double)RAND_MAX);
    }
  }
                            // basic Verlet
  for ( i=0 ; i<Npart ; i++ )
    {
      Rnew[i].x = 2*R[i].x - Rp[i].x + acc[i].ax*dt*dt;
      Rnew[i].y = 2*R[i].y - Rp[i].y + acc[i].ay*dt*dt;
      Rnew[i].z = 2*R[i].z - Rp[i].z + acc[i].az*dt*dt;

      V[i].vx = ( Rnew[i].x - Rp[i].x ) / ( 2*dt );
      V[i].vy = ( Rnew[i].y - Rp[i].y ) / ( 2*dt );
      V[i].vz = ( Rnew[i].z - Rp[i].z ) / ( 2*dt );
    }
                            // reset arrays
  for ( i=0 ; i<Npart ; i++ )
    {
      Rp[i].x = R[i].x;
      Rp[i].y = R[i].y;
      Rp[i].z = R[i].z;
      R[i].x = Rnew[i].x;
      R[i].y = Rnew[i].y;
      R[i].z = Rnew[i].z;
    }
}


                            // total energy & CM momentum 
                            // check conservation
void energy_momentum( int info, double time )
{
  double kinetic, potential, pot, etotal;
  momentum P;
  double xij, yij, zij, rij2, rij4, rij6, rij12;
  int i, j;

  kinetic = 0.0;
  for ( i=0 ; i<Npart ; i++ )
    {
      kinetic = kinetic + 0.5 * mass[i] * 
              ( V[i].vx*V[i].vx + V[i].vy*V[i].vy +  V[i].vz*V[i].vz );
    }

  potential = 0.0;      // sum over pairs
  for ( i=0 ; i<Npart ; i++ )
    {
      for ( j=i+1 ; j<Npart ; j++ )
        {
            xij = R[i].x - R[j].x;
            yij = R[i].y - R[j].y;
            zij = R[i].z - R[j].z;
            rij2 = xij*xij + yij*yij + zij*zij;
            rij4 = rij2*rij2;
            rij6 = rij2*rij4;
            rij12 = rij6*rij6;
            pot = 4 * ( 1.0/rij12 - 1.0/rij6 );
            potential = potential + pot;
	}
    }

  etotal = kinetic + potential;

  if ( info == 0 )
    {
       printf( "%f %f %f %f\n", time, kinetic, potential, etotal );
       return;
    }

                        // center of mass momentum
  P.px = 0.0;
  P.py = 0.0;
  P.pz = 0.0;
  for ( i=0 ; i<Npart ; i++ )
    {
       P.px = P.px + mass[i] * V[i].vx;
       P.py = P.py + mass[i] * V[i].vy;
       P.pz = P.pz + mass[i] * V[i].vz;
    }
  P.px = P.px / tot_mass;
  P.py = P.py / tot_mass;
  P.pz = P.pz / tot_mass;

  fprintf( stderr, "Kin + pot = energy :  %f %f %f\n", 
	  kinetic, potential, etotal ); 
  fprintf( stderr, 
          "Center of Mass P   :    %f %f %f\n", P.px, P.py, P.pz );

}


int main( int argn, char * argv[] )
{
  int dbg_print, special, print_energy, i;
  double t;

                        // extra debug print
  dbg_print = 0;
                        // No of particles
  Npart = 4;
                        // time grid
  tmin = 0.0;
  tmax = 15.0;
  dt = 0.0001;
                        //all mass 1.0
  special = 0;
                        // record energy
  print_energy = 0;

  for ( i=1 ; i<argn ; i++ )
    {
      if ( argv[i][0] == '-' )
	{
	  switch( argv[i][1] )
	  {
	  case( 'N' ):
            Npart = atoi( argv[++i] );
	    break;
	  case( 'e' ):
	    print_energy =1;
	    break;
	  case( 's' ):
	    special =1;
	    break;
	  case( 't' ):
	    tmax = atof( argv[++i] );
	    break;
	  default:
	    printf("Syntax: ./Nbody_basic_verlet <-N> <-e> <-s> <-t>\n");
	    exit(1);
	  }
	}
    }

  cur_ts = 0;
  total_ts = tmax / dt;

                        // information
  printf( "\n Npart: %d tmax %f\n", Npart, tmax ); 
                        // recording trajectories in file
  fp_traj = fopen( "trajectories", "w" );
                        // mass
  mass_setup( dbg_print, special );
                        // initial time
  t = tmin;
                        // initial positions & velocities
  initial_conditions( special );
                        // print positions & velocities
  fprintf( stderr,
   "\nInitial positions, velocities & energy-momentum (time %f)\n", t );
  output_x_v();
                        // record positions
  record_trajectories( );
                        // energy check
  energy_momentum( 1, t );
                        // loop over time
  while ( t < tmax )
    {
                        // time step -- velocity-verlet
      time_step_verlet( dbg_print );
      t = t + dt;
      cur_ts++;
                        // record positions
      record_trajectories( );
                        // optional energy
      if ( print_energy == 1 )
                     energy_momentum( 0, t );
    }
                        // print positions & velocities
  fprintf( stderr, 
   "\nFinal positions, velocities & energy-momentum (time %f)\n", t );
  output_x_v();
                        // energy check
  energy_momentum( 1, t );
                        // close trajectories file
  fclose( fp_traj );

}


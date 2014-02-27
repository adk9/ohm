/* 
 * Borrowed from:
 * http://charm.cs.uiuc.edu/cs498lvk/projects/chaomei/md_mpi.c
 *
 * This program implements a simple molecular dynamics simulation, using
 * the velocity Verlet time integration scheme. The particles interact
 * with a central pair potential
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <mpi.h>

#define frand() (rand()/(RAND_MAX+1.0))

/* Simulation Parameters */
#define NUMDIMS 3 //dimensionality of the physical space
#define NUMATOMS 1000 //number of atoms in the simulation
#define NUMSTEPS 16 //number of time steps in the simulation
#define MASS 1.0 //mass of the atom
#define DT 1.0e-4 //time step in the simulation
#define CUTOFF 0.5

#define PI2 (3.1415926/2.0)
#define MIN(x,y) ((x)>(y)?(y):(x))

/* Simple defines for the pair potential and its derivative.
 * This potential is a harmonic well which smoothly saturates
 * to a maximum value at PI/2
 */ 
#define V(x) (sin(MIN(x, PI2))*sin(MIN(x, PI2)))
#define DV(x) (2.0*sin(MIN(x, PI2))*cos(MIN(x, PI2)))

typedef struct AtomInfo{
    double mass;
    double position[NUMDIMS];
    double velocity[NUMDIMS];
    double acceleration[NUMDIMS];
    double force[NUMDIMS];
}AtomInfo;


void initialize(int numAtoms, AtomInfo *atoms);
void compute(int numAtoms, AtomInfo *atoms, double *pPotential, double  *pKinetic);
void update(int numAtoms, AtomInfo *atoms);

int main(int argc, char *argv[]){
    int pId; //processor's identifier
    int numProcs; //number of processors
    int numAtoms; //indicate the number of atoms in one processor
    AtomInfo *atoms;
    double potential=0.0, kinetic=0.0;
    double totalPotential=0.0, totalKinetic=0.0;
    int i;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &pId);
    MPI_Comm_size(MPI_COMM_WORLD, &numProcs);

    //assign atoms to each processor
    numAtoms = NUMATOMS*(pId+1)/numProcs - NUMATOMS*pId/numProcs;
    atoms = (AtomInfo *)malloc(sizeof(AtomInfo)*numAtoms);
    //set intial positions, velocities, and accelerations
    initialize(numAtoms, atoms);
    
    //main time stepping loop
    MPI_Barrier(MPI_COMM_WORLD);
    for(i=0; i<NUMSTEPS; i++){
	double execTime = -MPI_Wtime();
	compute(numAtoms, atoms, &potential, &kinetic);
	MPI_Reduce(&potential, &totalPotential, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
	MPI_Reduce(&kinetic, &totalKinetic, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
	update(numAtoms, atoms);
	MPI_Barrier(MPI_COMM_WORLD);
	if(pId==0){
	    execTime += MPI_Wtime();
	    printf("Potential: %lf, Kinetic: %lf, Timing: %lf sec/step\n", totalPotential, totalKinetic, execTime);
	    totalPotential = totalKinetic = 0.0;
	}
    }
    
    free(atoms);
    MPI_Finalize();
    return EXIT_SUCCESS;
}

void initialize(int numAtoms, AtomInfo *atoms){
    int i,j;
    
    for(i=0; i<numAtoms; i++){
	AtomInfo *singleOne = atoms + i;
	singleOne->mass = MASS;
	for(j=0; j<NUMDIMS; j++){
	    (singleOne->position)[j] = frand()*10;
	    (singleOne->velocity)[j] = 0.0;
	    (singleOne->force)[j] = 0.0;
	    (singleOne->acceleration)[j] = 0.0;
	}
    }
}

double distance(AtomInfo *atomA, AtomInfo *atomB);
double calKinetic(AtomInfo *atom);

void compute(int numAtoms, AtomInfo *atoms, double *pPotential, double *pKinetic){
    int i,j,k;
    int procIter;
    
    double potential = 0.0;
    double kinetic = 0.0;

    int procID, procNum;
    MPI_Comm_rank(MPI_COMM_WORLD, &procID);
    MPI_Comm_size(MPI_COMM_WORLD, &procNum);    
    
    //create MPI_Datatype for sending/receiving messages containing atoms
    MPI_Datatype atomInfoType, oldtypes[1];
    int blockcounts[1];
    MPI_Aint offsets[1];
    offsets[0] = 0;
    oldtypes[0] = MPI_DOUBLE;
    blockcounts[0] = 13;
    MPI_Type_struct(1, blockcounts, offsets, oldtypes,&atomInfoType);
    MPI_Type_commit(&atomInfoType);

    //firstly send my own atoms to other processors
    for(procIter=0; procIter<procID; procIter++)
	MPI_Send(atoms, numAtoms, atomInfoType, procIter, procID, MPI_COMM_WORLD);    
    for(procIter=procID+1; procIter<procNum; procIter++)
	MPI_Send(atoms, numAtoms, atomInfoType, procIter, procID, MPI_COMM_WORLD);
    
    //secondly compute interactions for atoms in my processor
    for(i=0; i<numAtoms; i++){
	AtomInfo *atomA = atoms+i;
	//compute potential energy and forces
	for(j=0; j<numAtoms; j++){
	    AtomInfo *atomB = atoms+j;
	    if(i != j){
		//first compute distance between atomA and atomB
		double d = distance(atomA, atomB);
		if(d > CUTOFF){
		    //attribute half of the potential energy to atomB
		    potential += 0.5*V(d);
		    for(k=0; k<NUMDIMS; k++)
			(atomA->force)[k] -= ((atomA->position)[k] - (atomB->position)[k])*DV(d)/d;
		}
	    }
	}
	//compute kinetic energy
	kinetic += calKinetic(atomA); 	
    }
    
    //finally receive atoms from other processors and compute interactions
    for(procIter=0; procIter<procNum; procIter++){
	if(procIter!=procID){
	    int otherAtomsNum = NUMATOMS*(procIter+1)/procNum - NUMATOMS*procIter/procNum;
    	    AtomInfo *otherAtoms = (AtomInfo *)malloc(sizeof(AtomInfo)*otherAtomsNum);
	    MPI_Status status;
	    MPI_Recv(otherAtoms, otherAtomsNum, atomInfoType, procIter, procIter, MPI_COMM_WORLD, &status); 
            for(i=0; i<numAtoms; i++){
		AtomInfo *atomA = atoms+i;
		//compute potential energy and forces
		for(j=0; j<otherAtomsNum; j++){
		    AtomInfo *atomB = atoms+j;
		    //first compute distance between atomA and atomB
		    double d = distance(atomA, atomB);
		    if(d > CUTOFF){
			//attribute half of the potential energy to atomB
			potential += 0.5*V(d);
			for(k=0; k<NUMDIMS; k++)
			    (atomA->force)[k] -= ((atomA->position)[k] - (atomB->position)[k])*DV(d)/d;
		    }
		}
		//compute kinetic energy
		kinetic += calKinetic(atomA);
	    }
	    free(otherAtoms);
	}
    }

    *pPotential = potential;
    *pKinetic = kinetic;
}

double distance(AtomInfo *atomA, AtomInfo *atomB){
    double dist = 0.0;
    int i;
    for(i=0; i<NUMDIMS; i++){
	double abDist = (atomA->position)[i] - (atomB->position)[i];
	dist += abDist*abDist;
    }
    return sqrt(dist);
}

double calKinetic(AtomInfo *atom){
    double kinetic = 0.0;
    int i;
    for(i=0; i<NUMDIMS; i++){
	kinetic += (atom->velocity)[i] * (atom->velocity)[i];
    }
    return kinetic*0.5*(atom->mass);
}

//Perform the time integration, using a velocity Verlet algorithm
void update(int numAtoms, AtomInfo *atoms){
    int i, j;
    
    for(i=0; i<numAtoms; i++){
	AtomInfo *one = atoms+i;
	for(j=0; j<NUMDIMS; j++){
	    (one->position)[j] += (one->velocity)[j]*DT + 0.5*DT*DT*(one->acceleration)[j];
	    (one->velocity)[j] += 0.5*DT*((one->force)[j]/one->mass + (one->acceleration)[j]); 
	    (one->acceleration)[j] = (one->force)[j]/one->mass;
	}
    }
}

/*
 *  Copyright (c) 2011-2012, Los Alamos National Security, LLC.
 *  All rights Reserved.
 *
 *  Copyright 2011-2012. Los Alamos National Security, LLC. This software was produced 
 *  under U.S. Government contract DE-AC52-06NA25396 for Los Alamos National 
 *  Laboratory (LANL), which is operated by Los Alamos National Security, LLC 
 *  for the U.S. Department of Energy. The U.S. Government has rights to use, 
 *  reproduce, and distribute this software.  NEITHER THE GOVERNMENT NOR LOS 
 *  ALAMOS NATIONAL SECURITY, LLC MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR 
 *  ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE.  If software is modified
 *  to produce derivative works, such modified software should be clearly marked,
 *  so as not to confuse it with the version available from LANL.
 *
 *  Additionally, redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Los Alamos National Security, LLC, Los Alamos 
 *       National Laboratory, LANL, the U.S. Government, nor the names of its 
 *       contributors may be used to endorse or promote products derived from 
 *       this software without specific prior written permission.
 *  
 *  THIS SOFTWARE IS PROVIDED BY THE LOS ALAMOS NATIONAL SECURITY, LLC AND 
 *  CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT 
 *  NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LOS ALAMOS NATIONAL
 *  SECURITY, LLC OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *  
 *  CLAMR -- LA-CC-11-094
 *  This research code is being developed as part of the 
 *  2011 X Division Summer Workshop for the express purpose
 *  of a collaborative code for development of ideas in
 *  the implementation of AMR codes for Exascale platforms
 *  
 *  AMR implementation of the Wave code previously developed
 *  as a demonstration code for regular grids on Exascale platforms
 *  as part of the Supercomputing Challenge and Los Alamos 
 *  National Laboratory
 *  
 *  Authors: Bob Robey       XCP-2   brobey@lanl.gov
 *           Neal Davis              davis68@lanl.gov, davis68@illinois.edu
 *           David Nicholaeff        dnic@lanl.gov, mtrxknight@aol.com
 *           Dennis Trujillo         dptrujillo@lanl.gov, dptru10@gmail.com
 * 
 */

#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>
#include "ezcl/ezcl.h"
#include "input.h"
#include "mesh/mesh.h"
#include "mesh/partition.h"
#include "state.h"
#include "display.h"
#include "timer/timer.h"
#include "l7/l7.h"
#include <mpi.h>

#ifndef DEBUG 
#define DEBUG 0
#endif

// Sync is to reduce numerical drift between cpu and gpu
#define DO_SYNC 
#define DO_COMPARISON
//#define DO_CPU
//#define DO_GPU

//TODO:  command-line option for OpenGL?
#ifdef DO_COMPARISON
#define DO_CPU
#define DO_GPU
static int do_comparison_calc = 1;
#else
static int do_comparison_calc = 0;
#endif

#ifdef DO_CPU
static int do_cpu_calc = 1;
#else
static int do_cpu_calc = 0;
#endif

#ifdef DO_GPU
static int do_gpu_calc = 1;
#else
static int do_gpu_calc = 0;
#endif

#ifdef DO_SYNC
static int do_sync = 1;
#else
static int do_sync = 0;
#endif
static int do_gpu_sync = 0;

#ifdef HAVE_CL_DOUBLE
typedef double      real_t;
typedef struct
{
   double s0;
   double s1;
}  real2_t;
typedef cl_double   cl_real;
typedef cl_double2  cl_real2;
typedef cl_double4  cl_real4;
typedef cl_double8  cl_real8;
#define CONSERVATION_EPS    .00001
#define STATE_EPS        .025
#define MPI_C_REAL MPI_DOUBLE
#define L7_REAL L7_DOUBLE
#else
typedef float       real_t;
typedef struct
{
   float s0;
   float s1;
}  real2_t;
typedef cl_float    cl_real;
typedef cl_float2   cl_real2;
typedef cl_float4   cl_real4;
typedef cl_float8   cl_real8;
#define CONSERVATION_EPS    .1
#define STATE_EPS      15.0
#define MPI_C_REAL MPI_FLOAT
#define L7_REAL L7_FLOAT
#endif

typedef unsigned int uint;

#ifdef HAVE_GRAPHICS
static double circle_radius=-1.0;

static int view_mode = 0;
#endif

bool        verbose,        //  Flag for verbose command-line output; init in input.cpp::parseInput().
            localStencil,   //  Flag for use of local stencil; init in input.cpp::parseInput().
            outline;        //  Flag for drawing outlines of cells; init in input.cpp::parseInput().
int         outputInterval, //  Periodicity of output; init in input.cpp::parseInput().
            enhanced_precision_sum,//  Flag for enhanced precision sum (default true); init in input.cpp::parseInput().
            lttrace_on,     //  Flag to turn on logical time trace package;
            do_quo_setup,   //  Flag to turn on quo dynamic scheduling policies package;
            levmx,          //  Maximum number of refinement levels; init in input.cpp::parseInput().
            nx,             //  x-resolution of coarse grid; init in input.cpp::parseInput().
            ny,             //  y-resolution of coarse grid; init in input.cpp::parseInput().
            niter,          //  Maximum time step; init in input.cpp::parseInput().
            ndim    = 2;    //  Dimensionality of problem (2 or 3).

enum partition_method initial_order,  //  Initial order of mesh.
                      cycle_reorder;  //  Order of mesh every cycle.
static Mesh       *mesh_global;    //  Object containing mesh information; init in grid.cpp::main().
static State      *state_global;   //  Object containing state information corresponding to mesh; init in grid.cpp::main().
static Mesh       *mesh_local;     //  Object containing mesh information; init in grid.cpp::main().
static State      *state_local;    //  Object containing state information corresponding to mesh; init in grid.cpp::main().

//  Set up timing information.
static struct timeval tstart;
static cl_event start_write_event, end_write_event;

static double  H_sum_initial = 0.0;
static long gpu_time_graphics = 0;

int main(int argc, char **argv) {
   int ierr;

   //  Process command-line arguments, if any.
   int mype=0;
   int numpe=0;
   parseInput(argc, argv);
   L7_Init(&mype, &numpe, &argc, argv, do_quo_setup, lttrace_on);
   //MPI_Init(&argc, &argv);

   //MPI_Comm_size(MPI_COMM_WORLD, &numpe);
   //MPI_Comm_rank(MPI_COMM_WORLD, &mype);

   ierr = ezcl_devtype_init(CL_DEVICE_TYPE_GPU, mype);
   if (ierr == EZCL_NODEVICE) {
      ierr = ezcl_devtype_init(CL_DEVICE_TYPE_CPU, mype);
   }
   if (ierr != EZCL_SUCCESS) {
      printf("No opencl device available -- aborting\n");
      L7_Terminate();
      exit(-1);
   }
   L7_Dev_Init();

   double circ_radius = 6.0;
   //  Scale the circle appropriately for the mesh size.
   circ_radius = circ_radius * (double) nx / 128.0;
   int boundary = 1;
   int parallel_in = 1;
   mesh_local = new Mesh(nx, ny, levmx, ndim, boundary, parallel_in, do_gpu_calc);
   if (DEBUG) { 
      //if (mype == 0) mesh->print();

      char filename[10];
      sprintf(filename,"out%1d",mype);
      mesh_local->fp=fopen(filename,"w");

      //mesh->print_local();
   }    
   mesh_local->init(nx, ny, circ_radius, initial_order, do_gpu_calc);
   size_t &ncells = mesh_local->ncells;
   int &noffset = mesh_local->noffset;

   int parallel_global_in = 0;
   mesh_global  = new Mesh(nx, ny, levmx, ndim, boundary, parallel_global_in, do_gpu_calc);
   size_t &ncells_global = mesh_global->ncells;

   MPI_Allreduce(&ncells, &ncells_global, 1, MPI_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);
   
   state_local = new State(mesh_local);
   state_local->init(do_gpu_calc);

   state_global = new State(mesh_global);
   state_global->allocate(mesh_global->ncells);
   
   cl_mem &dev_corners_i_global  = mesh_global->dev_corners_i;
   cl_mem &dev_corners_j_global  = mesh_global->dev_corners_j;

   cl_mem &dev_corners_i_local  = mesh_local->dev_corners_i;
   cl_mem &dev_corners_j_local  = mesh_local->dev_corners_j;

   vector<int>   &corners_i_global  = mesh_global->corners_i;
   vector<int>   &corners_j_global  = mesh_global->corners_j;

   vector<int>   &corners_i_local  = mesh_local->corners_i;
   vector<int>   &corners_j_local  = mesh_local->corners_j;

   vector<int>   &nsizes     = mesh_local->nsizes;
   vector<int>   &ndispl     = mesh_local->ndispl;

   vector<real_t>  &x_global  = mesh_global->x;
   vector<real_t>  &dx_global = mesh_global->dx;
   vector<real_t>  &y_global  = mesh_global->y;
   vector<real_t>  &dy_global = mesh_global->dy;

   cl_mem &dev_H  = state_local->dev_H;
   cl_mem &dev_U  = state_local->dev_U;
   cl_mem &dev_V  = state_local->dev_V;

   cl_mem &dev_H_global  = state_global->dev_H;
   cl_mem &dev_U_global  = state_global->dev_U;
   cl_mem &dev_V_global  = state_global->dev_V;

   cl_mem &dev_celltype = mesh_local->dev_celltype;
   cl_mem &dev_i        = mesh_local->dev_i;
   cl_mem &dev_j        = mesh_local->dev_j;
   cl_mem &dev_level    = mesh_local->dev_level;

   cl_mem &dev_celltype_global = mesh_global->dev_celltype;
   cl_mem &dev_i_global        = mesh_global->dev_i;
   cl_mem &dev_j_global        = mesh_global->dev_j;
   cl_mem &dev_level_global    = mesh_global->dev_level;

   vector<int>   &proc_global    = mesh_global->proc;

   vector<real_t> &x  = mesh_local->x;
   vector<real_t> &dx = mesh_local->dx;
   vector<real_t> &y  = mesh_local->y;
   vector<real_t> &dy = mesh_local->dy;

   nsizes.resize(numpe);
   ndispl.resize(numpe);
   MPI_Allgather(&ncells, 1, MPI_INT, &nsizes[0], 1, MPI_INT, MPI_COMM_WORLD);
   ndispl[0]=0;
   for (int ip=1; ip<numpe; ip++){
      ndispl[ip] = ndispl[ip-1] + nsizes[ip-1];
   }
   noffset=0;
   for (int ip=0; ip<mype; ip++){
     noffset += nsizes[ip];
   }

   size_t corners_size = corners_i_global.size();

   dev_corners_i_global  = ezcl_malloc(&corners_i_global[0],  const_cast<char *>("dev_corners_i_global"), &corners_size, sizeof(cl_int),  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, 0);
   dev_corners_j_global  = ezcl_malloc(&corners_j_global[0],  const_cast<char *>("dev_corners_j_global"), &corners_size, sizeof(cl_int),  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, 0);

   dev_corners_i_local  = ezcl_malloc(&corners_i_local[0],  const_cast<char *>("dev_corners_i_local"), &corners_size, sizeof(cl_int),  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, 0);
   dev_corners_j_local  = ezcl_malloc(&corners_j_local[0],  const_cast<char *>("dev_corners_j_local"), &corners_size, sizeof(cl_int),  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, 0);

   // Gather level, celltype, H, U, V for global calc

   mesh_global->celltype = (int *)mesh_global->mesh_memory.memory_malloc(ncells_global, sizeof(int), "celltype");
   mesh_global->level    = (int *)mesh_global->mesh_memory.memory_malloc(ncells_global, sizeof(int), "level");
   mesh_global->i        = (int *)mesh_global->mesh_memory.memory_malloc(ncells_global, sizeof(int), "i");
   mesh_global->j        = (int *)mesh_global->mesh_memory.memory_malloc(ncells_global, sizeof(int), "j");

   proc_global.resize(ncells_global);

   MPI_Allgatherv(&mesh_local->celltype[0], ncells, MPI_INT, &mesh_global->celltype[0], &nsizes[0], &ndispl[0], MPI_INT, MPI_COMM_WORLD);
   MPI_Allgatherv(&mesh_local->level[0], ncells, MPI_INT, &mesh_global->level[0], &nsizes[0], &ndispl[0], MPI_INT, MPI_COMM_WORLD);
   MPI_Allgatherv(&mesh_local->i[0], ncells, MPI_INT, &mesh_global->i[0], &nsizes[0], &ndispl[0], MPI_INT, MPI_COMM_WORLD);
   MPI_Allgatherv(&mesh_local->j[0], ncells, MPI_INT, &mesh_global->j[0], &nsizes[0], &ndispl[0], MPI_INT, MPI_COMM_WORLD);

   mesh_local->proc.resize(ncells);
   for (uint ic=0; ic<ncells; ic++){
      mesh_local->proc[ic] = mesh_local->mype;
   }
   MPI_Allgatherv(&mesh_local->proc[0], ncells, MPI_INT, &proc_global[0], &nsizes[0], &ndispl[0], MPI_INT, MPI_COMM_WORLD);

   size_t mem_request = (int)((float)ncells*mesh_local->mem_factor);
   dev_celltype  = ezcl_malloc(NULL, const_cast<char *>("dev_celltype"), &mem_request, sizeof(cl_int),  CL_MEM_READ_WRITE, 0);
   dev_i         = ezcl_malloc(NULL, const_cast<char *>("dev_i"),        &mem_request, sizeof(cl_int),  CL_MEM_READ_ONLY,  0);
   dev_j         = ezcl_malloc(NULL, const_cast<char *>("dev_j"),        &mem_request, sizeof(cl_int),  CL_MEM_READ_ONLY,  0);
   dev_level     = ezcl_malloc(NULL, const_cast<char *>("dev_level"),    &mem_request, sizeof(cl_int),  CL_MEM_READ_WRITE, 0);

   state_local->resize(ncells);

   state_global->resize(ncells_global);

   x_global.resize(ncells_global);
   dx_global.resize(ncells_global);
   y_global.resize(ncells_global);
   dy_global.resize(ncells_global);

   MPI_Allgatherv(&x[0],  ncells, MPI_C_REAL, &x_global[0], &nsizes[0], &ndispl[0], MPI_C_REAL, MPI_COMM_WORLD);
   MPI_Allgatherv(&dx[0], ncells, MPI_C_REAL, &dx_global[0], &nsizes[0], &ndispl[0], MPI_C_REAL, MPI_COMM_WORLD);
   MPI_Allgatherv(&y[0],  ncells, MPI_C_REAL, &y_global[0], &nsizes[0], &ndispl[0], MPI_C_REAL, MPI_COMM_WORLD);
   MPI_Allgatherv(&dy[0], ncells, MPI_C_REAL, &dy_global[0], &nsizes[0], &ndispl[0], MPI_C_REAL, MPI_COMM_WORLD);

   state_local->fill_circle(circ_radius, 100.0, 7.0);

   MPI_Allgatherv(&state_local->H[0], ncells, MPI_C_REAL, &state_global->H[0], &nsizes[0], &ndispl[0], MPI_C_REAL, MPI_COMM_WORLD);
   MPI_Allgatherv(&state_local->U[0], ncells, MPI_C_REAL, &state_global->U[0], &nsizes[0], &ndispl[0], MPI_C_REAL, MPI_COMM_WORLD);
   MPI_Allgatherv(&state_local->V[0], ncells, MPI_C_REAL, &state_global->V[0], &nsizes[0], &ndispl[0], MPI_C_REAL, MPI_COMM_WORLD);

   state_global->allocate_device_memory(ncells_global);
   state_local->allocate_device_memory(ncells);

   size_t one = 1;
   state_global->dev_deltaT   = ezcl_malloc(NULL, const_cast<char *>("dev_deltaT_global"), &one,    sizeof(cl_real),  CL_MEM_READ_WRITE, 0);
   state_local->dev_deltaT   = ezcl_malloc(NULL, const_cast<char *>("dev_deltaT"), &one,    sizeof(cl_real),  CL_MEM_READ_WRITE, 0);

   dev_celltype_global = ezcl_malloc(NULL, const_cast<char *>("dev_celltype_global"), &ncells_global, sizeof(cl_int),   CL_MEM_READ_ONLY, 0);
   dev_i_global        = ezcl_malloc(NULL, const_cast<char *>("dev_i_global"),        &ncells_global, sizeof(cl_int),   CL_MEM_READ_ONLY, 0);
   dev_j_global        = ezcl_malloc(NULL, const_cast<char *>("dev_j_global"),        &ncells_global, sizeof(cl_int),   CL_MEM_READ_ONLY, 0);
   dev_level_global    = ezcl_malloc(NULL, const_cast<char *>("dev_level_global"),    &ncells_global, sizeof(cl_int),   CL_MEM_READ_ONLY, 0);

   //  Set write buffers for data.
   cl_command_queue command_queue = ezcl_get_command_queue();
   ezcl_enqueue_write_buffer(command_queue, dev_H_global, CL_FALSE, 0, ncells_global*sizeof(cl_real),  (void *)&state_global->H[0],  &start_write_event);
   ezcl_enqueue_write_buffer(command_queue, dev_U_global, CL_FALSE, 0, ncells_global*sizeof(cl_real),  (void *)&state_global->U[0],  NULL              );
   ezcl_enqueue_write_buffer(command_queue, dev_V_global, CL_FALSE, 0, ncells_global*sizeof(cl_real),  (void *)&state_global->V[0],  NULL              );

   ezcl_enqueue_write_buffer(command_queue, dev_celltype_global, CL_FALSE, 0, ncells_global*sizeof(cl_int),  (void *)&mesh_global->celltype[0], NULL            );
   ezcl_enqueue_write_buffer(command_queue, dev_i_global,        CL_FALSE, 0, ncells_global*sizeof(cl_int),  (void *)&mesh_global->i[0],        NULL            );
   ezcl_enqueue_write_buffer(command_queue, dev_j_global,        CL_FALSE, 0, ncells_global*sizeof(cl_int),  (void *)&mesh_global->j[0],        NULL            );
   ezcl_enqueue_write_buffer(command_queue, dev_level_global,    CL_TRUE,  0, ncells_global*sizeof(cl_int),  (void *)&mesh_global->level[0],    &end_write_event);
   state_global->gpu_time_write += ezcl_timer_calc(&start_write_event, &end_write_event);

   ezcl_enqueue_write_buffer(command_queue, dev_H, CL_FALSE, 0, ncells*sizeof(cl_real),  (void *)&state_local->H[0],  &start_write_event);
   ezcl_enqueue_write_buffer(command_queue, dev_U, CL_FALSE, 0, ncells*sizeof(cl_real),  (void *)&state_local->U[0],  NULL);
   ezcl_enqueue_write_buffer(command_queue, dev_V, CL_FALSE, 0, ncells*sizeof(cl_real),  (void *)&state_local->V[0],  NULL);

   ezcl_enqueue_write_buffer(command_queue, dev_celltype, CL_FALSE, 0, ncells*sizeof(cl_int),  (void *)&mesh_local->celltype[0], NULL);
   ezcl_enqueue_write_buffer(command_queue, dev_i,        CL_FALSE, 0, ncells*sizeof(cl_int),  (void *)&mesh_local->i[0],        NULL);
   ezcl_enqueue_write_buffer(command_queue, dev_j,        CL_FALSE, 0, ncells*sizeof(cl_int),  (void *)&mesh_local->j[0],        NULL);
   ezcl_enqueue_write_buffer(command_queue, dev_level,    CL_TRUE,  0, ncells*sizeof(cl_int),  (void *)&mesh_local->level[0],    &end_write_event);
   state_local->gpu_time_write += ezcl_timer_calc(&start_write_event, &end_write_event);

   mesh_global->nlft = NULL;
   mesh_global->nrht = NULL;
   mesh_global->nbot = NULL;
   mesh_global->ntop = NULL;

   mesh_global->dev_nlft = NULL;
   mesh_global->dev_nrht = NULL;
   mesh_global->dev_nbot = NULL;
   mesh_global->dev_ntop = NULL;

   mesh_local->nlft = NULL;
   mesh_local->nrht = NULL;
   mesh_local->nbot = NULL;
   mesh_local->ntop = NULL;

   mesh_local->dev_nlft = NULL;
   mesh_local->dev_nrht = NULL;
   mesh_local->dev_nbot = NULL;
   mesh_local->dev_ntop = NULL;

   //  Kahan-type enhanced precision sum implementation.
   double H_sum = state_local->mass_sum(enhanced_precision_sum);
   if (mype == 0) printf ("Mass of initialized cells equal to %14.12lg\n", H_sum);
   H_sum_initial = H_sum;

   if (mype == 0) {
      printf("Iteration    0 timestep      n/a Sim Time      0.0 cells %ld Mass Sum %14.12lg\n", ncells_global, H_sum);
   }

   mesh_global->cpu_calc_neigh_counter=0;
   mesh_global->cpu_time_calc_neighbors=0.0;
   mesh_global->cpu_rezone_counter=0;
   mesh_global->cpu_time_rezone_all=0.0;
   mesh_global->cpu_refine_smooth_counter=0;

   //  Set up grid.

#ifdef HAVE_GRAPHICS
   set_mysize(ncells_global);
   set_viewmode(view_mode);
   set_window(mesh_global->xmin, mesh_global->xmax, mesh_global->ymin, mesh_global->ymax);
   set_outline((int)outline);
   init_display(&argc, argv, "Shallow Water", mype);
   set_cell_coordinates(&x_global[0], &dx_global[0], &y_global[0], &dy_global[0]);
   set_cell_data(&state_global->H[0]);
   set_cell_proc(&mesh_global->proc[0]);
   set_circle_radius(circle_radius);
   draw_scene();
   //if (verbose) sleep(5);
   sleep(2);

   //  Set flag to show mesh results rather than domain decomposition.
   view_mode = 1;
   
   //  Clear superposition of circle on grid output.
   circle_radius = -1.0;
   
   MPI_Barrier(MPI_COMM_WORLD);
   cpu_timer_start(&tstart);

   set_idle_function(&do_calc);
   start_main_loop();
#else
   MPI_Barrier(MPI_COMM_WORLD);
   cpu_timer_start(&tstart);
   for (int it = 0; it < 10000000; it++) {
      do_calc();
   }
#endif
   
   return 0;
}

static int     ncycle  = 0;
static double  simTime = 0.0;

extern "C" void do_calc(void)
{  double g     = 9.80;
   double sigma = 0.95; 
   int icount, jcount;
   int icount_global, jcount_global;
#ifdef HAVE_GRAPHICS
   static cl_event start_read_event, end_read_event;
#endif

   if (cycle_reorder == ZORDER || cycle_reorder == HILBERT_SORT) {
      do_sync = 0;
      do_gpu_sync = 1;
   }
   
   //  Initialize state variables for GPU calculation.
   int &mype = mesh_local->mype;
   int &numpe = mesh_local->numpe;

   vector<int>   &nsizes   = mesh_local->nsizes;
   vector<int>   &ndispl   = mesh_local->ndispl;

   //int levmx        = mesh->levmx;
   size_t &ncells_global    = mesh_global->ncells;
   size_t &ncells           = mesh_local->ncells;
   size_t &ncells_ghost     = mesh_local->ncells_ghost;

#ifdef HAVE_GRAPHICS
   vector<real_t>  &x  = mesh_local->x;
   vector<real_t>  &dx = mesh_local->dx;
   vector<real_t>  &y  = mesh_local->y;
   vector<real_t>  &dy = mesh_local->dy;

   vector<real_t>  &x_global  = mesh_global->x;
   vector<real_t>  &dx_global = mesh_global->dx;
   vector<real_t>  &y_global  = mesh_global->y;
   vector<real_t>  &dy_global = mesh_global->dy;
#endif

   cl_mem &dev_H_global = state_global->dev_H;
   cl_mem &dev_U_global = state_global->dev_U;
   cl_mem &dev_V_global = state_global->dev_V;

   cl_mem &dev_celltype_global = mesh_global->dev_celltype;
   cl_mem &dev_i_global        = mesh_global->dev_i;
   cl_mem &dev_j_global        = mesh_global->dev_j;
   cl_mem &dev_level_global    = mesh_global->dev_level;

   cl_mem &dev_mpot_global     = state_global->dev_mpot;

   cl_mem &dev_celltype = mesh_local->dev_celltype;
   cl_mem &dev_i        = mesh_local->dev_i;
   cl_mem &dev_j        = mesh_local->dev_j;
   cl_mem &dev_level    = mesh_local->dev_level;

   cl_mem &dev_mpot     = state_local->dev_mpot;

   vector<int>     mpot;
   vector<int>     mpot_global;

   size_t old_ncells = ncells;
   size_t old_ncells_global = ncells_global;
   size_t new_ncells = 0;
   size_t new_ncells_global = 0;
   double H_sum = -1.0;
   double deltaT = 0.0;

   cl_command_queue command_queue = ezcl_get_command_queue();

   //  Main loop.
   for (int nburst = 0; nburst < outputInterval && ncycle < niter; nburst++, ncycle++) {

      // To reduce drift in solution
      if (do_sync) {
         ezcl_enqueue_read_buffer(command_queue, state_local->dev_H, CL_FALSE, 0, ncells*sizeof(cl_real),  (void *)&state_local->H[0],  NULL);
         ezcl_enqueue_read_buffer(command_queue, state_local->dev_U, CL_FALSE, 0, ncells*sizeof(cl_real),  (void *)&state_local->U[0],  NULL);
         ezcl_enqueue_read_buffer(command_queue, state_local->dev_V, CL_TRUE,  0, ncells*sizeof(cl_real),  (void *)&state_local->V[0],  NULL);

         ezcl_enqueue_read_buffer(command_queue, dev_H_global, CL_FALSE, 0, ncells_global*sizeof(cl_real),  (void *)&state_global->H[0],  NULL);
         ezcl_enqueue_read_buffer(command_queue, dev_U_global, CL_FALSE, 0, ncells_global*sizeof(cl_real),  (void *)&state_global->U[0],  NULL);
         ezcl_enqueue_read_buffer(command_queue, dev_V_global, CL_TRUE,  0, ncells_global*sizeof(cl_real),  (void *)&state_global->V[0],  NULL);
      }

      size_t local_work_size_global  = MIN(ncells_global, TILE_SIZE);
      size_t global_work_size_global = ((ncells_global+local_work_size_global - 1) /local_work_size_global) * local_work_size_global;
      size_t block_size_global     = global_work_size_global/local_work_size_global;

      size_t local_work_size  = MIN(ncells, TILE_SIZE);
      size_t global_work_size = ((ncells+local_work_size - 1) /local_work_size) * local_work_size;
      size_t block_size     = global_work_size/local_work_size;

      //  Define basic domain decomposition parameters for GPU.
      old_ncells = ncells;
      old_ncells_global = ncells_global;

      //  Calculate the real time step for the current discrete time step.
      double deltaT_cpu = -1.0;
      double deltaT_cpu_local = -1.0;
      if (do_cpu_calc) {
         deltaT_cpu = state_global->set_timestep(g, sigma);
         deltaT_cpu_local = state_local->set_timestep(g, sigma);
      }  //  Complete CPU timestep calculation.

      double deltaT_gpu = -1.0;
      double deltaT_gpu_local = -1.0;
      if (do_gpu_calc) {
         deltaT_gpu = state_global->gpu_set_timestep(sigma);
         deltaT_gpu_local = state_local->gpu_set_timestep(sigma);
      }  //  Complete GPU calculation.

      //  Compare time step values and pass deltaT in to the kernel.
      if (do_comparison_calc) {
         int iflag = 0;
         if (fabs(deltaT_cpu_local - deltaT_cpu) > .000001) iflag = 1;
         if (fabs(deltaT_gpu_local - deltaT_gpu) > .000001) iflag = 1;
         if (fabs(deltaT_gpu - deltaT_cpu) > .000001) iflag = 1;
         if (fabs(deltaT_gpu_local - deltaT_cpu_local) > .000001) iflag = 1;
         if (iflag) {
            printf("Error with deltaT calc --- cpu_local %lf cpu_global %lf gpu_local %lf gpu_global %lf\n",
               deltaT_cpu_local, deltaT_cpu, deltaT_gpu_local, deltaT_gpu);
         }
      }
      
      deltaT = (do_gpu_calc) ? deltaT_gpu_local : deltaT_cpu_local;
      simTime += deltaT;

      if (do_cpu_calc) {
         if (mesh_local->nlft == NULL) {
            mesh_global->calc_neighbors();
            mesh_local->calc_neighbors_local();
         }
      }

      if (do_gpu_calc) {
         if (mesh_local->dev_nlft == NULL) {
            mesh_global->gpu_calc_neighbors();
            mesh_local->gpu_calc_neighbors_local();
         }
      }

      if (do_comparison_calc) {
         mesh_local->compare_neighbors_all_to_gpu_local(mesh_global, &nsizes[0], &ndispl[0]);
      }

      mesh_local->partition_measure();

      // Currently not working -- may need to be earlier?
      //if (do_cpu_calc && ! mesh->have_boundary) {
      //  state->add_boundary_cells(mesh);
      //}

      // Apply BCs is currently done as first part of gpu_finite_difference and so comparison won't work here

      //  Execute main kernel
      if (do_cpu_calc) {
         state_global->calc_finite_difference(deltaT);
         state_local->calc_finite_difference(deltaT);
      }
      
      if (do_gpu_calc) {
         state_global->gpu_calc_finite_difference(deltaT);
         state_local->gpu_calc_finite_difference(deltaT);
      }
      
      if (do_comparison_calc) {
         state_local->compare_state_all_to_gpu_local(state_global, ncells, ncells_global, mype, ncycle, &nsizes[0], &ndispl[0]);
      }

      //  Size of arrays gets reduced to just the real cells in this call for have_boundary = 0
      if (do_cpu_calc) {
         state_global->remove_boundary_cells();
         state_local->remove_boundary_cells();
      }
      
      vector<int>      ioffset(block_size);
      vector<int>      ioffset_global(block_size_global);
      //vector<int>      newcount_global(block_size_global);

      if (do_cpu_calc) {
         mpot.resize(ncells_ghost);
         mpot_global.resize(ncells_global);
         state_global->calc_refine_potential(mpot_global, icount_global, jcount_global);
         state_local->calc_refine_potential(mpot, icount, jcount);
      }  //  Complete CPU calculation.

      if (do_gpu_calc) {
         new_ncells_global = state_global->gpu_calc_refine_potential(icount_global, jcount_global);
         new_ncells = state_local->gpu_calc_refine_potential(icount, jcount);
      }
      
      if (do_comparison_calc) {
         int icount_test;
         MPI_Allreduce(&icount, &icount_test, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
         if (icount_test != icount_global) {
            printf("%d: DEBUG -- icount is %d icount_test %d icount_global is %d\n",mype,icount,icount_test,icount_global);
         }

         if (state_local->dev_mpot) {
            mesh_local->compare_mpot_all_to_gpu_local(&mpot[0], &mpot_global[0], state_local->dev_mpot, state_global->dev_mpot, ncells_global, &nsizes[0], &ndispl[0], ncycle);
         }
      }

      // Sync up cpu array with gpu version to reduce differences due to minor numerical differences
      // otherwise cell count will diverge causing code problems and crashes
      if (state_local->dev_mpot) {
         if (do_sync) {
            ezcl_enqueue_read_buffer(command_queue, dev_mpot, CL_TRUE,  0, ncells*sizeof(cl_int), &mpot[0], NULL);
            ezcl_enqueue_read_buffer(command_queue, dev_mpot_global, CL_TRUE,  0, ncells_global*sizeof(cl_int), &mpot_global[0], NULL);
         }
         if (do_gpu_sync) {
            ezcl_enqueue_write_buffer(command_queue, dev_mpot, CL_TRUE,  0, ncells*sizeof(cl_int), &mpot[0], NULL);
            ezcl_enqueue_write_buffer(command_queue, dev_mpot_global, CL_TRUE,  0, ncells_global*sizeof(cl_int), &mpot_global[0], NULL);
         }
      }

      if (state_local->dev_mpot) {
         int mcount, mtotal;
         if (do_comparison_calc) {
            mesh_local->compare_ioffset_all_to_gpu_local(old_ncells, old_ncells_global, block_size, block_size_global, &mpot[0], &mpot_global[0], mesh_local->dev_ioffset, mesh_global->dev_ioffset, &ioffset[0], &ioffset_global[0], &mesh_global->celltype[0], &mesh_global->i[0], &mesh_global->j[0]);
         }
         if (do_gpu_sync) {
           mtotal = 0;
           for (uint ig=0; ig<(old_ncells+TILE_SIZE-1)/TILE_SIZE; ig++){
              mcount = 0;
              for (uint ic=ig*TILE_SIZE; ic<(ig+1)*TILE_SIZE; ic++){
                  if (ic >= old_ncells) break;
                  if (mesh_local->celltype[ic] == REAL_CELL) {
                     mcount += mpot[ic] ? 4 : 1;
                  } else {
                     mcount += mpot[ic] ? 2 : 1;
                  }
              }
              //ioffset[ig] = mtotal;
              mtotal += mcount;
           }
           ezcl_enqueue_write_buffer(command_queue, mesh_local->dev_ioffset, CL_TRUE, 0, block_size*sizeof(cl_int),       &ioffset[0], NULL);
           mtotal = 0;
           for (uint ig=0; ig<(old_ncells_global+TILE_SIZE-1)/TILE_SIZE; ig++){
              mcount = 0;
              for (uint ic=ig*TILE_SIZE; ic<(ig+1)*TILE_SIZE; ic++){
                  if (ic >= old_ncells_global) break;
                  if (mesh_global->celltype[ic] == REAL_CELL) {
                     mcount += mpot_global[ic] ? 4 : 1;
                  } else {
                     mcount += mpot_global[ic] ? 2 : 1;
                  }
              }
              //ioffset_global[ig] = mtotal;
              mtotal += mcount;
            }
            ezcl_enqueue_write_buffer(command_queue, mesh_global->dev_ioffset, CL_TRUE, 0, block_size_global*sizeof(cl_int),       &ioffset_global[0], NULL);
         }
      }

      if (do_cpu_calc) {
         state_global->rezone_all(icount_global, jcount_global, mpot_global);
         state_local->rezone_all(icount, jcount, mpot);
         mpot_global.clear();
         mpot.clear();
      }

      //  Resize the mesh, inserting cells where refinement is necessary.
      if (do_gpu_calc) {
         if (state_local->dev_mpot){
            state_global->gpu_rezone_all(icount_global, jcount_global, localStencil);
            state_local->gpu_rezone_all(icount, jcount, localStencil);
         }
      }

      ncells = new_ncells;
      mesh_local->ncells = new_ncells;

      ncells_global = new_ncells_global;
      mesh_global->ncells = new_ncells_global;

      if (do_comparison_calc) {
         state_local->compare_state_all_to_gpu_local(state_global, ncells, ncells_global, mype, ncycle, &nsizes[0], &ndispl[0]);

         mesh_local->compare_indices_all_to_gpu_local(mesh_global, ncells_global, &nsizes[0], &ndispl[0], ncycle);
      } // do_comparison_calc

      vector<int> nsizes_save(numpe);
      vector<int> ndispl_save(numpe);
      nsizes_save = nsizes;
      ndispl_save = ndispl;

      if (do_cpu_calc) {
         state_local->do_load_balance_local(new_ncells);
      }

      nsizes = nsizes_save;
      ndispl = ndispl_save;

      if (do_gpu_calc) {
         state_local->gpu_do_load_balance_local(new_ncells);
      }

      if (do_comparison_calc) {
         state_local->compare_state_all_to_gpu_local(state_global, ncells, ncells_global, mype, ncycle, &nsizes[0], &ndispl[0]);

         mesh_local->compare_indices_all_to_gpu_local(mesh_global, ncells_global, &nsizes[0], &ndispl[0], ncycle);
      } // do_comparison_calc

#ifdef XXX // not rewritten yet
      if (do_gpu_calc) {
         mesh->gpu_count_BCs(block_size, local_work_size, global_work_size, dev_ioffset);
      }
#endif

      ioffset.clear();
      ioffset_global.clear();

      H_sum = -1.0;

      if (do_comparison_calc) {
         double cpu_mass_sum = state_global->mass_sum(enhanced_precision_sum);
         double cpu_mass_sum_local = state_local->mass_sum(enhanced_precision_sum);
         double gpu_mass_sum = state_global->gpu_mass_sum(enhanced_precision_sum);
         H_sum = state_local->gpu_mass_sum(enhanced_precision_sum);
         int iflag = 0;
         if (fabs(cpu_mass_sum_local - cpu_mass_sum) > CONSERVATION_EPS) iflag = 1;
         //if (fabs(H_sum - gpu_mass_sum) > CONSERVATION_EPS) iflag = 1;
         //if (fabs(gpu_mass_sum - cpu_mass_sum) > CONSERVATION_EPS) iflag = 1;
         if (fabs(H_sum - cpu_mass_sum_local) > CONSERVATION_EPS) iflag = 1;

         if (iflag) {
            printf("Error with mass sum calculation -- cpu_mass_sum_local %lf cpu_mass_sum %lf gpu_mass_sum_local %lf gpu_mass_sum %lf\n",
                    cpu_mass_sum_local, cpu_mass_sum, H_sum, gpu_mass_sum);
         }
      }

      mesh_global->proc.resize(ncells_global);
      mesh_local->proc.resize(ncells);

      if (state_local->dev_mpot) {
         vector<int> index(ncells);
         vector<int> index_global(ncells_global);
         mesh_global->partition_cells(numpe, index_global, cycle_reorder);
         mesh_local->partition_cells(numpe, index, cycle_reorder);
         //state->state_reorder(index);
         if (do_gpu_sync) {
            ezcl_enqueue_write_buffer(command_queue, dev_celltype, CL_FALSE, 0, ncells*sizeof(cl_int),  (void *)&mesh_local->celltype[0],  NULL);
            ezcl_enqueue_write_buffer(command_queue, dev_i,     CL_FALSE, 0, ncells*sizeof(cl_int),  (void *)&mesh_local->i[0],  NULL);
            ezcl_enqueue_write_buffer(command_queue, dev_j,     CL_FALSE, 0, ncells*sizeof(cl_int),  (void *)&mesh_local->j[0],  NULL);
            ezcl_enqueue_write_buffer(command_queue, dev_level, CL_TRUE,  0, ncells*sizeof(cl_int),  (void *)&mesh_local->level[0],  NULL);
            ezcl_enqueue_write_buffer(command_queue, dev_celltype_global, CL_FALSE, 0, ncells_global*sizeof(cl_int),  (void *)&mesh_global->celltype[0],  NULL);
            ezcl_enqueue_write_buffer(command_queue, dev_i_global,     CL_FALSE, 0, ncells_global*sizeof(cl_int),  (void *)&mesh_global->i[0],  NULL);
            ezcl_enqueue_write_buffer(command_queue, dev_j_global,     CL_FALSE, 0, ncells_global*sizeof(cl_int),  (void *)&mesh_global->j[0],  NULL);
            ezcl_enqueue_write_buffer(command_queue, dev_level_global, CL_TRUE,  0, ncells_global*sizeof(cl_int),  (void *)&mesh_global->level[0],  NULL);
         }
      }

      if (do_gpu_sync) {
         ezcl_enqueue_write_buffer(command_queue, state_local->dev_H, CL_FALSE, 0, ncells*sizeof(cl_real),  (void *)&state_local->H[0],  NULL);
         ezcl_enqueue_write_buffer(command_queue, state_local->dev_U, CL_FALSE, 0, ncells*sizeof(cl_real),  (void *)&state_local->U[0],  NULL);
         ezcl_enqueue_write_buffer(command_queue, state_local->dev_V, CL_TRUE,  0, ncells*sizeof(cl_real),  (void *)&state_local->V[0],  NULL);
         ezcl_enqueue_write_buffer(command_queue, dev_H_global, CL_FALSE, 0, ncells_global*sizeof(cl_real),  (void *)&state_global->H[0],  NULL);
         ezcl_enqueue_write_buffer(command_queue, dev_U_global, CL_FALSE, 0, ncells_global*sizeof(cl_real),  (void *)&state_global->U[0],  NULL);
         ezcl_enqueue_write_buffer(command_queue, dev_V_global, CL_TRUE,  0, ncells_global*sizeof(cl_real),  (void *)&state_global->V[0],  NULL);
      }
      
   }  //  End burst loop

   if (H_sum < 0) {
      H_sum = state_local->mass_sum(enhanced_precision_sum);
   }
   if (isnan(H_sum)) {
      printf("Got a NAN on cycle %d\n",ncycle);
      exit(-1);
   }
   if (mype == 0){
      printf("Iteration %4d timestep %lf Sim Time %lf cells %ld Mass Sum %14.12lg Mass Change %12.6lg\n",
         ncycle, deltaT, simTime, ncells_global, H_sum, H_sum - H_sum_initial);
   }

#ifdef HAVE_GRAPHICS
   cl_mem dev_x  = ezcl_malloc(NULL, const_cast<char *>("dev_x"),  &ncells, sizeof(cl_real),  CL_MEM_READ_WRITE, 0);
   cl_mem dev_dx = ezcl_malloc(NULL, const_cast<char *>("dev_dx"), &ncells, sizeof(cl_real),  CL_MEM_READ_WRITE, 0);
   cl_mem dev_y  = ezcl_malloc(NULL, const_cast<char *>("dev_y"),  &ncells, sizeof(cl_real),  CL_MEM_READ_WRITE, 0);
   cl_mem dev_dy = ezcl_malloc(NULL, const_cast<char *>("dev_dy"), &ncells, sizeof(cl_real),  CL_MEM_READ_WRITE, 0);

   mesh_local->gpu_calc_spatial_coordinates(dev_x, dev_dx, dev_y, dev_dy);

   x.resize(ncells);
   dx.resize(ncells);
   y.resize(ncells);
   dy.resize(ncells);
   vector<real_t> H_graphics(ncells);

   ezcl_enqueue_read_buffer(command_queue, dev_x,  CL_FALSE, 0, ncells*sizeof(cl_real), (void *)&x[0],  &start_read_event);
   ezcl_enqueue_read_buffer(command_queue, dev_dx, CL_FALSE, 0, ncells*sizeof(cl_real), (void *)&dx[0], NULL);
   ezcl_enqueue_read_buffer(command_queue, dev_y,  CL_FALSE, 0, ncells*sizeof(cl_real), (void *)&y[0],  NULL);
   ezcl_enqueue_read_buffer(command_queue, dev_dy, CL_FALSE, 0, ncells*sizeof(cl_real), (void *)&dy[0], NULL);
   ezcl_enqueue_read_buffer(command_queue, state_local->dev_H,  CL_TRUE,  0, ncells*sizeof(cl_real), (void *)&H_graphics[0],  &end_read_event);

   gpu_time_graphics += ezcl_timer_calc(&start_read_event, &end_read_event);

   struct timeval tstart_cpu;
   cpu_timer_start(&tstart_cpu);

   ezcl_device_memory_remove(dev_x);
   ezcl_device_memory_remove(dev_dx);
   ezcl_device_memory_remove(dev_y);
   ezcl_device_memory_remove(dev_dy);

   x_global.resize(ncells_global);
   dx_global.resize(ncells_global);
   y_global.resize(ncells_global);
   dy_global.resize(ncells_global);
   vector<real_t> H_graphics_global(ncells_global);

   MPI_Allgatherv(&x[0],  ncells, MPI_C_REAL, &x_global[0],  &nsizes[0], &ndispl[0], MPI_C_REAL, MPI_COMM_WORLD);
   MPI_Allgatherv(&dx[0], ncells, MPI_C_REAL, &dx_global[0], &nsizes[0], &ndispl[0], MPI_C_REAL, MPI_COMM_WORLD);
   MPI_Allgatherv(&y[0],  ncells, MPI_C_REAL, &y_global[0],  &nsizes[0], &ndispl[0], MPI_C_REAL, MPI_COMM_WORLD);
   MPI_Allgatherv(&dy[0], ncells, MPI_C_REAL, &dy_global[0], &nsizes[0], &ndispl[0], MPI_C_REAL, MPI_COMM_WORLD);
   MPI_Allgatherv(&H_graphics[0],  ncells, MPI_C_REAL, &H_graphics_global[0],  &nsizes[0], &ndispl[0], MPI_C_REAL, MPI_COMM_WORLD);

   for (int ic=0; ic<(int)ncells; ic++){mesh_local->proc[ic]=mype;} 
   //MPI_Allgatherv(&mesh_local->proc[0],  ncells, MPI_INT, &mesh_global->proc[0],  &nsizes[0], &ndispl[0], MPI_C_REAL, MPI_COMM_WORLD);

   set_mysize(ncells_global);
   set_viewmode(view_mode);
   set_cell_coordinates(&x_global[0], &dx_global[0], &y_global[0], &dy_global[0]);
   set_cell_data(&H_graphics_global[0]);
   set_cell_proc(&mesh_global->proc[0]);
   set_circle_radius(circle_radius);
   draw_scene();
   MPI_Barrier(MPI_COMM_WORLD);

   gpu_time_graphics += (long)(cpu_timer_stop(tstart_cpu)*1.0e-9);
#endif

   //  Output final results and timing information.
   if (ncycle >= niter) {
      //free_display();
      
      //  Get overall program timing.
      double elapsed_time = cpu_timer_stop(tstart);
      
      state_global->output_timing_info(do_cpu_calc, do_gpu_calc, elapsed_time);
      state_local->output_timing_info(do_cpu_calc, do_gpu_calc, elapsed_time);

      state_local->parallel_timer_output(numpe,mype,"GPU:  graphics                 time was",(double) gpu_time_graphics * 1.0e-9 );

      mesh_local->print_partition_measure();
      mesh_local->print_calc_neighbor_type();
      mesh_local->print_partition_type();

      if (mype ==0){
         printf("CPU:  rezone frequency                \t %8.4f\tpercent\n",     (double)mesh_local->get_cpu_rezone_count()/(double)ncycle*100.0 );
         printf("CPU:  calc neigh frequency            \t %8.4f\tpercent\n",     (double)mesh_local->get_cpu_calc_neigh_count()/(double)ncycle*100.0 );
         printf("CPU:  load balance frequency          \t %8.4f\tpercent\n",     (double)mesh_local->get_cpu_load_balance_count()/(double)ncycle*100.0 );
         printf("GPU:  rezone frequency                \t %8.4f\tpercent\n",     (double)mesh_local->get_gpu_rezone_count()/(double)ncycle*100.0 );
         printf("GPU:  calc neigh frequency            \t %8.4f\tpercent\n",     (double)mesh_local->get_gpu_calc_neigh_count()/(double)ncycle*100.0 );
         printf("GPU:  load balance frequency          \t %8.4f\tpercent\n",     (double)mesh_local->get_gpu_load_balance_count()/(double)ncycle*100.0 );
         printf("GPU:  refine_smooth_iter per rezone   \t %8.4f\t\n",            (double)mesh_local->get_gpu_refine_smooth_count()/(double)mesh_local->get_gpu_rezone_count() );

         printf("CPU:  rezone frequency global         \t %8.4f\tpercent\n",     (double)mesh_global->get_cpu_rezone_count()/(double)ncycle*100.0 );
         printf("CPU:  calc neigh frequency global     \t %8.4f\tpercent\n",     (double)mesh_global->get_cpu_calc_neigh_count()/(double)ncycle*100.0 );
         printf("CPU:  load balance frequency global   \t %8.4f\tpercent\n",     (double)mesh_global->get_cpu_load_balance_count()/(double)ncycle*100.0 );
         printf("GPU:  rezone frequency global         \t %8.4f\tpercent\n",     (double)mesh_global->get_gpu_rezone_count()/(double)ncycle*100.0 );
         printf("GPU:  calc neigh frequency global     \t %8.4f\tpercent\n",     (double)mesh_global->get_gpu_calc_neigh_count()/(double)ncycle*100.0 );
         printf("GPU:  load balance frequency global   \t %8.4f\tpercent\n",     (double)mesh_global->get_gpu_load_balance_count()/(double)ncycle*100.0 );
         printf("GPU:  refine_smooth_iter per rezone   \t %8.4f\t\n",            (double)mesh_global->get_gpu_refine_smooth_count()/(double)mesh_global->get_gpu_rezone_count() );
      }

      ezcl_device_memory_remove(mesh_local->dev_corners_i);
      ezcl_device_memory_remove(mesh_local->dev_corners_j);
      ezcl_device_memory_remove(mesh_global->dev_corners_i);
      ezcl_device_memory_remove(mesh_global->dev_corners_j);

      if (mesh_local->dev_nlft != NULL){
         ezcl_device_memory_remove(mesh_local->dev_nlft);
         ezcl_device_memory_remove(mesh_local->dev_nrht);
         ezcl_device_memory_remove(mesh_local->dev_nbot);
         ezcl_device_memory_remove(mesh_local->dev_ntop);

         ezcl_device_memory_remove(mesh_global->dev_nlft);
         ezcl_device_memory_remove(mesh_global->dev_nrht);
         ezcl_device_memory_remove(mesh_global->dev_nbot);
         ezcl_device_memory_remove(mesh_global->dev_ntop);
      }

      mesh_local->terminate();
      state_local->terminate();
      mesh_global->terminate();
      state_global->terminate();
      ezcl_terminate();
      if (mesh_local->numpe > 1) L7_Free(&mesh_local->cell_handle);
      L7_Dev_Free();

      //  Release kernels and finalize the OpenCL elements.
      ezcl_finalize();

      ezcl_mem_walk_all();

      L7_Terminate();
      exit(0);
   }  //  Complete final output.
   
}


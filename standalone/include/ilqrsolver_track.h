#pragma once

#ifndef ILQRSOLVER_H
#define ILQRSOLVER_H

#include "config.h"
#include "kuka_arm_track.h"
#include "cost_function_kuka_arm_track.h"
#include <numeric>
#include <sys/time.h>
#include "SoftContactModel.h"


#include <Eigen/Dense>
#include <Eigen/StdVector>
#include <Eigen/Cholesky>
//#include <qpOASES.hpp>
//#include <qpOASES/QProblemB.hpp>

#define ENABLE_QPBOX 0
#define DISABLE_QPBOX 1
#define ENABLE_FULLDDP 0
#define DISABLE_FULLDDP 1

#ifndef DEBUG_ILQR
#define DEBUG_ILQR 1
#else
    #if PREFIX1(DEBUG_ILQR)==1
    #define DEBUG_ILQR 1
    #endif
#endif

#define TRACE(x) do { if (DEBUG_ILQR) printf(x);} while (0)

using namespace Eigen;
//USING_NAMESPACE_QPOASES


class ILQRSolver_TRK
{
public:
    struct traj
    {
        stateVecTab_t xList;
        commandVecTab_t uList;
        unsigned int iter;
        double finalCost;
        double finalGrad;
        double finalLambda;
        Eigen::VectorXd time_forward, time_backward, time_derivative; //computation time?
    };

    struct tOptSet {
        int n_hor;
        int debug_level;
        stateVec_t xInit;
        double new_cost, cost, dcost, lambda, dlambda, g_norm, expected;
        double **p;
        const double *alpha;
        int n_alpha;
        double lambdaMax;
        double lambdaMin;
        double lambdaInit;
        double dlambdaInit;
        double lambdaFactor;
        unsigned int max_iter;
        double tolGrad;
        double tolFun;
        double tolConstraint;
        double zMin;
        int regType;
        int iterations;
        int *log_linesearch;
        double *log_z;
        double *log_cost;
        double dV[2];
        
        double w_pen_l;
        double w_pen_f;
        double w_pen_max_l;
        double w_pen_max_f;
        double w_pen_init_l;
        double w_pen_init_f;
        double w_pen_fact1;
        double w_pen_fact2;
        
        int print;
        double print_head; // print headings every print_head lines
        double last_head;
        Eigen::VectorXd time_backward, time_forward, time_derivative;
        Eigen::VectorXd alphaList;
        // traj_t *nominal;
        // traj_t *candidates[NUMBER_OF_THREADS]; 
        // traj_t trajectories[NUMBER_OF_THREADS+1];
        // multipliers_t multipliers;
    };

public:
    ILQRSolver_TRK(KukaArm_TRK& iiwaDynamicModel, CostFunctionKukaArm_TRK& iiwaCostFunction, bool fullDDP=0,bool QPBox=0);
    stateVecTab_t updatedxList;
    stateVecTab_t xList; // vector/array of stateVec_t = basically knot config over entire time horizon
    commandVecTab_t uList;
    costVecTab_t costList;
private:
protected:
    // attributes //
public:
private:
    KukaArm_TRK* dynamicModel;
    CostFunctionKukaArm_TRK* costFunction;
    unsigned int stateNb;
    unsigned int commandNb;
    stateVec_t xInit; //matrix of <statesize, 1> = essentially a vector
    stateVec_t xgoal;
    unsigned int N;
    unsigned int iter;
    double dt;

    // stateVecTab_t xList; // vector/array of stateVec_t = basically knot config over entire time horizon
    // commandVecTab_t uList;

    stateVecTab_t xList_bar; 
    commandVecTab_t uList_bar;
    commandVecTab_t initCommand;

    commandVecTab_t uListFull;
    commandVecTab_t uList_bar_Full;
    commandVec_t u_NAN; //matrix of <commandsize, 1> = essentially a vector
    // stateVecTab_t updatedxList;
    commandVecTab_t updateduList;
    stateVecTab_t FList;
   
    costVecTab_t costListNew;
    struct traj lastTraj;
    struct timeval tbegin_time_fwd, tend_time_fwd, tbegin_time_bwd, tend_time_bwd, tbegin_time_deriv, tend_time_deriv;

    stateVecTab_t Vx;
    stateMatTab_t Vxx;

    stateVec_t Qx;
    stateMat_t Qxx;
    commandVec_t Qu;
    commandMat_t Quu;
    commandMat_t QuuF;
    commandMat_t QuuInv;
    commandR_stateC_t Qux;
    commandVec_t k;
    commandR_stateC_t K;
    commandVecTab_t kList;
    commandR_stateC_tab_t KList;
    double alpha;

    stateMat_t lambdaEye;
    unsigned int backPassDone;
    unsigned int fwdPassDone;
    unsigned int initFwdPassDone;
    unsigned int diverge;

    /* QP variables */
    //QProblemB* qp;
    bool enableQPBox;
    bool enableFullDDP;
    commandMat_t H;
    commandVec_t g;
    commandVec_t lowerCommandBounds;
    commandVec_t upperCommandBounds;
    commandVec_t lb;
    commandVec_t ub;
    // int nWSR;
    //real_t* xOpt;

    tOptSet Op;
    Eigen::Vector2d dV;
    bool debugging_print;    
    int newDeriv;
    double g_norm_i, g_norm_max, g_norm_sum;
    bool isUNan;
protected:
    // methods
public:
    void firstInitSolver(stateVec_t& iiwaxInit, stateVec_t& iiwaxDes, stateVecTab_t& x_bar, commandVecTab_t& u_bar, 
                    commandVecTab_t initialTorque, unsigned int& iiwaN, double& iiwadt, unsigned int& iiwamax_iter, double& iiwatolFun, double& iiwatolGrad);
    void solveTrajectory();
    void initializeTraj();
    void standardizeParameters(tOptSet *o);
    struct traj getLastSolvedTrajectory();
    void doBackwardPass();
    void doForwardPass();
    bool isPositiveDefinite(const commandMat_t & Quu); 
protected:
};


#endif // ILQRSOLVER_H
